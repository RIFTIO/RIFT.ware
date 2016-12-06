
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
 * @file rwmsg_util.c
 * @author RIFT.io <info@riftio.com>
 * @date 2014/09/15
 * @brief DTS utility functions
 */

#include <rw_keyspec.h>
#include <rwdts_int.h>
#include <rwdts.h>
#include <rw_dts_int.pb-c.h>
#include "rw-dts.pb-c.h"



static char*  rwdts_print_code_point(RWDtsMemberCodePoints codepoint);
void rwdts_dbg_add_tr(RWDtsDebug *dbg, rwdts_trace_filter_t *filt) {
  RW_ASSERT(!dbg->tr);
  dbg->tr = RW_MALLOC0(sizeof(*dbg->tr));
  rwdts_traceroute__init(dbg->tr);
  if (filt) {
    dbg->tr->print_ = filt->print;
    dbg->tr->has_print_ = 1;
    dbg->tr->break_start = filt->break_start;
    dbg->tr->has_break_start = 1;
    dbg->tr->break_prepare = filt->break_prepare;
    dbg->tr->has_break_prepare = 1;
    dbg->tr->break_end = filt->break_end;
    dbg->tr->has_break_end = 1;
  }
}


/* Enable traceroute on this xact */
static void rwdts_xact_add_dbg(RWDtsXact *xact) {
  RW_ASSERT(!xact->dbg);
  xact->dbg = RW_MALLOC0(sizeof(*xact->dbg));
  rwdts_debug__init(xact->dbg);
}
void rwdts_xact_dbg_tracert_start(RWDtsXact *xact, rwdts_trace_filter_t *filt) {
  if (!xact->dbg) {
    rwdts_xact_add_dbg(xact);
  }
  xact->dbg->dbgopts1 |= RWDTS_DEBUG_TRACEROUTE;
  xact->dbg->has_dbgopts1 = TRUE;
  if (!xact->dbg->tr) {
    rwdts_dbg_add_tr(xact->dbg, filt);
  }
}

static void rwdts_xactres_add_dbg(RWDtsXactResult *xact) {
  RW_ASSERT(!xact->dbg);
  xact->dbg = RW_MALLOC0(sizeof(*xact->dbg));
  rwdts_debug__init(xact->dbg);
}
void rwdts_xactres_dbg_tracert_start(RWDtsXactResult *xact) {
  if (!xact->dbg) {
    rwdts_xactres_add_dbg(xact);
  }
  xact->dbg->dbgopts1 |= RWDTS_DEBUG_TRACEROUTE;
  xact->dbg->has_dbgopts1 = TRUE;
  if (!xact->dbg->tr) {
    rwdts_dbg_add_tr(xact->dbg, NULL);
  }
}

const char *
RWDtsXactEvt_str( RWDtsXactEvt evt)
{
#define ITOA(x) evt==RWDTS_EVT_##x?#x

  return( ITOA(PREPARE): ITOA(PRECOMMIT): ITOA(PREPARE_IDLE):
          ITOA(ABORT):   ITOA(QUERY):     ITOA(INIT):
          ITOA(COMMIT):  ITOA(SUBCOMMIT): ITOA(SUBABORT):
          ITOA(DONE):    ITOA(TERM):      "??EVENT??");
}

static int tr_bytime(const void *a, const void *b) {
  RWDtsTracerouteEnt *a_ent = *(RWDtsTracerouteEnt **)a;
  RWDtsTracerouteEnt *b_ent = *(RWDtsTracerouteEnt **)b;
  RW_ASSERT(a_ent);
  RW_ASSERT(b_ent);
  if (a_ent->tv_sec > b_ent->tv_sec) {
    return 1;
  } else if (a_ent->tv_sec < b_ent->tv_sec) {
    return -1;
  } else {
    if (a_ent->tv_usec > b_ent->tv_usec) {
      return 1;
    } else if (a_ent->tv_usec < b_ent->tv_usec) {
      return -1;
    } else {
      return 0;
    }
  }
}

void rwdts_dbg_tracert_dump(RWDtsTraceroute *tr,
                            rwdts_xact_t    *xact)
{
  int e, i, j;
  if (!tr) {
    return;
  }

  if (tr->n_ent) {
    qsort(tr->ent, tr->n_ent, sizeof(tr->ent[0]), tr_bytime);
  }

  char xact_str[512] = "";
  fprintf (stderr,  "Tracert xact-id[%s] %d entries:\n", 
           (char*)rwdts_xact_id_str(&xact->id, xact_str, sizeof(xact_str)), 
           (int)tr->n_ent);
  const rw_yang_pb_schema_t *schema = xact->apih->ypbc_schema;
  for (i=0; i<tr->n_ent; i++) {
    RWDtsTracerouteEnt *ent = tr->ent[i];
    struct timeval zero, tv_ent, delta;
    zero.tv_sec = tr->ent[0]->tv_sec;
    zero.tv_usec = tr->ent[0]->tv_usec;
    tv_ent.tv_sec = ent->tv_sec;
    tv_ent.tv_usec = ent->tv_usec;
    {
      int p;
      for (p=0;p<ent->n_evt;p+=3)
      {
        tv_ent.tv_sec = ent->evt[p+1];
        tv_ent.tv_usec = ent->evt[p+2];
        timersub(&tv_ent, &zero, &delta);
        fprintf (stderr,  "%d: @Location %u  +%03u.%06us \n",
                p,
                (unsigned)ent->evt[p],
                (unsigned)delta.tv_sec,
                (unsigned)delta.tv_usec);
      }
    }
    timersub(&tv_ent, &zero, &delta);

    if (ent->what != RWDTS_TRACEROUTE_WHAT_CODEPOINTS
        || getenv("RWMSG_DEBUG")) {
      fprintf (stderr,  "  %3d: +%03u.%06us",
              i,
              (unsigned)delta.tv_sec,
              (unsigned)delta.tv_usec);
      if (0) {
        fprintf (stderr,  " %s:%d",
                ent->func,
                ent->line);
      }
    }
    switch (ent->what) {
      default:
      case RWDTS_TRACEROUTE_WHAT_UNK:
        fprintf (stderr,  " ??WHAT??\n");
        break;
      case RWDTS_TRACEROUTE_WHAT_ABORT:
        fprintf (stderr,  " Sending req %s %s -> %s\n",
                RWDtsXactEvt_str(ent->event),
                ent->srcpath,
                ent->dstpath);
        break;
      case RWDTS_TRACEROUTE_WHAT_REQ:
        fprintf (stderr,  " client req %s %s -> %s\n",
                RWDtsXactEvt_str(ent->event),
                ent->srcpath,
                ent->dstpath);
        if (ent->event != RWDTS_EVT_QUERY) {
          for (j=0; j<ent->n_queryid; j++) {
            fprintf (stderr,  "                    [%d]: queryIdx %u/%03u\n",
                    j,
                    (unsigned int)((ent->queryid[j] >> 32) & 0xffffffff),
                    (unsigned int)(ent->queryid[j] & 0xffffffff));
          }
        }
        fprintf (stderr,  "                    state %s, evtrsp %s, %d results in %luus\n",
                ent->state == RWDTS_TRACEROUTE_STATE_NONE ? "NONE"
                : ent->state == RWDTS_TRACEROUTE_STATE_SENT ? "SENT"
                : ent->state == RWDTS_TRACEROUTE_STATE_RESPONSE ? "RESPONSE"
                : ent->state == RWDTS_TRACEROUTE_STATE_BOUNCE ? "BOUNCE"
                : "??STATE??",
                rwdts_evtrsp_to_str(ent->res_code),
                ent->res_count,
                ent->elapsed_us);
        break; 
      case RWDTS_TRACEROUTE_WHAT_PREP_TO:

        fprintf (stderr,  " prepcheck timeout '%s'\n",
                ent->srcpath);
        break;


      case RWDTS_TRACEROUTE_WHAT_END_FLAG:
        fprintf (stderr,  " END recvd from %s at %s\n",
                ent->dstpath, ent->srcpath);
        break;


      case RWDTS_TRACEROUTE_WHAT_MEMBER_MATCH:

        fprintf (stderr,  " member match in '%s'\n"
                "                    query key '%s'\n"
                "                    member key '%s'\n"
                "                    reg id '%u'\n",
                ent->dstpath,
                ent->srcpath,
                ent->matchpath,
                ent->reg_id);
        if (ent->event != RWDTS_EVT_QUERY) {
          for (j=0; j<ent->n_queryid; j++) {
            fprintf (stderr,  "                    [%d]: queryIdx %u/%03u\n",
                    j,
                    (unsigned int)((ent->queryid[j] >> 32) & 0xffffffff),
                    (unsigned int)(ent->queryid[j] & 0xffffffff));
          }
        }
        break;


      case RWDTS_TRACEROUTE_WHAT_ROUTE:
        fprintf (stderr,  " router route query %s matchdesc '%s'\n",
                ent->matchpath ? ent->matchpath : "???",
                ent->matchdesc ? ent->matchdesc : "???");
        fprintf (stderr, 
                "                    member key '%s'\n",
                ent->dstpath);
        if (ent->event != RWDTS_EVT_QUERY) {
          for (j=0; j<ent->n_queryid; j++) {
            fprintf (stderr,  "                    [%d]: queryIdx %u/%03u\n",
                    j,
                    (unsigned int)((ent->queryid[j] >> 32) & 0xffffffff),
                    (unsigned int)(ent->queryid[j] & 0xffffffff));
          }
        }
        break;
      case RWDTS_TRACEROUTE_WHAT_ADDBLOCK:
        if (ent->block) {
          fprintf (stderr,  " router add block id %u at %s:\n",
                  ent->block->block->blockidx,
                  ent->dstpath);
          for (j=0; j<ent->block->n_query; j++) {
            RWDtsQuery *q = ent->block->query[j];
            char *keystr_buf = NULL;
            char *keystr = NULL;
            char flagstr[256] = "\0"; /* two nuls, we print from flagstr[1] */
            if (q->flags & RWDTS_FLAG_SUBSCRIBER) {
              strcat(flagstr, "|SUBSCRIBER");
            }
            if (q->flags & RWDTS_FLAG_PUBLISHER) {
              strcat(flagstr, "|PUBLISHER");
            }
            if (q->flags & RWDTS_FLAG_DATASTORE) {
              strcat(flagstr, "|DATASTORE");
            }
            if (q->flags & RWDTS_XACT_FLAG_ANYCAST) {
              strcat(flagstr, "|ANYCAST");
            }
            if (q->flags & RWDTS_XACT_FLAG_ADVISE) {
              strcat(flagstr, "|ADVISE");
            }
            if (q->flags & RWDTS_FLAG_CACHE) {
              strcat(flagstr, "|CACHE");
            }
            if (q->flags & RWDTS_XACT_FLAG_REPLACE) {
              strcat(flagstr, "|REPLACE");
            }
            if (q->flags & RWDTS_XACT_FLAG_BLOCK_MERGE) {
              strcat(flagstr, "|BLOCK_MERGE");
            }
            if (q->flags & RWDTS_FLAG_SHARED) {
              strcat(flagstr, "|SHARED");
            }
            if (q->flags & RWDTS_FLAG_SHARDING) {
              strcat(flagstr, "|SHARDING");
            }
            if (q->key) {
              if (q->key->ktype == RWDTS_KEY_RWKEYSPEC
                  && q->key->keyspec) {
                rw_keyspec_path_get_new_print_buffer((rw_keyspec_path_t*)q->key->keyspec, NULL, schema, &keystr_buf);
                keystr = keystr_buf;
              }
              if (q->key->keystr) {
                keystr = q->key->keystr;
              }
            }
            fprintf (stderr,  "                    %d/%03d: %s corrid:%lu flags:%s payload:%s\n                             %s\n",
                    ent->block->block->blockidx,
                    j,
                    ( q->action == RWDTS_QUERY_CREATE ? "CREATE"
                     : q->action == RWDTS_QUERY_READ ? "READ"
                     : q->action == RWDTS_QUERY_UPDATE ? "UPDATE"
                     : q->action == RWDTS_QUERY_DELETE ? "DELETE"
                     : q->action == RWDTS_QUERY_RPC ? "RPC"
                     : "??QUERYVERB??" ),
                    q->corrid,
                    &flagstr[1],        /* skip leading '|' */
                    q->payload ? "Y" : "n",
                    keystr ? keystr : "??KEY??");
            if (keystr != keystr_buf && keystr_buf) {
              /* Also print the unschema'd string-o-numbers for comparison in other schema-less displays */
              fprintf (stderr,  "                             %s\n", keystr_buf);
            }
            free(keystr_buf);
          }
        }
        break;
      case RWDTS_TRACEROUTE_WHAT_RESULTS:
        fprintf (stderr,  " member results in '%s': res_code=%s res_count=%d\n",
                ent->srcpath,
                (ent->res_code == RWDTS_EVTRSP_ASYNC ? "ASYNC"
                 : ent->res_code == RWDTS_EVTRSP_NACK ? "NACK"
                 : ent->res_code == RWDTS_EVTRSP_ACK ? "ACK"
                 : ent->res_code == RWDTS_EVTRSP_NA ? "NA"
                 : ent->res_code == RWDTS_EVTRSP_INTERNAL ? "INTERNAL"
                 : "??"),
                ent->res_count);
        break;

      case RWDTS_TRACEROUTE_WHAT_CODEPOINTS:
        if (getenv("RWMSG_DEBUG")) {
          if (ent->n_evt)
            {
              fprintf (stderr,  " member code points");
              for (e = 0; e < ent->n_evt; e ++) {
                fprintf (stderr,  "%s%s",
                        e ? " -> " : "",
                        rwdts_print_code_point(ent->evt[e]));
              }
            }
          fprintf (stderr,  "\n");
        }
        break;
    }
  }
}

int rwdts_dbg_tracert_add_ent(RWDtsTraceroute *tr, RWDtsTracerouteEnt *in) {
  if (!tr) {
    return -1;
  }
  int idx = tr->n_ent++;
  tr->ent = RW_REALLOC(tr->ent, sizeof(tr->ent[0]) * tr->n_ent);
  RWDtsTracerouteEnt *ent = (RWDtsTracerouteEnt*)protobuf_c_message_duplicate(&protobuf_c_default_instance, &in->base, in->base.descriptor);
  struct timeval tv;
  gettimeofday(&tv, NULL);
  ent->tv_sec = tv.tv_sec;
  ent->tv_usec = tv.tv_usec;
  tr->ent[idx] = ent;
  return idx;
}

/* Copy error report items from src onto the end of dst's tracert list */
void rwdts_dbg_errrep_append(RWDtsErrorReport *dst, RWDtsErrorReport *src) {
  if (src->n_ent) {
    int idx = dst->n_ent;
    int newct = dst->n_ent + src->n_ent;
    dst->ent = RW_REALLOC(dst->ent, sizeof(dst->ent[0]) * newct);
    int sidx;
    for (sidx = 0; sidx < src->n_ent; sidx++) {
      RWDtsErrorEntry *ent = (RWDtsErrorEntry*)protobuf_c_message_duplicate(&protobuf_c_default_instance,
                                                                            &src->ent[sidx]->base,
                                                                            src->ent[sidx]->base.descriptor);
      dst->ent[idx + sidx] = ent;
    }
    dst->n_ent = newct;
  }
}

/* Copy tracert items from src onto the end of dst's tracert list */
void rwdts_dbg_tracert_append(RWDtsTraceroute *dst, RWDtsTraceroute *src) {
  if (src->n_ent) {
    int idx = dst->n_ent;
    int newct = dst->n_ent + src->n_ent;
    dst->ent = RW_REALLOC(dst->ent, sizeof(dst->ent[0]) * newct);
    int sidx;
    for (sidx = 0; sidx < src->n_ent; sidx++) {
      RWDtsTracerouteEnt *ent = (RWDtsTracerouteEnt*)protobuf_c_message_duplicate(&protobuf_c_default_instance,
                                                                                  &src->ent[sidx]->base,
                                                                                  src->ent[sidx]->base.descriptor);
      dst->ent[idx + sidx] = ent;
    }
    dst->print_ = src->print_;
    dst->break_start = src->break_start;
    dst->break_end = src->break_end;
    dst->break_prepare = src->break_prepare;
    dst->has_print_ = 1;
    dst->has_break_end = 1;
    dst->has_break_start = 1;
    dst->has_break_prepare = 1;
    dst->n_ent = newct;
  }
}


void rwdts_trace_event_print_block(RWDtsTracerouteEnt *ent, char *logbuf, rw_yang_pb_schema_t * schema)
{
  int len=0, j;

  if (ent->block) {
    for (j=0; j<ent->block->n_query; j++) {
      RWDtsQuery *q = ent->block->query[j];
      char *keystr_buf = NULL;
      char *keystr = NULL;
      char flagstr[256] = "\0";
      char act[32];
      if (q->flags & RWDTS_FLAG_SUBSCRIBER) { strcat(flagstr, "|SUBSCRIBER"); }
      if (q->flags & RWDTS_FLAG_PUBLISHER) { strcat(flagstr, "|PUBLISHER"); }
      if (q->flags & RWDTS_FLAG_DATASTORE) { strcat(flagstr, "|DATASTORE"); }
      if (q->flags & RWDTS_XACT_FLAG_ANYCAST) { strcat(flagstr, "|ANYCAST"); }
      if (q->flags & RWDTS_XACT_FLAG_ADVISE) { strcat(flagstr, "|ADVISE"); }
      if (q->flags & RWDTS_FLAG_CACHE) { strcat(flagstr, "|CACHE"); }
      if (q->flags & RWDTS_XACT_FLAG_REPLACE) { strcat(flagstr, "|REPLACE"); }
      if (q->flags & RWDTS_XACT_FLAG_BLOCK_MERGE) { strcat(flagstr, "|BLOCK_MERGE"); }
      if (q->flags & RWDTS_XACT_FLAG_STREAM) { strcat(flagstr, "|STREAM"); }
      if (q->flags & RWDTS_FLAG_SHARED) { strcat(flagstr, "|SHARED"); }
      if (q->flags & RWDTS_FLAG_SHARDING) { strcat(flagstr, "|SHARDING"); }
      if (q->flags & RWDTS_XACT_FLAG_DEPTH_FULL) { strcat(flagstr, "|DEPTH_FULL"); }
      if (q->flags & RWDTS_XACT_FLAG_DEPTH_LISTS) { strcat(flagstr, "|DEPTH_LISTS"); }
      if (q->flags & RWDTS_XACT_FLAG_DEPTH_ONE) { strcat(flagstr, "|DEPTH_ONE"); }
      if (q->flags & RWDTS_FLAG_SUBOBJECT) { strcat(flagstr, "|SUBOBJECT"); }
      if (q->key && schema) {
        if (q->key->ktype == RWDTS_KEY_RWKEYSPEC && q->key->keyspec) {
          rw_keyspec_path_get_new_print_buffer((rw_keyspec_path_t*)q->key->keyspec, NULL, schema, &keystr_buf);
          keystr = keystr_buf;
        }
        if (q->key->keystr) { keystr = q->key->keystr; }
      }
      len += snprintf(logbuf+len, RWDTS_MAX_LOGBUFSZ-len," %d/%03d: %s corrid:%lu flags:%s payload:%s  %s ",
              (ent->block->block->blockidx?ent->block->block->blockidx:0), j,
              rwdts_query_action_to_str(q->action,act,32),
              q->corrid, &flagstr[1],
              q->payload ? "Y" : "n", keystr ? keystr : "??KEY??");
      if (keystr != keystr_buf && keystr_buf) {
        len+= snprintf(logbuf+len, RWDTS_MAX_LOGBUFSZ-len,  " %s ", keystr_buf);
      }
      free(keystr_buf);
    }
   }
}

void rwdts_trace_print_req(RWDtsTracerouteEnt *ent, char *logbuf,char *evt, char *state,char *res_code)
{
    int len=0;
    logbuf[0]=0;
    snprintf(evt,32,"%s", RWDtsXactEvt_str(ent->event));
    if (ent->event != RWDTS_EVT_QUERY) {
      int j;
      for (j=0; j<ent->n_queryid; j++) {
        if (RWDTS_MAX_LOGBUFSZ<=len) break;
        len += snprintf(logbuf+len, RWDTS_MAX_LOGBUFSZ-len, "[%d]: queryIdx %u/%03u ",
                j, (unsigned int)((ent->queryid[j] >> 32) & 0xffffffff),
                (unsigned int)(ent->queryid[j] & 0xffffffff));
      }
    }
    snprintf(state,32,"%s", ent->state == RWDTS_TRACEROUTE_STATE_NONE ? "NONE"
            : ent->state == RWDTS_TRACEROUTE_STATE_SENT ? "SENT"
            : ent->state == RWDTS_TRACEROUTE_STATE_RESPONSE ? "RESPONSE"
            : ent->state == RWDTS_TRACEROUTE_STATE_BOUNCE ? "BOUNCE"
            : "??STATE??");
    snprintf(res_code,64,"%s", rwdts_evtrsp_to_str(ent->res_code));
}
RWDtsErrorReport *
rwdts_xact_create_err_report()
{
  RWDtsErrorReport *rep = NULL;
  rep = RW_MALLOC0(sizeof(RWDtsErrorReport));
  rwdts_error_report__init(rep);
  return rep;
}

rw_status_t
rwdts_xact_append_err_report(RWDtsErrorReport *rep,
                             RWDtsErrorEntry *ent)
{
  int idx = rep->n_ent++;
  rep->ent = RW_REALLOC(rep->ent, sizeof(rep->ent[0]) * rep->n_ent);
  RWDtsErrorEntry *entry = (RWDtsErrorEntry *)protobuf_c_message_duplicate(&protobuf_c_default_instance,
                                                                           &ent->base,
                                                                           ent->base.descriptor);

  struct timeval tv;
  gettimeofday(&tv, NULL);
  entry->tv_sec = tv.tv_sec;
  entry->tv_usec = tv.tv_usec;
  rep->ent[idx] = entry;
  return RW_STATUS_SUCCESS;
}
char*  rwdts_print_ent_type(RWDtsTracerouteEntType type)
{
  switch (type) {
    case RWDTS_TRACEROUTE_WHAT_UNK:
      return ("Traceroute Unknown");

    case RWDTS_TRACEROUTE_WHAT_ADDBLOCK:
      return ("Traceroute Add-Block");

    case RWDTS_TRACEROUTE_WHAT_REQ:
      return ("Traceroute Req");

    case RWDTS_TRACEROUTE_WHAT_RESULTS:
      return ("Traceroute Results");

    case RWDTS_TRACEROUTE_WHAT_ROUTE:
      return ("Traceroute Route");

    case RWDTS_TRACEROUTE_WHAT_MEMBER_MATCH:
      return ("Traceroute Member-Match");

    case RWDTS_TRACEROUTE_WHAT_ABORT:
      return ("Traceroute Abort");

    case RWDTS_TRACEROUTE_WHAT_QUEUED:
      return ("Traceroute Queued");

    default:
      return ("Unknown");
  }
  return ("Unknown");
}

char* rwdts_print_ent_state(RWDtsTracerouteEntState state)
{
  switch(state) {
    case  RWDTS_TRACEROUTE_STATE_NONE:
      return ("None");

    case RWDTS_TRACEROUTE_STATE_SENT:
      return ("Sent");

    case RWDTS_TRACEROUTE_STATE_RESPONSE:
      return ("response");

    case RWDTS_TRACEROUTE_STATE_BOUNCE:
      return ("bounce");

    default:
      return ("Unknown");
  }
  return ("Unknown");
}

char* rwdts_print_ent_event(RWDtsXactEvt evt)
{
  switch(evt) {
    case RWDTS_EVT_PREPARE:
      return ("Prepare");

    case RWDTS_EVT_PRECOMMIT:
      return ("Precommit");

    case RWDTS_EVT_COMMIT:
      return ("Commit");

    case RWDTS_EVT_ABORT:
      return ("Abort");

    case RWDTS_EVT_QUERY:
      return ("Query");

    case RWDTS_EVT_INIT:
      return ("Init");

    case RWDTS_EVT_PREPARE_IDLE:
      return ("Idle");

    case RWDTS_EVT_SUBCOMMIT:
      return ("SubCommit");

    case RWDTS_EVT_SUBABORT:
      return ("SubAbort");

    case RWDTS_EVT_DONE:
      return ("Done");

    case RWDTS_EVT_TERM:
      return ("Term");

    case RWDTS_EVT_QUEUED:
      return ("Queued");

    default:
      return ("Unknown");

  }

  return ("Unknown");
}

void
rwdts_report_unknown_fields(rw_keyspec_path_t* keyspec,
                            rwdts_api_t* apih,
                            ProtobufCMessage *msg)
{

  char *ks_str = NULL;
  const  rw_yang_pb_schema_t* ks_schema = NULL;

  RW_ASSERT(apih);
  RW_ASSERT(msg);
  RW_ASSERT(keyspec);

    // Get the schema for this registration
  if (!((ProtobufCMessage*)keyspec)->descriptor->ypbc_mdesc) {
    return;
  }
  ks_schema = ((ProtobufCMessage*)keyspec)->descriptor->ypbc_mdesc->module->schema;

  // If registration schema is NULL, set it to the schema from API
  if (ks_schema == NULL) {
    ks_schema = rwdts_api_get_ypbc_schema(apih);
  }

  rw_keyspec_path_get_new_print_buffer(keyspec, NULL, ks_schema, &ks_str);

  RWDTS_API_LOG_EVENT(apih, MsgUnknownField,
                      msg->descriptor->name, "Message sent contains unknown fields. Pruning unknown fields - Associated keyspec:",
                      ks_str);
  RW_FREE(ks_str);
  return;
}
char*  rwdts_print_code_point(RWDtsMemberCodePoints codepoint)
{
  switch (codepoint)
  {
    case RWDTS_MEMBER_dispatch_query_response:
      return "RWDTS_MEMBER_dispatch_query_response";
    case RWDTS_MEMBER_dispatch_query_response_f:
      return "RWDTS_MEMBER_dispatch_query_response_f";
    case RWDTS_MEMBER_send_query_rsp_evt:
      return "RWDTS_MEMBER_send_query_rsp_evt";
    case RWDTS_MEMBER_member_split_rsp:
      return "RWDTS_MEMBER_member_split_rsp";
    case RWDTS_MEMBER_respond_router:
      return "RWDTS_MEMBER_respond_router";
    case RWDTS_MEMBER_respond_router_f:
      return "RWDTS_MEMBER_respond_router_f";
    case RWDTS_MEMBER_respond_router_immediate:
      return "RWDTS_MEMBER_respond_router_immediate";
    default:
      return "CodePointNotFound";
  }
}

void rwdts_xact_id_memcpy(RWDtsXactID *dst, const RWDtsXactID *src) {
  dst->router_idx = src->router_idx;
  dst->taskstamp = src->taskstamp;
  dst->serialno = src->serialno;
  dst->client_idx = src->client_idx;
}

static __inline__ rwdts_xact_query_t*
rwdts_journal_find_inflt_query(rwdts_journal_entry_t *journal_entry, unsigned long serial)
{
  rwdts_xact_query_t  *xquery;
  HASH_FIND(hh_inflt_query, journal_entry->inflt_queries,  
           &(serial), sizeof(serial), xquery);
  return xquery;
}

static __inline__ void rwdts_journal_inflt_q_elem_init(rwdts_xact_query_t *xquery)
{
  rwdts_xact_query_ref(xquery, __PRETTY_FUNCTION__, __LINE__);
  xquery->inflt_links = RW_MALLOC0_TYPE(sizeof(rwdts_xact_query_journal_t), rwdts_xact_query_journal_t);
  RW_ASSERT_TYPE(xquery->inflt_links, rwdts_xact_query_journal_t);
}

static __inline__ void rwdts_journal_inflt_q_elem_deinit(rwdts_xact_query_t *xquery)
{
  rwdts_xact_query_journal_t *inflt_links = xquery->inflt_links;
  xquery->inflt_links = NULL;
  rwdts_xact_query_unref(xquery, __PRETTY_FUNCTION__, __LINE__);

  RW_ASSERT_TYPE(inflt_links, rwdts_xact_query_journal_t);
  RW_FREE_TYPE(inflt_links, rwdts_xact_query_journal_t);
}

static __inline__ void rwdts_journal_inflt_q_deq_all(rwdts_journal_entry_t *journal_entry)
{
  rwdts_xact_query_t *xquery = journal_entry->inflt_q;
  while (xquery) {
    journal_entry->inflt_q = RWDTS_JOURNAL_XQUERY_NEXT(xquery);
    if (journal_entry->inflt_q) {
      RWDTS_JOURNAL_XQUERY_PREV(journal_entry->inflt_q) = NULL;
    }
    journal_entry->inflt_q_len--;

    rwdts_journal_inflt_q_elem_deinit(xquery);
    xquery = journal_entry->inflt_q;
    RW_ASSERT(journal_entry->inflt_q_len > -1);
  }
  journal_entry->least_inflt_serial = ~(0UL);
  journal_entry->inflt_q_tail = NULL;
  RW_ASSERT(!journal_entry->inflt_q_len);
}

static __inline__ void rwdts_journal_inflt_q_enq(rwdts_journal_entry_t *journal_entry,
                                                 rwdts_xact_query_t *xquery)
{
  if (!journal_entry->inflt_q_tail) {
    journal_entry->inflt_q = xquery;
  }
  else {
    RWDTS_JOURNAL_XQUERY_NEXT(journal_entry->inflt_q_tail) = xquery;
    RWDTS_JOURNAL_XQUERY_PREV(xquery) = journal_entry->inflt_q_tail;
  }

  journal_entry->inflt_q_tail = xquery;
  journal_entry->inflt_q_len++;
  if (RWDTS_JOURNAL_QRY_SERIAL(xquery->query) <
      journal_entry->least_inflt_serial) {
    journal_entry->least_inflt_serial = RWDTS_JOURNAL_QRY_SERIAL(xquery->query);
  }
}

static __inline__ void rwdts_journal_set_least_inflt_serial(rwdts_journal_entry_t *journal_entry)
{
  rwdts_xact_query_t *xquery = journal_entry->inflt_q;
  journal_entry->least_inflt_serial = ~(0UL);
  while (xquery) {
    if (RWDTS_JOURNAL_QRY_SERIAL(xquery->query) <
        journal_entry->least_inflt_serial) {
      journal_entry->least_inflt_serial = 
          RWDTS_JOURNAL_QRY_SERIAL(xquery->query);
    }
    xquery = RWDTS_JOURNAL_XQUERY_NEXT(xquery);
  }
}

static __inline__ void
rwdts_journal_hash_rmv_inflt_query(rwdts_journal_entry_t *journal_entry, 
                                   rwdts_xact_query_t *xquery);
static __inline__ void rwdts_journal_inflt_q_deq(rwdts_journal_entry_t *journal_entry)
{
  rwdts_xact_query_t *xquery = journal_entry->inflt_q;
  journal_entry->inflt_q = RWDTS_JOURNAL_XQUERY_NEXT(xquery);
  if (journal_entry->inflt_q) {
    RWDTS_JOURNAL_XQUERY_PREV(journal_entry->inflt_q) = NULL;
  }
  else {
    RW_ASSERT((unsigned long)journal_entry->inflt_q_tail == (unsigned long)xquery);
    journal_entry->inflt_q_tail = NULL;
  }
  journal_entry->inflt_q_len--;
  RW_ASSERT(journal_entry->inflt_q_len > -1);
  if (RWDTS_JOURNAL_QRY_SERIAL(xquery->query) == 
      journal_entry->least_inflt_serial) {
    rwdts_journal_set_least_inflt_serial(journal_entry);
  }
  rwdts_journal_hash_rmv_inflt_query(journal_entry, xquery);
  rwdts_journal_inflt_q_elem_deinit(xquery);
}

static __inline__ void
rwdts_journal_rmv_inflt_q(rwdts_journal_entry_t *journal_entry,
                          rwdts_xact_query_t *xquery)
{
  if ((unsigned long)journal_entry->inflt_q == (unsigned long)xquery) {
    journal_entry->inflt_q = RWDTS_JOURNAL_XQUERY_NEXT(xquery);
    if (journal_entry->inflt_q) {
      RWDTS_JOURNAL_XQUERY_PREV(journal_entry->inflt_q) = NULL;
    }
    else {
      RW_ASSERT((unsigned long)journal_entry->inflt_q_tail == (unsigned long)xquery);
      journal_entry->inflt_q_tail = NULL;
    }
  }
  else if ((unsigned long)journal_entry->inflt_q_tail == (unsigned long)xquery) {
    journal_entry->inflt_q_tail = RWDTS_JOURNAL_XQUERY_PREV(xquery);
    if (journal_entry->inflt_q_tail) {
      RWDTS_JOURNAL_XQUERY_NEXT(journal_entry->inflt_q_tail) = NULL;
    }
    else {
      RW_CRASH();
    }
  }
  else {
    RWDTS_JOURNAL_XQUERY_NEXT(RWDTS_JOURNAL_XQUERY_PREV(xquery)) = RWDTS_JOURNAL_XQUERY_NEXT(xquery);
    RWDTS_JOURNAL_XQUERY_PREV(RWDTS_JOURNAL_XQUERY_NEXT(xquery)) = RWDTS_JOURNAL_XQUERY_PREV(xquery);
  }

  journal_entry->inflt_q_len--;
  RW_ASSERT(journal_entry->inflt_q_len > -1);
  if (RWDTS_JOURNAL_QRY_SERIAL(xquery->query) == journal_entry->least_inflt_serial) {
    rwdts_journal_set_least_inflt_serial(journal_entry);
  }
  rwdts_journal_inflt_q_elem_deinit(xquery);
}

static __inline__ void
rwdts_journal_add_inflt_query(rwdts_journal_entry_t *journal_entry, rwdts_xact_query_t *xquery)
{
  rwdts_xact_query_t *find_xquery = rwdts_journal_find_inflt_query(journal_entry, RWDTS_JOURNAL_QRY_SERIAL(xquery->query));

  if (!find_xquery) {
    HASH_ADD(hh_inflt_query, journal_entry->inflt_queries,  
             query->serial->serial,
             sizeof(RWDTS_JOURNAL_QRY_SERIAL(xquery->query)), xquery);
    rwdts_xact_query_ref(xquery, __PRETTY_FUNCTION__, __LINE__);
    rwdts_journal_inflt_q_elem_init(xquery);
    rwdts_journal_inflt_q_enq(journal_entry, xquery);
  }
  else {
    RW_ASSERT((unsigned long)find_xquery == (unsigned long)xquery);
  }
}

static __inline__ void
rwdts_journal_hash_rmv_inflt_query(rwdts_journal_entry_t *journal_entry, 
                                   rwdts_xact_query_t *xquery)

{
  HASH_DELETE(hh_inflt_query, journal_entry->inflt_queries,  xquery);
  RW_ASSERT_TYPE(xquery, rwdts_xact_query_t);
  rwdts_xact_query_unref(xquery, __PRETTY_FUNCTION__, __LINE__);
}

static __inline__ void
rwdts_journal_rmv_inflt_query(rwdts_journal_entry_t *journal_entry, 
                              rwdts_xact_query_t *xquery)

{
  rwdts_xact_query_t *find_xquery = rwdts_journal_find_inflt_query(journal_entry, RWDTS_JOURNAL_QRY_SERIAL(xquery->query));
  if (find_xquery) {
    RW_ASSERT((unsigned long)find_xquery == (unsigned long)xquery);
    rwdts_journal_hash_rmv_inflt_query(journal_entry, xquery);
    rwdts_journal_rmv_inflt_q(journal_entry, xquery);
  }
}


static __inline__ void
rwdts_journal_rmv_all_inflt_query(rwdts_journal_entry_t *journal_entry)
{
  rwdts_xact_query_t *xquery, *nxt_xquery;
  HASH_ITER(hh_inflt_query, journal_entry->inflt_queries,  xquery, nxt_xquery) {
    rwdts_journal_hash_rmv_inflt_query(journal_entry, xquery);
  }
  rwdts_journal_inflt_q_deq_all(journal_entry);
}

static __inline__ int
rwdts_journal_num_inflt_query(rwdts_journal_entry_t *journal_entry)
{
  RW_ASSERT(HASH_CNT(hh_inflt_query, journal_entry->inflt_queries) == journal_entry->inflt_q_len);
  return (HASH_CNT(hh_inflt_query, journal_entry->inflt_queries));
}

static __inline__ rwdts_journal_q_element_t
*rwdts_journal_done_q_elem_init(rwdts_xact_query_t *xquery,
                               rwdts_member_xact_evt_t evt)
{
  rwdts_journal_q_element_t *done_q_elem = RW_MALLOC0_TYPE(sizeof(*done_q_elem), rwdts_journal_q_element_t);
  done_q_elem->xquery = xquery;
  done_q_elem->evt = evt;
  done_q_elem->router_idx = xquery->xact->id.router_idx;
  done_q_elem->client_idx = xquery->xact->id.client_idx;
  done_q_elem->serialno = xquery->xact->id.serialno;
  rwdts_xact_query_ref(xquery, __PRETTY_FUNCTION__, __LINE__);
  return done_q_elem;
}

static __inline__ void rwdts_journal_done_q_elem_deinit(rwdts_journal_q_element_t *done_q_elem)
{
  RW_ASSERT_TYPE(done_q_elem, rwdts_journal_q_element_t);;
  rwdts_xact_query_unref(done_q_elem->xquery, __PRETTY_FUNCTION__, __LINE__);
  RW_FREE_TYPE(done_q_elem, rwdts_journal_q_element_t);
}

static __inline__ void rwdts_journal_done_q_deq_all(rwdts_journal_entry_t *journal_entry)
{
  rwdts_journal_q_element_t *done_q_elem = journal_entry->done_q;
  while (done_q_elem) {
    journal_entry->done_q = done_q_elem->next;
    journal_entry->done_q_len--;

    rwdts_journal_done_q_elem_deinit(done_q_elem);
    done_q_elem = journal_entry->done_q;
    RW_ASSERT(journal_entry->done_q_len > -1);
  }
  journal_entry->least_done_serial = ~(0UL);
  journal_entry->done_q_tail = NULL;
  RW_ASSERT(!journal_entry->done_q_len);
}

static __inline__ void rwdts_journal_done_q_enq(rwdts_journal_entry_t *journal_entry,
                                             rwdts_journal_q_element_t *done_q_elem)
{
  if (!journal_entry->done_q_tail) {
    journal_entry->done_q = done_q_elem;
  }
  else {
    journal_entry->done_q_tail->next = done_q_elem;
  }

  journal_entry->done_q_tail = done_q_elem;
  journal_entry->done_q_len++;
  if (RWDTS_JOURNAL_QRY_SERIAL(done_q_elem->xquery->query) <
      journal_entry->least_done_serial) {
    journal_entry->least_done_serial = RWDTS_JOURNAL_QRY_SERIAL(done_q_elem->xquery->query);
  }
}


static __inline__ void rwdts_journal_set_least_done_serial(rwdts_journal_entry_t *journal_entry)
{
  rwdts_journal_q_element_t *done_q_elem = journal_entry->done_q;
  journal_entry->least_done_serial = ~(0UL);
  while (done_q_elem) {
    if (RWDTS_JOURNAL_QRY_SERIAL(done_q_elem->xquery->query) <
        journal_entry->least_done_serial) {
      journal_entry->least_done_serial = 
          RWDTS_JOURNAL_QRY_SERIAL(done_q_elem->xquery->query);
    }
    done_q_elem = done_q_elem->next;
  }
}

static __inline__ rwdts_journal_q_element_t *rwdts_journal_done_q_deq(rwdts_journal_entry_t *journal_entry)
{
  rwdts_journal_q_element_t *done_q_elem = journal_entry->done_q;
  journal_entry->done_q = done_q_elem->next;
  journal_entry->done_q_len--;
  RW_ASSERT(journal_entry->done_q_len > -1);
  if (RWDTS_JOURNAL_QRY_SERIAL(done_q_elem->xquery->query) == 
      journal_entry->least_done_serial) {
    rwdts_journal_set_least_done_serial(journal_entry);
  }
  return (done_q_elem);
}

static __inline__ rwdts_journal_entry_t *rwdts_journal_find_entry(rwdts_journal_t *journal,
                                                                  RwDtsPubIdentifier *id)
{
  rwdts_journal_entry_t *journal_entry = NULL;
  RWDTS_FIND_WITH_PBKEY_IN_HASH(hh_journal_entry, journal->journal_entries, id, has_member_id, journal_entry);
  return (journal_entry);
}

static __inline__ int rwdts_journal_tracking_count(rwdts_journal_entry_t *journal_entry)
{
  return (rwdts_journal_num_inflt_query(journal_entry) + journal_entry->done_q_len);
}

static __inline__ void rwdts_journal_in_use_restrict(rwdts_journal_entry_t *journal_entry)
{
  if (rwdts_journal_tracking_count(journal_entry) >= RWDTS_JOURNAL_IN_USE_QLEN) {
    if (rwdts_journal_num_inflt_query(journal_entry)) {
      rwdts_journal_inflt_q_deq(journal_entry);
    }
    else {
      rwdts_journal_q_element_t *done_q_elem = rwdts_journal_done_q_deq(journal_entry);
      rwdts_journal_done_q_elem_deinit(done_q_elem);
    }
  }
}

rw_status_t rwdts_journal_update(rwdts_member_registration_t *reg,
                                 rwdts_xact_query_t *xquery,
                                 rwdts_member_xact_evt_t evt)
{
  rwdts_journal_t *journal = reg->journal;
  if (journal && RWDTS_JOURNAL_USED(journal->journal_mode)) {
    RW_ASSERT_TYPE(journal, rwdts_journal_t);
    RWDtsQuery *query = xquery->query;
    if (!RWDTS_JOURNAL_IS_VALID_QUERY(query) || 0) {
      return RW_STATUS_SUCCESS;
    }

    rwdts_journal_entry_t *journal_entry = rwdts_journal_find_entry(journal, 
                                                                    RWDTS_JOURNAL_QRY_ID_PTR(query));
    if (!journal_entry) {
      journal_entry = RW_MALLOC0_TYPE(sizeof(*journal_entry), rwdts_journal_entry_t);;
      journal_entry->least_done_serial = ~(0UL);
      protobuf_c_message_memcpy(&journal_entry->id.base, &(RWDTS_JOURNAL_QRY_ID_PTR(query)->base));
      RWDTS_ADD_PBKEY_TO_HASH(hh_journal_entry, journal->journal_entries, (&journal_entry->id), has_member_id, journal_entry);
    }
    RW_ASSERT_TYPE(journal_entry, rwdts_journal_entry_t);

    switch (evt) {
      case RWDTS_MEMB_XACT_EVT_PREPARE:
        switch (journal->journal_mode) {
          case RWDTS_JOURNAL_IN_USE: 
            rwdts_journal_in_use_restrict(journal_entry);
            /* Fall through */
          case RWDTS_JOURNAL_LIVE: 
            rwdts_journal_add_inflt_query(journal_entry, xquery);
            break;
          default:
            break;
        }
        break;
      case RWDTS_MEMB_XACT_EVT_COMMIT:
      case RWDTS_MEMB_XACT_EVT_ABORT:
      case RWDTS_MEMB_XACT_EVT_END:
        switch (journal->journal_mode) {
          case RWDTS_JOURNAL_IN_USE: 
            rwdts_journal_in_use_restrict(journal_entry);
          case RWDTS_JOURNAL_LIVE: 
            {
              rwdts_journal_q_element_t *done_q_elem = rwdts_journal_done_q_elem_init(xquery, evt);
              RW_ASSERT_TYPE(done_q_elem, rwdts_journal_q_element_t);
              rwdts_journal_done_q_enq(journal_entry, done_q_elem);
            }
            break;
          default:
            break;
        }
        /* though not possible to have xact gone at this point 
         * it is better to remove from inflight after enqueued to
         * hold on to ref for xquery
         */
        rwdts_journal_rmv_inflt_query(journal_entry, xquery);
        break;
      default:
        break;
    }
  }
  return RW_STATUS_SUCCESS;
}

static __inline__ rw_status_t rwdts_journal_cleanup(rwdts_journal_t *journal)
{
  if (journal) {
    rwdts_journal_entry_t *journal_entry, *nxt_journal_entry;
    HASH_ITER(hh_journal_entry, journal->journal_entries, journal_entry, nxt_journal_entry) {
      HASH_DELETE(hh_journal_entry, journal->journal_entries, journal_entry);
      RW_ASSERT_TYPE(journal_entry, rwdts_journal_entry_t);
      rwdts_journal_rmv_all_inflt_query(journal_entry);
      rwdts_journal_done_q_deq_all(journal_entry);
      RW_FREE_TYPE(journal_entry, rwdts_journal_entry_t);
    }
  }
  return RW_STATUS_SUCCESS;
}

rw_status_t rwdts_journal_set_mode(rwdts_member_registration_t *reg,
                                   rwdts_journal_mode_t journal_mode)
{
  rwdts_journal_t *journal = reg->journal;
  if (!reg->journal) {
    journal = RW_MALLOC0_TYPE(sizeof(*journal), rwdts_journal_t);
    reg->journal = journal;
  }
  
  if (journal_mode != RWDTS_JOURNAL_LIVE) {
    rwdts_journal_cleanup(journal);
  }
  journal->journal_mode = journal_mode;
  return RW_STATUS_SUCCESS;
}

void rwdts_journal_consume_xquery(rwdts_xact_query_t *xquery,
                                  rwdts_member_xact_evt_t evt)
{
  rwdts_match_info_t *match;
  rw_status_t rs;
  for (match = RW_SKLIST_HEAD(&(xquery->reg_matches), rwdts_match_info_t);
       match && 0;
       match = RW_SKLIST_NEXT(match, rwdts_match_info_t, match_elt)) {
    if (((match->evtrsp == RWDTS_EVTRSP_ACK)
         || (match->evtrsp == RWDTS_EVTRSP_ASYNC))
        && (xquery->query->flags & RWDTS_XACT_FLAG_ADVISE) 
        && (match->reg->flags & RWDTS_FLAG_SUBSCRIBER)) {
      rs = rwdts_journal_update(match->reg, xquery, evt);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
    }
  }
}

static rwdts_member_rsp_code_t 
rwdts_journal_show_prepare(const rwdts_xact_info_t* xact_info,
                            RWDtsQueryAction         action,
                            const rw_keyspec_path_t*      key,
                            const ProtobufCMessage*  msg,
                            uint32_t credits,
                            void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_api_t *apih = (rwdts_api_t *)xact_info->ud;

  rwdts_member_registration_t *reg;
  int count = 0;
  for (reg = RW_SKLIST_HEAD (&apih->reg_list, rwdts_member_registration_t);
       reg;
       reg = RW_SKLIST_NEXT(reg, rwdts_member_registration_t, element)) {
    if ((reg->flags & RWDTS_FLAG_SUBSCRIBER)
        && (reg->flags & RWDTS_FLAG_CACHE)
        && (reg->journal)) {
      count++;
    }
  }


  RWPB_T_MSG(RwDts_data_JournalInfo_Subs) subs, *subs_p;
  subs_p = &subs;
  RWPB_F_MSG_INIT(RwDts_data_JournalInfo_Subs, subs_p);

  subs_p->path =  strdup (apih->client_path);
  subs_p->reg_info =  RW_MALLOC0(sizeof(subs_p->reg_info[0]) * count);
  rwdts_journal_t *journal;
  rwdts_journal_entry_t *journal_entry, *nxt_journal_entry;
  for (reg = RW_SKLIST_HEAD (&apih->reg_list, rwdts_member_registration_t);
       reg;
       reg = RW_SKLIST_NEXT(reg, rwdts_member_registration_t, element)) {
    if ((reg->flags & RWDTS_FLAG_SUBSCRIBER)
        && (reg->flags & RWDTS_FLAG_CACHE)
        && (reg->journal)) {
      RWPB_T_MSG(RwDts_data_JournalInfo_Subs_RegInfo) *reg_info_p = RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_JournalInfo_Subs_RegInfo)));
      RWPB_F_MSG_INIT(RwDts_data_JournalInfo_Subs_RegInfo, reg_info_p);
      reg_info_p->id = reg->reg_id;
      reg_info_p->has_id = 1;
      reg_info_p->keystr = strdup (reg->keystr);
      journal = reg->journal;
      reg_info_p->has_journal_mode = 1;
      reg_info_p->journal_mode = reg->journal->journal_mode;
      reg_info_p->pub_list = RW_MALLOC0(sizeof(reg_info_p->pub_list[0]) * HASH_CNT(hh_journal_entry, journal->journal_entries));
      HASH_ITER(hh_journal_entry, journal->journal_entries, journal_entry, nxt_journal_entry) {
        RW_ASSERT_TYPE(journal_entry, rwdts_journal_entry_t);
        RWPB_T_MSG(RwDts_data_JournalInfo_Subs_RegInfo_PubList) *pub_list_p = RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_JournalInfo_Subs_RegInfo_PubList)));
        RWPB_F_MSG_INIT(RwDts_data_JournalInfo_Subs_RegInfo_PubList, pub_list_p);
        pub_list_p->member_id = journal_entry->id.member_id;
        pub_list_p->router_id = journal_entry->id.router_id;
        pub_list_p->pub_id = journal_entry->id.pub_id;
        pub_list_p->has_member_id = 1;
        pub_list_p->has_router_id = 1;
        pub_list_p->has_pub_id = 1;

        pub_list_p->inflt_query = RW_MALLOC0(sizeof(pub_list_p->inflt_query[0]) * journal_entry->inflt_q_len);
        rwdts_xact_query_t *inflt_query = journal_entry->inflt_q;
        while (inflt_query) {
          RWPB_T_MSG(RwDts_data_JournalInfo_Subs_RegInfo_PubList_InfltQuery) *inflt_query_p = RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_JournalInfo_Subs_RegInfo_PubList_InfltQuery)));
          RWPB_F_MSG_INIT(RwDts_data_JournalInfo_Subs_RegInfo_PubList_InfltQuery, inflt_query_p);

          int r = asprintf (&inflt_query_p->xact_id, "%lu:%lu:%lu", 
                            inflt_query->xact->id.router_idx,
                            inflt_query->xact->id.client_idx,
                            inflt_query->xact->id.serialno);
          RW_ASSERT(r != -1);
          inflt_query_p->query_idx = inflt_query->query->queryidx;
          inflt_query_p->has_query_idx = 1;
          inflt_query_p->has_query_action = 1;
          inflt_query_p->query_action = inflt_query->query->action;
          inflt_query_p->query_keystr = strdup(inflt_query->query->key->keystr);
          inflt_query_p->has_corrid = 1;
          inflt_query_p->corrid = inflt_query->query->corrid;
          inflt_query_p->has_serial = 1;
          inflt_query_p->serial = RWDTS_JOURNAL_QRY_SERIAL(inflt_query->query);
          pub_list_p->inflt_query[pub_list_p->n_inflt_query++] = inflt_query_p;
          inflt_query = RWDTS_JOURNAL_XQUERY_NEXT(inflt_query);
        }
        RW_ASSERT (pub_list_p->n_inflt_query == journal_entry->inflt_q_len);
        pub_list_p->has_least_inflt_serial = 1;
        pub_list_p->least_inflt_serial = journal_entry->least_inflt_serial;

        pub_list_p->done_q = RW_MALLOC0(sizeof(pub_list_p->done_q[0]) * journal_entry->done_q_len);
        rwdts_journal_q_element_t *done_q_elem = journal_entry->done_q;
        while(done_q_elem) {
          RWPB_T_MSG(RwDts_data_JournalInfo_Subs_RegInfo_PubList_DoneQ) *done_q_p = RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_JournalInfo_Subs_RegInfo_PubList_DoneQ)));
          RWPB_F_MSG_INIT(RwDts_data_JournalInfo_Subs_RegInfo_PubList_DoneQ, done_q_p);
    
          int r = asprintf (&done_q_p->xact_id, "%lu:%lu:%lu", 
                            done_q_elem->router_idx,
                            done_q_elem->client_idx,
                            done_q_elem->serialno);
          RW_ASSERT(r != -1);
          done_q_p->query_idx = done_q_elem->xquery->query->queryidx;
          done_q_p->has_query_idx = 1;
          done_q_p->has_query_action = 1;
          done_q_p->query_action = done_q_elem->xquery->query->action;
          done_q_p->query_keystr = strdup(done_q_elem->xquery->query->key->keystr);
          done_q_p->has_corrid = 1;
          done_q_p->corrid = done_q_elem->xquery->query->corrid;
          done_q_p->has_serial = 1;
          done_q_p->serial = RWDTS_JOURNAL_QRY_SERIAL(done_q_elem->xquery->query);
          done_q_p->has_done_event = 1;
          done_q_p->done_event = done_q_elem->evt;
          pub_list_p->done_q[pub_list_p->n_done_q++] = done_q_p;
          done_q_elem = done_q_elem->next;
        }
        RW_ASSERT (pub_list_p->n_done_q == journal_entry->done_q_len);
        pub_list_p->has_least_done_serial = 1;
        pub_list_p->least_done_serial = journal_entry->least_done_serial;

        reg_info_p->pub_list[reg_info_p->n_pub_list++] = pub_list_p;
      }
      subs_p->reg_info[subs_p->n_reg_info++] = reg_info_p;
    }
  }
  rw_keyspec_path_t *ks = NULL;
  rw_keyspec_path_create_dup(&RWPB_G_PATHSPEC_VALUE(RwDts_data_JournalInfo_Subs)->rw_keyspec_path_t,
                               &apih->ksi,
                               &ks);
  RWPB_T_PATHSPEC(RwDts_data_JournalInfo_Subs) *subs_ks  =
        (RWPB_T_PATHSPEC(RwDts_data_JournalInfo_Subs)*)ks;
  subs_ks->has_dompath = 1;
  subs_ks->dompath.has_path001 = 1;
  subs_ks->dompath.path001.has_key00 = 1;
  subs_ks->dompath.path001.key00.path = strdup (apih->client_path);

  rwdts_member_query_rsp_t rsp = {
    .ks = (rw_keyspec_path_t *)ks,
    .n_msgs = 1,
    .msgs = (ProtobufCMessage**)&subs_p,
    .evtrsp = RWDTS_EVTRSP_ACK
  };

  rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
  protobuf_c_message_free_unpacked_usebody(NULL, (ProtobufCMessage*)subs_p);
  rw_keyspec_path_free((rw_keyspec_path_t*)ks, &apih->ksi);
  return RWDTS_ACTION_OK;
}

rw_status_t rwdts_journal_show_init(rwdts_api_t *apih) 
{
    
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rw_keyspec_path_t *ks = NULL;
  rw_keyspec_path_create_dup(&RWPB_G_PATHSPEC_VALUE(RwDts_data_JournalInfo_Subs)->rw_keyspec_path_t,
                               &apih->ksi,
                               &ks);
  RW_ASSERT(ks);
  rw_keyspec_path_set_category (ks, &apih->ksi, RW_SCHEMA_CATEGORY_DATA);
  RWPB_T_PATHSPEC(RwDts_data_JournalInfo_Subs) *subs_ks  =
        (RWPB_T_PATHSPEC(RwDts_data_JournalInfo_Subs)*)ks;
  subs_ks->has_dompath = 1;
  subs_ks->dompath.has_path001 = 1;
  subs_ks->dompath.path001.has_key00 = 1;
  subs_ks->dompath.path001.key00.path = strdup (apih->client_path);

  rwdts_member_event_cb_t           callback = { 
    .cb.prepare = rwdts_journal_show_prepare,
    .ud = (void*)apih
  };


  apih->journal_regh = rwdts_member_register (
      NULL,
      apih,
      ks,
      &callback,
      RWPB_G_MSG_PBCMD(RwDts_data_JournalInfo_Subs),
      RWDTS_FLAG_PUBLISHER,
      NULL);

  rw_keyspec_path_free(ks, &apih->ksi);

  return RW_STATUS_SUCCESS;
}

rw_status_t rwdts_journal_compare(rwdts_member_registration_t *reg)
{
  return RW_STATUS_SUCCESS;
}

rw_status_t rwdts_journal_merge(rwdts_member_registration_t *reg)
{
  return RW_STATUS_SUCCESS;
}

rw_status_t rwdts_journal_destroy(rwdts_member_registration_t *reg)
{
  rwdts_journal_t *journal = reg->journal;
  reg->journal = NULL;
  rwdts_journal_cleanup(journal);
  RW_FREE_TYPE(journal, rwdts_journal_t);
  return RW_STATUS_SUCCESS;
}
