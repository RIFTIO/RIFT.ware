
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */


/**
 * @file rwdts_xpath.c
 * @author Philip Joseph <philip.joseph@riftio.com>
 * @date 2015/09/04
 * @brief DTS XPath response reduction
 */

#include <stdlib.h>
#include <stdarg.h>
#include <protobuf-c/rift-protobuf-c.h>
#include <rwtypes.h>
#include <rwlib.h>
#include <rwdts.h>
#include <rwdts_int.h>
#include <rwdts_keyspec.h>
#include <rw_dts_int.pb-c.h>
#include <rwdts_xpath.h>

/********************************************************************
 *                                                                   *
 *                       V A R I A B L E S                           *
 *                                                                   *
 *********************************************************************/

const char *rwdts_xpath_action_str[] = {
  "~NONE~",
  "ancestor",
  "ancestor-or-self",
  "attribute",
  "child",
  "descendant",
  "descendant-or-self",
  "following",
  "following-sibling",
  "namespace",
  "parent",
  "preceding",
  "preceding-sibling",
  "self",
  "boolean",
  "ceiling",
  "concat",
  "contains",
  "count",
  "current",
  "false",
  "floor",
  "id",
  "lang",
  "last",
  "local-name",
  "name",
  "namespace-uri",
  "normalize-space",
  "not",
  "number",
  "position",
  "round",
  "starts-with",
  "string",
  "string-length",
  "substring",
  "substring-after",
  "substring-before",
  "sum",
  "translate",
  "true",
  "module-loaded",
  "feature-enabled",
  "rift-min",
  "rift-max",
  "rift-avg",
  "comment",
  "text",
  "processing-instruction",
  "node",
  " and ",
  " or ",
  " = ",
  " != ",
  " < ",
  " > ",
  " <= ",
  " >= ",
  " + ",
  " - ",
  " * ",
  " div ",
  " mod ",
  " -",
  " | ",
  "filter1",
  "filter2",
  "predicate",
  "~MAX~"
};

static struct {
  int checked;
  int val;
} venv_g;

// Struct for passing as user data to callbacks
typedef struct rwdts_xpath_cb_ud_s {
  int32_t id; // ID to correlate with the XPath expression part
  rwdts_xpath_pcb_t *rwpcb;
}  rwdts_xpath_cb_ud_t;


/***********************************************************
 * External functions
 ***********************************************************/

extern rw_status_t
rwdts_ncx_xpath1_parse_expr (rwdts_xpath_pcb_t *rwpcb,
                             const xmlChar *xpath);

extern ncx_instance_t*
rwdts_xpath_lib_init_ncx(const rw_yang_pb_schema_t* schema);

extern rw_status_t
rwdts_xpath_pcb_free_ncx (rwdts_xpath_pcb_t *rwpcb);

extern rw_status_t
rwdts_xpath_parse_ncx(rwdts_xpath_pcb_t *rwpcb,
                      const char* xpath);

extern rw_status_t
rwdts_xpath_load_schema_ncx(void *instance,
                            const rw_yang_pb_schema_t* schema);

extern rw_status_t
rwdts_xpath_lib_inst_free_ncx(void *instance);

/***********************************************************
 * Forward declarations
 ***********************************************************/

typedef struct rwdts_xpath_fn_names_hash_s rwdts_xpath_fn_names_hash_t;

// Do subsequent XPath processing including reductions and
// post processing in the callback
static void
rwdts_xpath_query_xact_cb(rwdts_xact_t *xact,
                          rwdts_xact_status_t*xact_status,
                          void *ud);

static void
rwdts_xpath_query_block_cb(rwdts_xact_t *xact,
                           rwdts_xact_status_t*xact_status,
                           void *ud);

static rw_status_t
rwdts_xpath_query_cont(rwdts_xpath_pcb_t* rwpcb);

static void
rwdts_xpath_expr_set_flag(rwdts_xpath_pcb_t *rwpcb,
                          int32_t id,
                          rwdts_xpath_flags_t flag);

static rw_status_t
rwdts_xpath_add_redn(rwdts_xpath_pcb_t *rwpcb,
                     RWDtsReduction* redn);

static RWDtsReduction*
rwdts_xpath_alloc_redn(rwdts_xpath_pcb_t *rwpcb,
                       int32_t deps);

static rw_status_t
rwdts_xpath_eval_id(rwdts_xpath_pcb_t *rwpcb,
                    int32_t id);

static rw_status_t
rwdts_xpath_eval_exop(rwdts_xpath_pcb_t *rwpcb,
                      int32_t id);

static rw_status_t
rwdts_xpath_eval_fn(rwdts_xpath_pcb_t *rwpcb,
                    int32_t id);

static rw_status_t
rwdts_xpath_eval_const(rwdts_xpath_pcb_t *rwpcb,
                       int32_t id);

static rw_status_t
rwdts_xpath_eval_path(rwdts_xpath_pcb_t *rwpcb,
                      int32_t id);

static rw_status_t
rwdts_xpath_eval_predicate(rwdts_xpath_pcb_t *rwpcb,
                           int32_t id);

static char*
rwdts_xpath_to_str (rwdts_xpath_pcb_t *rwpcb,
                    int32_t id);

static rw_status_t
rwdts_xpath_expr_to_str(rwdts_xpath_pcb_t *rwpcb,
                        int32_t id,
                        char *str);

static rw_status_t
rwdts_xpath_node_to_str(rwdts_xpath_pcb_t *rwpcb,
                        int32_t id,
                        char *str);

static rw_status_t
rwdts_xpath_predicate_to_str(rwdts_xpath_pcb_t *rwpcb,
                             int32_t id,
                             char *str);

static rw_status_t
rwdts_xpath_op_2_to_str(rwdts_xpath_pcb_t *rwpcb,
                        int32_t id,
                        char *str);

static rw_status_t
rwdts_xpath_const_to_str(rwdts_xpath_pcb_t *rwpcb,
                         int32_t id,
                         char *str);

static rw_status_t
rwdts_xpath_check_deps_flag(rwdts_xpath_pcb_t *rwpcb,
                            int32_t id,
                            rwdts_xpath_flags_t flag);

// static rw_status_t
// rwdts_xpath_alloc_resp_str(RWDtsRednRespEntry *entry);

static rw_status_t
rwdts_xpath_alloc_result(rwdts_xpath_pcb_t *rwpcb,
                         int32_t id);

static int32_t
rwdts_xpath_find_path_start(rwdts_xpath_pcb_t *rwpcb);

static rwdts_xpath_fn_names_hash_t*
rwdts_xpath_fname_hash();

static char *
rwdts_xpath_strcat (char* buf, const char *fmt, ...);

static RWDtsRednRespEntry*
rwdts_xpath_find_resp_entry(rwdts_xpath_pcb_t *rwpcb, int32_t id);

/*******************************************************************************/
void*
rwdts_xpath_lib_inst_new(const rw_yang_pb_schema_t* schema)
{
  return rwdts_xpath_lib_init_ncx(schema);
}

rw_status_t
rwdts_xpath_load_schema(void *inst, const rw_yang_pb_schema_t* schema)
{
  return rwdts_xpath_load_schema_ncx(inst, schema);
}

rw_status_t
rwdts_xpath_lib_inst_free(void *inst)
{
  return rwdts_xpath_lib_inst_free_ncx(inst);
}

rw_status_t
rwdts_xpath_pcb_free(rwdts_xpath_pcb_t *rwpcb)
{
  if (rwpcb == NULL) {
    return RW_STATUS_SUCCESS;
  }

  RW_ASSERT_TYPE(rwpcb, rwdts_xpath_pcb_t);

  if (rwpcb->xact) {
    rwdts_xact_t *xact = rwpcb->xact;
    RW_ASSERT_TYPE(xact, rwdts_xact_t);
    rwpcb->xact->redn_pcb = NULL;
    rwdts_xact_unref(xact, __FUNCTION__, __LINE__);
    rwpcb->xact = NULL;
  }

  rw_status_t rs = rwdts_xpath_pcb_free_ncx(rwpcb);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  if (rwpcb->query) {
    RW_FREE(rwpcb->query);
    rwpcb->query = NULL;
  }

  if (rwpcb->expr) {
    RWDtsRednExpression *expr = rwpcb->expr;
    while(expr->n_redn) {
      RWDtsReduction *entry = expr->redn[--expr->n_redn];
      rwdts_reduction__free_unpacked(NULL, entry);
#if 0
      if (entry->val_str) {
        RW_FREE(entry->val_str);
        entry->val_str = NULL;
      }
      RW_FREE(entry);
#endif
      expr->redn[expr->n_redn] = NULL;
    }
    RW_FREE(expr->redn);
    expr->redn = NULL;
    RW_FREE(expr);
    rwpcb->expr = NULL;
  }

  if (rwpcb->results) {
    RWDtsRednResp *results = rwpcb->results;
    while (results->n_entries) {
      RWDtsRednRespEntry *entry = results->entries[--results->n_entries];
      if (entry) {
        if (entry->key) {
          if (entry->key->keyspec) {
            rw_keyspec_entry_free((rw_keyspec_entry_t *)entry->key->keyspec, rwpcb->ksi);
            entry->key->keyspec = NULL;
          }
          if (entry->key->keystr) {
            RW_FREE(entry->key->keystr);
            entry->key->keystr = NULL;
          }
          RW_FREE(entry->key);
          entry->key = NULL;
        }
        if (entry->leaf) {
          if (entry->leaf->keyspec) {
            rw_keyspec_entry_free((rw_keyspec_entry_t *)entry->leaf->keyspec, rwpcb->ksi);
            entry->leaf->keyspec = NULL;
          }
          if (entry->leaf->keystr) {
            RW_FREE(entry->leaf->keystr);
            entry->leaf->keystr = NULL;
          }
          RW_FREE(entry->leaf);
          entry->leaf = NULL;
        }
        if (entry->val_str) {
          RW_FREE(entry->val_str);
          entry->val_str = NULL;
        }
        if (entry->xpath) {
          RW_FREE(entry->xpath);
        }
        RW_FREE(entry);
        results->entries[results->n_entries] = NULL;
      }
    }
    if (results->entries) {
      RW_FREE(results->entries);
      results->entries = NULL;
    }
    RW_FREE(rwpcb->results);
    rwpcb->results = NULL;
  }

  RW_FREE_TYPE(rwpcb, rwdts_xpath_pcb_t);
  return rs;
} /*rwdts_xpath_pcb_free */

rw_status_t
rwdts_xpath_parse(const char* xpath,
                  rw_xpath_type_t xpath_type,
                  rw_keyspec_instance_t* ksi,
                  rwdts_api_t *apih,
                  rwdts_xpath_pcb_t **rwpcb)
{
  rw_status_t rs = RW_STATUS_FAILURE;

  RW_ASSERT(xpath);
  RW_ASSERT(xpath_type == RW_XPATH_KEYSPEC);

  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RW_ASSERT(NULL == *rwpcb);
  rwdts_xpath_pcb_t* xpcb = RW_MALLOC0_TYPE(sizeof(rwdts_xpath_pcb_t), rwdts_xpath_pcb_t);
  RW_ASSERT(xpcb);
  xpcb->loc_path_id = -1;

  const rw_yang_pb_schema_t *schema = rwdts_api_get_ypbc_schema(apih);
  xpcb->apih = apih;
  xpcb->schema = schema;

  if (apih->xpath_lib_inst) {
    xpcb->lib_inst = apih->xpath_lib_inst;
    xpcb->local_inst = false;
  }
  else {
    xpcb->lib_inst = (void *)rwdts_xpath_lib_init_ncx(schema);
    if (!xpcb->lib_inst) {
      RW_FREE_TYPE(xpcb, rwdts_xpath_pcb_t);
      xpcb = NULL;
      return RW_STATUS_FAILURE;
    }
    apih->xpath_lib_inst = xpcb->lib_inst;
    xpcb->local_inst = false;
  }

  xpcb->ksi = ksi?ksi:(apih?&(apih->ksi):NULL);
  RW_ASSERT(xpcb->ksi);

  // Check if category is specified in the xpath
  RwSchemaCategory category = RW_SCHEMA_CATEGORY_ANY;
  if ((strlen(xpath) > 2) && (xpath[1] == ',')) {
    switch (xpath[0]) {
      case 'D':
        category = RW_SCHEMA_CATEGORY_DATA;
        break;
      case 'C':
        category = RW_SCHEMA_CATEGORY_CONFIG;
        break;
      case 'I':
        category = RW_SCHEMA_CATEGORY_RPC_INPUT;
        break;
      case 'O':
        category = RW_SCHEMA_CATEGORY_RPC_OUTPUT;
        break;
      case 'N':
        category = RW_SCHEMA_CATEGORY_NOTIFICATION;
        break;
      case 'A':
        break;
      default:
        RW_CRASH();
        break;
    }
    xpath += 2; // Skip the category
  }
  xpcb->cat = category;

  if (xpath) {
    xpcb->query = strndup(xpath, RWDTS_XPATH_PATH_MAX_SIZE);
  }

  rs = rwdts_xpath_parse_ncx(xpcb, xpath);
  if (RW_STATUS_SUCCESS == rs) {
    // TODO: Integrate with tracing or logging instead of this hack
    if (!venv_g.checked) {
      const char *e = getenv("V");
      if (e && atoi(e) == 1) {
        venv_g.val = 1;
      }
      venv_g.checked = 1;
    }
    if (venv_g.val) {
      xpcb->debug = true;
    }

    xpcb->parsed = true;
    *rwpcb = xpcb;
  }
  else {
    *rwpcb = NULL;
    rwdts_xpath_pcb_free(xpcb);
  }
  return rs;
} /* rwdts_xpath_parse */

// Start the XPath expression evaluation
rw_status_t
rwdts_xpath_eval(rwdts_xpath_pcb_t *rwpcb)
{
  RW_ASSERT_TYPE(rwpcb, rwdts_xpath_pcb_t);
  RW_ASSERT(rwpcb->parsed);
  rw_status_t rs = RW_STATUS_SUCCESS;

  if (rwpcb->results == NULL) {
    rwpcb->results = RW_MALLOC0(sizeof(RWDtsRednResp));
    RW_ASSERT(rwpcb->results);
    rwdts_redn_resp__init(rwpcb->results);
    rwpcb->loc_path_id = -1;
    rwpcb->predicate = 0;
    rwpcb->depth = 0;
  }

  rwpcb->pass++;

  bool pending = false;
  int32_t i = 0;
  while ((RW_STATUS_FAILURE != rs) && (i < rwpcb->expr->n_redn)){
    rs = rwdts_xpath_eval_id(rwpcb, i++);
    if (RW_STATUS_PENDING == rs) {
      pending = true;
    }
  }

  if ((RW_STATUS_FAILURE != rs) && (pending)) {
    rs = RW_STATUS_PENDING;
  }

  return rs;
} /* rwdts_xpath_eval */

rw_keyspec_path_t*
rwdts_xpath_get_single_query_ks(rwdts_xpath_pcb_t *rwpcb)
{
  RW_ASSERT_TYPE(rwpcb, rwdts_xpath_pcb_t);
  RW_ASSERT(rwpcb->pass); // Make sure atleast one eval is done

  rw_keyspec_path_t* ks = NULL;

  // Check if the whole expression is a single path
  bool single = true;
  int32_t i = 0;
  // Check if the whole expression has been parsed
  for (; i<rwpcb->expr->n_redn; i++) {
    if (!(rwpcb->expr->redn[i]->flags & RWDTS_XPATH_FLAG_PARSED)) {
      single = false;
      break;
    }
  }
  if (single) {
    i = 0;
    RWDtsReduction *redn = rwpcb->expr->redn[0];
    if (redn->has_start_id && redn->id == redn->start_id) {
      int32_t start_id = redn->start_id;
      while (redn->has_next_id) {
        redn = rwpcb->expr->redn[redn->next_id];
        if (!redn->has_start_id || start_id != redn->start_id) {
          single = false;
          break;
        }
      }
      // Check end of path is same as expression
      if (redn->id + 1 != rwpcb->expr->n_redn) {
        single = false;
      }
    }
  }

  if (single) {
    // There should be a single result entry
    if (rwpcb->results && rwpcb->results->n_entries) {
      RWDtsRednRespEntry* resp = rwpcb->results->entries[0];
      RW_ASSERT(resp);
      if (!resp->leaf) {
        if (resp->key && resp->key->keyspec) {
          rw_keyspec_path_create_dup((rw_keyspec_path_t*)resp->key->keyspec, rwpcb->ksi, &ks);
          RW_ASSERT(ks);
        }
      }
    }
  }

  return ks;
} /* rwdts_xpath_get_single_query */

RWDtsRednRespEntry*
rwdts_xpath_alloc_pbcm (int32_t id)
{
  RWDtsRednRespEntry *entry = RW_MALLOC0(sizeof(RWDtsRednRespEntry));
  if (entry) {
    rwdts_redn_resp_entry__init(entry);
    entry->id = id;
  }
  return entry;
}

#define RWDTS_XPATH_QUERY_RESULT_SLAB 10

// Update xact with the reduced results
rw_status_t
rwdts_xpath_query_result_update(rwdts_xpath_pcb_t *rwpcb)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  RW_ASSERT_TYPE(rwpcb, rwdts_xpath_pcb_t);

  rwdts_xact_t *xact = rwpcb->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  // Check if any results are pending in xact
  rwdts_query_result_t** qres = RW_MALLOC0(sizeof(rwdts_query_result_t*)*RWDTS_XPATH_QUERY_RESULT_SLAB);
  RW_ASSERT(qres);
  int32_t idx = 0;
  rwdts_query_result_t *qrslt = rwdts_xact_query_result(xact, 0);
  while (qrslt){
    if (idx && (idx%RWDTS_XPATH_QUERY_RESULT_SLAB == 0)) {
      qres = realloc(qres, sizeof(rwdts_query_result_t*)*(idx+RWDTS_XPATH_QUERY_RESULT_SLAB));
      RW_ASSERT(qres);
    }
    qres[idx] = RW_MALLOC0(sizeof(rwdts_query_result_t));
    RW_ASSERT(qres[idx]);
    rs = rw_keyspec_path_create_dup(qrslt->keyspec, &xact->ksi, &qres[idx]->keyspec);
    RW_ASSERT(RW_STATUS_SUCCESS == rs);
    qres[idx]->message = protobuf_c_message_duplicate(NULL, qrslt->message,
                                                      qrslt->message->descriptor);
    RW_ASSERT(qres[idx]->message);
#if 0 /* TBD Fix me */
    if (qres[idx]->key_xpath) {
      size_t len = strlen(qrslt->key_xpath);
      qres[idx]->key_xpath = RW_MALLOC0(len+1);
      if (qres[idx]->key_xpath) {
        strncpy(qres[idx]->key_xpath, qrslt->key_xpath, len);
      }
    }
#endif
    qres[idx]->corrid = qrslt->corrid;
    qrslt = rwdts_xact_query_result(xact, 0);
    idx++;
  }
  uint32_t cnt = idx;

  // Set all the blocks to done
  int blk_count;
  for (blk_count = 0; blk_count < xact->blocks_ct; blk_count++) {
    if (xact->blocks[blk_count]) {
      xact->blocks[blk_count]->responses_done = TRUE;
    }
  }

  // Clear all the results in the xact
  rwdts_xact_query_result_list_deinit(xact);
  RW_ASSERT(xact->track.query_results == NULL);

  // Initialize a response block
  RWDtsXactResult* rsp = RW_MALLOC0(sizeof(RWDtsXactResult));
  RW_ASSERT(rsp);
  rwdts_xact_result__init(rsp);
  RWDtsXactBlockResult *b_result = RW_MALLOC0(sizeof(RWDtsXactBlockResult));
  RW_ASSERT(b_result);
  rwdts_xact_block_result__init(b_result);
  b_result->blockidx = xact->blocks[0]->subx.block->blockidx;
  rsp->result = RW_MALLOC0(sizeof(RWDtsXactBlockResult*));
  RW_ASSERT(rsp->result);
  rsp->result[0] = b_result;
  rsp->n_result++;

  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RWDtsRednResp *resp = rwpcb->results;
  RW_ASSERT(resp);

  RWDtsQueryResult *q_result = RW_MALLOC0(sizeof(RWDtsQueryResult));
  RW_ASSERT(q_result);
  rwdts_query_result__init(q_result);

  int32_t i = 0;
  for(; i<resp->n_entries; i++){
    RWDtsRednRespEntry *entry = resp->entries[i];
    if (!(entry->flags & RWDTS_XPATH_RESP_DONE)) {
      if (entry->key) {
        rw_keyspec_path_t *key = NULL;
        ProtobufCMessage *msg = NULL;
        RWDtsQuerySingleResult* s_result = NULL;
        protobuf_c_boolean ok = false;
        rwdts_xpath_pbcm_type_t msg_type = RWDTS_XPATH_PBCM_NONE;
        if (entry->leaf) {
          key = (rw_keyspec_path_t*)entry->leaf->keyspec;
          if (entry->has_count|entry->has_val_uint64|entry->has_val_sint64|entry->has_val_double) {
            if ((expr->redn[entry->id]->action == RWDTS_XPATH_ACTION_FN_AVG) ||
                (expr->redn[entry->id]->action == RWDTS_XPATH_ACTION_FN_COUNT)){
              msg = protobuf_c_message_duplicate(NULL, (ProtobufCMessage*)entry, entry->base.descriptor);
              RW_ASSERT(msg);
              msg_type = RWDTS_XPATH_PBCM_MSG;
              ok = true;
            }
            else {
              const ProtobufCFieldDescriptor* fd = rw_keyspec_path_get_leaf_field_desc(key);
              RW_ASSERT(fd);
              // Store the complete keyspec with leaf, so we can identify later which leaf to extract
              msg = rw_keyspec_pbcm_create_from_keyspec(NULL, key);
              RW_ASSERT(msg);

              char buf[64];
              buf[0] = 0;
              if (entry->has_val_uint64) {
                snprintf (buf, 63, "%lu", entry->val_uint64);
              }
              else if (entry->has_val_sint64) {
                snprintf (buf, 63, "%li", entry->val_sint64);
              }
              else if (entry->has_val_double) {
                snprintf (buf, 63, "%lf", entry->val_double);
              }
              ok = protobuf_c_message_set_field_text_value(NULL, msg, fd, buf, strlen(buf));
              msg_type = RWDTS_XPATH_PBCM_LEAF;
            }
          }
        }
        else {
          key = (rw_keyspec_path_t*)entry->key->keyspec;
          if (entry->has_count) {
            if (expr->redn[entry->id]->action == RWDTS_XPATH_ACTION_FN_COUNT) {
              msg = protobuf_c_message_duplicate(NULL, (ProtobufCMessage*)entry, entry->base.descriptor);
              RW_ASSERT(msg);
              msg_type = RWDTS_XPATH_PBCM_MSG;
              ok = true;
            }
          }
        }
        if (ok) {
          s_result = rwdts_alloc_single_query_result(xact, msg, key);
          RW_ASSERT(s_result);

          s_result->redn_resp_id = msg_type;
          if (q_result->n_result) {
            q_result->result = realloc(q_result, (q_result->n_result+1)*sizeof(RWDtsQuerySingleResult*));
          }
          else {
            q_result->result = RW_MALLOC0(sizeof(RWDtsQuerySingleResult*));
          }
          q_result->result[q_result->n_result++] = s_result;
          if (q_result->redn_resp_id < s_result->redn_resp_id) {
            q_result->redn_resp_id = RWDTS_XPATH_PBCM_LEAF;
          }
        }
        entry->flags |= RWDTS_XPATH_RESP_DONE;
        if (msg) {
          protobuf_c_message_free_unpacked(NULL, msg); 
          msg = NULL;
        }
      }
    }
  }

  if (rwpcb->debug) {
    rwdts_xpath_print_pcb(rwpcb, __FUNCTION__);
  }

  if (RW_STATUS_SUCCESS == rs) {
    if (q_result->n_result) {
      rwdts_xact_result_t *res = RW_MALLOC0_TYPE(sizeof(rwdts_xact_result_t), rwdts_xact_result_t);
      RW_ASSERT(res);
      q_result->block = (RWDtsXactBlkID*)protobuf_c_message_duplicate(NULL, &xact->blocks[0]->subx.block->base,
                                                                      xact->blocks[0]->subx.block->base.descriptor);
      res->result = q_result;
      res->blockidx = b_result->blockidx;
      b_result->result = RW_MALLOC0(sizeof(RWDtsQueryResult*));
      RW_ASSERT(b_result->result);
      b_result->result[0] = q_result;
      b_result->n_result = 1;
      RW_DL_ENQUEUE(&(xact->track.pending), res, elem);
      rsp->has_redn_resp = true;
      rsp->redn_resp = true;
      q_result = NULL;
    }

    if (cnt) {
      if (b_result->n_result) {
        b_result->result = realloc(b_result->result, (b_result->n_result+cnt)*sizeof(RWDtsQueryResult*));
      }
      else {
        b_result->result = RW_MALLOC0(cnt*sizeof(RWDtsQueryResult*));
        rsp->has_redn_resp = true;
        rsp->redn_resp = true;
      }
      RW_ASSERT(b_result->result);
      for(idx=0; idx<cnt; idx++) {
        rwdts_xact_result_t *res = RW_MALLOC0_TYPE(sizeof(rwdts_xact_result_t), rwdts_xact_result_t);
        RW_ASSERT(res);
        res->blockidx = b_result->blockidx;
        if (q_result == NULL) {
          q_result = RW_MALLOC0(sizeof(RWDtsQueryResult));
          RW_ASSERT(q_result);
          rwdts_query_result__init(q_result);
        }
        RWDtsQuerySingleResult* s_result = rwdts_alloc_single_query_result(xact, qres[idx]->message,
                                                                           qres[idx]->keyspec);
        RW_ASSERT(s_result);
        s_result->redn_resp_id = RWDTS_XPATH_PBCM_NC;
        q_result->result = RW_MALLOC0(sizeof(RWDtsQuerySingleResult*));
        q_result->result[0] = s_result;
        q_result->n_result = 1;
        q_result->redn_resp_id = RWDTS_XPATH_PBCM_NC;
        q_result->block = (RWDtsXactBlkID*)protobuf_c_message_duplicate(NULL, &xact->blocks[0]->subx.block->base,
                                                                        xact->blocks[0]->subx.block->base.descriptor);
        RW_ASSERT(q_result->block);
        b_result->result[b_result->n_result++] = q_result;
        res->result = q_result;
        q_result = NULL;
        RW_DL_ENQUEUE(&(xact->track.pending), res, elem);
      }
    }

    if (rsp) {
      // Insert the result to the transaction so that it can be cleaned up when the transaction is cleaned up
      rwdts_xact_result_track_t* track_result = RW_MALLOC0_TYPE(sizeof(rwdts_xact_result_track_t), rwdts_xact_result_track_t);
      track_result->result = rsp;
      RW_DL_PUSH(&(xact->track.results), track_result, res_elem);
    }
    xact->xres = rsp;
  }

  for(idx=0; idx<cnt; idx++) {
    if (qres[idx]->keyspec) {
      rw_keyspec_path_free(qres[idx]->keyspec, &xact->ksi);
      qres[idx]->keyspec = NULL;
    }
    if (qres[idx]->message) {
      protobuf_c_message_free_unpacked(NULL, qres[idx]->message);
      qres[idx]->message = NULL;
    }
    RW_FREE(qres[idx]);
    qres[idx] = NULL;
  }
  RW_FREE(qres);

  return rs;
}

// Custom callback for internally generated queries
static void
rwdts_xpath_query_xact_cb(rwdts_xact_t *xact,
                          rwdts_xact_status_t* xact_status,
                          void *ud)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT(xact_status);
  RW_ASSERT(ud);

  rwdts_xpath_pcb_t *rwpcb = (rwdts_xpath_pcb_t *)ud;
  RW_ASSERT_TYPE(rwpcb, rwdts_xpath_pcb_t);
  RW_ASSERT(rwpcb->expr);
  RW_ASSERT(rwpcb->results);

  rwpcb->xact = xact;
  rw_status_t rs = rwdts_xpath_eval(rwpcb);

  if (rwpcb->debug) {
    rwdts_xpath_print_pcb(rwpcb, __FUNCTION__);
  }

  if (RW_STATUS_PENDING == rs) {
    rs = rwdts_xpath_query_cont(rwpcb);
  }
  else {
    rs = rwdts_xpath_query_result_update(rwpcb);

    // Call the client cb
    if (rwpcb->xact_cb.cb) {
      (rwpcb->xact_cb.cb)(xact, xact_status, (void*)rwpcb->xact_cb.ud);
    }

    xact->redn_pcb = NULL;
    rwpcb->xact = NULL;
    rwdts_xact_unref(xact, __FUNCTION__, __LINE__);
    rwdts_xpath_pcb_free(rwpcb);
  }
} /* rwdts_xpath_query_xact_cb */

static void
rwdts_xpath_query_block_cb(rwdts_xact_t *xact,
                           rwdts_xact_status_t* xact_status,
                           void *ud)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT(xact_status);
  RW_ASSERT(ud);

  rwdts_xpath_pcb_t *rwpcb = (rwdts_xpath_pcb_t *)ud;
  RW_ASSERT(rwpcb->expr);
  RW_ASSERT(rwpcb->results);

  // TODO: Do the reduction

  // Call the client cb
  if (rwpcb->blk_cb.cb) {
    (rwpcb->blk_cb.cb)(xact, xact_status, (void*)rwpcb->blk_cb.ud);
  }

  // ATTN: Not complete
} /* rwdts_xpath_query_block_cb */

static rw_status_t
rwdts_xpath_query_cont(rwdts_xpath_pcb_t* rwpcb)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  RW_ASSERT_TYPE(rwpcb, rwdts_xpath_pcb_t);
  RWDtsRednExpression *expr = rwpcb->expr;
  RWDtsRednResp *resp = rwpcb->results;

  rwdts_xact_t *xact = rwpcb->xact;
  RW_ASSERT(xact);
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  rwdts_xact_block_t *blk = rwdts_xact_block_create(xact);
  if (blk == NULL) {
    snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
            "%s: Error adding a block to xact for %s",
            __FUNCTION__, rwpcb->query);
    rs = RW_STATUS_FAILURE;
  }

  uint32_t i = 0;
  bool found = false;
  for(; (rs == RW_STATUS_SUCCESS) && (i<resp->n_entries); i++) {
    RWDtsRednRespEntry *entry = resp->entries[i];
    if ((entry->flags & RWDTS_XPATH_RESP_DONE) ||
        (entry->flags & RWDTS_XPATH_RESP_PATH_PART)) {
      continue;
    }
    else if (entry->key) {
      if (expr->redn[entry->id]->flags & RWDTS_XPATH_FLAG_QUERY) {
        // Query already sent
        continue;
      }
      found = true;
      if (rwpcb->msg && protobuf_c_message_unknown_get_count(rwpcb->msg)) {
        rwdts_report_unknown_fields((rw_keyspec_path_t*) entry->key->keyspec, rwpcb->apih,
                                    (ProtobufCMessage*)rwpcb->msg);
      }

      rw_status_t rs = rwdts_xact_block_add_query_ks(blk,
                                                     (rw_keyspec_path_t *)entry->key->keyspec,
                                                     rwpcb->action,
                                                     rwpcb->flags,
                                                     entry->id+1, // Add the entry index as corr id
                                                     rwpcb->msg);

      if (rs != RW_STATUS_SUCCESS) {
        snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
                "%s: Error adding query (%s) to block",
                __FUNCTION__, entry->xpath);
        break;
      }

      // Mark path as queried
      rwdts_xpath_expr_set_flag(rwpcb, entry->id, RWDTS_XPATH_FLAG_QUERY);
      rwpcb->qcount++;
    }
  }

  if (found) {
    if (rs == RW_STATUS_SUCCESS) {
      rs = rwdts_xact_block_execute(blk, rwpcb->flags, rwdts_xpath_query_block_cb, rwpcb, NULL);
      if (rs != RW_STATUS_SUCCESS) {
        snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
                 "%s: Error executing query (%s)",
                 __FUNCTION__, rwpcb->query);
      }
    }
  }
  else {
    snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
             "%s: Did not any keyspec in query (%s)",
             __FUNCTION__, rwpcb->query);
    rs = RW_STATUS_FAILURE;
  }

  if (rwpcb->debug) {
    rwdts_xpath_print_pcb(rwpcb, __FUNCTION__);
  }

  return rs;
}

rw_status_t
rwdts_xpath_query_int(rwdts_xpath_pcb_t*       rwpcb,
                      RWDtsQueryAction         action,
                      uint32_t                 flags,
                      rwdts_event_cb_t*        cb,
                      const ProtobufCMessage*  msg)
{
  RW_ASSERT(rwpcb);
  RW_ASSERT(cb);

  /* Set singleton query flags */
  if (action == RWDTS_QUERY_READ || action == RWDTS_QUERY_RPC) {
    flags |= (RWDTS_XACT_FLAG_NOTRAN);
  }
  flags |= (RWDTS_XACT_FLAG_EXECNOW | RWDTS_XACT_FLAG_END);

  rwpcb->action = action;
  rwpcb->flags = flags;
  rwpcb->msg = msg;

  rwpcb->xact_cb.cb = cb->cb;
  rwpcb->xact_cb.ud = cb->ud;

  rwdts_xact_t *xact = rwdts_api_xact_create(rwpcb->apih, flags, rwdts_xpath_query_xact_cb, rwpcb);
  RW_ASSERT_MESSAGE(xact, "Xact init failed");

  xact->redn_pcb = (void*)rwpcb;
  rwpcb->xact = xact;
  rwdts_xact_ref(xact, __FUNCTION__, __LINE__);

  return rwdts_xpath_query_cont(rwpcb);
} /* rwdts_xpath_query_int */

static void
rwdts_xpath_expr_set_flag(rwdts_xpath_pcb_t *rwpcb,
                          int32_t id,
                          rwdts_xpath_flags_t flag)
{
  uint32_t i = 0;
  for (; i<rwpcb->expr->redn[id]->n_dep_ids; i++) {
    rwdts_xpath_expr_set_flag(rwpcb, rwpcb->expr->redn[id]->dep_ids[i], flag);
  }
  if (rwpcb->expr->redn[id]->has_next_id){
    rwdts_xpath_expr_set_flag(rwpcb, rwpcb->expr->redn[id]->next_id, flag);
  }
  rwpcb->expr->redn[id]->flags |= flag;
}

// Allocate a result entry PB
static rw_status_t
rwdts_xpath_alloc_result(rwdts_xpath_pcb_t *rwpcb,
                         int32_t id)
{
  rw_status_t rs = RW_STATUS_FAILURE;
  RWDtsRednResp *resp = rwpcb->results;
  if (resp) {
    if (0 == (resp->n_entries%RWDTS_XPATH_RESP_ALLOC_CHUNK_SIZE)) {
      RWDtsRednRespEntry **entries = resp->entries;
      resp->entries = realloc(resp->entries,
                              sizeof(RWDtsRednRespEntry*)*(resp->n_entries+RWDTS_XPATH_RESP_ALLOC_CHUNK_SIZE));
      if (resp->entries) {
        rs = RW_STATUS_SUCCESS;
      }
      else {
        resp->entries = entries;
      }
    }
    else {
      rs = RW_STATUS_SUCCESS;
    }
  }
  else {
    resp = RW_MALLOC0(sizeof(RWDtsRednResp));
    if (resp) {
      rwdts_redn_resp__init(resp);
      resp->entries = RW_MALLOC0(sizeof(RWDtsRednRespEntry*)*RWDTS_XPATH_RESP_ALLOC_CHUNK_SIZE);
      if (resp->entries) {
        rs = RW_STATUS_SUCCESS;
      }
    }
  }
  if (RW_STATUS_SUCCESS == rs) {
    RWDtsRednRespEntry *entry = RW_MALLOC0(sizeof(RWDtsRednRespEntry));
    if (entry) {
      rwdts_redn_resp_entry__init(entry);
      entry->id = id;
      resp->entries[resp->n_entries++] = entry;
      rs = RW_STATUS_SUCCESS;
    }
  }

  return rs;
} /* rwdts_xpath_alloc_result */

// Allocate a xpath entry in the result PB
static rw_status_t
rwdts_xpath_alloc_resp_xpath(RWDtsRednRespEntry *entry)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  RW_ASSERT(entry);

  if (entry->xpath) {
    rs = RW_STATUS_FAILURE;
  }
  else {
    entry->xpath = RW_MALLOC0(RWDTS_XPATH_PATH_MAX_SIZE);
    if (!entry->xpath) {
      rs = RW_STATUS_FAILURE;
    }
  }
  return rs;
} /* rwdts_xpath_alloc_resp_xpath */

static rw_status_t
rwdts_xpath_alloc_resp_key(rwdts_xpath_pcb_t *rwpcb,
                           RWDtsRednRespEntry* entry,
                           rw_keyspec_path_t* ks,
                           rw_keyspec_path_t* leaf)
{
  rw_status_t rs = RW_STATUS_FAILURE;
  RW_ASSERT(entry);
  RW_ASSERT(ks);

  RWDtsKey *key  = NULL;
  if (entry->key) {
    key = entry->key;
    if (key && key->keyspec) {
      rw_keyspec_path_free((rw_keyspec_path_t*)key->keyspec, rwpcb->ksi);
      key->keyspec = NULL;
    }
  }
  if (!entry->key) {
    entry->key = RW_MALLOC0(sizeof(RWDtsKey));
    key = entry->key;
  }
  if (key) {
    rwdts_key__init(key);
    key->keyspec = (RwSchemaPathSpec *)ks;
    key->ktype = RWDTS_KEY_RWKEYSPEC;
    rs = RW_STATUS_SUCCESS;
  }
  key = NULL;

  if (entry->leaf) {
    key = entry->leaf;
    if (key && key->keyspec) {
      rw_keyspec_path_free((rw_keyspec_path_t*)key->keyspec, rwpcb->ksi);
      key->keyspec = NULL;
    }
  }
  if (leaf) {
    if (!entry->leaf) {
      entry->leaf = RW_MALLOC0(sizeof(RWDtsKey));
      key = entry->leaf;
    }
    if (key) {
      rwdts_key__init(key);
      key->keyspec = (RwSchemaPathSpec *)leaf;
      key->ktype = RWDTS_KEY_RWKEYSPEC;
      rs = RW_STATUS_SUCCESS;
    }
  }
  return rs;

} /* rwdts_xpath_alloc_resp_key */

#if 0
// Allocate a string entry in the result PB
static rw_status_t
rwdts_xpath_alloc_resp_str(RWDtsRednRespEntry *entry)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  RW_ASSERT(entry);

  if (!entry->val_str) {
    entry->val_str = RW_MALLOC0(RWDTS_XPATH_PATH_MAX_SIZE);
    if (!entry->val_str) {
      rs = RW_STATUS_FAILURE;
    }
  }
  return rs;
} /* rwdts_xpath_alloc_resp_str */

#endif

// Convert XPath to keyspec
// TODO: Currently using the XpathIIParser,
//       See if we can do this locally as we
//       already have a tokenized xpath.
static rw_status_t
rwdts_xpath_get_keyspec(rwdts_xpath_pcb_t *rwpcb,
                        RWDtsRednRespEntry* entry)
{
  rw_status_t rs = RW_STATUS_FAILURE;
  RW_ASSERT(entry);
  RW_ASSERT(entry->xpath);
  RW_ASSERT(!(entry->flags & RWDTS_XPATH_RESP_PATH_PART));

  rw_keyspec_path_t* leaf = NULL;
  rw_keyspec_path_t* ks = rw_keyspec_path_from_xpath(rwpcb->schema,
                                                     entry->xpath,
                                                     RW_XPATH_KEYSPEC,
                                                     rwpcb->ksi);
  if (ks) {
    rs = RW_STATUS_SUCCESS;
    // Set the keyspec category
    rw_keyspec_path_set_category(ks, NULL, rwpcb->cat);
    if (rw_keyspec_path_is_leafy(ks)) {
      // Cannot send leaf as query
      rs = rw_keyspec_path_create_dup(ks, rwpcb->ksi, &leaf);
      if (RW_STATUS_SUCCESS == rs) {
        size_t depth = rw_keyspec_path_get_depth(ks);
        rs = rw_keyspec_path_trunc_suffix_n(ks, rwpcb->ksi, depth-1);
        if (RW_STATUS_SUCCESS == rs) {
          rs = rw_keyspec_path_update_binpath(ks, rwpcb->ksi);
        }
      }
    }
  }
  if (RW_STATUS_SUCCESS == rs) {
    // Store in the entry
    rs = rwdts_xpath_alloc_resp_key(rwpcb, entry, ks, leaf);
  }
  return rs;
} /* rwdts_xpath_get_keyspec */

// Check if the flag is set on all dependent IDs
static rw_status_t
rwdts_xpath_check_deps_flag(rwdts_xpath_pcb_t *rwpcb,
                            int32_t id,
                            rwdts_xpath_flags_t flag)
{
  rw_status_t rs= RW_STATUS_SUCCESS;
  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RW_ASSERT(id < expr->n_redn);

  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn);

  uint32_t i = 0;
  for(; i < redn->n_dep_ids; i++) {
    if (!(expr->redn[redn->dep_ids[i]]->flags & flag)) {
      rs = RW_STATUS_FAILURE;
    }
  }

  return rs;
} /* rwdts_xpath_check_deps_flag */

// convert const value to string
static rw_status_t
rwdts_xpath_const_to_str(rwdts_xpath_pcb_t *rwpcb,
                         int32_t id,
                         char *str)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  RW_ASSERT(str);

  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RW_ASSERT(id < expr->n_redn);

  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn);

  RW_ASSERT(redn->action == RWDTS_XPATH_ACTION_NONE);

  if (redn->has_val_double) {
    char buf[32];
    buf[0] = 0;
    if (rwpcb->predicate) {
      // Currently XPath keyspec expects only strings
      snprintf(buf, 32, "'%u'", (uint32_t)redn->val_double);
    }
    else {
      snprintf(buf, 32, "%lf", redn->val_double);
    }
    strncat(str, buf, RWDTS_XPATH_PATH_MAX_SIZE);
  }
  else if (redn->val_str) {
    if (rwpcb->predicate) {
      strncat(str, "'", RWDTS_XPATH_PATH_MAX_SIZE);
    }
    strncat(str, redn->val_str, RWDTS_XPATH_PATH_MAX_SIZE);
    if (rwpcb->predicate) {
      strncat(str, "'", RWDTS_XPATH_PATH_MAX_SIZE);
    }
  }
  else {
    snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
             "%i: No string or decimal value found", id);
    rs = RW_STATUS_FAILURE;
  }
  return rs;
} /* rwdts_xpath_const_to_str */

// convert const value to string
static rw_status_t
rwdts_xpath_fn_to_str(rwdts_xpath_pcb_t *rwpcb,
                         int32_t id,
                         char *str)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  RW_ASSERT(str);

  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RW_ASSERT(id < expr->n_redn);

  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn);

  if (rwpcb->predicate) {
    // Check if the function is evaluated
    RWDtsRednRespEntry* entry = rwdts_xpath_find_resp_entry(rwpcb, id);
    if (entry) {
      if (entry->flags & RWDTS_XPATH_RESP_RESULT) {
        if (entry->has_val_uint64) {
          char buf[32];
          buf[0] = 0;
          if (rwpcb->predicate) {
            // Currently XPath keyspec expects only strings
            if (entry->has_ltype && ((entry->ltype == PROTOBUF_C_TYPE_UINT32)
                                     || (entry->ltype == PROTOBUF_C_TYPE_FIXED32))) {
              snprintf(buf, 32, "'%u'", (uint32_t)entry->val_uint64);
            }
            else {
              snprintf(buf, 32, "'%lu'", entry->val_uint64);
            }
          }
          else {
            snprintf(buf, 32, "%lu", entry->val_uint64);
          }
          strncat(str, buf, RWDTS_XPATH_PATH_MAX_SIZE);
        }
        else if (entry->has_val_sint64) {
          char buf[32];
          buf[0] = 0;
          if (rwpcb->predicate) {
            // Currently XPath keyspec expects only strings
            if (entry->has_ltype && ((entry->ltype == PROTOBUF_C_TYPE_INT32)
                                     || (entry->ltype == PROTOBUF_C_TYPE_SINT32)
                                     || (entry->ltype == PROTOBUF_C_TYPE_SFIXED32))) {
              snprintf(buf, 32, "'%i'", (int32_t)entry->val_sint64);
            }
            else {
              snprintf(buf, 32, "'%li'", entry->val_sint64);
            }
          }
          else {
            snprintf(buf, 32, "%li", entry->val_sint64);
          }
          strncat(str, buf, RWDTS_XPATH_PATH_MAX_SIZE);
        }
        else if (entry->has_val_double) {
          char buf[32];
          buf[0] = 0;
          if (rwpcb->predicate) {
            // Currently XPath keyspec expects only strings
            snprintf(buf, 32, "'%lf'", entry->val_double);
          }
          else {
            snprintf(buf, 32, "%lf", entry->val_double);
          }
          strncat(str, buf, RWDTS_XPATH_PATH_MAX_SIZE);
        }
        else if (redn->val_str) {
          if (rwpcb->predicate) {
            strncat(str, "'", RWDTS_XPATH_PATH_MAX_SIZE);
          }
          strncat(str, entry->val_str, RWDTS_XPATH_PATH_MAX_SIZE);
          if (rwpcb->predicate) {
            strncat(str, "'", RWDTS_XPATH_PATH_MAX_SIZE);
          }
        }
        else {
          snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
                   "%i: No string or decimal value found", id);
          rs = RW_STATUS_FAILURE;
        }
        entry->flags |= RWDTS_XPATH_RESP_DONE; // Mark as already used
      }
    }

  }
  else {
    //TODO:
    rs = RW_STATUS_FAILURE;
  }

  return rs;
} /* rwdts_xpath_fn_to_str */

// Convert to string equivalent of operator with
// 2 arguments
static rw_status_t
rwdts_xpath_op_2_to_str(rwdts_xpath_pcb_t *rwpcb,
                        int32_t id,
                        char *str)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  RW_ASSERT(str);

  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RW_ASSERT(id < expr->n_redn);

  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn);

  // Implemented for '=' now
  RW_ASSERT(redn->action == RWDTS_XPATH_ACTION_EXOP_EQUAL);

  // If in predicate, first part is just the node str
  if (rwpcb->predicate) {
    strncat(str, expr->redn[redn->dep_ids[0]]->loc_path,
            RWDTS_XPATH_PATH_MAX_SIZE);
  }
  else {
    rs = rwdts_xpath_expr_to_str(rwpcb, redn->dep_ids[0], str);
  }
  if (RW_STATUS_SUCCESS == rs) {
    strncat(str,
            rwdts_xpath_action_str[redn->action],
            RWDTS_XPATH_PATH_MAX_SIZE);
    rs = rwdts_xpath_expr_to_str(rwpcb, redn->dep_ids[1], str);
  }
  return rs;
} /* rwdts_xpath_op_2_to_str */

// Convert predicate expression to string
static rw_status_t
rwdts_xpath_predicate_to_str(rwdts_xpath_pcb_t *rwpcb,
                             int32_t id,
                             char *str)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  RW_ASSERT(str);

  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RW_ASSERT(id < expr->n_redn);

  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn);

  RW_ASSERT(redn->action == RWDTS_XPATH_ACTION_PREDICATE);

  strncat(str, "[", RWDTS_XPATH_PATH_MAX_SIZE);
  rs = rwdts_xpath_expr_to_str(rwpcb, redn->dep_ids[0], str);
  if (RW_STATUS_SUCCESS == rs) {
    strncat(str, "]", RWDTS_XPATH_PATH_MAX_SIZE);
  }
  return rs;
} /* rwdts_xpath_predicate_to_str */

// Not used currently
static rw_status_t
rwdts_xpath_node_to_str(rwdts_xpath_pcb_t *rwpcb,
                        int32_t id,
                        char *str)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  RW_ASSERT(str);

  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RW_ASSERT(id < expr->n_redn);

  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn);

  RW_ASSERT(redn->action == RWDTS_XPATH_ACTION_NT_NODE);

  // TODO:

  return rs;
} /* rwdts_xpath_node_to_str */

static rw_status_t
rwdts_xpath_expr_to_str(rwdts_xpath_pcb_t *rwpcb,
                        int32_t id,
                        char *str)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  RW_ASSERT(str);

  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RW_ASSERT(id < expr->n_redn);

  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn);

  switch (redn->action) {
    case RWDTS_XPATH_ACTION_NONE:
      rs = rwdts_xpath_const_to_str(rwpcb, id, str);
      break;
    case RWDTS_XPATH_ACTION_NT_NODE:
      rs = rwdts_xpath_node_to_str(rwpcb, id, str);
      break;
    case RWDTS_XPATH_ACTION_EXOP_EQUAL:
      rs = rwdts_xpath_op_2_to_str(rwpcb, id, str);
      break;
    case RWDTS_XPATH_ACTION_PREDICATE:
      rs = rwdts_xpath_predicate_to_str(rwpcb, id, str);
      break;
    default:
      if ((redn->action >= RWDTS_XPATH_ACTION_FN_BEGIN) &&
          (redn->action <= RWDTS_XPATH_ACTION_FN_END)) {
        rs = rwdts_xpath_fn_to_str(rwpcb, id, str);
      }
      else {
        snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
                 "Unsupported action: %s (%u)",
                 rwdts_xpath_action_str[redn->action],
                 redn->action);
        rs = RW_STATUS_FAILURE;
      }
      break;
  }

  return rs;
} /* rwdts_xpath_expr_to_str */

static char*
rwdts_xpath_to_str (rwdts_xpath_pcb_t *rwpcb,
                    int32_t id)
{
  RW_ASSERT(id < rwpcb->expr->n_redn);

  char *str = RW_MALLOC0(RWDTS_XPATH_PATH_MAX_SIZE);
  if (str) {
    if (rwdts_xpath_expr_to_str(rwpcb, id, str) != RW_STATUS_SUCCESS) {
      RW_FREE(str);
      str = NULL;
    }
  }
  return str;
} /* rwdts_xpath_to_str  */

// Find resp entry corresponding to an id
static RWDtsRednRespEntry*
rwdts_xpath_find_resp_entry(rwdts_xpath_pcb_t *rwpcb, int32_t id)
{
  RWDtsRednRespEntry* entry = NULL;
  RWDtsRednResp *resp = rwpcb->results;
  RW_ASSERT(resp);

  if (id != -1) {
    int32_t i = 0;
    for (; i < resp->n_entries; i++) {
      if (resp->entries[i]->id == id) {
        entry = resp->entries[i];
        break;
      }
    }
  }

  return entry;
} /* rwdts_xpath_find_resp_entry */


// Find the start of the path
static int32_t
rwdts_xpath_find_path_start(rwdts_xpath_pcb_t *rwpcb)
{
  RWDtsRednExpression *expr = rwpcb->expr;

  int32_t start_id = -1;
  int32_t i = rwpcb->depth-1;
  if (rwpcb->predicate) {
    // Skip to the id before the current one
    i--;
  }
  if (i>=0) {
    if ((expr->redn[rwpcb->ids[i]]->action == RWDTS_XPATH_ACTION_NT_NODE) ||
        (expr->redn[rwpcb->ids[i]]->action == RWDTS_XPATH_ACTION_PREDICATE)) {
      start_id = expr->redn[rwpcb->ids[i]]->start_id;
    }
  }
  return start_id;
} /* rwdts_xpath_find_path_start */

// Find the result entry specific to a path
static RWDtsRednRespEntry*
rwdts_xpath_find_path_resp(rwdts_xpath_pcb_t *rwpcb, int32_t id)
{
  RWDtsRednRespEntry* entry = NULL;
  RWDtsRednResp *resp = rwpcb->results;

  if (resp->n_entries) {
    if (id == -1) {
      id = rwdts_xpath_find_path_start(rwpcb);
    }
    if (id != -1) {
      int32_t i = 0;
      for (; i < resp->n_entries; i++) {
        if (resp->entries[i]->id == id) {
          RW_ASSERT(resp->entries[i]->flags & RWDTS_XPATH_RESP_PATH);
          entry = resp->entries[i];
          break;
        }
      }
    }
  }

  return entry;
}

// Check if the path is part of path expression
// in a predicate
// [LHS = expr] - This check if this is the LHS part
static bool
rwdts_xpath_check_predicate_lhs(rwdts_xpath_pcb_t *rwpcb,
                                int32_t id)
{
  bool lhs = false;
  RWDtsRednExpression *expr = rwpcb->expr;
  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn->action == RWDTS_XPATH_ACTION_NT_NODE);

  RWDtsReduction *pred = NULL;
  if (rwpcb->depth > 1){
    if (expr->redn[rwpcb->ids[rwpcb->depth-2]]->action == RWDTS_XPATH_ACTION_PREDICATE) {
      pred = expr->redn[rwpcb->ids[rwpcb->depth-2]];
    }
  }
  if ((pred == NULL) && (rwpcb->depth > 2)) {
    if (expr->redn[rwpcb->ids[rwpcb->depth-3]]->action == RWDTS_XPATH_ACTION_PREDICATE) {
      pred = expr->redn[rwpcb->ids[rwpcb->depth-3]];
    }
  }
  if (pred) {
    RWDtsReduction *r = expr->redn[pred->dep_ids[0]];
    if (r->id != id) {
      r = expr->redn[r->dep_ids[0]];
      if (r->id == id) {
        lhs = true;
      }
    }
    else {
      lhs = true;
    }
  }

  return lhs;
} /* rwdts_xpath_check_predicate_lhs */

static rw_status_t
rwdts_xpath_eval_predicate(rwdts_xpath_pcb_t *rwpcb,
                           int32_t id)
{
  rw_status_t rs = RW_STATUS_PENDING;
  RWDtsRednExpression *expr = rwpcb->expr;
  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn->action == RWDTS_XPATH_ACTION_PREDICATE);

  if (!(redn->flags & RWDTS_XPATH_FLAG_PARSED)) {
    // Currently, expecting the predicate to have 1 dependency
    if (expr->redn[redn->dep_ids[0]]->flags & RWDTS_XPATH_FLAG_PARSED) {
      rwpcb->predicate++;
      // Build out the rest of the path
      RWDtsRednRespEntry* entry = rwdts_xpath_find_path_resp(rwpcb,
                                                             (redn->start_id != -1)?redn->start_id:-1);
      RW_ASSERT(entry); // Expect a partial path here for predicate
      RW_ASSERT(entry->flags & RWDTS_XPATH_RESP_PATH_PART);
      RW_ASSERT(entry->xpath);
      // We are part of an existing path expr
      char *predicate = rwdts_xpath_to_str(rwpcb, id);
      if (predicate) {
        strncat(entry->xpath,
                predicate,
                RWDTS_XPATH_PATH_MAX_SIZE);
        RW_FREE(predicate);
        predicate = NULL;
        redn->flags |= RWDTS_XPATH_FLAG_PARSED;
        if (redn->has_next_id) {
          rs = rwdts_xpath_eval_id(rwpcb, redn->next_id);
        }
        else {
          // Mark the path as complete
          /* int32_t next_id = entry->id; */
          /* while(expr->redn[next_id]->has_next_id) { */
          /*   expr->redn[next_id]->flags |= RWDTS_XPATH_FLAG_PARSED; */
          /*   next_id = expr->redn[next_id]->next_id; */
          /* } */
          /* expr->redn[entry->id]->flags |= RWDTS_XPATH_FLAG_PARSED; */
          // Reset the partial path flag
          entry->flags &= ~RWDTS_XPATH_RESP_PATH_PART;
          rs = rwdts_xpath_get_keyspec(rwpcb, entry);
          // Mark as pending as we need to generate query for this path
          if (RW_STATUS_SUCCESS == rs) {
            rs = RW_STATUS_PENDING;
            redn->flags |= RWDTS_XPATH_FLAG_PARSED;
          }
        }
      }
      else {
        rs = RW_STATUS_FAILURE;
      }
      rwpcb->predicate--;
    }
    else if (redn->flags & RWDTS_XPATH_FLAG_QUERY) {
      redn->flags |= RWDTS_XPATH_FLAG_DONE;
      rs = RW_STATUS_SUCCESS;
    }
  }

  return rs;
} /* rwdts_xpath_eval_predicate */

static rw_status_t
rwdts_xpath_eval_path(rwdts_xpath_pcb_t *rwpcb,
                      int32_t id)
{
  rw_status_t rs = RW_STATUS_PENDING;
  RWDtsRednExpression *expr = rwpcb->expr;
  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn->loc_path);
  RWDtsRednResp *resp = rwpcb->results;

  if (!(redn->flags & RWDTS_XPATH_FLAG_PARSED)) {
    RWDtsRednRespEntry* entry = rwdts_xpath_find_path_resp(rwpcb, -1);
    if (entry) {
      // We are part of existing path expr
      RW_ASSERT(entry->flags & RWDTS_XPATH_RESP_PATH_PART);
      RW_ASSERT(entry->xpath);
      strncat(entry->xpath,
              "/",
              RWDTS_XPATH_PATH_MAX_SIZE);
      strncat(entry->xpath,
              redn->loc_path,
              RWDTS_XPATH_PATH_MAX_SIZE);
      redn->flags |= RWDTS_XPATH_FLAG_PARSED;
      if (redn->has_next_id) {
        rs = rwdts_xpath_eval_id(rwpcb, redn->next_id);
      }
      else {
        // Mark the path as complete
        /* int32_t next_id = entry->id; */
        /* while(next_id != -1) { */
        /*   expr->redn[next_id]->flags |= RWDTS_XPATH_FLAG_PARSED; */
        /*   if (expr->redn[next_id]->has_next_id) { */
        /*     next_id = expr->redn[next_id]->next_id; */
        /*   } */
        /*   else { */
        /*     next_id = -1; */
        /*   } */
        /* } */
        // Reset the partial path flag
        entry->flags &= ~RWDTS_XPATH_RESP_PATH_PART;
        rs = rwdts_xpath_get_keyspec(rwpcb, entry);
        // Mark as pending as we need to generate query for this path
        if (RW_STATUS_SUCCESS == rs) {
          rs = RW_STATUS_PENDING;
        }
      }
    }
    else {
      // Check if this is the LHS of a predicate
      if (rwdts_xpath_check_predicate_lhs(rwpcb, id)) {
        redn->flags |= RWDTS_XPATH_FLAG_PARSED;
        rs = RW_STATUS_SUCCESS;
      }
      else {
        rs = rwdts_xpath_alloc_result(rwpcb, id);
        if (RW_STATUS_SUCCESS == rs) {
          resp = rwpcb->results;
          RWDtsRednRespEntry *entry = resp->entries[resp->n_entries-1];
          RW_ASSERT(entry->id == id);

          rs = rwdts_xpath_alloc_resp_xpath(entry);
          if (RW_STATUS_SUCCESS == rs) {
            strncpy(entry->xpath,
                    "/",
                    RWDTS_XPATH_PATH_MAX_SIZE);
            strncat(entry->xpath,
                    redn->loc_path,
                    RWDTS_XPATH_PATH_MAX_SIZE);
            if (redn->has_next_id) {
              entry->flags |= RWDTS_XPATH_RESP_PATH_PART|RWDTS_XPATH_RESP_PATH;
              redn->flags |= RWDTS_XPATH_FLAG_PARSED;
              rs = rwdts_xpath_eval_id(rwpcb, redn->next_id);
            }
            else {
              // Only one part, so mark as complete
              entry->flags |= RWDTS_XPATH_RESP_PATH;
              redn->flags |= RWDTS_XPATH_FLAG_PARSED;
              rs = rwdts_xpath_get_keyspec(rwpcb, entry);
              // Mark as pending as we need to generate query for this path
              if (RW_STATUS_SUCCESS == rs) {
                rs = RW_STATUS_PENDING;
              }
            }
          }
        }
      }
    }
  }
  else if (redn->flags & RWDTS_XPATH_FLAG_QUERY) {
    // Already queried, mark as done
    redn->flags |= RWDTS_XPATH_FLAG_DONE;
    while (redn->next_id) {
      redn = expr->redn[redn->next_id];
      rwdts_xpath_expr_set_flag(rwpcb, redn->id, RWDTS_XPATH_FLAG_DONE);
      redn->flags |= RWDTS_XPATH_FLAG_DONE;
    }
    rs = RW_STATUS_SUCCESS;
  }
  return rs;
} /* rwdts_xpath_eval_path */


static rw_status_t
rwdts_xpath_eval_const(rwdts_xpath_pcb_t *rwpcb,
                       int32_t id)
{
  rw_status_t rs = RW_STATUS_SUCCESS;

  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RW_ASSERT(id < expr->n_redn);

  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn);

  RW_ASSERT(redn->action == RWDTS_XPATH_ACTION_NONE);
  RW_ASSERT(redn->n_dep_ids == 0);

  if (redn->has_val_double || redn->val_str) {
    // Nothing else to be done
    redn->flags |= RWDTS_XPATH_FLAG_DONE | RWDTS_XPATH_FLAG_QUERY | RWDTS_XPATH_FLAG_PARSED;
  }
  else {
    snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
             "%s: No string or decimal value found in %d", __FUNCTION__, id);
    rs = RW_STATUS_FAILURE;
  }

  return rs;
} /* rwdts_xpath_eval_const */

static rw_status_t
rwdts_xpath_eval_fn_numeric(rwdts_xpath_pcb_t *rwpcb,
                            int32_t id)
{
  rw_status_t rs = RW_STATUS_FAILURE;

  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RW_ASSERT(id < expr->n_redn);

  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn);

  // ATTN: Currently expecting only 1 direct dependent which is a numeric leaf
  RWDtsRednRespEntry* pentry = rwdts_xpath_find_path_resp(rwpcb, expr->redn[redn->dep_ids[0]]->id);
  RW_ASSERT(pentry);

  if (!(redn->flags & RWDTS_XPATH_FLAG_PARSED)) {
    rs = rwdts_xpath_check_deps_flag(rwpcb, id, RWDTS_XPATH_FLAG_PARSED);
    if (RW_STATUS_SUCCESS == rs) {
      // Check if the keyspec represents a leaf and of numeric type
      if (pentry->leaf) {
        // Get the leaf type and label
        const ProtobufCFieldDescriptor *fd = rw_keyspec_path_get_leaf_field_desc((rw_keyspec_path_t*)pentry->leaf->keyspec);
        RW_ASSERT(fd);

        pentry->ltype = fd->type;
        pentry->llabel = fd->label;
        if (fd->type < PROTOBUF_C_TYPE_BOOL) {
          // Numeric type
          rs = RW_STATUS_SUCCESS;
          redn->flags |= RWDTS_XPATH_FLAG_PARSED;
        }
      }
      if (RW_STATUS_FAILURE == rs) {
        snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
                 "%s: Element '%s' not correct type for %d", __FUNCTION__,
                 pentry->xpath, id);
        rs = RW_STATUS_FAILURE;
      }
    }
    else {
      rs = RW_STATUS_PENDING;
    }
  }
  else if (!(redn->flags & RWDTS_XPATH_FLAG_DONE)) {
    rs = rwdts_xpath_check_deps_flag(rwpcb, id,
                                     RWDTS_XPATH_FLAG_QUERY|RWDTS_XPATH_FLAG_DONE);
    if (RW_STATUS_SUCCESS == rs) {
      rs = rwdts_xpath_alloc_result(rwpcb, id);
      if (RW_STATUS_SUCCESS == rs) {
        RWDtsRednRespEntry* entry = rwpcb->results->entries[rwpcb->results->n_entries-1];
        RW_ASSERT(entry);
        entry->key = pentry->key;
        pentry->key = NULL;
        entry->leaf = pentry->leaf;
        pentry->leaf = NULL;
        pentry->has_ltype = false;
        pentry->has_llabel = false;
        pentry->flags |= RWDTS_XPATH_RESP_DONE;

        const ProtobufCFieldDescriptor *fd = rw_keyspec_path_get_leaf_field_desc((rw_keyspec_path_t*)entry->leaf->keyspec);
        RW_ASSERT(fd);
        entry->has_ltype = true;
        entry->ltype = fd->type;
        entry->has_llabel = true;
        entry->llabel = fd->label;

        rwdts_query_result_t *qrslt = rwdts_xact_query_result(rwpcb->xact, pentry->id+1);
        while (qrslt) {
          const ProtobufCMessage *msg = rwdts_query_result_get_protobuf(qrslt);
          RW_ASSERT(msg);
          switch (entry->ltype) {
            case PROTOBUF_C_TYPE_INT32:
            case PROTOBUF_C_TYPE_SINT32:
            case PROTOBUF_C_TYPE_SFIXED32:
            case PROTOBUF_C_TYPE_INT64:
            case PROTOBUF_C_TYPE_SINT64:
            case PROTOBUF_C_TYPE_SFIXED64:
              {
                ProtobufCFieldInfo finfo;
                bool ok = protobuf_c_message_get_field_instance(NULL, msg,
                                                                fd, 0, &finfo);
                if (ok) {
                  int64_t val = *(int64_t*)finfo.element;
                  if (entry->has_val_sint64) {
                    // int64_t last = entry->val_sint64;
                    if (redn->action == RWDTS_XPATH_ACTION_FN_SUM) {
                      entry->val_sint64 += val;
                      // TODO: Check for over/underflow
                    }
                    else if (redn->action == RWDTS_XPATH_ACTION_FN_MIN) {
                      if (val < entry->val_sint64) {
                        entry->val_sint64 = val;
                      }
                    }
                    else if (redn->action == RWDTS_XPATH_ACTION_FN_MAX) {
                      if (val > entry->val_sint64) {
                        entry->val_sint64 = val;
                      }
                    }
                  }
                  else {
                    entry->has_val_sint64 = true;
                    entry->val_sint64 = val;
                  }
                }
              }
              break;
            case PROTOBUF_C_TYPE_UINT32:
            case PROTOBUF_C_TYPE_FIXED32:
            case PROTOBUF_C_TYPE_UINT64:
            case PROTOBUF_C_TYPE_FIXED64:
              {
                ProtobufCFieldInfo finfo;
                bool ok = protobuf_c_message_get_field_instance(NULL, msg,
                                                                fd, 0, &finfo);
                if (ok) {
                  uint64_t val = *(uint64_t*)finfo.element;
                  if (entry->has_val_uint64) {
                    uint64_t last = entry->val_uint64;
                    if (redn->action == RWDTS_XPATH_ACTION_FN_SUM) {
                      entry->val_uint64 += val;
                      // Check for overflow
                      if (entry->val_uint64 < last) {
                        entry->flags |= RWDTS_XPATH_RESP_OVERFLOW;
                      }
                    }
                    else if (redn->action == RWDTS_XPATH_ACTION_FN_MIN) {
                      if (val < entry->val_uint64) {
                        entry->val_uint64 = val;
                      }
                    }
                    else if (redn->action == RWDTS_XPATH_ACTION_FN_MAX) {
                      if (val > entry->val_uint64) {
                        entry->val_uint64 = val;
                      }
                    }
                  }
                  else {
                    entry->has_val_uint64 = true;
                    entry->val_uint64 = val;
                  }
                }
              }
              break;
            case PROTOBUF_C_TYPE_FLOAT:
            case PROTOBUF_C_TYPE_DOUBLE:
              {
                ProtobufCFieldInfo finfo;
                bool ok = protobuf_c_message_get_field_instance(NULL, msg,
                                                                fd, 0, &finfo);
                if (ok) {
                  double val = *(double*)finfo.element;
                  if (entry->has_val_double) {
                    // double last = entry->val_double;
                    if (redn->action == RWDTS_XPATH_ACTION_FN_SUM) {
                      entry->val_double += val;
                      // TODO: Check for over/underflow
                    }
                    else if (redn->action == RWDTS_XPATH_ACTION_FN_MIN) {
                      if (val < entry->val_double) {
                        entry->val_double = val;
                      }
                    }
                    else if (redn->action == RWDTS_XPATH_ACTION_FN_MAX) {
                      if (val > entry->val_double) {
                        entry->val_double = val;
                      }
                    }
                  }
                  else {
                    entry->has_val_double = true;
                    entry->val_double = val;
                  }
                }
              }
              break;
            default:
              // Should not reach here
              RW_CRASH();
          }
          entry->has_count = true;
          entry->count++;
          qrslt = rwdts_xact_query_result(rwpcb->xact, pentry->id+1);
        }
        redn->flags |= RWDTS_XPATH_FLAG_DONE;
        entry->flags |= RWDTS_XPATH_RESP_RESULT;
        rs = RW_STATUS_SUCCESS;
      }
    }
    else {
      rs = RW_STATUS_PENDING;
    }
  }
  else {
    snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
             "%s: Unknown state %u in %d", __FUNCTION__,
             redn->flags, id);
    rs = RW_STATUS_FAILURE;
  }

  return rs;
}

static rw_status_t
rwdts_xpath_eval_fn_count(rwdts_xpath_pcb_t *rwpcb,
                          int32_t id)
{
  rw_status_t rs = RW_STATUS_FAILURE;

  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RW_ASSERT(id < expr->n_redn);

  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn);
  RW_ASSERT(redn->action == RWDTS_XPATH_ACTION_FN_COUNT);

  // ATTN: Currently expecting only 1 direct dependent which is a numeric leaf
  RWDtsRednRespEntry* pentry = rwdts_xpath_find_path_resp(rwpcb, expr->redn[redn->dep_ids[0]]->id);
  RW_ASSERT(pentry);

  if (!(redn->flags & RWDTS_XPATH_FLAG_PARSED)) {
    rs = rwdts_xpath_check_deps_flag(rwpcb, id, RWDTS_XPATH_FLAG_PARSED);
    if (RW_STATUS_SUCCESS == rs) {
      rs = RW_STATUS_SUCCESS;
      redn->flags |= RWDTS_XPATH_FLAG_PARSED;
    }
    else {
      rs = RW_STATUS_PENDING;
    }
  }
  else if (!(redn->flags & RWDTS_XPATH_FLAG_DONE)) {
    rs = rwdts_xpath_check_deps_flag(rwpcb, id,
                                     RWDTS_XPATH_FLAG_QUERY|RWDTS_XPATH_FLAG_DONE);
    if (RW_STATUS_SUCCESS == rs) {
      rs = rwdts_xpath_alloc_result(rwpcb, id);
      if (RW_STATUS_SUCCESS == rs) {
        RWDtsRednRespEntry* entry = rwpcb->results->entries[rwpcb->results->n_entries-1];
        RW_ASSERT(entry);
        entry->key = pentry->key;
        pentry->key = NULL;
        entry->leaf = pentry->leaf;
        pentry->leaf = NULL;
        pentry->has_ltype = false;
        pentry->has_llabel = false;
        pentry->flags |= RWDTS_XPATH_RESP_DONE;

        const ProtobufCFieldDescriptor *fd = NULL;
        if (entry->leaf) {
          fd = rw_keyspec_path_get_leaf_field_desc((rw_keyspec_path_t*)entry->leaf->keyspec);
          RW_ASSERT(fd);
          entry->has_ltype = true;
          entry->ltype = fd->type;
          entry->has_llabel = true;
          entry->llabel = fd->label;
        }

        rwdts_query_result_t *qrslt = rwdts_xact_query_result(rwpcb->xact, pentry->id+1);
        while (qrslt) {
          const ProtobufCMessage *msg = rwdts_query_result_get_protobuf(qrslt);
          RW_ASSERT(msg);
          if (entry->leaf) {
            ProtobufCFieldInfo finfo;
            bool ok = protobuf_c_message_get_field_instance(NULL, msg,
                                                            fd, 0, &finfo);
            if (ok) {
              entry->has_count = true;
              entry->count++;
            }
          }
          else {
            entry->has_count = true;
            entry->count++;
          }
          qrslt = rwdts_xact_query_result(rwpcb->xact, pentry->id+1);
        }
        redn->flags |= RWDTS_XPATH_FLAG_DONE;
        entry->flags |= RWDTS_XPATH_RESP_RESULT;
        rs = RW_STATUS_SUCCESS;
      }
    }
    else {
      rs = RW_STATUS_PENDING;
    }
  }
  else {
    snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
             "%s: Unknown state %u in %d", __FUNCTION__,
             redn->flags, id);
    rs = RW_STATUS_FAILURE;
  }

  return rs;
} /* rwdts_xpath_eval_fn_avg */

static rw_status_t
rwdts_xpath_eval_fn_avg(rwdts_xpath_pcb_t *rwpcb,
                        int32_t id)
{
  rw_status_t rs = RW_STATUS_FAILURE;

  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RW_ASSERT(id < expr->n_redn);

  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn);

  // ATTN: Currently expecting only 1 direct dependent which is a numeric leaf
  RWDtsRednRespEntry* pentry = rwdts_xpath_find_path_resp(rwpcb, expr->redn[redn->dep_ids[0]]->id);
  RW_ASSERT(pentry);

  if (!(redn->flags & RWDTS_XPATH_FLAG_PARSED)) {
    rs = rwdts_xpath_check_deps_flag(rwpcb, id, RWDTS_XPATH_FLAG_PARSED);
    if (RW_STATUS_SUCCESS == rs) {
      // Check if the keyspec represents a leaf and of numeric type
      if (pentry->leaf) {
        // Get the leaf type and label
        const ProtobufCFieldDescriptor *fd = rw_keyspec_path_get_leaf_field_desc((rw_keyspec_path_t*)pentry->leaf->keyspec);
        RW_ASSERT(fd);

        pentry->ltype = fd->type;
        pentry->llabel = fd->label;
        if (fd->type < PROTOBUF_C_TYPE_BOOL) {
          // Numeric type
          rs = RW_STATUS_SUCCESS;
          redn->flags |= RWDTS_XPATH_FLAG_PARSED;
        }
      }
      if (RW_STATUS_FAILURE == rs) {
        snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
                 "%s: Element '%s' not correct type for %d", __FUNCTION__,
                 pentry->xpath, id);
        rs = RW_STATUS_FAILURE;
      }
    }
    else {
      rs = RW_STATUS_PENDING;
    }
  }
  else if (!(redn->flags & RWDTS_XPATH_FLAG_DONE)) {
    rs = rwdts_xpath_check_deps_flag(rwpcb, id,
                                     RWDTS_XPATH_FLAG_QUERY|RWDTS_XPATH_FLAG_DONE);
    if (RW_STATUS_SUCCESS == rs) {
      rs = rwdts_xpath_alloc_result(rwpcb, id);
      if (RW_STATUS_SUCCESS == rs) {
        RWDtsRednRespEntry* entry = rwpcb->results->entries[rwpcb->results->n_entries-1];
        RW_ASSERT(entry);
        entry->key = pentry->key;
        pentry->key = NULL;
        entry->leaf = pentry->leaf;
        pentry->leaf = NULL;
        pentry->has_ltype = false;
        pentry->has_llabel = false;
        pentry->flags |= RWDTS_XPATH_RESP_DONE;

        const ProtobufCFieldDescriptor *fd = rw_keyspec_path_get_leaf_field_desc((rw_keyspec_path_t*)entry->leaf->keyspec);
        RW_ASSERT(fd);
        entry->has_ltype = true;
        entry->ltype = fd->type;
        entry->has_llabel = true;
        entry->llabel = fd->label;

        rwdts_query_result_t *qrslt = rwdts_xact_query_result(rwpcb->xact, pentry->id+1);
        while (qrslt) {
          const ProtobufCMessage *msg = rwdts_query_result_get_protobuf(qrslt);
          RW_ASSERT(msg);
          ProtobufCFieldInfo finfo;
          bool ok = protobuf_c_message_get_field_instance(NULL, msg,
                                                          fd, 0, &finfo);
          if (ok) {
            double val;
            switch (entry->ltype) {
              case PROTOBUF_C_TYPE_INT32:
              case PROTOBUF_C_TYPE_SINT32:
              case PROTOBUF_C_TYPE_SFIXED32:
              case PROTOBUF_C_TYPE_INT64:
              case PROTOBUF_C_TYPE_SINT64:
              case PROTOBUF_C_TYPE_SFIXED64:
                val = (double)*(int64_t*)finfo.element;
                break;
              case PROTOBUF_C_TYPE_UINT32:
              case PROTOBUF_C_TYPE_FIXED32:
              case PROTOBUF_C_TYPE_UINT64:
              case PROTOBUF_C_TYPE_FIXED64:
                val = (double)*(uint64_t*)finfo.element;
                break;
              case PROTOBUF_C_TYPE_FLOAT:
              case PROTOBUF_C_TYPE_DOUBLE:
                val = *(double*)finfo.element;
                break;
              default:
                // Should not reach here
                RW_CRASH();
            }

            if (entry->has_val_double) {
              entry->val_double = ((entry->val_double*entry->count)+val)/(entry->count+1);
            }
            else {
              entry->has_count = true;
              entry->val_double = val;
              entry->has_val_double = val;
            }
          }
          entry->count++;
          qrslt = rwdts_xact_query_result(rwpcb->xact, pentry->id+1);
        }
        redn->flags |= RWDTS_XPATH_FLAG_DONE;
        entry->flags |= RWDTS_XPATH_RESP_RESULT;
        rs = RW_STATUS_SUCCESS;
      }
    }
    else {
      rs = RW_STATUS_PENDING;
    }
  }
  else {
    snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
             "%s: Unknown state %u in %d", __FUNCTION__,
             redn->flags, id);
    rs = RW_STATUS_FAILURE;
  }

  return rs;
} /* rwdts_xpath_eval_fn_avg */

static rw_status_t
rwdts_xpath_eval_fn(rwdts_xpath_pcb_t *rwpcb,
                       int32_t id)
{
  rw_status_t rs = RW_STATUS_SUCCESS;

  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RW_ASSERT(id < expr->n_redn);

  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn);

  RW_ASSERT((redn->action >= RWDTS_XPATH_ACTION_FN_BEGIN) &&
            (redn->action <= RWDTS_XPATH_ACTION_FN_END));
  RW_ASSERT(redn->n_dep_ids);

  switch (redn->action) {
    case RWDTS_XPATH_ACTION_FN_SUM:
    case RWDTS_XPATH_ACTION_FN_MIN:
    case RWDTS_XPATH_ACTION_FN_MAX:
      rs =  rwdts_xpath_eval_fn_numeric(rwpcb, id);
      break;
    case RWDTS_XPATH_ACTION_FN_AVG:
      rs =  rwdts_xpath_eval_fn_avg(rwpcb, id);
      break;
    case RWDTS_XPATH_ACTION_FN_COUNT:
      rs =  rwdts_xpath_eval_fn_count(rwpcb, id);
      break;
    default:
      rs = RW_STATUS_FAILURE;
      snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
               "%s: Unsupported function %s in %d", __FUNCTION__,
               rwdts_xpath_action_str[redn->action], id);
  }

  return rs;
} /* rwdts_xpath_eval_fn */

static rw_status_t
rwdts_xpath_eval_exop_equal(rwdts_xpath_pcb_t *rwpcb,
                            int32_t id)
{
  rw_status_t rs = RW_STATUS_FAILURE;

  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RW_ASSERT(id < expr->n_redn);

  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn);

  RW_ASSERT (redn->action == RWDTS_XPATH_ACTION_EXOP_EQUAL);

  if (!(redn->flags & RWDTS_XPATH_FLAG_PARSED)) {
    rs = rwdts_xpath_check_deps_flag(rwpcb, id, RWDTS_XPATH_FLAG_PARSED);
    if (RW_STATUS_SUCCESS == rs) {
      if (expr->redn[redn->dep_ids[1]]->flags & RWDTS_XPATH_FLAG_DONE) {
        redn->flags |= RWDTS_XPATH_FLAG_PARSED;
      }
      else {
        rs = RW_STATUS_PENDING;
      }
    }
    else {
      rs = RW_STATUS_PENDING;
    }
  }
  else if ((redn->flags & RWDTS_XPATH_FLAG_QUERY)) {
    redn->flags |= RWDTS_XPATH_FLAG_DONE;
    rs = RW_STATUS_SUCCESS;
  }
  else if (!(redn->flags & RWDTS_XPATH_FLAG_DONE)) {
    // TODO: Handle non predicate equal handling
  }
  else {
    snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
             "%s: Unknown state %u in %d", __FUNCTION__,
             redn->flags, id);
    rs = RW_STATUS_FAILURE;
  }

  return rs;
} /* rwdts_xpath_eval_exop_equal */

static rw_status_t
rwdts_xpath_eval_exop_add(rwdts_xpath_pcb_t *rwpcb,
                          int32_t id)
{
  rw_status_t rs = RW_STATUS_FAILURE;

  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RW_ASSERT(id < expr->n_redn);

  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn);

  RW_ASSERT (redn->action == RWDTS_XPATH_ACTION_EXOP_ADD);

  rs = rwdts_xpath_check_deps_flag(rwpcb, id, RWDTS_XPATH_FLAG_DONE);

  if (RW_STATUS_SUCCESS == rs) {
    double param1 = 0;
    double param2 = 0;
    RWDtsReduction *p1 = expr->redn[redn->dep_ids[0]];
    if ((RWDTS_XPATH_ACTION_NONE == p1->action) && (p1->has_val_double)) {
        param1 = p1->val_double;
    }
    else {
      rs = RW_STATUS_FAILURE;
      snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
               "%s: Param %s not supported for %i",
               __FUNCTION__, rwdts_xpath_action_str[p1->action],
               p1->id);
    }
    RWDtsReduction *p2 = expr->redn[redn->dep_ids[1]];
    if ((RW_STATUS_SUCCESS == rs) && (RWDTS_XPATH_ACTION_NONE == p2->action)
        && (p2->has_val_double)) {
        param2 = p2->val_double;
    }
    else {
      rs = RW_STATUS_FAILURE;
      snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
               "%s: Param %s not supported for %i",
               __FUNCTION__, rwdts_xpath_action_str[p2->action],
               p2->id);
    }

    if (RW_STATUS_SUCCESS == rs) {
      rs = rwdts_xpath_alloc_result(rwpcb, id);
      if (RW_STATUS_SUCCESS == rs) {
        RWDtsRednRespEntry *entry = rwpcb->results->entries[rwpcb->results->n_entries-1];
        RW_ASSERT(entry->id == id);
        entry->val_double = param1 + param2;
        entry->has_val_double = true;
        entry->flags |= RWDTS_XPATH_RESP_DONE;
        redn->flags |= RWDTS_XPATH_FLAG_PARSED | RWDTS_XPATH_FLAG_QUERY | RWDTS_XPATH_FLAG_DONE;
      }
    }
  }
  else {
    rs = RW_STATUS_PENDING;
  }
  return rs;
} /* rwdts_xpath_eval_exop_add */

static rw_status_t
rwdts_xpath_eval_exop(rwdts_xpath_pcb_t *rwpcb,
                      int32_t id)
{
  rw_status_t rs = RW_STATUS_SUCCESS;

  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RW_ASSERT(id < expr->n_redn);

  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn);

  RW_ASSERT((redn->action >= RWDTS_XPATH_ACTION_EXOP_BEGIN) &&
            (redn->action <= RWDTS_XPATH_ACTION_EXOP_END));
  RW_ASSERT(redn->n_dep_ids);

  if (redn->action == RWDTS_XPATH_ACTION_EXOP_EQUAL) {
    rs = rwdts_xpath_eval_exop_equal(rwpcb, id);
  }
  else if (redn->action == RWDTS_XPATH_ACTION_EXOP_ADD) {
    rs = rwdts_xpath_eval_exop_add(rwpcb, id);
  }
  else {
    snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
             "%s: Unsupported operator %s for %d",
             __FUNCTION__, rwdts_xpath_action_str[redn->action], id);
    rs = RW_STATUS_FAILURE;
  }
  return rs;
} /* rwdts_xpath_eval_exop */

static rw_status_t
rwdts_xpath_eval_id(rwdts_xpath_pcb_t *rwpcb,
                    int32_t id)
{
  rw_status_t rs = RW_STATUS_SUCCESS;

  RWDtsRednExpression *expr = rwpcb->expr;
  RW_ASSERT(expr);
  RW_ASSERT(id < expr->n_redn);

  RWDtsReduction *redn = expr->redn[id];
  RW_ASSERT(redn);

  if (redn->flags & RWDTS_XPATH_FLAG_DONE) {
    return rs;
  }

  // If it has already been through this iteration, return
  if (redn->pass_ >= rwpcb->pass) {
    return rs;
  }

  rwpcb->ids[rwpcb->depth++] = id;

  bool pending = false;
  uint32_t i = 0;
  for(; i < redn->n_dep_ids; i++) {
    rs = rwdts_xpath_eval_id(rwpcb, redn->dep_ids[i]);
    if (RW_STATUS_FAILURE == rs) {
      break;
    }
    if (RW_STATUS_PENDING == rs) {
      pending = true;
    }
  }

  if (RW_STATUS_FAILURE != rs) {
    redn->pass_++;
    rwdts_xpath_action_t action = redn->action;
    if (action != RWDTS_XPATH_ACTION_NONE)  {
      if (action == RWDTS_XPATH_ACTION_NT_NODE) {
        rs = rwdts_xpath_eval_path(rwpcb, id);
      }
      else if (action == RWDTS_XPATH_ACTION_PREDICATE) {
        rs = rwdts_xpath_eval_predicate(rwpcb, id);
      }
      else if ((action >= RWDTS_XPATH_ACTION_FN_BEGIN) &&
               (action <= RWDTS_XPATH_ACTION_FN_END)) {
        rs = rwdts_xpath_eval_fn(rwpcb, id);
      }
      else if ((action >= RWDTS_XPATH_ACTION_EXOP_BEGIN) &&
               (action <= RWDTS_XPATH_ACTION_EXOP_END)) {
        rs = rwdts_xpath_eval_exop(rwpcb, id);
      }
      else {
        snprintf(rwpcb->errmsg, RWDTS_XPATH_ERR_MSG_MAX_SIZE,
                 "%s: Unknown action '%s'(%u) in %d",
                 __FUNCTION__,
                 rwdts_xpath_action_str[action],
                 action, id);
        rs = RW_STATUS_FAILURE;
      }
    }
    else {
      rs = rwdts_xpath_eval_const(rwpcb, id);
    }
  }

  rwpcb->depth--;

  if ((RW_STATUS_FAILURE != rs) && pending) {
    rs = RW_STATUS_PENDING;
  }

  return rs;
} /* rwdts_xpath_eval_id */

static RWDtsReduction*
rwdts_xpath_alloc_redn(rwdts_xpath_pcb_t *rwpcb,
                       int32_t deps)
{
  RW_ASSERT(rwpcb);

  RWDtsReduction* redn = RW_MALLOC0(sizeof(RWDtsReduction));
  if (redn) {
    rwdts_reduction__init(redn);
    redn->id = rwpcb->next_id++;
    redn->action = RWDTS_XPATH_ACTION_NONE;
    if (deps) {
      redn->n_dep_ids = deps;
      redn->dep_ids = RW_MALLOC0(sizeof(int32_t)*redn->n_dep_ids);
      if (redn->dep_ids) {
      }
      else {
        RW_FREE(redn);
        redn = NULL;
      }
    }
  }

  return redn;
} /* rwdts_xpath_alloc_redn */

static rw_status_t
rwdts_xpath_add_redn(rwdts_xpath_pcb_t *rwpcb,
                     RWDtsReduction* redn)
{
  rw_status_t rs = RW_STATUS_FAILURE;

  void *prev_block = NULL;
  RWDtsRednExpression *expr = rwpcb->expr;
  if (expr) {
    prev_block = expr->redn;
  }
  else {
    expr = (RWDtsRednExpression*)RW_MALLOC0(sizeof(RWDtsRednExpression));
    if (expr) {
      rwdts_redn_expression__init(expr);
      rwpcb->expr = expr;
    }
    else {
      return rs;
    }
  }

  if (expr->n_redn == 0) {
    expr->redn = (RWDtsReduction**)RW_MALLOC0(sizeof(RWDtsReduction*)*RWDTS_XPATH_EXPR_ALLOC_CHUNK_SIZE);
  }
  else if ((expr->n_redn % RWDTS_XPATH_EXPR_ALLOC_CHUNK_SIZE) == 0) {
    expr->redn = realloc(expr->redn,
                         (expr->n_redn+RWDTS_XPATH_EXPR_ALLOC_CHUNK_SIZE)*sizeof(RWDtsReduction*));
  }
  if (expr->redn) {
    expr->redn[rwpcb->expr->n_redn] = redn;
    expr->n_redn++;
    rs = RW_STATUS_SUCCESS;
  }
  else {
    if (prev_block) { // Realloc failed which had previous allocation
      expr->redn = prev_block; //Cleanup later
    }
  }
  return rs;
} /* rwdts_xpath_add_redn */

// Find the start of the path
rw_status_t
rwdts_xpath_build_expr_float(rwdts_xpath_pcb_t *rwpcb,
                             double val)
{
  RW_ASSERT(rwpcb);
  rw_status_t rs = RW_STATUS_FAILURE;

  RWDtsReduction* redn = rwdts_xpath_alloc_redn(rwpcb, 0);
  if (redn) {
    redn->has_val_double = TRUE;
    redn->val_double = val;
    rs = rwdts_xpath_add_redn(rwpcb, redn);
    if (RW_STATUS_SUCCESS != rs) {
      rwdts_reduction__free_unpacked(NULL, redn);
      redn = NULL;
    }
  }
  return rs;
} /* rwdts_xpath_build_expr_float */

rw_status_t
rwdts_xpath_build_expr_str(rwdts_xpath_pcb_t *rwpcb,
                           const char* val)
{
  RW_ASSERT(rwpcb);
  rw_status_t rs = RW_STATUS_FAILURE;

  RWDtsReduction* redn = rwdts_xpath_alloc_redn(rwpcb, 0);
  if (redn) {
    redn->val_str = strdup(val); //TODO: Use strndup and restrict length
    rs = rwdts_xpath_add_redn(rwpcb, redn);
    if (RW_STATUS_SUCCESS != rs) {
      RW_FREE(redn);
      redn = NULL;
    }
  }
  return rs;
} /* rwdts_xpath_build_expr_str */

static int32_t
rwdts_xpath_get_start_id(rwdts_xpath_pcb_t *rwpcb,
                         int32_t id)
{
  RW_ASSERT(rwpcb);
  RW_ASSERT(id < rwpcb->next_id);
  RW_ASSERT(id != -1);

  RWDtsReduction *redn = rwpcb->expr->redn[id];
  RW_ASSERT(redn);

  if ((redn->action == RWDTS_XPATH_ACTION_NT_NODE) ||
      (redn->action == RWDTS_XPATH_ACTION_PREDICATE)) {
    RW_ASSERT(redn->has_start_id);
    return redn->start_id;
  }
  else {
    return id;
  }
}

rw_status_t
rwdts_xpath_build_oper_ids(rwdts_xpath_pcb_t *rwpcb,
                           uint32_t action,
                           int32_t ids[],
                           int32_t count)
{
  RW_ASSERT(rwpcb);
  RW_ASSERT(action != RWDTS_XPATH_ACTION_NONE);
  RW_ASSERT(count <= RWDTS_XPATH_FN_MAX_PARAMS);
  if (count) {
    uint32_t i = 0;
    for (; i<count; i++) {
      RW_ASSERT(ids[i] < rwpcb->next_id);
    }
  }
  rw_status_t rs = RW_STATUS_FAILURE;

  RWDtsReduction* redn = rwdts_xpath_alloc_redn(rwpcb, count);
  if (redn) {
    uint32_t i = 0;
    for(;i<count; i++) {
      redn->dep_ids[i] = rwdts_xpath_get_start_id(rwpcb, ids[i]);
    }
    redn->action = action;

    rs = rwdts_xpath_add_redn(rwpcb, redn);
    if (RW_STATUS_SUCCESS != rs) {
      RW_FREE(redn);
      redn = NULL;
    }
  }

  return rs;
} /* rwdts_xpath_build_oper_ids */

rw_status_t
rwdts_xpath_build_oper_1_id(rwdts_xpath_pcb_t *rwpcb,
                            uint32_t action,
                            int32_t id1)
{
  int32_t ids[1];
  ids[0] = id1;
  return rwdts_xpath_build_oper_ids(rwpcb, action, ids, 1);
} /* rwdts_xpath_build_oper_1_id */

rw_status_t
rwdts_xpath_build_oper_2_ids(rwdts_xpath_pcb_t *rwpcb,
                             uint32_t action,
                             int32_t id1,
                             int32_t id2)
{
  int32_t ids[2];
  ids[0] = id1;
  ids[1] = id2;
  return rwdts_xpath_build_oper_ids(rwpcb, action, ids, 2);
} /* rwdts_xpath_build_oper_2_ids */

rw_status_t
rwdts_xpath_build_path(rwdts_xpath_pcb_t *rwpcb,
                       char* path,
                       int32_t prev_id)
{
  RW_ASSERT(rwpcb);
  rw_status_t rs = RW_STATUS_FAILURE;

  RWDtsReduction* redn = rwdts_xpath_alloc_redn(rwpcb, 0);
  if (redn) {
    redn->loc_path = path;
    redn->action = RWDTS_XPATH_ACTION_NT_NODE;
    if (prev_id != -1){
      RW_ASSERT(prev_id < rwpcb->next_id);
      rwpcb->expr->redn[prev_id]->next_id = redn->id;
      rwpcb->expr->redn[prev_id]->has_next_id = true;
      RW_ASSERT(rwpcb->expr->redn[prev_id]->has_start_id);
      redn->start_id = rwpcb->expr->redn[prev_id]->start_id;
      redn->has_start_id = true;
    }
    else {
      redn->start_id = redn->id;
      redn->has_start_id = true;
    }
    rwpcb->loc_path_id = redn->id;
    rs = rwdts_xpath_add_redn(rwpcb, redn);
    if (RW_STATUS_SUCCESS != rs) {
      RW_FREE(redn);
      redn = NULL;
      RW_FREE(path);
    }
  }
  else {
    RW_FREE(path);
  }

  return rs;
} /* rwdts_xpath_build_path */

rw_status_t
rwdts_xpath_build_predicate(rwdts_xpath_pcb_t *rwpcb,
                            int32_t path_id)
{
  RW_ASSERT(rwpcb);
  rw_status_t rs = RW_STATUS_FAILURE;

  RWDtsReduction* redn = rwdts_xpath_alloc_redn(rwpcb, 1);
  if (redn) {
    redn->action = RWDTS_XPATH_ACTION_PREDICATE;
    redn->dep_ids[0] = redn->id - 1;
    RW_ASSERT (path_id != -1);
    RW_ASSERT(path_id < rwpcb->next_id);
    rwpcb->expr->redn[path_id]->next_id = redn->id;
    rwpcb->expr->redn[path_id]->has_next_id = TRUE;
    RW_ASSERT(rwpcb->expr->redn[path_id]->has_start_id);
    redn->start_id = rwpcb->expr->redn[path_id]->start_id;
    redn->has_start_id = true;
    rwpcb->loc_path_id = redn->id;
    rs = rwdts_xpath_add_redn(rwpcb, redn);
    if (RW_STATUS_SUCCESS != rs) {
      RW_FREE(redn);
      redn = NULL;
    }
  }

  return rs;
} /* rwdts_xpath_build_predicate */

const char*
rwdts_xpath_action_to_str(rwdts_xpath_action_t act)
{
  RW_ASSERT(act >= RWDTS_XPATH_ACTION_NONE);
  RW_ASSERT(act < RWDTS_XPATH_ACTION_MAX_VAL);
  return rwdts_xpath_action_str[act];
} /* rwdts_xpath_action_to_str */

static char *
rwdts_xpath_strcat (char* buf, const char *fmt, ...)
{
  RW_ASSERT(buf);
  RW_ASSERT(fmt);

  size_t pos = strlen(buf);
  char *tmp = &buf[pos];
  va_list args;
  va_start(args, fmt);
  vsnprintf(tmp, RWDTS_XPATH_TK_DUMP_BUF_LEN-pos, fmt, args);
  va_end(args);

  return buf;
} /* rwdts_xpath_strcat  */

char*
rwdts_xpath_dump_tokens(rwdts_xpath_pcb_t *rwpcb)
{
  RW_ASSERT(rwpcb);

  const size_t len = RWDTS_XPATH_TK_DUMP_BUF_LEN;
  char* buf = RW_MALLOC0(len+1);

  RW_ASSERT(buf);

  if (rwpcb->expr && rwpcb->expr->n_redn) {
    RWDtsRednExpression *expr = rwpcb->expr;
    uint64_t i = 0;
    for(; i<expr->n_redn; i++) {
      RWDtsReduction *redn = expr->redn[i];
      RW_ASSERT(redn);
      rwdts_xpath_strcat(buf,
                         "ID:%i, action:%s, flags:0x%X, iter:%u, DepIDs:",
                         redn->id,
                         rwdts_xpath_action_to_str(redn->action),
                         redn->flags, redn->pass_);

      if (redn->n_dep_ids) {
        uint64_t j=0;
        for(;j<redn->n_dep_ids; j++) {
          rwdts_xpath_strcat(buf, "%i;", redn->dep_ids[j]);
        }
        rwdts_xpath_strcat(buf, ", ");
      }
      else {
        rwdts_xpath_strcat(buf, "NONE, ");
      }

      if (redn->has_next_id) {
        rwdts_xpath_strcat(buf, "Next ID:%i, ", redn->next_id);
      }
      else {
        rwdts_xpath_strcat(buf, "Next ID:NONE, ");
      }

      if (redn->has_start_id) {
        rwdts_xpath_strcat(buf, "Start ID:%i, ", redn->start_id);
      }
      else {
        rwdts_xpath_strcat(buf, "Start ID:NONE, ");
      }

      if (redn->loc_path) {
        rwdts_xpath_strcat(buf, "Path:%s, ", redn->loc_path);
      }
      else {
        rwdts_xpath_strcat(buf, "NONE, ");
      }

      if (redn->has_val_double) {
        rwdts_xpath_strcat(buf, "Float:%lf, ", redn->val_double);
      }

      if (redn->val_str) {
        rwdts_xpath_strcat(buf, "String:%s, ", redn->val_str);
      }

      rwdts_xpath_strcat(buf, "\n");
    }
  }

  return buf;
} /* rwdts_xpath_dump_tokens */

char*
rwdts_xpath_dump_results(rwdts_xpath_pcb_t *rwpcb)
{
  RW_ASSERT(rwpcb);

  const size_t len = RWDTS_XPATH_TK_DUMP_BUF_LEN;
  char* buf = RW_MALLOC0(len+1);

  RW_ASSERT(buf);

  if (rwpcb->results && rwpcb->results->n_entries) {
    RWDtsRednResp *results = rwpcb->results;
    uint64_t i = 0;
    for(; i<results->n_entries; i++) {
      RWDtsRednRespEntry *resp = results->entries[i];
      RW_ASSERT(resp);
      rwdts_xpath_strcat(buf,
                         "ID:%i, flags:0x%X, ",
                         resp->id,
                         resp->flags);

      if (resp->xpath) {
        rwdts_xpath_strcat(buf, "XPath:%s, ", resp->xpath);
      }

      if (resp->key) {
        if (!resp->key->keystr) {
          char *str = RW_MALLOC0(RWDTS_XPATH_PATH_MAX_SIZE);
          if (str) {
            if (rw_keyspec_path_get_print_buffer((rw_keyspec_path_t *)resp->key->keyspec,
                                                 rwpcb->ksi, rwpcb->schema,
                                                 str, RWDTS_XPATH_PATH_MAX_SIZE)) {
              resp->key->keystr = str;
            }
          }
        }
        rwdts_xpath_strcat(buf, "Key:%s, ", resp->key->keystr);
      }

      if (resp->leaf) {
        if (!resp->leaf->keystr) {
          char *str = RW_MALLOC0(RWDTS_XPATH_PATH_MAX_SIZE);
          if (str) {
            if (rw_keyspec_path_get_print_buffer((rw_keyspec_path_t *)resp->leaf->keyspec,
                                                 rwpcb->ksi, rwpcb->schema,
                                                 str, RWDTS_XPATH_PATH_MAX_SIZE)) {
              resp->leaf->keystr = str;
            }
          }
        }
        rwdts_xpath_strcat(buf, "Leaf:%s (type:%u, label:%u), ", resp->leaf->keystr,
                           resp->ltype, resp->llabel);
      }

      if (resp->has_count) {
        rwdts_xpath_strcat(buf, "count:%lu, ", resp->count);
      }

      if (resp->has_val_uint64) {
        rwdts_xpath_strcat(buf, "uint:%lu, ", resp->val_uint64);
      }

      if (resp->has_val_sint64) {
        rwdts_xpath_strcat(buf, "int:%li, ", resp->val_sint64);
      }

      if (resp->has_val_double) {
        rwdts_xpath_strcat(buf, "Double:%lf, ", resp->val_double);
      }

      if (resp->val_str) {
        rwdts_xpath_strcat(buf, "String:%s, ", resp->val_str);
      }

      rwdts_xpath_strcat(buf, "\n");
    }
  }

  return buf;

} /* rwdts_xpath_dump_results */

void
rwdts_xpath_print_pcb(rwdts_xpath_pcb_t *rwpcb,
                      const char* func)
{
  RW_ASSERT_TYPE(rwpcb, rwdts_xpath_pcb_t);

  fprintf(stderr, "\nRWDTS XPath pcb details in %s:\n", func);

  fprintf(stderr, "Query: %s\n", rwpcb->query);

  char *str = rwdts_xpath_dump_tokens(rwpcb);
  fprintf(stderr, "Tokens:\n%s", str);
  RW_FREE(str);
  str = NULL;

  str = rwdts_xpath_dump_results(rwpcb);
  fprintf(stderr, "Results:\n%s", str);
  RW_FREE(str);
  str = NULL;

  fprintf(stderr, "MISC: Iter:%u, Queries:%u, NextID:%i, PathID:%i, Predicate:%i, Depth:%i\n",
          rwpcb->pass, rwpcb->qcount, rwpcb->next_id, rwpcb->loc_path_id,
          rwpcb->predicate, rwpcb->depth);
  fprintf(stderr, "Errors:%s\n", rwpcb->errmsg);
  fprintf(stderr, "\n");
}

typedef struct rwdts_xpath_fn_names_hash_s {
  const char* name;
  rwdts_xpath_action_t fn;
  UT_hash_handle hh;
} rwdts_xpath_fn_names_hash_t;


// TODO: Need to protect the initialization or do
// the initialization differently
static rwdts_xpath_fn_names_hash_t*
rwdts_xpath_fname_hash()
{
  static rwdts_xpath_fn_names_hash_t* fns = NULL;

  if (fns) {
    return fns;
  }

  uint32_t i = 1; // Skip NONE
  // Will not work beyond FN_END as the strings are different
  for (;i<=RWDTS_XPATH_ACTION_FN_END; i++) {
    rwdts_xpath_fn_names_hash_t *h = RW_MALLOC0(sizeof(rwdts_xpath_fn_names_hash_t));
    h->name = rwdts_xpath_action_str[i];
    h->fn = i;
    HASH_ADD_KEYPTR(hh, fns, h->name, strlen(h->name), h);
  }

  return fns;
} /* rwdts_xpath_fname_hash */

rwdts_xpath_action_t
rwdts_xpath_map_fn(const char* fname)
{
  RW_ASSERT(fname);

  rwdts_xpath_fn_names_hash_t* fns = rwdts_xpath_fname_hash();
  RW_ASSERT(fns);

  rwdts_xpath_fn_names_hash_t* s = NULL;
  HASH_FIND_STR(fns, fname, s);
  RW_ASSERT(s);

  return s->fn;
} /* rwdts_xpath_map_fn */

