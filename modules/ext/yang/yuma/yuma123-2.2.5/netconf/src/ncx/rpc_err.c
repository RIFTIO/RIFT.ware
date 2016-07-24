/*
 * Copyright (c) 2008 - 2012, Andy Bierman, All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
/*  FILE: rpc_err.c

   NETCONF Protocol Operations: Common RPC Error Support

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
05may05      abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <memory.h>

#include <xmlstring.h>

#ifndef _H_procdefs
#include  "procdefs.h"
#endif

#ifndef _H_ncxconst
#include  "ncxconst.h"
#endif

#ifndef _H_log
#include  "log.h"
#endif

#ifndef _H_ncx
#include  "ncx.h"
#endif

#ifndef _H_ncx_num
#include  "ncx_num.h"
#endif

#ifndef _H_rpc
#include  "rpc.h"
#endif

#ifndef _H_rpc_err
#include  "rpc_err.h"
#endif

#ifndef _H_status
#include  "status.h"
#endif

#ifndef _H_xmlns
#include  "xmlns.h"
#endif

#ifndef _H_xml_util
#include  "xml_util.h"
#endif


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

#define RPC_ERR_NUM_ERRORS   ((uint32)RPC_ERR_LAST_ERROR)


/********************************************************************
*                                                                   *
*                           T Y P E S                               *
*                                                                   *
*********************************************************************/

/* map internal enum to string value for each error code */    
typedef struct rpc_err_map_t_ {
    rpc_err_t      errid;
    const xmlChar *errtag;
} rpc_err_map_t;


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/

/* error-tag names */
static const rpc_err_map_t  rpc_err_map[] = {
    { RPC_ERR_NONE, (const xmlChar *)"none" },
    { RPC_ERR_IN_USE, (const xmlChar *)"in-use" },
    { RPC_ERR_INVALID_VALUE, (const xmlChar *)"invalid-value" },
    { RPC_ERR_TOO_BIG, (const xmlChar *)"too-big" },
    { RPC_ERR_MISSING_ATTRIBUTE, (const xmlChar *)"missing-attribute" },
    { RPC_ERR_BAD_ATTRIBUTE, (const xmlChar *)"bad-attribute" },
    { RPC_ERR_UNKNOWN_ATTRIBUTE, (const xmlChar *)"unknown-attribute" },
    { RPC_ERR_MISSING_ELEMENT, (const xmlChar *)"missing-element" },
    { RPC_ERR_BAD_ELEMENT, (const xmlChar *)"bad-element" },
    { RPC_ERR_UNKNOWN_ELEMENT, (const xmlChar *)"unknown-element" },
    { RPC_ERR_UNKNOWN_NAMESPACE, (const xmlChar *)"unknown-namespace" },
    { RPC_ERR_ACCESS_DENIED, (const xmlChar *)"access-denied" },
    { RPC_ERR_LOCK_DENIED, (const xmlChar *)"lock-denied" },
    { RPC_ERR_RESOURCE_DENIED, (const xmlChar *)"resource-denied" },
    { RPC_ERR_ROLLBACK_FAILED, (const xmlChar *)"rollback-failed" },
    { RPC_ERR_DATA_EXISTS, (const xmlChar *)"data-exists" },
    { RPC_ERR_DATA_MISSING, (const xmlChar *)"data-missing" },
    { RPC_ERR_OPERATION_NOT_SUPPORTED,  
      (const xmlChar *)"operation-not-supported" },
    { RPC_ERR_OPERATION_FAILED, (const xmlChar *)"operation-failed" },
    { RPC_ERR_PARTIAL_OPERATION, (const xmlChar *)"partial-operation" },
    { RPC_ERR_MALFORMED_MESSAGE, (const xmlChar *)"malformed-message" },
    { RPC_ERR_NONE, NULL }
};


/********************************************************************
* FUNCTION dump_error_info
*
* Dump the error-info struct
*
* INPUTS:
*   errinfo == rpc_err_info_t struct to dump
*
*********************************************************************/
static void
    dump_error_info (ncx_instance_t *instance, const rpc_err_info_t  *errinfo)
{
    const xmlChar *prefix;

    log_write(instance, 
           "\n  error-info: %s T:%s = ", 
           (errinfo->name) ? (const char *)errinfo->name : "--",
           tk_get_btype_sym(errinfo->val_btype));

    if (errinfo->isqname) {
        prefix = xmlns_get_ns_prefix(instance, errinfo->val_nsid);
        log_write(instance, 
                  "%s:%s", 
                  (prefix) ? prefix : (const xmlChar *)"--",
                  (errinfo->v.strval) ? 
                  (const char *)errinfo->v.strval : "--");
        return;
    }

    switch (errinfo->val_btype) {
    case NCX_BT_BINARY:
    case NCX_BT_STRING:
    case NCX_BT_INSTANCE_ID:
        log_write(instance, "%s", (errinfo->v.strval) ? 
               (const char *)errinfo->v.strval : "--");
        break;
    case NCX_BT_BOOLEAN:
        SET_ERROR(instance, ERR_NCX_OPERATION_NOT_SUPPORTED);
        /***/
        break;
    case NCX_BT_INT8:
    case NCX_BT_INT16:
    case NCX_BT_INT32:
    case NCX_BT_INT64:
    case NCX_BT_UINT8:
    case NCX_BT_UINT16:
    case NCX_BT_UINT32:
    case NCX_BT_UINT64:
    case NCX_BT_DECIMAL64:
    case NCX_BT_FLOAT64:
        ncx_printf_num(instance, &errinfo->v.numval, errinfo->val_btype);
        break;
    default:
        val_dump_value(instance,
                       (val_value_t *)errinfo->v.cpxval,
                       NCX_DEF_INDENT*2);
    }

}  /* dump_error_info */


/********************************************************************
* FUNCTION dump_error
*
* Dump the error message
*
* INPUTS:
*   err == rpc_err_rec_t struct to dump
*
*********************************************************************/
static void
    dump_error (ncx_instance_t *instance, const rpc_err_rec_t  *err)
{
    const rpc_err_info_t  *errinfo;

    log_write(instance, 
              "\nrpc-error: (%u) %s L:%s S:%s ", 
              err->error_res,
              (err->error_tag) ? (const char *)err->error_tag : "--",
              ncx_get_layer(instance, err->error_type),
              rpc_err_get_severity(instance, err->error_severity));
    if (err->error_app_tag) {
        log_write(instance, "app-tag:%s ", err->error_app_tag);
    }
    if (err->error_path) {
        log_write(instance, "\n   path:%s ", err->error_path);
    }
    if (err->error_message_lang) {
        log_write(instance, "lang:%s ", err->error_message_lang);
    }
    if (err->error_message) {
        log_write(instance, "\n  msg:%s ", err->error_message);
    }

    for (errinfo = (const rpc_err_info_t *)dlq_firstEntry(instance, &err->error_info);
         errinfo != NULL;
         errinfo = (const rpc_err_info_t *)dlq_nextEntry(instance, errinfo)) {
        dump_error_info(instance, errinfo);
    }
    log_write(instance, "\n");

}  /* dump_error */


/**************    E X T E R N A L   F U N C T I O N S **********/


/********************************************************************
* FUNCTION rpc_err_get_errtag
*
* Get the RPC error-tag for an rpc_err_t enumeration
*
* INPUTS:
*   errid == rpc error enum to convert to a string
*
* RETURNS:
*   string for the specified error-tag enum
*********************************************************************/
const xmlChar *
    rpc_err_get_errtag (ncx_instance_t *instance, rpc_err_t errid)
{
    if (errid > RPC_ERR_LAST_ERROR) {
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        errid = RPC_ERR_NONE;
    }
    return rpc_err_map[errid].errtag;

} /* rpc_err_get_errtag */


/********************************************************************
* FUNCTION rpc_err_get_errtag_enum
*
* Get the RPC error-tag enum for an error-tag string
*
* INPUTS:
*   errtag == error-tag string to check
*
* RETURNS:
*   enum for this error-tag
*********************************************************************/
rpc_err_t
    rpc_err_get_errtag_enum (ncx_instance_t *instance, const xmlChar *errtag)
{
    rpc_err_t   errcode;

#ifdef DEBUG
    if (!errtag) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return RPC_ERR_NONE;
    }
#endif

    for (errcode = RPC_ERR_IN_USE;
         errcode <= RPC_ERR_LAST_ERROR;
         errcode++) {

        if (!xml_strcmp(instance, errtag, rpc_err_map[errcode].errtag)) {
            return rpc_err_map[errcode].errid;
        }
    }
    return RPC_ERR_NONE;

} /* rpc_err_get_errtag_enum */


/********************************************************************
* FUNCTION rpc_err_new_record
*
* Malloc and init an rpc_err_rec_t struct
*
* RETURNS:
*   malloced error record or NULL if memory error
*********************************************************************/
rpc_err_rec_t *
    rpc_err_new_record (ncx_instance_t *instance)
{
    // ATTN: This is odd - I think the idea is to have a way to record an error struct even when out of memory.
    rpc_err_rec_t *err = m__getObj(instance, rpc_err_rec_t);
    if (!err) {
        if (!instance->staterr_inuse) {
            err = instance->staterr;
            instance->staterr_inuse = TRUE;
        } else {
            return NULL;
        }
    }
    rpc_err_init_record(instance, err);
    return err;

} /* rpc_err_new_record */


/********************************************************************
* FUNCTION rpc_err_init_record
*
* Init an rpc_err_rec_t struct
*
* INPUTS:
*   err == rpc_err_rec_t struct to init
* RETURNS:
*   none
*********************************************************************/
void
    rpc_err_init_record (ncx_instance_t *instance, rpc_err_rec_t *err)
{
#ifdef DEBUG
    if (!err) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    memset(err, 0x0, sizeof(rpc_err_rec_t));
    dlq_createSQue(instance, &err->error_info);

} /* rpc_err_init_record */


/********************************************************************
* FUNCTION rpc_err_free_record
*
* Clean and free an rpc_err_rec_t struct
*
* INPUTS:
*   err == rpc_err_rec_t struct to clean and free
* RETURNS:
*   none
*********************************************************************/
void
    rpc_err_free_record (ncx_instance_t *instance, rpc_err_rec_t *err)
{
#ifdef DEBUG
    if (!err) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    rpc_err_clean_record(instance, err);
    
    if (err == instance->staterr) {
        instance->staterr_inuse = FALSE;
    } else {
        m__free(instance, err);
    }

} /* rpc_err_free_record */


/********************************************************************
* FUNCTION rpc_err_clean_record
*
* Clean an rpc_err_rec_t struct
*
* INPUTS:
*   err == rpc_err_rec_t struct to clean
* RETURNS:
*   none
*********************************************************************/
void
    rpc_err_clean_record (ncx_instance_t *instance, rpc_err_rec_t  *err)
{
    rpc_err_info_t  *errinfo;

#ifdef DEBUG
    if (!err) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    err->error_res = NO_ERR;
    err->error_id = RPC_ERR_NONE;
    err->error_type = NCX_LAYER_NONE;
    err->error_severity = RPC_ERR_SEV_NONE;
    err->error_tag = NULL;
    err->error_app_tag = NULL;
    if (err->error_path) {
        m__free(instance, err->error_path);
        err->error_path = NULL;
    }
    if (err->error_message) {
        m__free(instance, err->error_message);
    }
    err->error_message_lang = NULL;

    while (!dlq_empty(instance, &err->error_info)) {
        errinfo = (rpc_err_info_t *)dlq_deque(instance, &err->error_info);
        rpc_err_free_info(instance, errinfo);
    }

}  /* rpc_err_clean_record */


/********************************************************************
* FUNCTION rpc_err_new_info
*
* Malloc and init an rpc_err_info_t struct
*
* RETURNS:
*   malloced error-info record, or NULL if memory error
*********************************************************************/
rpc_err_info_t *
    rpc_err_new_info (ncx_instance_t *instance)
{
    rpc_err_info_t *errinfo;

    errinfo = m__getObj(instance, rpc_err_info_t);
    if (!errinfo) {
        return NULL;
    }
    memset(errinfo, 0x0, sizeof(rpc_err_info_t));
    return errinfo;

} /* rpc_err_new_info */


/********************************************************************
* FUNCTION rpc_err_free_info
*
* Clean and free an rpc_err_info_t struct
*
* INPUTS:
*   errinfo == rpc_err_info_t struct to clean and free
* RETURNS:
*   none
*********************************************************************/
void
    rpc_err_free_info (ncx_instance_t *instance, rpc_err_info_t *errinfo)
{
#ifdef DEBUG
    if (!errinfo) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    if (errinfo->badns) {
        m__free(instance, errinfo->badns);
    }
    if (errinfo->dname) {
        m__free(instance, errinfo->dname);
    }
    if (errinfo->dval) {
        m__free(instance, errinfo->dval);
    }
    switch (errinfo->val_btype) {
    case NCX_BT_ANY:
    case NCX_BT_CONTAINER:
    case NCX_BT_CHOICE:
    case NCX_BT_LIST:
        if (errinfo->v.cpxval) {
            val_free_value(instance, (val_value_t *)errinfo->v.cpxval);
        }
        break;
    default:
        ;
    }
    m__free(instance, errinfo);

} /* rpc_err_free_info */
    

/********************************************************************
* FUNCTION rpc_err_dump_errors
*
* Dump the error messages in the RPC message error Q
*
* INPUTS:
*   msg == rpc_msg_t struct to check for errors
*
*********************************************************************/
void
    rpc_err_dump_errors (ncx_instance_t *instance, const rpc_msg_t  *msg)
{
    const rpc_err_rec_t  *err;

#ifdef DEBUG
    if (!msg) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    for (err = (const rpc_err_rec_t *)dlq_firstEntry(instance, &msg->mhdr.errQ);
         err != NULL;
         err = (const rpc_err_rec_t *)dlq_nextEntry(instance, err)) {
        dump_error(instance, err);
    }

}  /* rpc_err_dump_errors */
    

/********************************************************************
* FUNCTION rpc_err_get_severity
*
* Translate an rpc_err_sev_t to a string 
*
* INPUTS:
*   sev == rpc_err_sev_t enum to translate
*
* RETURNS:
* const pointer to the enum string
*********************************************************************/
const xmlChar *
    rpc_err_get_severity (ncx_instance_t *instance, rpc_err_sev_t  sev)
{
    switch (sev) {
    case RPC_ERR_SEV_WARNING:
        return NCX_EL_WARNING;
    case RPC_ERR_SEV_ERROR:
        return NCX_EL_ERROR;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return EMPTY_STRING;
    }
}  /* rpc_err_get_severity */


/********************************************************************
* FUNCTION rpc_err_clean_errQ
*
* Clean all the entries from a Q of rpc_err_rec_t
*
* INPUTS:
*   errQ == Q of rpc_err_rec_t to clean
* RETURNS:
*   none
*********************************************************************/
void 
    rpc_err_clean_errQ (ncx_instance_t *instance, dlq_hdr_t *errQ)
{
    rpc_err_rec_t *err;

#ifdef DEBUG
    if (!errQ) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    /* clean error queue */
    while (!dlq_empty(instance, errQ)) {
        err = (rpc_err_rec_t *)dlq_deque(instance, errQ);
        rpc_err_free_record(instance, err);
    }

} /* rpc_err_clean_errQ */


/********************************************************************
* FUNCTION rpc_err_any_errors
*
* Check if there are any errors in the RPC message error Q
*
* INPUTS:
*   msg == rpc_msg_t struct to check for errors
*
* RETURNS:
*   TRUE if any errors recorded; FALSE if none
*********************************************************************/
boolean
    rpc_err_any_errors (ncx_instance_t *instance, const rpc_msg_t  *msg)
{

#ifdef DEBUG
    if (!msg) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return FALSE;
    }
#endif

    return (dlq_empty(instance, &msg->mhdr.errQ)) ? FALSE : TRUE;

}  /* rpc_err_any_errors */


/* END file rpc_err.c */
