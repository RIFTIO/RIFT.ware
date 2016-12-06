/*
 * Copyright (c) 2015 Nicira, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <config.h>

#include <errno.h>

#include "bitmap.h"
#include "column.h"
#include "dynamic-string.h"
#include "json.h"
#include "jsonrpc.h"
#include "ovsdb-error.h"
#include "ovsdb-parser.h"
#include "ovsdb.h"
#include "row.h"
#include "simap.h"
#include "hash.h"
#include "table.h"
#include "timeval.h"
#include "transaction.h"
#include "jsonrpc-server.h"
#include "monitor.h"
#include "openvswitch/vlog.h"


static const struct ovsdb_replica_class ovsdb_jsonrpc_replica_class;

/*  Backend monitor.
 *
 *  ovsdb_monitor keep track of the ovsdb changes.
 */

/* A collection of tables being monitored. */
struct ovsdb_monitor {
    struct ovsdb_replica replica;
    struct shash tables;     /* Holds "struct ovsdb_monitor_table"s. */
    struct ovs_list jsonrpc_monitors;  /* Contains "jsonrpc_monitor_node"s. */
    struct ovsdb *db;
    uint64_t n_transactions;      /* Count number of committed transactions. */
};

struct jsonrpc_monitor_node {
    struct ovsdb_jsonrpc_monitor *jsonrpc_monitor;
    struct ovs_list node;
};

/* A particular column being monitored. */
struct ovsdb_monitor_column {
    const struct ovsdb_column *column;
    enum ovsdb_monitor_selection select;
};

/* A row that has changed in a monitored table. */
struct ovsdb_monitor_row {
    struct hmap_node hmap_node; /* In ovsdb_jsonrpc_monitor_table.changes. */
    struct uuid uuid;           /* UUID of row that changed. */
    struct ovsdb_datum *old;    /* Old data, NULL for an inserted row. */
    struct ovsdb_datum *new;    /* New data, NULL for a deleted row. */
};

/* Contains 'struct ovsdb_monitor_row's for rows that have been
 * updated but not yet flushed to all the jsonrpc connection.
 *
 * 'n_refs' represent the number of jsonrpc connections that have
 * not received updates. Generate the update for the last jsonprc
 * connection will also destroy the whole "struct ovsdb_monitor_changes"
 * object.
 *
 * 'transaction' stores the first update's transaction id.
 * */
struct ovsdb_monitor_changes {
    struct ovsdb_monitor_table *mt;
    struct hmap rows;
    int n_refs;
    uint64_t transaction;
    struct hmap_node hmap_node;  /* Element in ovsdb_monitor_tables' changes
                                    hmap.  */
};

/* A particular table being monitored. */
struct ovsdb_monitor_table {
    const struct ovsdb_table *table;

    /* This is the union (bitwise-OR) of the 'select' values in all of the
     * members of 'columns' below. */
    enum ovsdb_monitor_selection select;

    /* Columns being monitored. */
    struct ovsdb_monitor_column *columns;
    size_t n_columns;

    /* Contains 'ovsdb_monitor_changes' indexed by 'transaction'. */
    struct hmap changes;
};

static void ovsdb_monitor_destroy(struct ovsdb_monitor *dbmon);
static void ovsdb_monitor_table_add_changes(struct ovsdb_monitor_table *mt,
                                            uint64_t next_txn);
static struct ovsdb_monitor_changes *ovsdb_monitor_table_find_changes(
    struct ovsdb_monitor_table *mt, uint64_t unflushed);
static void ovsdb_monitor_changes_destroy(
                                  struct ovsdb_monitor_changes *changes);
static void ovsdb_monitor_table_track_changes(struct ovsdb_monitor_table *mt,
                                  uint64_t unflushed);

static int
compare_ovsdb_monitor_column(const void *a_, const void *b_)
{
    const struct ovsdb_monitor_column *a = a_;
    const struct ovsdb_monitor_column *b = b_;

    return a->column < b->column ? -1 : a->column > b->column;
}

static struct ovsdb_monitor *
ovsdb_monitor_cast(struct ovsdb_replica *replica)
{
    ovs_assert(replica->class == &ovsdb_jsonrpc_replica_class);
    return CONTAINER_OF(replica, struct ovsdb_monitor, replica);
}

/* Finds and returns the ovsdb_monitor_row in 'mt->changes->rows' for the
 * given 'uuid', or NULL if there is no such row. */
static struct ovsdb_monitor_row *
ovsdb_monitor_changes_row_find(const struct ovsdb_monitor_changes *changes,
                               const struct uuid *uuid)
{
    struct ovsdb_monitor_row *row;

    HMAP_FOR_EACH_WITH_HASH (row, hmap_node, uuid_hash(uuid),
                             &changes->rows) {
        if (uuid_equals(uuid, &row->uuid)) {
            return row;
        }
    }
    return NULL;
}

/* Allocates an array of 'mt->n_columns' ovsdb_datums and initializes them as
 * copies of the data in 'row' drawn from the columns represented by
 * mt->columns[].  Returns the array.
 *
 * If 'row' is NULL, returns NULL. */
static struct ovsdb_datum *
clone_monitor_row_data(const struct ovsdb_monitor_table *mt,
                       const struct ovsdb_row *row)
{
    struct ovsdb_datum *data;
    size_t i;

    if (!row) {
        return NULL;
    }

    data = xmalloc(mt->n_columns * sizeof *data);
    for (i = 0; i < mt->n_columns; i++) {
        const struct ovsdb_column *c = mt->columns[i].column;
        const struct ovsdb_datum *src = &row->fields[c->index];
        struct ovsdb_datum *dst = &data[i];
        const struct ovsdb_type *type = &c->type;

        ovsdb_datum_clone(dst, src, type);
    }
    return data;
}

/* Replaces the mt->n_columns ovsdb_datums in row[] by copies of the data from
 * in 'row' drawn from the columns represented by mt->columns[]. */
static void
update_monitor_row_data(const struct ovsdb_monitor_table *mt,
                        const struct ovsdb_row *row,
                        struct ovsdb_datum *data)
{
    size_t i;

    for (i = 0; i < mt->n_columns; i++) {
        const struct ovsdb_column *c = mt->columns[i].column;
        const struct ovsdb_datum *src = &row->fields[c->index];
        struct ovsdb_datum *dst = &data[i];
        const struct ovsdb_type *type = &c->type;

        if (!ovsdb_datum_equals(src, dst, type)) {
            ovsdb_datum_destroy(dst, type);
            ovsdb_datum_clone(dst, src, type);
        }
    }
}

/* Frees all of the mt->n_columns ovsdb_datums in data[], using the types taken
 * from mt->columns[], plus 'data' itself. */
static void
free_monitor_row_data(const struct ovsdb_monitor_table *mt,
                      struct ovsdb_datum *data)
{
    if (data) {
        size_t i;

        for (i = 0; i < mt->n_columns; i++) {
            const struct ovsdb_column *c = mt->columns[i].column;

            ovsdb_datum_destroy(&data[i], &c->type);
        }
        free(data);
    }
}

/* Frees 'row', which must have been created from 'mt'. */
static void
ovsdb_monitor_row_destroy(const struct ovsdb_monitor_table *mt,
                          struct ovsdb_monitor_row *row)
{
    if (row) {
        free_monitor_row_data(mt, row->old);
        free_monitor_row_data(mt, row->new);
        free(row);
    }
}

struct ovsdb_monitor *
ovsdb_monitor_create(struct ovsdb *db,
                     struct ovsdb_jsonrpc_monitor *jsonrpc_monitor)
{
    struct ovsdb_monitor *dbmon;
    struct jsonrpc_monitor_node *jm;

    dbmon = xzalloc(sizeof *dbmon);

    ovsdb_replica_init(&dbmon->replica, &ovsdb_jsonrpc_replica_class);
    ovsdb_add_replica(db, &dbmon->replica);
    list_init(&dbmon->jsonrpc_monitors);
    dbmon->db = db;
    dbmon->n_transactions = 0;
    shash_init(&dbmon->tables);

    jm = xzalloc(sizeof *jm);
    jm->jsonrpc_monitor = jsonrpc_monitor;
    list_push_back(&dbmon->jsonrpc_monitors, &jm->node);

    return dbmon;
}

void
ovsdb_monitor_add_table(struct ovsdb_monitor *m,
                        const struct ovsdb_table *table)
{
    struct ovsdb_monitor_table *mt;

    mt = xzalloc(sizeof *mt);
    mt->table = table;
    shash_add(&m->tables, table->schema->name, mt);
    hmap_init(&mt->changes);
}

void
ovsdb_monitor_add_column(struct ovsdb_monitor *dbmon,
                         const struct ovsdb_table *table,
                         const struct ovsdb_column *column,
                         enum ovsdb_monitor_selection select,
                         size_t *allocated_columns)
{
    struct ovsdb_monitor_table *mt;
    struct ovsdb_monitor_column *c;

    mt = shash_find_data(&dbmon->tables, table->schema->name);

    if (mt->n_columns >= *allocated_columns) {
        mt->columns = x2nrealloc(mt->columns, allocated_columns,
                                 sizeof *mt->columns);
    }

    mt->select |= select;
    c = &mt->columns[mt->n_columns++];
    c->column = column;
    c->select = select;
}

/* Check for duplicated column names. Return the first
 * duplicated column's name if found. Otherwise return
 * NULL.  */
const char * OVS_WARN_UNUSED_RESULT
ovsdb_monitor_table_check_duplicates(struct ovsdb_monitor *m,
                                     const struct ovsdb_table *table)
{
    struct ovsdb_monitor_table *mt;
    int i;

    mt = shash_find_data(&m->tables, table->schema->name);

    if (mt) {
        /* Check for duplicate columns. */
        qsort(mt->columns, mt->n_columns, sizeof *mt->columns,
              compare_ovsdb_monitor_column);
        for (i = 1; i < mt->n_columns; i++) {
            if (mt->columns[i].column == mt->columns[i - 1].column) {
                return mt->columns[i].column->name;
            }
        }
    }

    return NULL;
}

static void
ovsdb_monitor_table_add_changes(struct ovsdb_monitor_table *mt,
                                uint64_t next_txn)
{
    struct ovsdb_monitor_changes *changes;

    changes = xzalloc(sizeof *changes);

    changes->transaction = next_txn;
    changes->mt = mt;
    changes->n_refs = 1;
    hmap_init(&changes->rows);
    hmap_insert(&mt->changes, &changes->hmap_node, hash_uint64(next_txn));
};

static struct ovsdb_monitor_changes *
ovsdb_monitor_table_find_changes(struct ovsdb_monitor_table *mt,
                                 uint64_t transaction)
{
    struct ovsdb_monitor_changes *changes;
    size_t hash = hash_uint64(transaction);

    HMAP_FOR_EACH_WITH_HASH(changes, hmap_node, hash, &mt->changes) {
        if (changes->transaction == transaction) {
            return changes;
        }
    }

    return NULL;
}

/* Stop currently tracking changes to table 'mt' since 'transaction'.
 *
 * Return 'true' if the 'transaction' is being tracked. 'false' otherwise. */
static void
ovsdb_monitor_table_untrack_changes(struct ovsdb_monitor_table *mt,
                                    uint64_t transaction)
{
    struct ovsdb_monitor_changes *changes =
                ovsdb_monitor_table_find_changes(mt, transaction);
    if (changes) {
        ovs_assert(changes->transaction == transaction);
        if (--changes->n_refs == 0) {
            hmap_remove(&mt->changes, &changes->hmap_node);
            ovsdb_monitor_changes_destroy(changes);
        }
    }
}

/* Start tracking changes to table 'mt' begins from 'transaction' inclusive.
 */
static void
ovsdb_monitor_table_track_changes(struct ovsdb_monitor_table *mt,
                                  uint64_t transaction)
{
    struct ovsdb_monitor_changes *changes;

    changes = ovsdb_monitor_table_find_changes(mt, transaction);
    if (changes) {
        changes->n_refs++;
    } else {
        ovsdb_monitor_table_add_changes(mt, transaction);
    }
}

static void
ovsdb_monitor_changes_destroy(struct ovsdb_monitor_changes *changes)
{
    struct ovsdb_monitor_row *row, *next;

    HMAP_FOR_EACH_SAFE (row, next, hmap_node, &changes->rows) {
        hmap_remove(&changes->rows, &row->hmap_node);
        ovsdb_monitor_row_destroy(changes->mt, row);
    }
    hmap_destroy(&changes->rows);
    free(changes);
}

/* Returns JSON for a <row-update> (as described in RFC 7047) for 'row' within
 * 'mt', or NULL if no row update should be sent.
 *
 * The caller should specify 'initial' as true if the returned JSON is going to
 * be used as part of the initial reply to a "monitor" request, false if it is
 * going to be used as part of an "update" notification.
 *
 * 'changed' must be a scratch buffer for internal use that is at least
 * bitmap_n_bytes(mt->n_columns) bytes long. */
static struct json *
ovsdb_monitor_compose_row_update(
    const struct ovsdb_monitor_table *mt,
    const struct ovsdb_monitor_row *row,
    bool initial, unsigned long int *changed)
{
    enum ovsdb_monitor_selection type;
    struct json *old_json, *new_json;
    struct json *row_json;
    size_t i;

    type = (initial ? OJMS_INITIAL
            : !row->old ? OJMS_INSERT
            : !row->new ? OJMS_DELETE
            : OJMS_MODIFY);
    if (!(mt->select & type)) {
        return NULL;
    }

    if (type == OJMS_MODIFY) {
        size_t n_changes;

        n_changes = 0;
        memset(changed, 0, bitmap_n_bytes(mt->n_columns));
        for (i = 0; i < mt->n_columns; i++) {
            const struct ovsdb_column *c = mt->columns[i].column;
            if (!ovsdb_datum_equals(&row->old[i], &row->new[i], &c->type)) {
                bitmap_set1(changed, i);
                n_changes++;
            }
        }
        if (!n_changes) {
            /* No actual changes: presumably a row changed and then
             * changed back later. */
            return NULL;
        }
    }

    row_json = json_object_create();
    old_json = new_json = NULL;
    if (type & (OJMS_DELETE | OJMS_MODIFY)) {
        old_json = json_object_create();
        json_object_put(row_json, "old", old_json);
    }
    if (type & (OJMS_INITIAL | OJMS_INSERT | OJMS_MODIFY)) {
        new_json = json_object_create();
        json_object_put(row_json, "new", new_json);
    }
    for (i = 0; i < mt->n_columns; i++) {
        const struct ovsdb_monitor_column *c = &mt->columns[i];

        if (!(type & c->select)) {
            /* We don't care about this type of change for this
             * particular column (but we will care about it for some
             * other column). */
            continue;
        }

        if ((type == OJMS_MODIFY && bitmap_is_set(changed, i))
            || type == OJMS_DELETE) {
            json_object_put(old_json, c->column->name,
                            ovsdb_datum_to_json(&row->old[i],
                                                &c->column->type));
        }
        if (type & (OJMS_INITIAL | OJMS_INSERT | OJMS_MODIFY)) {
            json_object_put(new_json, c->column->name,
                            ovsdb_datum_to_json(&row->new[i],
                                                &c->column->type));
        }
    }

    return row_json;
}

/* Constructs and returns JSON for a <table-updates> object (as described in
 * RFC 7047) for all the outstanding changes within 'monitor', and deletes all
 * the outstanding changes from 'monitor'.  Returns NULL if no update needs to
 * be sent.
 *
 * The caller should specify 'initial' as true if the returned JSON is going to
 * be used as part of the initial reply to a "monitor" request, false if it is
 * going to be used as part of an "update" notification.
 *
 * 'unflushed' should point to value that is the transaction ID that did
 * was not updated. The update contains changes between
 * ['unflushed, ovsdb->n_transcations]. Before the function returns, this
 * value will be updated to ovsdb->n_transactions + 1, ready for the next
 * update.  */
struct json *
ovsdb_monitor_compose_update(const struct ovsdb_monitor *dbmon,
                             bool initial, uint64_t *unflushed)
{
    struct shash_node *node;
    unsigned long int *changed;
    struct json *json;
    size_t max_columns;
    uint64_t prev_txn = *unflushed;
    uint64_t next_txn = dbmon->n_transactions + 1;

    max_columns = 0;
    SHASH_FOR_EACH (node, &dbmon->tables) {
        struct ovsdb_monitor_table *mt = node->data;

        max_columns = MAX(max_columns, mt->n_columns);
    }
    changed = xmalloc(bitmap_n_bytes(max_columns));

    json = NULL;
    SHASH_FOR_EACH (node, &dbmon->tables) {
        struct ovsdb_monitor_table *mt = node->data;
        struct ovsdb_monitor_row *row, *next;
        struct ovsdb_monitor_changes *changes;
        struct json *table_json = NULL;

        changes = ovsdb_monitor_table_find_changes(mt, prev_txn);
        if (!changes) {
            ovsdb_monitor_table_track_changes(mt, next_txn);
            continue;
        }

        HMAP_FOR_EACH_SAFE (row, next, hmap_node, &changes->rows) {
            struct json *row_json;

            row_json = ovsdb_monitor_compose_row_update(
                mt, row, initial, changed);
            if (row_json) {
                char uuid[UUID_LEN + 1];

                /* Create JSON object for transaction overall. */
                if (!json) {
                    json = json_object_create();
                }

                /* Create JSON object for transaction on this table. */
                if (!table_json) {
                    table_json = json_object_create();
                    json_object_put(json, mt->table->schema->name, table_json);
                }

                /* Add JSON row to JSON table. */
                snprintf(uuid, sizeof uuid, UUID_FMT, UUID_ARGS(&row->uuid));
                json_object_put(table_json, uuid, row_json);
            }

            hmap_remove(&changes->rows, &row->hmap_node);
            ovsdb_monitor_row_destroy(mt, row);
        }

        ovsdb_monitor_table_untrack_changes(mt, prev_txn);
        ovsdb_monitor_table_track_changes(mt, next_txn);
    }

    *unflushed = next_txn;
    free(changed);
    return json;
}

bool
ovsdb_monitor_needs_flush(struct ovsdb_monitor *dbmon,
                          uint64_t next_transaction)
{
    ovs_assert(next_transaction <= dbmon->n_transactions + 1);
    return (next_transaction <= dbmon->n_transactions);
}

void
ovsdb_monitor_table_add_select(struct ovsdb_monitor *dbmon,
                               const struct ovsdb_table *table,
                               enum ovsdb_monitor_selection select)
{
    struct ovsdb_monitor_table * mt;

    mt = shash_find_data(&dbmon->tables, table->schema->name);
    mt->select |= select;
}

struct ovsdb_monitor_aux {
    const struct ovsdb_monitor *monitor;
    struct ovsdb_monitor_table *mt;
};

static void
ovsdb_monitor_init_aux(struct ovsdb_monitor_aux *aux,
                       const struct ovsdb_monitor *m)
{
    aux->monitor = m;
    aux->mt = NULL;
}

static void
ovsdb_monitor_changes_update(const struct ovsdb_row *old,
                             const struct ovsdb_row *new,
                             const struct ovsdb_monitor_table *mt,
                             struct ovsdb_monitor_changes *changes)
{
    const struct uuid *uuid = ovsdb_row_get_uuid(new ? new : old);
    struct ovsdb_monitor_row *change;

    change = ovsdb_monitor_changes_row_find(changes, uuid);
    if (!change) {
        change = xzalloc(sizeof *change);
        hmap_insert(&changes->rows, &change->hmap_node, uuid_hash(uuid));
        change->uuid = *uuid;
        change->old = clone_monitor_row_data(mt, old);
        change->new = clone_monitor_row_data(mt, new);
    } else {
        if (new) {
            update_monitor_row_data(mt, new, change->new);
        } else {
            free_monitor_row_data(mt, change->new);
            change->new = NULL;

            if (!change->old) {
                /* This row was added then deleted.  Forget about it. */
                hmap_remove(&changes->rows, &change->hmap_node);
                free(change);
            }
        }
    }
}

static bool
ovsdb_monitor_change_cb(const struct ovsdb_row *old,
                        const struct ovsdb_row *new,
                        const unsigned long int *changed OVS_UNUSED,
                        void *aux_)
{
    struct ovsdb_monitor_aux *aux = aux_;
    const struct ovsdb_monitor *m = aux->monitor;
    struct ovsdb_table *table = new ? new->table : old->table;
    struct ovsdb_monitor_table *mt;
    struct ovsdb_monitor_changes *changes;

    if (!aux->mt || table != aux->mt->table) {
        aux->mt = shash_find_data(&m->tables, table->schema->name);
        if (!aux->mt) {
            /* We don't care about rows in this table at all.  Tell the caller
             * to skip it.  */
            return false;
        }
    }
    mt = aux->mt;

    HMAP_FOR_EACH(changes, hmap_node, &mt->changes) {
        ovsdb_monitor_changes_update(old, new, mt, changes);
    }
    return true;
}

void
ovsdb_monitor_get_initial(const struct ovsdb_monitor *dbmon)
{
    struct ovsdb_monitor_aux aux;
    struct shash_node *node;

    ovsdb_monitor_init_aux(&aux, dbmon);
    SHASH_FOR_EACH (node, &dbmon->tables) {
        struct ovsdb_monitor_table *mt = node->data;

        if (mt->select & OJMS_INITIAL) {
            struct ovsdb_row *row;

            if (hmap_is_empty(&mt->changes)) {
                ovsdb_monitor_table_add_changes(mt, 0);
            }

            HMAP_FOR_EACH (row, hmap_node, &mt->table->rows) {
                ovsdb_monitor_change_cb(NULL, row, NULL, &aux);
            }
        }
    }
}

void
ovsdb_monitor_remove_jsonrpc_monitor(struct ovsdb_monitor *dbmon,
                   struct ovsdb_jsonrpc_monitor *jsonrpc_monitor)
{
    struct jsonrpc_monitor_node *jm;

    /* Find and remove the jsonrpc monitor from the list.  */
    LIST_FOR_EACH(jm, node, &dbmon->jsonrpc_monitors) {
        if (jm->jsonrpc_monitor == jsonrpc_monitor) {
            list_remove(&jm->node);
            free(jm);

            /* Destroy ovsdb monitor if this is the last user.  */
            if (list_is_empty(&dbmon->jsonrpc_monitors)) {
                ovsdb_monitor_destroy(dbmon);
            }

            return;
        };
    }

    /* Should never reach here. jsonrpc_monitor should be on the list.  */
    OVS_NOT_REACHED();
}

static void
ovsdb_monitor_destroy(struct ovsdb_monitor *dbmon)
{
    struct shash_node *node;

    list_remove(&dbmon->replica.node);

    SHASH_FOR_EACH (node, &dbmon->tables) {
        struct ovsdb_monitor_table *mt = node->data;
        struct ovsdb_monitor_changes *changes, *next;

        HMAP_FOR_EACH_SAFE (changes, next, hmap_node, &mt->changes) {
            hmap_remove(&mt->changes, &changes->hmap_node);
            ovsdb_monitor_changes_destroy(changes);
        }
        free(mt->columns);
        free(mt);
    }
    shash_destroy(&dbmon->tables);
    free(dbmon);
}

static struct ovsdb_error *
ovsdb_monitor_commit(struct ovsdb_replica *replica,
                     const struct ovsdb_txn *txn,
                     bool durable OVS_UNUSED)
{
    struct ovsdb_monitor *m = ovsdb_monitor_cast(replica);
    struct ovsdb_monitor_aux aux;

    ovsdb_monitor_init_aux(&aux, m);
    ovsdb_txn_for_each_change(txn, ovsdb_monitor_change_cb, &aux);
    m->n_transactions++;

    return NULL;
}

static void
ovsdb_monitor_destroy_callback(struct ovsdb_replica *replica)
{
    struct ovsdb_monitor *dbmon = ovsdb_monitor_cast(replica);
    struct jsonrpc_monitor_node *jm, *next;

    /* Delete all front end monitors. Removing the last front
     * end monitor will also destroy the corresponding 'ovsdb_monitor'.
     * ovsdb monitor will also be destroied.  */
    LIST_FOR_EACH_SAFE(jm, next, node, &dbmon->jsonrpc_monitors) {
        ovsdb_jsonrpc_monitor_destroy(jm->jsonrpc_monitor);
    }
}

static const struct ovsdb_replica_class ovsdb_jsonrpc_replica_class = {
    ovsdb_monitor_commit,
    ovsdb_monitor_destroy_callback,
};
