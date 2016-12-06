/**
 * @file rwdts_xpath_ncx.c
 * @author Philip Joseph <philip.joseph@riftio.com>
 * @date 2015/09/08
 * @brief DTS XPath implementation using NCX library
 */


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
/*
    Xpath 1.0 support

    *********************************************************************
    *                                                                   *
    *                  C H A N G E   H I S T O R Y                      *
    *                                                                   *
    *********************************************************************

    date         init     comment
    ----------------------------------------------------------------------
    13nov08      abb      begun
    08sep15      pj       modified for Riftware DTS

    *********************************************************************
    *                                                                   *
    *                     I N C L U D E    F I L E S                    *
    *                                                                   *
    *********************************************************************/

#include  <stdio.h>
#include  <stdlib.h>
#include  <memory.h>
#include  <assert.h>

#include <libxml2/libxml/xmlstring.h>

#include <yuma/platform/procdefs.h>
#include <yuma/ncx/def_reg.h>
#include <yuma/ncx/dlq.h>
#include <yuma/ncx/grp.h>
#include <yuma/ncx/ncxconst.h>
#include <yuma/ncx/ncx.h>
#include <yuma/ncx/ncx_feature.h>
#include <yuma/ncx/ncx_num.h>
#include <yuma/ncx/obj.h>
#include <yuma/ncx/tk.h>
#include <yuma/ncx/typ.h>
#include <yuma/ncx/xpath.h>
#include <yuma/ncx/xpath1.h>
#include <yuma/ncx/yangconst.h>

#include <rwdts_xpath.h>

/********************************************************************
 *                                                                   *
 *                       C O N S T A N T S                           *
 *                                                                   *
 *********************************************************************/

/* #define XPATH1_PARSE_DEBUG 1 */
/* #define EXTRA_DEBUG 1 */

#define TEMP_BUFFSIZE  1024

/********************************************************************
 *                                                                   *
 *           F O R W A R D   D E C L A R A T I O N S                 *
 *                                                                   *
 *********************************************************************/
rw_status_t
rwdts_ncx_status (status_t status);

static xpath_result_t*
parse_expr(rwdts_xpath_pcb_t *rwpcb,
           xpath_pcb_t *pcb,
           status_t  *res);

static xpath_result_t*
boolean_fn(struct ncx_instance_t_ *instance,
           xpath_pcb_t *pcb,
           dlq_hdr_t *parmQ,
           status_t *res );

static xpath_result_t*
ceiling_fn(struct ncx_instance_t_ *instance,
           xpath_pcb_t *pcb,
           dlq_hdr_t *parmQ,
           status_t *res);

static xpath_result_t*
concat_fn(struct ncx_instance_t_ *instance,
          xpath_pcb_t *pcb,
          dlq_hdr_t *parmQ,
          status_t *res);

static xpath_result_t*
contains_fn(struct ncx_instance_t_ *instance,
            xpath_pcb_t *pcb,
            dlq_hdr_t *parmQ,
            status_t *res);

static xpath_result_t*
count_fn(struct ncx_instance_t_ *instance,
         xpath_pcb_t *pcb,
         dlq_hdr_t *parmQ,
         status_t *res);

static xpath_result_t*
current_fn(struct ncx_instance_t_ *instance,
           xpath_pcb_t *pcb,
           dlq_hdr_t *parmQ,
           status_t *res);

static xpath_result_t*
false_fn(struct ncx_instance_t_ *instance,
         xpath_pcb_t *pcb,
         dlq_hdr_t *parmQ,
         status_t *res);

static xpath_result_t*
floor_fn(struct ncx_instance_t_ *instance,
         xpath_pcb_t *pcb,
         dlq_hdr_t *parmQ,
         status_t *res);

static xpath_result_t*
id_fn(struct ncx_instance_t_ *instance,
      xpath_pcb_t *pcb,
      dlq_hdr_t *parmQ,
      status_t *res);

static xpath_result_t*
lang_fn(struct ncx_instance_t_ *instance,
        xpath_pcb_t *pcb,
        dlq_hdr_t *parmQ,
        status_t *res);

static xpath_result_t*
last_fn(struct ncx_instance_t_ *instance,
        xpath_pcb_t *pcb,
        dlq_hdr_t *parmQ,
        status_t *res);

static xpath_result_t*
local_name_fn(struct ncx_instance_t_ *instance,
              xpath_pcb_t *pcb,
              dlq_hdr_t *parmQ,
              status_t *res);

static xpath_result_t*
namespace_uri_fn(struct ncx_instance_t_ *instance,
                 xpath_pcb_t *pcb,
                 dlq_hdr_t *parmQ,
                 status_t *res);

static xpath_result_t*
name_fn(struct ncx_instance_t_ *instance,
        xpath_pcb_t *pcb,
        dlq_hdr_t *parmQ,
        status_t *res);

static xpath_result_t*
normalize_space_fn(struct ncx_instance_t_ *instance,
                   xpath_pcb_t *pcb,
                   dlq_hdr_t *parmQ,
                   status_t *res);

static xpath_result_t*
not_fn(struct ncx_instance_t_ *instance,
       xpath_pcb_t *pcb,
       dlq_hdr_t *parmQ,
       status_t *res);

static xpath_result_t*
number_fn(struct ncx_instance_t_ *instance,
          xpath_pcb_t *pcb,
          dlq_hdr_t *parmQ,
          status_t *res);

static xpath_result_t*
position_fn(struct ncx_instance_t_ *instance,
            xpath_pcb_t *pcb,
            dlq_hdr_t *parmQ,
            status_t *res);

static xpath_result_t*
round_fn(struct ncx_instance_t_ *instance,
         xpath_pcb_t *pcb,
         dlq_hdr_t *parmQ,
         status_t *res);

static xpath_result_t*
starts_with_fn(struct ncx_instance_t_ *instance,
               xpath_pcb_t *pcb,
               dlq_hdr_t *parmQ,
               status_t *res);

static xpath_result_t*
string_fn(struct ncx_instance_t_ *instance,
          xpath_pcb_t *pcb,
          dlq_hdr_t *parmQ,
          status_t *res);

static xpath_result_t*
string_length_fn(struct ncx_instance_t_ *instance,
                 xpath_pcb_t *pcb,
                 dlq_hdr_t *parmQ,
                 status_t *res);

static xpath_result_t*
substring_fn(struct ncx_instance_t_ *instance,
             xpath_pcb_t *pcb,
             dlq_hdr_t *parmQ,
             status_t *res);

static xpath_result_t*
substring_after_fn(struct ncx_instance_t_ *instance,
                   xpath_pcb_t *pcb,
                   dlq_hdr_t *parmQ,
                   status_t *res);

static xpath_result_t*
substring_before_fn(struct ncx_instance_t_ *instance,
                    xpath_pcb_t *pcb,
                    dlq_hdr_t *parmQ,
                    status_t *res);

static xpath_result_t*
sum_fn(struct ncx_instance_t_ *instance,
       xpath_pcb_t *pcb,
       dlq_hdr_t *parmQ,
       status_t *res);

static xpath_result_t*
translate_fn(struct ncx_instance_t_ *instance,
             xpath_pcb_t *pcb,
             dlq_hdr_t *parmQ,
             status_t *res);

static xpath_result_t*
true_fn(struct ncx_instance_t_ *instance,
        xpath_pcb_t *pcb,
        dlq_hdr_t *parmQ,
        status_t *res);

static xpath_result_t*
module_loaded_fn(struct ncx_instance_t_ *instance,
                 xpath_pcb_t *pcb,
                 dlq_hdr_t *parmQ,
                 status_t *res);

static xpath_result_t*
feature_enabled_fn(struct ncx_instance_t_ *instance,
                   xpath_pcb_t *pcb,
                   dlq_hdr_t *parmQ,
                   status_t *res);

rw_status_t
rwdts_ncx_xpath1_parse_expr(rwdts_xpath_pcb_t *rwpcb,
                            const xmlChar *xpath);

static xpath_result_t*
rift_min_fn(struct ncx_instance_t_ *instance,
            xpath_pcb_t *pcb,
            dlq_hdr_t *parmQ,
            status_t *res);

static xpath_result_t*
rift_max_fn(struct ncx_instance_t_ *instance,
            xpath_pcb_t *pcb,
            dlq_hdr_t *parmQ,
            status_t *res);

static xpath_result_t*
rift_avg_fn(struct ncx_instance_t_ *instance,
            xpath_pcb_t *pcb,
            dlq_hdr_t *parmQ,
            status_t *res);

/********************************************************************
 *                                                                   *
 *                         V A R I A B L E S                         *
 *                                                                   *
 *********************************************************************/

#define XP_FN_RIFT_MIN                  (const xmlChar *)"rift-min"
#define XP_FN_RIFT_MAX                  (const xmlChar *)"rift-max"
#define XP_FN_RIFT_AVG                  (const xmlChar *)"rift-avg"

static const xpath_fncb_t functions [] = {
    { XP_FN_BOOLEAN, XP_RT_BOOLEAN, 1, boolean_fn },
    { XP_FN_CEILING, XP_RT_NUMBER, 1, ceiling_fn },
    { XP_FN_CONCAT, XP_RT_STRING, -1, concat_fn },
    { XP_FN_CONTAINS, XP_RT_BOOLEAN, 2, contains_fn },
    { XP_FN_COUNT, XP_RT_NUMBER, 1, count_fn },
    { XP_FN_CURRENT, XP_RT_NODESET, 0, current_fn },
    { XP_FN_FALSE, XP_RT_BOOLEAN, 0, false_fn },
    { XP_FN_FLOOR, XP_RT_NUMBER, 1, floor_fn },
    { XP_FN_ID, XP_RT_NODESET, 1, id_fn },
    { XP_FN_LANG, XP_RT_BOOLEAN, 1, lang_fn },
    { XP_FN_LAST, XP_RT_NUMBER, 0, last_fn },
    { XP_FN_LOCAL_NAME, XP_RT_STRING, -1, local_name_fn },
    { XP_FN_NAME, XP_RT_STRING, -1, name_fn },
    { XP_FN_NAMESPACE_URI, XP_RT_STRING, -1, namespace_uri_fn },
    { XP_FN_NORMALIZE_SPACE, XP_RT_STRING, -1, normalize_space_fn },
    { XP_FN_NOT, XP_RT_BOOLEAN, 1, not_fn },
    { XP_FN_NUMBER, XP_RT_NUMBER, -1, number_fn },
    { XP_FN_POSITION, XP_RT_NUMBER, 0, position_fn },
    { XP_FN_ROUND, XP_RT_NUMBER, 1, round_fn },
    { XP_FN_STARTS_WITH, XP_RT_BOOLEAN, 2, starts_with_fn },
    { XP_FN_STRING, XP_RT_STRING, -1, string_fn },
    { XP_FN_STRING_LENGTH, XP_RT_NUMBER, -1, string_length_fn },
    { XP_FN_SUBSTRING, XP_RT_STRING, -1, substring_fn },
    { XP_FN_SUBSTRING_AFTER, XP_RT_STRING, 2, substring_after_fn },
    { XP_FN_SUBSTRING_BEFORE, XP_RT_STRING, 2, substring_before_fn },
    { XP_FN_SUM, XP_RT_NUMBER, 1, sum_fn },
    { XP_FN_TRANSLATE, XP_RT_STRING, 3, translate_fn },
    { XP_FN_TRUE, XP_RT_BOOLEAN, 0, true_fn },
    { XP_FN_MODULE_LOADED, XP_RT_BOOLEAN, -1, module_loaded_fn },
    { XP_FN_FEATURE_ENABLED, XP_RT_BOOLEAN, 2, feature_enabled_fn },
    { XP_FN_RIFT_MIN, XP_RT_NUMBER, 1, rift_min_fn },
    { XP_FN_RIFT_MAX, XP_RT_NUMBER, 1, rift_max_fn },
    { XP_FN_RIFT_AVG, XP_RT_NUMBER, 1, rift_avg_fn },
    { NULL, XP_RT_NONE, 0, NULL }   /* last entry marker */
};

static const rwdts_xpath_action_t rwdts_xpath_map_op_ncx [] = {
  RWDTS_XPATH_ACTION_NONE,
  RWDTS_XPATH_ACTION_EXOP_AND,         /* keyword 'and' */
  RWDTS_XPATH_ACTION_EXOP_OR,          /* keyword 'or' */
  RWDTS_XPATH_ACTION_EXOP_EQUAL,       /* equals '=' */
  RWDTS_XPATH_ACTION_EXOP_NOTEQUAL,    /* bang equals '!=' */
  RWDTS_XPATH_ACTION_EXOP_LT,          /* left angle bracket '<' */
  RWDTS_XPATH_ACTION_EXOP_GT,          /* right angle bracket '>' */
  RWDTS_XPATH_ACTION_EXOP_LEQUAL,      /* l. angle-equals '<= */
  RWDTS_XPATH_ACTION_EXOP_GEQUAL,      /* r. angle-equals '>=' */
  RWDTS_XPATH_ACTION_EXOP_ADD,         /* plus sign '+' */
  RWDTS_XPATH_ACTION_EXOP_SUBTRACT,    /* minus '-' */
  RWDTS_XPATH_ACTION_EXOP_MULTIPLY,    /* asterisk '*' */
  RWDTS_XPATH_ACTION_EXOP_DIV,         /* keyword 'div' */
  RWDTS_XPATH_ACTION_EXOP_MOD,         /* keyword 'mod' */
  RWDTS_XPATH_ACTION_EXOP_NEGATE,      /* unary '-' */
  RWDTS_XPATH_ACTION_EXOP_UNION,       /* vert. bar '|' */
  RWDTS_XPATH_ACTION_EXOP_FILTER1,     /* fwd slash '/' */
  RWDTS_XPATH_ACTION_EXOP_FILTER2      /* double fwd slash (C++ comment) */
};

static rwdts_xpath_action_t
rwdts_xpath_map_oper_ncx(xpath_exop_t op)
{
    RW_ASSERT(op >= XP_EXOP_NONE);
    RW_ASSERT(op <= XP_EXOP_FILTER2);
    return rwdts_xpath_map_op_ncx[op];
}

static char*
rwdts_xpath_get_path_ncx (rwdts_xpath_pcb_t *rwpcb,
                          xpath_result_t *result)
{
    xpath_resnode_t       *resnode;
    val_value_t           *val;
    const obj_template_t  *obj;
    ncx_instance_t        *instance = (ncx_instance_t*)rwpcb->lib_inst;
    char *str = NULL;

    if (result == NULL) {
        return str;
    }
    switch (result->restype) {
        case XP_RT_NODESET:
            {
                const size_t len = RWDTS_XPATH_PATH_MAX_SIZE;
                str = RW_MALLOC0(len+1);
                if (str == NULL) {
                    return str;
                }
                size_t pos = 0;
                char *buf = NULL;
                for (resnode = (xpath_resnode_t *)
                               dlq_firstEntry(instance, &result->r.nodeQ);
                     resnode != NULL;
                     resnode = (xpath_resnode_t *)dlq_nextEntry(instance, resnode)) {
                    pos = strlen(str);
                    buf = &str[pos];
                    if (result->isval) {
                        val = resnode->node.valptr;
                        if (val) {
                            snprintf(buf, len-pos,
                                     "%s:%s",
                                     xmlns_get_ns_prefix(instance, val->nsid),
                                     val->name);
                        }
                    } else {
                        obj = resnode->node.objptr;
                        if (obj) {
                            snprintf(buf, len-pos,
                                     "%s:%s",
                                     obj_get_mod_prefix(obj),
                                     obj_get_name(instance, obj));
                        }
                    }
                }
            }
            break;
        case XP_RT_NONE:
        case XP_RT_NUMBER:
        case XP_RT_STRING:
        case XP_RT_BOOLEAN:
        default:
            break;
    }
    return str;
}

/********************************************************************
 * FUNCTION set_uint32_num
 *
 * Set an ncx_num_t with a uint32 number
 *
 * INPUTS:
 *    innum == number value to use
 *    outnum == address ofoutput number
 *
 * OUTPUTS:
 *   *outnum is set with the converted value
 *
 *********************************************************************/
static void
set_uint32_num (uint32 innum,
                ncx_num_t  *outnum)
{
#ifdef HAS_FLOAT
    outnum->d = (double)innum;
#else
    outnum->d = (int64)innum;
#endif

}  /* set_uint32_num */


/********************************************************************
 * FUNCTION malloc_failed_error
 *
 * Generate a malloc failed error if OK
 *
 * INPUTS:
 *    pcb == parser control block to use
 *********************************************************************/
static void
malloc_failed_error (ncx_instance_t *instance, xpath_pcb_t *pcb)
{
    if (pcb->logerrors) {
        if (pcb->exprstr) {
            log_error(instance,
                      "\nError: malloc failed in "
                      "Xpath expression '%s'.",
                      pcb->exprstr);
        } else {
            log_error(instance, "\nError: malloc failed in "
                      "Xpath expression");
        }
        ncx_print_errormsg(instance,
                           pcb->tkc,
                           pcb->tkerr.mod,
                           ERR_INTERNAL_MEM);
    }

}  /* malloc_failed_error */


/********************************************************************
 * FUNCTION no_parent_warning
 *
 * Generate a no parent available error if OK
 *
 * INPUTS:
 *    pcb == parser control block to use
 *********************************************************************/
static void
no_parent_warning (ncx_instance_t *instance, xpath_pcb_t *pcb)
{
    if (pcb->logerrors &&
        ncx_warning_enabled(instance, ERR_NCX_NO_XPATH_PARENT)) {
        log_warn(instance, "\nWarning: no parent found "
                 "in XPath expr '%s'", pcb->exprstr);
        ncx_print_errormsg(instance,
                           pcb->tkc,
                           pcb->objmod,
                           ERR_NCX_NO_XPATH_PARENT);
    } else if (pcb->objmod != NULL) {
        ncx_inc_warnings(pcb->objmod);
    }

}  /* no_parent_warning */


/********************************************************************
 * FUNCTION unexpected_error
 *
 * Generate an unexpected token error if OK
 *
 * INPUTS:
 *    pcb == parser control block to use
 *********************************************************************/
static void
unexpected_error (ncx_instance_t *instance, xpath_pcb_t *pcb)
{
    if (pcb->logerrors) {
        if (TK_CUR(pcb->tkc)) {
            log_error(instance,
                      "\nError: Unexpected token '%s' in "
                      "XPath expression '%s'",
                      tk_get_token_name(TK_CUR_TYP(pcb->tkc)),
                      pcb->exprstr);
        } else {
            log_error(instance,  "\nError: End reached in "
                      "XPath expression '%s'",  pcb->exprstr);
        }
        ncx_print_errormsg(instance,
                           pcb->tkc,
                           pcb->tkerr.mod,
                           ERR_NCX_WRONG_TKTYPE);
    }

}  /* unexpected_error */


/********************************************************************
 * FUNCTION wrong_parmcnt_error
 *
 * Generate a wrong function parameter count error if OK
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmcnt == the number of parameters received
 *    res == error result to use
 *********************************************************************/
static void
wrong_parmcnt_error (ncx_instance_t *instance,
                     xpath_pcb_t *pcb,
                     uint32 parmcnt,
                     status_t res)
{
    if (pcb->logerrors) {
        log_error(instance,
                  "\nError: wrong function arg count '%u' in "
                  "Xpath expression '%s'",
                  parmcnt,
                  pcb->exprstr);
        ncx_print_errormsg(instance,
                           pcb->tkc,
                           pcb->tkerr.mod,
                           res);
    }

}  /* wrong_parmcnt_error */


/********************************************************************
 * FUNCTION invalid_instanceid_error
 *
 * Generate a invalid instance identifier error
 *
 * INPUTS:
 *    pcb == parser control block to use
 *********************************************************************/
static void
invalid_instanceid_error (ncx_instance_t *instance, xpath_pcb_t *pcb)
{
    if (pcb->logerrors) {
        log_error(instance,
                  "\nError: XPath found in instance-identifier '%s'",
                  pcb->exprstr);
        ncx_print_errormsg(instance,
                           pcb->tkc,
                           pcb->tkerr.mod,
                           ERR_NCX_INVALID_INSTANCEID);
    }

}  /* invalid_instanceid_error */


/********************************************************************
 * FUNCTION check_instanceid_expr
 *
 * Check the form of an instance-identifier expression
 * The operation has been checked already and the
 * expression is:
 *
 *       leftval = rightval
 *
 * Make sure the CLRs in the YANG spec are followed
 * DOES NOT check the actual result value, just
 * tries to keep track of the expression syntax
 *
 * Use check_instanceid_result to finish the
 * instance-identifier tests
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    leftval == LHS result
 *    rightval == RHS result
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
check_instanceid_expr (ncx_instance_t *instance,
                       xpath_pcb_t *pcb,
                       xpath_result_t *leftval,
                       xpath_result_t *rightval)
{
    val_value_t           *contextval;
    const char            *msg;
    status_t               res;
    boolean                haserror;

    /* skip unless this is a real value tree eval */
    if (!pcb->val || !pcb->val_docroot) {
        return NO_ERR;
    }

    haserror = FALSE;
    res = ERR_NCX_INVALID_INSTANCEID;
    msg = NULL;

    /* check LHS which is supposed to be a nodeset.
     * it can only be an empty nodeset if the
     * instance-identifier is constrained to
     * existing values by 'require-instance true'
     */
    if (leftval) {
        if (leftval->restype != XP_RT_NODESET) {
            msg = "LHS has wrong data type";
            haserror = TRUE;
        } else {
            /* check out the nodeset contents */
            contextval = pcb->context.node.valptr;
            if (!contextval) {
                return SET_ERROR(instance, ERR_INTERNAL_VAL);
            }

            if (contextval->obj->objtype == OBJ_TYP_LIST ||
                contextval->obj->objtype == OBJ_TYP_LEAF_LIST) {
                /* the '.=result' must be the format, and be
                 * the same as the context for a leaf-list
                 * For a list, the name='fred' expr is used,
                 *   'name' must be a key leaf
                 *    of the context node
                 * check the actual result later
                 */
                ;
            } else {
                msg = "wrong context node type";
                haserror = TRUE;
            }
        }
    } else {
        haserror = TRUE;
        msg = "missing LHS value in predicate";
    }

    if (!haserror) {
        if (rightval) {
            if (!(rightval->restype == XP_RT_STRING ||
                  rightval->restype == XP_RT_NUMBER)) {
                msg = "RHS has wrong data type";
                haserror = TRUE;
            }
        } else {
            haserror = TRUE;
            msg = "missing RHS value in predicate";
        }
    }

    if (haserror) {
        if (pcb->logerrors) {
            log_error(instance,
                      "\nError: %s in instance-identifier '%s'",
                      msg, pcb->exprstr);
            ncx_print_errormsg(instance,
                               pcb->tkc,
                               pcb->tkerr.mod,
                               res);
        }
        return res;
    } else {
        return NO_ERR;
    }

}  /* check_instanceid_expr */


/********************************************************************
 * FUNCTION new_result
 *
 * Get a new result from the cache or malloc if none available
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    restype == desired result type
 *
 * RETURNS:
 *    result from the cache or malloced; NULL if malloc fails
 *********************************************************************/
static xpath_result_t *
new_result (ncx_instance_t *instance,
            xpath_pcb_t *pcb,
            xpath_restype_t restype)
{
    xpath_result_t  *result;

    result = (xpath_result_t *)dlq_deque(instance, &pcb->result_cacheQ);
    if (result) {
        pcb->result_count--;
        xpath_init_result(instance, result, restype);
    } else {
        result = xpath_new_result(instance, restype);
    }

    if (!result) {
        malloc_failed_error(instance, pcb);
    }

    return result;

} /* new_result */


/********************************************************************
 * FUNCTION free_result
 *
 * Free a result struct: put in cache or free if cache maxed out
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    result == result struct to free
 *
 *********************************************************************/
static void free_result (ncx_instance_t *instance, xpath_pcb_t *pcb, xpath_result_t *result)
{
    xpath_resnode_t *resnode;

    if (result->restype == XP_RT_NODESET) {
        while (!dlq_empty(instance, &result->r.nodeQ) &&
               pcb->resnode_count < XPATH_RESNODE_CACHE_MAX) {

            resnode = (xpath_resnode_t *)dlq_deque(instance, &result->r.nodeQ);
            xpath_clean_resnode(instance, resnode);
            dlq_enque(instance, resnode, &pcb->resnode_cacheQ);
            pcb->resnode_count++;
        }
    }

    if (pcb->result_count < XPATH_RESULT_CACHE_MAX) {
        xpath_clean_result(instance, result);
        dlq_enque(instance, result, &pcb->result_cacheQ);
        pcb->result_count++;
    } else {
        xpath_free_result(instance, result);
    }

} /* free_result */


/********************************************************************
 * FUNCTION new_obj_resnode
 *
 * Get a new result node from the cache or malloc if none available
 *
 * INPUTS:
 *    pcb == parser control block to use
 *           if pcb->val set then node.valptr will be used
 *           else node.objptr will be used instead
 *    position == position within the current search
 *    dblslash == TRUE if // present on this result node
 *                and has not been converted to separate
 *                nodes for all descendants instead
 *             == FALSE if '//' is not in effect for the
 *                current step
 *    objptr == object pointer value to use
 *
 * RETURNS:
 *    result from the cache or malloced; NULL if malloc fails
 *********************************************************************/
static xpath_resnode_t *
new_obj_resnode (ncx_instance_t *instance,
                 xpath_pcb_t *pcb,
                 int64 position,
                 boolean dblslash,
                 obj_template_t *objptr)
{
    xpath_resnode_t  *resnode;

    resnode = (xpath_resnode_t *)dlq_deque(instance, &pcb->resnode_cacheQ);
    if (resnode) {
        pcb->resnode_count--;
    } else {
        resnode = xpath_new_resnode(instance);
    }

    if (!resnode) {
        malloc_failed_error(instance, pcb);
    } else  {
        resnode->position = position;
        resnode->dblslash = dblslash;
        resnode->node.objptr = objptr;
    }

    return resnode;

} /* new_obj_resnode */


/********************************************************************
 * FUNCTION new_val_resnode
 *
 * Get a new result node from the cache or malloc if none available
 *
 * INPUTS:
 *    pcb == parser control block to use
 *           if pcb->val set then node.valptr will be used
 *           else node.objptr will be used instead
 *    position == position within the search context
 *    dblslash == TRUE if // present on this result node
 *                and has not been converted to separate
 *                nodes for all descendants instead
 *             == FALSE if '//' is not in effect for the
 *                current step
 *    valptr == variable pointer value to use
 *
 * RETURNS:
 *    result from the cache or malloced; NULL if malloc fails
 *********************************************************************/
static xpath_resnode_t *
new_val_resnode (ncx_instance_t *instance,
                 xpath_pcb_t *pcb,
                 int64 position,
                 boolean dblslash,
                 val_value_t *valptr)
{
    xpath_resnode_t  *resnode;

    resnode = (xpath_resnode_t *)dlq_deque(instance, &pcb->resnode_cacheQ);
    if (resnode) {
        pcb->resnode_count--;
    } else {
        resnode = xpath_new_resnode(instance);
    }

    if (!resnode) {
        malloc_failed_error(instance, pcb);
    } else {
        resnode->position = position;
        resnode->dblslash = dblslash;
        resnode->node.valptr = valptr;
    }

    return resnode;

} /* new_val_resnode */


/********************************************************************
 * FUNCTION free_resnode
 *
 * Free a result node struct: put in cache or free if cache maxed out
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    resnode == result node struct to free
 *
 *********************************************************************/
static void
free_resnode (ncx_instance_t *instance,
              xpath_pcb_t *pcb,
              xpath_resnode_t *resnode)
{
    if (pcb->resnode_count < XPATH_RESNODE_CACHE_MAX) {
        xpath_clean_resnode(instance, resnode);
        dlq_enque(instance, resnode, &pcb->resnode_cacheQ);
        pcb->resnode_count++;
    } else {
        xpath_free_resnode(instance, resnode);
    }

} /* free_resnode */


/********************************************************************
 * FUNCTION new_nodeset
 *
 * Start a nodeset result with a specified object or value ptr
 * Only one of obj or val will really be stored,
 * depending on the current parsing mode
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    obj == object ptr to store as the first resnode
 *    val == value ptr to store as the first resnode
 *    dblslash == TRUE if unprocessed dblslash in effect
 *                FALSE if not
 *
 * RETURNS:
 *    malloced data structure or NULL if out-of-memory error
 *********************************************************************/
static xpath_result_t *
new_nodeset (ncx_instance_t *instance,
             xpath_pcb_t *pcb,
             obj_template_t *obj,
             val_value_t *val,
             int64 position,
             boolean dblslash)
{
    xpath_result_t  *result;
    xpath_resnode_t *resnode;

    result = new_result(instance, pcb, XP_RT_NODESET);
    if (!result) {
        return NULL;
    }

    if (obj || val) {
        if (pcb->val) {
            resnode = new_val_resnode(instance,
                                      pcb,
                                      position,
                                      dblslash,
                                      val);
            result->isval = TRUE;
        } else {
            resnode = new_obj_resnode(instance,
                                      pcb,
                                      position,
                                      dblslash,
                                      obj);
            result->isval = FALSE;
        }
        if (!resnode) {
            xpath_free_result(instance, result);
            return NULL;
        }
        dlq_enque(instance, resnode, &result->r.nodeQ);
    }

    result->last = 1;

    return result;

} /* new_nodeset */


/********************************************************************
 * FUNCTION convert_compare_result
 *
 * Convert an int32 compare result to a boolean
 * based on the XPath relation op
 *
 * INPUTS:
 *    cmpresult == compare result
 *    exop == XPath relational or equality expression OP
 *
 * RETURNS:
 *    TRUE if relation is TRUE
 *    FALSE if relation is FALSE
 *********************************************************************/
static boolean
convert_compare_result (ncx_instance_t *instance,
                        int32 cmpresult,
                        xpath_exop_t exop)
{
    switch (exop) {
        case XP_EXOP_EQUAL:
            return (cmpresult) ? FALSE : TRUE;
        case XP_EXOP_NOTEQUAL:
            return (cmpresult) ? TRUE : FALSE;
        case XP_EXOP_LT:
            return (cmpresult < 0) ? TRUE : FALSE;
        case XP_EXOP_GT:
            return (cmpresult > 0) ? TRUE : FALSE;
        case XP_EXOP_LEQUAL:
            return (cmpresult <= 0) ? TRUE : FALSE;
        case XP_EXOP_GEQUAL:
            return (cmpresult >= 0) ? TRUE : FALSE;
        default:
            SET_ERROR(instance, ERR_INTERNAL_VAL);
            return TRUE;
    }
    /*NOTREACHED*/

} /* convert_compare_result */


/********************************************************************
 * FUNCTION compare_strings
 *
 * Compare str1 to str2
 * Use the specified operation
 *
 *    str1  <op>  str2
 *
 * INPUTS:
 *    str1 == left hand side string
 *    str2 == right hand side string
 *    exop == XPath expression op to use
 *
 * RETURNS:
 *    TRUE if relation is TRUE
 *    FALSE if relation is FALSE
 *********************************************************************/
static boolean
compare_strings (ncx_instance_t *instance,
                 const xmlChar *str1,
                 const xmlChar *str2,
                 xpath_exop_t  exop)
{
    int32  cmpresult;

    cmpresult = xml_strcmp(instance, str1, str2);
    return convert_compare_result(instance, cmpresult, exop);

} /* compare_strings */


/********************************************************************
 * FUNCTION compare_numbers
 *
 * Compare str1 to str2
 * Use the specified operation
 *
 *    num1  <op>  num2
 *
 * INPUTS:
 *    num1 == left hand side number
 *    numstr2 == right hand side string to convert to num2
 *               and then compare to num1
 *           == NULL or empty string to compare to NAN
 *    exop == XPath expression op to use
 *
 * RETURNS:
 *    TRUE if relation is TRUE
 *    FALSE if relation is FALSE
 *********************************************************************/
static boolean
compare_numbers (ncx_instance_t *instance,
                 const ncx_num_t *num1,
                 const xmlChar *numstr2,
                 xpath_exop_t  exop)
{
    int32        cmpresult;
    ncx_num_t    num2;
    status_t     res;
    ncx_numfmt_t numfmt;

    cmpresult = 0;
    numfmt = NCX_NF_DEC;
    res = NO_ERR;

    if (numstr2 && *numstr2) {
        numfmt = ncx_get_numfmt(instance, numstr2);
        if (numfmt == NCX_NF_OCTAL) {
            numfmt = NCX_NF_DEC;
        }
    }

    if (numfmt == NCX_NF_DEC || numfmt == NCX_NF_REAL) {
        ncx_init_num(instance, &num2);

        if (numstr2 && *numstr2) {
            res = ncx_convert_num(instance,
                                  numstr2,
                                  numfmt,
                                  NCX_BT_FLOAT64,
                                  &num2);
        } else {
            ncx_set_num_nan(instance, &num2, NCX_BT_FLOAT64);
        }
        if (res == NO_ERR) {
            cmpresult = ncx_compare_nums(instance,
                                         num1,
                                         &num2,
                                         NCX_BT_FLOAT64);
        }
        ncx_clean_num(instance, NCX_BT_FLOAT64, &num2);
    } else if (numfmt == NCX_NF_NONE) {
        res = ERR_NCX_INVALID_VALUE;
    } else {
        res = ERR_NCX_WRONG_NUMTYP;
    }

    if (res != NO_ERR) {
        return FALSE;
    }
    return convert_compare_result(instance, cmpresult, exop);

} /* compare_numbers */


/********************************************************************
 * FUNCTION compare_booleans
 *
 * Compare bool1 to bool2
 * Use the specified operation
 *
 *    bool1  <op>  bool2
 *
 * INPUTS:
 *    num1 == left hand side number
 *    str2 == right hand side string to convert to a number
 *            and then compare to num1
 *    exop == XPath expression op to use
 *
 * RETURNS:
 *    TRUE if relation is TRUE
 *    FALSE if relation is FALSE
 *********************************************************************/
static boolean
compare_booleans (ncx_instance_t *instance,
                  boolean bool1,
                  boolean bool2,
                  xpath_exop_t  exop)
{

    int32  cmpresult;

    if ((bool1 && bool2) || (!bool1 && !bool2)) {
        cmpresult = 0;
    } else if (bool1) {
        cmpresult = 1;
    } else {
        cmpresult = -1;
    }

    return convert_compare_result(instance, cmpresult, exop);

}  /* compare_booleans */


/********************************************************************
 * FUNCTION compare_walker_fn
 *
 * Compare the parm string to the current value
 * Stop the walk on the first TRUE comparison
 *
 *    val1 <op>  val2
 *
 *  val1 == parms.cmpstring or parms.cmpnum
 *  val2 == string value of node passed by val walker
 *    op == parms.exop
 *
 * Matches val_walker_fn_t template in val.h
 *
 * INPUTS:
 *    val == value node found in the search, used as 'val2'
 *    cookie1 == xpath_pcb_t * : parser control block to use
 *               currently not used!!!
 *    cookie2 == xpath_compwalkerparms_t *: walker parms to use
 *                cmpstring or cmpnum is used, not result2
 * OUTPUTS:
 *    *cookie2 contents adjusted  (parms.cmpresult and parms.res)
 *
 * RETURNS:
 *    TRUE to keep walk going
 *    FALSE to terminate walk
 *********************************************************************/
static boolean
compare_walker_fn (ncx_instance_t *instance,
                   val_value_t *val,
                   void *cookie1,
                   void *cookie2)
{
    val_value_t              *useval, *v_val;
    xpath_compwalkerparms_t  *parms;
    xmlChar                  *buffer;
    status_t                  res;
    uint32                    cnt;

    (void)cookie1;
    parms = (xpath_compwalkerparms_t *)cookie2;

    /* skip all complex nodes */
    if (!typ_is_simple(instance, val->btyp)) {
        return TRUE;
    }

    v_val = NULL;
    res = NO_ERR;

    if (val_is_virtual(instance, val)) {
        v_val = val_get_virtual_value(instance, NULL, val, &res);
        if (v_val == NULL) {
            parms->res = res;
            parms->cmpresult = FALSE;
            return FALSE;
        } else {
            useval = v_val;
        }
    } else {
        useval = val;
    }

    if (obj_is_password(instance, val->obj)) {
        parms->cmpresult = FALSE;
    } else if (typ_is_string(val->btyp)) {
        if (parms->cmpstring) {
            parms->cmpresult =
                    compare_strings(instance,
                                    parms->cmpstring,
                                    VAL_STR(useval),
                                    parms->exop);
        } else {
            parms->cmpresult =
                    compare_numbers(instance,
                                    parms->cmpnum,
                                    VAL_STR(useval),
                                    parms->exop);
        }
    } else {
        /* get value sprintf size */
        res = val_sprintf_simval_nc(instance, NULL, useval, &cnt);
        if (res != NO_ERR) {
            parms->res = res;
            return FALSE;
        }

        if (cnt < parms->buffsize) {
            /* use pre-allocated buffer */
            res = val_sprintf_simval_nc(instance, parms->buffer, useval, &cnt);
            if (res == NO_ERR) {
                if (parms->cmpstring) {
                    parms->cmpresult =
                            compare_strings(instance,
                                            parms->cmpstring,
                                            parms->buffer,
                                            parms->exop);
                } else {
                    parms->cmpresult =
                            compare_numbers(instance,
                                            parms->cmpnum,
                                            parms->buffer,
                                            parms->exop);
                }
            } else {
                parms->res = res;
                return FALSE;
            }
        } else {
            /* use a temp buffer */
            buffer = m__getMem(instance, cnt+1);
            if (!buffer) {
                parms->res = ERR_INTERNAL_MEM;
                return FALSE;
            }

            res = val_sprintf_simval_nc(instance, buffer, useval, &cnt);
            if (res != NO_ERR) {
                m__free(instance, buffer);
                parms->res = res;
                return FALSE;
            }

            if (parms->cmpstring) {
                parms->cmpresult =
                        compare_strings(instance,
                                        parms->cmpstring,
                                        buffer,
                                        parms->exop);
            } else {
                parms->cmpresult =
                        compare_numbers(instance,
                                        parms->cmpnum,
                                        buffer,
                                        parms->exop);
            }

            m__free(instance, buffer);
        }
    }

    return !parms->cmpresult;

}  /* compare_walker_fn */


/********************************************************************
 * FUNCTION top_compare_walker_fn
 *
 * Compare the current string in the 1st nodeset
 * to the entire 2nd node-set
 *
 * This callback should get called once for every
 * node in the first parmset.  Each time a simple
 * type is passed into the callback, the entire
 * result2 is processed with new callbacks to
 * compare the 'cmpstring' against each simple
 * type in the 2nd node-set.
 *
 * Stop the walk on the first TRUE comparison
 *
 * Matches val_walker_fn_t template in val.h
 *
 * INPUTS:
 *    val == value node found in the search
 *    cookie1 == xpath_pcb_t * : parser control block to use
 *               currently not used!!!
 *    cookie2 == xpath_compwalkerparms_t *: walker parms to use
 *               result2 is used, not cmpstring
 * OUTPUTS:
 *    *cookie2 contents adjusted  (parms.cmpresult and parms.res)
 *
 * RETURNS:
 *    TRUE to keep walk going
 *    FALSE to terminate walk
 *********************************************************************/
static boolean
top_compare_walker_fn (ncx_instance_t *instance,
                       val_value_t *val,
                       void *cookie1,
                       void *cookie2)
{
    xpath_compwalkerparms_t  *parms, newparms;
    xmlChar                  *buffer, *comparestr;
    xpath_pcb_t              *pcb;
    xpath_resnode_t          *resnode;
    val_value_t              *testval, *useval, *newval;
    status_t                  res;
    uint32                    cnt;
    boolean                   fnresult, cfgonly;

    pcb = (xpath_pcb_t *)cookie1;
    parms = (xpath_compwalkerparms_t *)cookie2;

    /* skip all complex nodes */
    if (!typ_is_simple(instance, val->btyp)) {
        return TRUE;
    }

    res = NO_ERR;
    buffer = NULL;
    newval = NULL;

    cfgonly = (pcb->flags & XP_FL_CONFIGONLY) ? TRUE : FALSE;

    if (val_is_virtual(instance, val)) {
        newval = val_get_virtual_value(instance, NULL, val, &res);
        if (newval == NULL) {
            parms->res = res;
            parms->cmpresult = FALSE;
            return FALSE;
        } else {
            useval = newval;
        }
    } else {
        useval = val;
    }

    if (typ_is_string(val->btyp)) {
        comparestr = VAL_STR(useval);
    } else {
        /* get value sprintf size */
        res = val_sprintf_simval_nc(instance, NULL, useval, &cnt);
        if (res != NO_ERR) {
            parms->res = res;
            return FALSE;
        }

        if (cnt < parms->buffsize) {
            /* use pre-allocated buffer */
            res = val_sprintf_simval_nc(instance, parms->buffer, useval, &cnt);
            if (res != NO_ERR) {
                parms->res = res;
                return FALSE;
            }
            comparestr = parms->buffer;
        } else {
            /* use a temp buffer */
            buffer = m__getMem(instance, cnt+1);
            if (!buffer) {
                parms->res = ERR_INTERNAL_MEM;
                return FALSE;
            }

            res = val_sprintf_simval_nc(instance, buffer, useval, &cnt);
            if (res != NO_ERR) {
                m__free(instance, buffer);
                parms->res = res;
                return FALSE;
            }
            comparestr = buffer;
        }
    }

    /* setup 2nd walker parms */
    memset(&newparms, 0x0, sizeof(xpath_compwalkerparms_t));
    newparms.cmpstring = comparestr;
    newparms.buffer = m__getMem(instance, TEMP_BUFFSIZE);
    if (!newparms.buffer) {
        if (buffer) {
            m__free(instance, buffer);
        }
        parms->res = ERR_INTERNAL_MEM;
        return FALSE;
    }
    newparms.buffsize = TEMP_BUFFSIZE;
    newparms.exop = parms->exop;
    newparms.cmpresult = FALSE;
    newparms.res = NO_ERR;

    /* go through all the nodes in the first node-set
     * and compare each leaf against all the leafs
     * in the other node-set; stop when condition
     * is met or both node-sets completely searched
     */
    for (resnode = (xpath_resnode_t *)
                   dlq_firstEntry(instance, &parms->result2->r.nodeQ);
         resnode != NULL && res == NO_ERR;
         resnode = (xpath_resnode_t *)dlq_nextEntry(instance, resnode)) {

        testval = resnode->node.valptr;

        fnresult =
                val_find_all_descendants(instance,
                                         compare_walker_fn,
                                         pcb,
                                         &newparms,
                                         testval,
                                         NULL,
                                         NULL,
                                         cfgonly,
                                         FALSE,
                                         TRUE,
                                         TRUE);
        if (newparms.res != NO_ERR) {
            res = newparms.res;
            parms->res = res;
        } else if (!fnresult) {
            /* condition was met if return FALSE and
             * walkerparms.res == NO_ERR
             */
            if (buffer) {
                m__free(instance, buffer);
            }
            m__free(instance, newparms.buffer);
            parms->res = NO_ERR;
            return FALSE;
        }
    }

    if (buffer != NULL) {
        m__free(instance, buffer);
    }

    m__free(instance, newparms.buffer);

    return TRUE;

}  /* top_compare_walker_fn */


/********************************************************************
 * FUNCTION compare_nodeset_to_other
 *
 * Compare 2 results, 1 of them is a node-set
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    val1 == first result struct to compare
 *    val2 == second result struct to compare
 *    exop == XPath expression operator to use
 *    res == address of resturn status
 *
 * OUTPUTS:
 *   *res == return status
 *
 * RETURNS:
 *    TRUE if relation is TRUE
 *    FALSE if relation is FALSE or some error
 *********************************************************************/
static boolean
compare_nodeset_to_other (ncx_instance_t *instance,
                          xpath_pcb_t *pcb,
                          xpath_result_t *val1,
                          xpath_result_t *val2,
                          xpath_exop_t exop,
                          status_t *res)
{
    xpath_resnode_t         *resnode;
    xpath_result_t          *tempval;
    val_value_t             *testval, *newval;
    xmlChar                 *cmpstring;
    ncx_num_t                cmpnum;
    int32                    cmpresult;
    boolean                  bool1, bool2, fnresult;
    status_t                 myres;

    *res = NO_ERR;
    myres = NO_ERR;

    /* only compare real results, not objects */
    if (!pcb->val) {
        return TRUE;
    }

    if (val2->restype == XP_RT_NODESET) {
        /* invert the exop; use val2 as val1 */
        switch (exop) {
            case XP_EXOP_EQUAL:
            case XP_EXOP_NOTEQUAL:
                break;
            case XP_EXOP_LT:
                exop = XP_EXOP_GT;
                break;
            case XP_EXOP_GT:
                exop = XP_EXOP_LT;
                break;
            case XP_EXOP_LEQUAL:
                exop = XP_EXOP_GEQUAL;
                break;
            case XP_EXOP_GEQUAL:
                exop = XP_EXOP_LEQUAL;
                break;
            default:
                *res = SET_ERROR(instance, ERR_INTERNAL_VAL);
                return TRUE;
        }

        /* swap the parameters so the parmset is on the LHS */
        tempval = val1;
        val1 = val2;
        val2 = tempval;
    }

    if (dlq_empty(instance, &val1->r.nodeQ)) {
        return FALSE;
    }

    if (val2->restype == XP_RT_BOOLEAN) {
        bool1 = xpath_cvt_boolean(instance, val1);
        bool2 = val2->r.boo;
        return compare_booleans(instance, bool1, bool2, exop);
    }

    /* compare the LHS node-set to the cmpstring or cmpnum
     * first match will end the loop
     */
    fnresult = FALSE;
    for (resnode = (xpath_resnode_t *)dlq_firstEntry(instance, &val1->r.nodeQ);
         resnode != NULL && !fnresult && *res == NO_ERR;
         resnode = (xpath_resnode_t *)dlq_nextEntry(instance, resnode)) {

        testval = resnode->node.valptr;
        newval = NULL;
        myres = NO_ERR;

        if (val_is_virtual(instance, testval)) {
            newval = val_get_virtual_value(instance, NULL, testval, &myres);
            if (newval == NULL) {
                *res = myres;
                continue;
            } else {
                testval = newval;
            }
        }

        switch (val2->restype) {
            case XP_RT_STRING:
                cmpstring = NULL;
                *res = xpath1_stringify_node(instance,
                                             pcb,
                                             testval,
                                             &cmpstring);
                if (*res == NO_ERR) {
                    fnresult = compare_strings(instance,
                                               cmpstring,
                                               val2->r.str,
                                               exop);
                }
                if (cmpstring) {
                    m__free(instance, cmpstring);
                }
                break;
            case XP_RT_NUMBER:
                ncx_init_num(instance, &cmpnum);
                if (typ_is_number(testval->btyp)) {
                    myres = ncx_cast_num(instance,
                                         &testval->v.num,
                                         testval->btyp,
                                         &cmpnum,
                                         NCX_BT_FLOAT64);
                    if (myres != NO_ERR) {
                        ncx_set_num_nan(instance, &cmpnum, NCX_BT_FLOAT64);
                    }
                } else if (testval->btyp == NCX_BT_STRING) {
                    myres = ncx_convert_num(instance,
                                            VAL_STR(testval),
                                            NCX_NF_NONE,
                                            NCX_BT_FLOAT64,
                                            &cmpnum);
                    if (myres != NO_ERR) {
                        ncx_set_num_nan(instance, &cmpnum, NCX_BT_FLOAT64);
                    }
                } else {
                    ncx_set_num_nan(instance, &cmpnum, NCX_BT_FLOAT64);
                }

                cmpresult = ncx_compare_nums(instance,
                                             &cmpnum,
                                             &val2->r.num,
                                             NCX_BT_FLOAT64);

                fnresult = convert_compare_result(instance, cmpresult, exop);

                ncx_clean_num(instance, NCX_BT_FLOAT64, &cmpnum);
                break;
            default:
                SET_ERROR(instance, ERR_INTERNAL_VAL);
        }
    }

    return fnresult;

} /* compare_nodeset_to_other */


/********************************************************************
 * FUNCTION compare_nodesets
 *
 * Compare 2 nodeset results
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    val1 == first result struct to compare
 *    val2 == second result struct to compare
 *    exop == XPath expression op to use
 *    res == address of resturn status
 *
 * OUTPUTS:
 *   *res == return status
 *
 * RETURNS:
 *    TRUE if relation is TRUE
     FALSE if relation is FALSE or some error (check *res)
*********************************************************************/
static boolean
compare_nodesets (ncx_instance_t *instance,
                  xpath_pcb_t *pcb,
                  xpath_result_t *val1,
                  xpath_result_t *val2,
                  xpath_exop_t exop,
                  status_t *res)
{
    xpath_resnode_t         *resnode;
    val_value_t             *testval;
    boolean                  fnresult, cfgonly;
    xpath_compwalkerparms_t  walkerparms;

    *res = NO_ERR;
    if (!pcb->val) {
        return FALSE;
    }

    if ((val1->restype != val2->restype) ||
        (val1->restype != XP_RT_NODESET)) {
        *res = SET_ERROR(instance, ERR_INTERNAL_VAL);
        return FALSE;
    }

    /* make sure both node sets are non-empty */
    if (dlq_empty(instance, &val1->r.nodeQ) || dlq_empty(instance, &val2->r.nodeQ)) {
        /* cannot be a matching node in both node-sets
         * if 1 or both node-sets are empty
         */
        return FALSE;
    }

    /* both node-sets have at least 1 node */
    cfgonly = (pcb->flags & XP_FL_CONFIGONLY) ? TRUE : FALSE;

    walkerparms.result2 = val2;
    walkerparms.cmpstring = NULL;
    walkerparms.cmpnum = NULL;
    walkerparms.buffer = m__getMem(instance, TEMP_BUFFSIZE);
    if (!walkerparms.buffer) {
        *res = ERR_INTERNAL_MEM;
        return FALSE;
    }
    walkerparms.buffsize = TEMP_BUFFSIZE;
    walkerparms.exop = exop;
    walkerparms.cmpresult = FALSE;
    walkerparms.res = NO_ERR;

    /* go through all the nodes in the first node-set
     * and compare each leaf against all the leafs
     * in the other node-set; stop when condition
     * is met or both node-sets completely searched
     */
    for (resnode = (xpath_resnode_t *)dlq_firstEntry(instance, &val1->r.nodeQ);
         resnode != NULL && *res == NO_ERR;
         resnode = (xpath_resnode_t *)dlq_nextEntry(instance, resnode)) {

        testval = resnode->node.valptr;

        fnresult =
                val_find_all_descendants(instance,
                                         top_compare_walker_fn,
                                         pcb,
                                         &walkerparms,
                                         testval,
                                         NULL,
                                         NULL,
                                         cfgonly,
                                         FALSE,
                                         TRUE,
                                         TRUE);
        if (walkerparms.res != NO_ERR) {
            *res = walkerparms.res;
        } else if (!fnresult) {
            /* condition was met if return FALSE and
             * walkerparms.res == NO_ERR
             */
            m__free(instance, walkerparms.buffer);
            return TRUE;
        }
    }

    m__free(instance, walkerparms.buffer);

    return FALSE;

} /* compare_nodesets */


/********************************************************************
 * FUNCTION compare_results
 *
 * Compare 2 results, using the specified logic operator
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    val1 == first result struct to compare
 *    val2 == second result struct to compare
 *    exop == XPath exression operator to use
 *    res == address of resturn status
 *
 * OUTPUTS:
 *   *res == return status
 *
 * RETURNS:
 *     relation result (TRUE or FALSE)
 *********************************************************************/
static boolean
compare_results (ncx_instance_t *instance,
                 xpath_pcb_t *pcb,
                 xpath_result_t *val1,
                 xpath_result_t *val2,
                 xpath_exop_t    exop,
                 status_t *res)
{
    xmlChar         *str1, *str2;
    ncx_num_t        num1, num2;
    boolean          retval, bool1, bool2;
    int32            cmpval;

    *res = NO_ERR;

    /* only compare real results, not objects */
    if (!pcb->val) {
        return TRUE;
    }

    cmpval = 0;

    /* compare directly if the vals are the same result type */
    if (val1->restype == val2->restype) {
        switch (val1->restype) {
            case XP_RT_NODESET:
                return compare_nodesets(instance, pcb, val1, val2,
                                        exop, res);
            case XP_RT_NUMBER:
                cmpval = ncx_compare_nums(instance,
                                          &val1->r.num,
                                          &val2->r.num,
                                          NCX_BT_FLOAT64);
                return convert_compare_result(instance, cmpval, exop);
            case XP_RT_STRING:
                return compare_strings(instance,
                                       val1->r.str,
                                       val2->r.str,
                                       exop);
            case XP_RT_BOOLEAN:
                return compare_booleans(instance,
                                        val1->r.boo,
                                        val2->r.boo,
                                        exop);
                break;
            default:
                *res = SET_ERROR(instance, ERR_INTERNAL_VAL);
                return TRUE;
        }
        /*NOTREACHED*/
    }

    /* if 1 nodeset is involved, then compare each node
     * to the other value until the relation is TRUE
     */
    if (val1->restype == XP_RT_NODESET ||
        val2->restype == XP_RT_NODESET) {
        return compare_nodeset_to_other(instance,
                                        pcb,
                                        val1,
                                        val2,
                                        exop,
                                        res);
    }

    /* no nodesets involved, so the specific exop matters */
    if (exop == XP_EXOP_EQUAL || exop == XP_EXOP_NOTEQUAL) {
        /* no nodesets involved
         * not the same result types, so figure out the
         * correct comparision to make.  Priority defined
         * by XPath is
         *
         *  1) boolean
         *  2) number
         *  3) string
         */
        if (val1->restype == XP_RT_BOOLEAN) {
            bool2 = xpath_cvt_boolean(instance, val2);
            return compare_booleans(instance, val1->r.boo, bool2, exop);
        }  else if (val2->restype == XP_RT_BOOLEAN) {
            bool1 = xpath_cvt_boolean(instance, val1);
            return compare_booleans(instance, bool1, val2->r.boo, exop);
        } else if (val1->restype == XP_RT_NUMBER) {
            ncx_init_num(instance, &num2);
            xpath_cvt_number(instance, val2, &num2);
            cmpval = ncx_compare_nums(instance, &val1->r.num, &num2,
                                      NCX_BT_FLOAT64);
            ncx_clean_num(instance, NCX_BT_FLOAT64, &num2);
            return convert_compare_result(instance, cmpval, exop);
        } else if (val2->restype == XP_RT_NUMBER) {
            ncx_init_num(instance, &num1);
            xpath_cvt_number(instance, val1, &num1);
            cmpval = ncx_compare_nums(instance, &num1, &val2->r.num,
                                      NCX_BT_FLOAT64);
            ncx_clean_num(instance, NCX_BT_FLOAT64, &num1);
            return convert_compare_result(instance, cmpval, exop);
        } else if (val1->restype == XP_RT_STRING) {
            str2 = NULL;
            *res = xpath_cvt_string(instance, pcb, val2, &str2);
            if (*res == NO_ERR) {
                cmpval = xml_strcmp(instance, val1->r.str, str2);
            }
            if (str2) {
                m__free(instance, str2);
            }
            if (*res == NO_ERR) {
                return convert_compare_result(instance, cmpval, exop);
            } else {
                return TRUE;
            }
        } else if (val2->restype == XP_RT_STRING) {
            str1 = NULL;
            *res = xpath_cvt_string(instance, pcb, val1, &str1);
            if (*res == NO_ERR) {
                cmpval = xml_strcmp(instance, str1, val2->r.str);
            }
            if (str1) {
                m__free(instance, str1);
            }
            if (*res == NO_ERR) {
                return convert_compare_result(instance, cmpval, exop);
            } else {
                return TRUE;
            }
        } else {
            *res = ERR_NCX_INVALID_VALUE;
            return TRUE;
        }
    }

    /* no nodesets involved
     * expression op is a relational operator
     * not the same result types, so convert to numbers and compare
     */
    ncx_init_num(instance, &num1);
    ncx_init_num(instance, &num2);

    xpath_cvt_number(instance, val1, &num1);
    xpath_cvt_number(instance, val2, &num2);

    cmpval = ncx_compare_nums(instance,&num1,&num2, NCX_BT_FLOAT64);
    retval = convert_compare_result(instance, cmpval, exop);

    ncx_clean_num(instance, NCX_BT_FLOAT64, &num1);
    ncx_clean_num(instance, NCX_BT_FLOAT64, &num2);

    return retval;

} /* compare_results */

/********************************************************************
 * FUNCTION dump_result
 *
 * Generate log output displaying the contents of a result
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    result == result to dump
 *    banner == optional first banner string to use
 *********************************************************************/
static void
dump_result (ncx_instance_t *instance,
             xpath_pcb_t *pcb,
             xpath_result_t *result,
             const char *banner)
{
    xpath_resnode_t       *resnode;
    val_value_t           *val;
    const obj_template_t  *obj;

    if (banner) {
        log_write(instance, "\n%s", banner);
    }
    if (pcb->tkerr.mod) {
        log_write(instance,
                  " mod:%s, line:%u",
                  ncx_get_modname(pcb->tkerr.mod),
                  pcb->tkerr.linenum);
    }
    log_write(instance, "\nxpath result for '%s'", pcb->exprstr);

    switch (result->restype) {
        case XP_RT_NONE:
            log_write(instance, "\n  typ: none");
            break;
        case XP_RT_NODESET:
            log_write(instance, "\n  typ: nodeset = ");
            for (resnode = (xpath_resnode_t *)
                           dlq_firstEntry(instance, &result->r.nodeQ);
                 resnode != NULL;
                 resnode = (xpath_resnode_t *)dlq_nextEntry(instance, resnode)) {

                log_write(instance, "\n   node ");
                if (result->isval) {
                    val = resnode->node.valptr;
                    if (val) {
                        log_write(instance,
                                  "%s:%s [L:%u]",
                                  xmlns_get_ns_prefix(instance, val->nsid),
                                  val->name,
                                  val_get_nest_level(instance, val));
                    } else {
                        log_write(instance, "NULL");
                    }
                } else {
                    obj = resnode->node.objptr;
                    if (obj) {
                        log_write(instance,
                                  "%s:%s [L:%u]",
                                  obj_get_mod_prefix(obj),
                                  obj_get_name(instance, obj),
                                  obj_get_level(instance, obj));
                    } else {
                        log_write(instance, "NULL");
                    }
                }
            }
            break;
        case XP_RT_NUMBER:
            if (result->isval) {
                log_write(instance, "\n  typ: number = ");
                ncx_printf_num(instance, &result->r.num, NCX_BT_FLOAT64);
            } else {
                log_write(instance, "\n  typ: number");
            }
            break;
        case XP_RT_STRING:
            if (result->isval) {
                log_write(instance, "\n  typ: string = %s", result->r.str);
            } else {
                log_write(instance, "\n  typ: string", result->r.str);
            }
            break;
        case XP_RT_BOOLEAN:
            if (result->isval) {
                log_write(instance,
                          "\n  typ: boolean = %s",
                          (result->r.boo) ? "true" : "false");
            } else {
                log_write(instance, "\n  typ: boolean");
            }
            break;
        default:
            log_write(instance, "\n  typ: INVALID (%d)", result->restype);
            SET_ERROR(instance, ERR_INTERNAL_VAL);
    }
    log_write(instance, "\n");

}  /* dump_result */


/********************************************************************
 * FUNCTION get_axis_id
 *
 * Check a string token tfor a match of an AxisName
 *
 * INPUTS:
 *    name == name string to match
 *
 * RETURNS:
 *   enum of axis name or XP_AX_NONE (0) if not an axis name
 *********************************************************************/
static ncx_xpath_axis_t
get_axis_id (ncx_instance_t *instance, const xmlChar *name)
{
    if (!name || !*name) {
        return XP_AX_NONE;
    }

    if (!xml_strcmp(instance, name, XP_AXIS_ANCESTOR)) {
        return XP_AX_ANCESTOR;
    }
    if (!xml_strcmp(instance, name, XP_AXIS_ANCESTOR_OR_SELF)) {
        return XP_AX_ANCESTOR_OR_SELF;
    }
    if (!xml_strcmp(instance, name, XP_AXIS_ATTRIBUTE)) {
        return XP_AX_ATTRIBUTE;
    }
    if (!xml_strcmp(instance, name, XP_AXIS_CHILD)) {
        return XP_AX_CHILD;
    }
    if (!xml_strcmp(instance, name, XP_AXIS_DESCENDANT)) {
        return XP_AX_DESCENDANT;
    }
    if (!xml_strcmp(instance, name, XP_AXIS_DESCENDANT_OR_SELF)) {
        return XP_AX_DESCENDANT_OR_SELF;
    }
    if (!xml_strcmp(instance, name, XP_AXIS_FOLLOWING)) {
        return XP_AX_FOLLOWING;
    }
    if (!xml_strcmp(instance, name, XP_AXIS_FOLLOWING_SIBLING)) {
        return XP_AX_FOLLOWING_SIBLING;
    }
    if (!xml_strcmp(instance, name, XP_AXIS_NAMESPACE)) {
        return XP_AX_NAMESPACE;
    }
    if (!xml_strcmp(instance, name, XP_AXIS_PARENT)) {
        return XP_AX_PARENT;
    }
    if (!xml_strcmp(instance, name, XP_AXIS_PRECEDING)) {
        return XP_AX_PRECEDING;
    }
    if (!xml_strcmp(instance, name, XP_AXIS_PRECEDING_SIBLING)) {
        return XP_AX_PRECEDING_SIBLING;
    }
    if (!xml_strcmp(instance, name, XP_AXIS_SELF)) {
        return XP_AX_SELF;
    }
    return XP_AX_NONE;

} /* get_axis_id */


/********************************************************************
 * FUNCTION get_nodetype_id
 *
 * Check a string token tfor a match of a NodeType
 *
 * INPUTS:
 *    name == name string to match
 *
 * RETURNS:
 *   enum of node type or XP_EXNT_NONE (0) if not a node type name
 *********************************************************************/
static xpath_nodetype_t
get_nodetype_id (ncx_instance_t *instance, const xmlChar *name)
{
    if (!name || !*name) {
        return XP_EXNT_NONE;
    }

    if (!xml_strcmp(instance, name, XP_NT_COMMENT)) {
        return XP_EXNT_COMMENT;
    }
    if (!xml_strcmp(instance, name, XP_NT_TEXT)) {
        return XP_EXNT_TEXT;
    }
    if (!xml_strcmp(instance, name, XP_NT_PROCESSING_INSTRUCTION)) {
        return XP_EXNT_PROC_INST;
    }
    if (!xml_strcmp(instance, name, XP_NT_NODE)) {
        return XP_EXNT_NODE;
    }
    return XP_EXNT_NONE;

} /* get_nodetype_id */


/********************************************************************
 * FUNCTION location_path_end
 *
 * Check if the current location path is ending
 * by checking if the next token is going to start some
 * sub-expression (or end the expression)
 *
 * INPUTS:
 *    pcb == parser control block to check
 *
 * RETURNS:
 *   TRUE if the location path is ended
 *   FALSE if the next token is part of the continuing
 *   location path
 *********************************************************************/
static boolean
location_path_end (ncx_instance_t *instance, xpath_pcb_t *pcb)
{
    tk_type_t nexttyp;

    /* check corner-case path '/' */
    nexttyp = tk_next_typ(instance, pcb->tkc);
    if (nexttyp == TK_TT_NONE ||
        nexttyp == TK_TT_EQUAL ||
        nexttyp == TK_TT_BAR ||
        nexttyp == TK_TT_PLUS ||
        nexttyp == TK_TT_MINUS ||
        nexttyp == TK_TT_LT ||
        nexttyp == TK_TT_GT ||
        nexttyp == TK_TT_NOTEQUAL ||
        nexttyp == TK_TT_LEQUAL ||
        nexttyp == TK_TT_GEQUAL) {

        return TRUE;
    } else {
        return FALSE;
    }

} /* location_path_end */


/**********   X P A T H    F U N C T I O N S   ************/


/********************************************************************
 * FUNCTION boolean_fn
 *
 * boolean boolean(object) function [4.3]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 1 object to convert to boolean
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
boolean_fn (ncx_instance_t *instance,
            xpath_pcb_t *pcb,
            dlq_hdr_t *parmQ,
            status_t  *res)
{
    xpath_result_t  *parm, *result;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (!parm) {
        /* should already be reported in obj mode */
        *res = ERR_NCX_MISSING_PARM;
        return NULL;
    }

    result = new_result(instance, pcb, XP_RT_BOOLEAN);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    result->r.boo = xpath_cvt_boolean(instance, parm);
    *res = NO_ERR;
    return result;

}  /* boolean_fn */


/********************************************************************
 * FUNCTION ceiling_fn
 *
 * number ceiling(number) function [4.4]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 1 number to convert to ceiling(number)
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
ceiling_fn (ncx_instance_t *instance,
            xpath_pcb_t *pcb,
            dlq_hdr_t *parmQ,
            status_t  *res)
{
    xpath_result_t  *parm, *result;
    ncx_num_t        num;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (!parm) {
        *res = ERR_NCX_MISSING_PARM;
        return NULL;
    }

    result = new_result(instance, pcb, XP_RT_NUMBER);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    if (parm->restype == XP_RT_NUMBER) {
        *res = ncx_num_ceiling(instance,
                               &parm->r.num,
                               &result->r.num,
                               NCX_BT_FLOAT64);

    } else {
        ncx_init_num(instance, &num);
        xpath_cvt_number(instance, parm, &num);
        *res = ncx_num_ceiling(instance,
                               &num,
                               &result->r.num,
                               NCX_BT_FLOAT64);
        ncx_clean_num(instance, NCX_BT_FLOAT64, &num);
    }

    if (*res != NO_ERR) {
        free_result(instance, pcb, result);
        result = NULL;
    }

    return result;

}  /* ceiling_fn */


/********************************************************************
 * FUNCTION concat_fn
 *
 * string concat(string, string, string*) function [4.2]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 2 or more strings to concatenate
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
concat_fn (ncx_instance_t *instance,
           xpath_pcb_t *pcb,
           dlq_hdr_t *parmQ,
           status_t  *res)
{
    xpath_result_t  *parm, *result;
    xmlChar         *str, *returnstr;
    uint32           parmcnt, len;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    *res = NO_ERR;
    len = 0;

    /* check at least 2 strings to concat */
    parmcnt = dlq_count(instance, parmQ);
    if (parmcnt < 2) {
        *res = ERR_NCX_MISSING_PARM;
        wrong_parmcnt_error(instance, pcb, parmcnt, *res);
        return NULL;
    }

    /* check all parms are strings and get total len */
    for (parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
         parm != NULL && *res == NO_ERR;
         parm = (xpath_result_t *)dlq_nextEntry(instance, parm)) {
        if (parm->restype != XP_RT_STRING) {
            *res = xpath_cvt_string(instance, pcb, parm, &str);
            if (*res == NO_ERR) {
                len += xml_strlen(instance, str);
                m__free(instance, str);
            }
        } else if (parm->r.str) {
            len += xml_strlen(instance, parm->r.str);
        }
    }

    if (*res != NO_ERR) {
        return NULL;
    }

    result = new_result(instance, pcb, XP_RT_STRING);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    result->r.str = m__getMem(instance, len+1);
    if (!result->r.str) {
        free_result(instance, pcb, result);
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    returnstr = result->r.str;

    for (parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
         parm != NULL && *res == NO_ERR;
         parm = (xpath_result_t *)dlq_nextEntry(instance, parm)) {

        if (parm->restype != XP_RT_STRING) {
            *res = xpath_cvt_string(instance, pcb, parm, &str);
            if (*res == NO_ERR) {
                returnstr += xml_strcpy(instance, returnstr, str);
                m__free(instance, str);
            }
        } else if (parm->r.str) {
            returnstr += xml_strcpy(instance, returnstr, parm->r.str);
        }
    }

    if (*res != NO_ERR) {
        xpath_free_result(instance, result);
        result = NULL;
    }

    return result;

}  /* concat_fn */


/********************************************************************
 * FUNCTION contains_fn
 *
 * boolean contains(string, string) function [4.2]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 2 strings
 *             returns true if the 1st string contains the 2nd string
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
contains_fn (ncx_instance_t *instance,
             xpath_pcb_t *pcb,
             dlq_hdr_t *parmQ,
             status_t  *res)
{
    xpath_result_t  *parm1, *parm2, *result;
    xmlChar         *str1, *str2;
    boolean          malloc1, malloc2;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    *res = NO_ERR;
    str1 = NULL;
    str2 = NULL;
    malloc1 = FALSE;
    malloc2 = FALSE;

    parm1 = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    parm2 = (xpath_result_t *)dlq_nextEntry(instance, parm1);

    if (parm1->restype != XP_RT_STRING) {
        *res = xpath_cvt_string(instance, pcb, parm1, &str1);
        malloc1 = TRUE;
    } else {
        str1 = parm1->r.str;
    }
    if (*res != NO_ERR) {
        return NULL;
    }

    if (parm2->restype != XP_RT_STRING) {
        *res = xpath_cvt_string(instance, pcb, parm2, &str2);
        malloc2 = TRUE;
    } else {
        str2 = parm2->r.str;
    }
    if (*res != NO_ERR) {
        if (malloc1) {
            m__free(instance, str1);
        }
        return NULL;
    }

    result = new_result(instance, pcb, XP_RT_BOOLEAN);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
    } else {
        if (strstr((const char *)str1,
                   (const char *)str2)) {
            result->r.boo = TRUE;
        } else {
            result->r.boo = FALSE;
        }
        *res = NO_ERR;
    }

    if (malloc1) {
        m__free(instance, str1);
    }

    if (malloc2) {
        m__free(instance, str2);
    }

    return result;

}  /* contains_fn */


/********************************************************************
 * FUNCTION count_fn
 *
 * number count(nodeset) function [4.1]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 1 parm (nodeset to get node count for)
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
count_fn (ncx_instance_t *instance,
          xpath_pcb_t *pcb,
          dlq_hdr_t *parmQ,
          status_t  *res)
{
    xpath_result_t *parm, *result;
    ncx_num_t       tempnum;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }
    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);

    *res = NO_ERR;
    result = new_result(instance, pcb, XP_RT_NUMBER);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
    } else {
        if (parm->restype != XP_RT_NODESET) {
            ncx_set_num_zero(instance, &result->r.num, NCX_BT_FLOAT64);
        } else {
            if (pcb->val) {
                ncx_init_num(instance, &tempnum);
                tempnum.u = dlq_count(instance, &parm->r.nodeQ);
                *res = ncx_cast_num(instance,
                                    &tempnum,
                                    NCX_BT_UINT32,
                                    &result->r.num,
                                    NCX_BT_FLOAT64);
                ncx_clean_num(instance, NCX_BT_UINT32, &tempnum);
            } else {
                ncx_set_num_zero(instance, &result->r.num, NCX_BT_FLOAT64);
            }

            if (*res != NO_ERR) {
                free_result(instance, pcb, result);
                result = NULL;
            }
        }
    }
    return result;

}  /* count_fn */


/********************************************************************
 * FUNCTION current_fn
 *
 * number current() function [XPATH 2.0 used in YANG]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == empty parmQ
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
current_fn (ncx_instance_t *instance,
            xpath_pcb_t *pcb,
            dlq_hdr_t *parmQ,
            status_t  *res)
{
    xpath_result_t *result;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    (void)parmQ;

    result = new_nodeset(instance,
                         pcb,
                         pcb->orig_context.node.objptr,
                         pcb->orig_context.node.valptr,
                         1,
                         FALSE);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
    } else {
        *res = NO_ERR;
    }

    return result;

}  /* current_fn */


/********************************************************************
 * FUNCTION false_fn
 *
 * boolean false() function [4.3]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == empty parmQ
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
false_fn (ncx_instance_t *instance,
          xpath_pcb_t *pcb,
          dlq_hdr_t *parmQ,
          status_t  *res)
{
    xpath_result_t *result;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    (void)parmQ;

    result = new_result(instance, pcb, XP_RT_BOOLEAN);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
    } else {
        result->r.boo = FALSE;
        *res = NO_ERR;
    }

    return result;

}  /* false_fn */


/********************************************************************
 * FUNCTION floor_fn
 *
 * number floor(number) function [4.4]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 1 number to convert to floor(number)
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
floor_fn (ncx_instance_t *instance,
          xpath_pcb_t *pcb,
          dlq_hdr_t *parmQ,
          status_t  *res)
{
    xpath_result_t  *parm, *result;
    ncx_num_t        num;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (!parm) {
        *res = ERR_NCX_MISSING_PARM;
        return NULL;
    }

    result = new_result(instance, pcb, XP_RT_NUMBER);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    if (parm->restype == XP_RT_NUMBER) {
        *res = ncx_num_floor(instance,
                             &parm->r.num,
                             &result->r.num,
                             NCX_BT_FLOAT64);
    } else {
        ncx_init_num(instance, &num);
        xpath_cvt_number(instance, parm, &num);
        *res = ncx_num_floor(instance,
                             &num,
                             &result->r.num,
                             NCX_BT_FLOAT64);
        ncx_clean_num(instance, NCX_BT_FLOAT64, &num);
    }

    if (*res != NO_ERR) {
        free_result(instance, pcb, result);
        result = NULL;
    }

    return result;

}  /* floor_fn */


/********************************************************************
 * FUNCTION id_fn
 *
 * nodeset id(object) function [4.1]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 1 parm, which is the object to match
 *             against the current result in progress
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
id_fn (ncx_instance_t *instance,
       xpath_pcb_t *pcb,
       dlq_hdr_t *parmQ,
       status_t  *res)
{
    xpath_result_t  *result;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    (void)parmQ;  /*****/

    result = new_result(instance, pcb, XP_RT_NODESET);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
    } else {
        *res = NO_ERR;
    }

    /**** get all nodes with the specified ID ****/
    /**** No IDs in YANG so return empty nodeset ****/
    /**** Change this later if there are standard IDs ****/

    return result;

}  /* id_fn */


/********************************************************************
 * FUNCTION lang_fn
 *
 * boolean lang(string) function [4.3]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 1 parm; lang string to match
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
lang_fn (ncx_instance_t *instance,
         xpath_pcb_t *pcb,
         dlq_hdr_t *parmQ,
         status_t  *res)
{
    xpath_result_t  *result;

    (void)parmQ;

    /* no attributes in YANG, so no lang attr either */
    result = new_result(instance, pcb, XP_RT_NODESET);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
    } else {
        *res = NO_ERR;
    }
    return result;

}  /* lang_fn */


/********************************************************************
 * FUNCTION last_fn
 *
 * number last() function [4.1]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == empty parmQ
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
last_fn (ncx_instance_t *instance,
         xpath_pcb_t *pcb,
         dlq_hdr_t *parmQ,
         status_t  *res)
{
    xpath_result_t *result;
    ncx_num_t       tempnum;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    (void)parmQ;
    *res = NO_ERR;
    result = new_result(instance, pcb, XP_RT_NUMBER);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
    } else {
        ncx_init_num(instance, &tempnum);
        tempnum.l = pcb->context.last;
        *res = ncx_cast_num(instance,
                            &tempnum,
                            NCX_BT_INT64,
                            &result->r.num,
                            NCX_BT_FLOAT64);

        ncx_clean_num(instance, NCX_BT_INT64, &tempnum);

        if (*res != NO_ERR) {
            free_result(instance, pcb, result);
            result = NULL;
        }
    }

    return result;

}  /* last_fn */


/********************************************************************
 * FUNCTION local_name_fn
 *
 * string local-name(nodeset?) function [4.1]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with optional node-set parm
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
local_name_fn (ncx_instance_t *instance,
               xpath_pcb_t *pcb,
               dlq_hdr_t *parmQ,
               status_t  *res)
{
    xpath_result_t  *parm, *result;
    xpath_resnode_t *resnode;
    const xmlChar   *str;
    uint32           parmcnt;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    parmcnt = dlq_count(instance, parmQ);
    if (parmcnt > 1) {
        *res = ERR_NCX_EXTRA_PARM;
        wrong_parmcnt_error(instance, pcb, parmcnt, *res);
        return NULL;
    }

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    str = NULL;

    if (parm && parm->restype != XP_RT_NODESET) {
        ;
    } else if (parm) {
        resnode = (xpath_resnode_t *)
                  dlq_firstEntry(instance, &parm->r.nodeQ);
        if (resnode) {
            if (pcb->val) {
                str = resnode->node.valptr->name;
            } else {
                str = obj_get_name(instance, resnode->node.objptr);
            }
        }
    } else {
        if (pcb->val) {
            str = pcb->context.node.valptr->name;
        } else {
            str = obj_get_name(instance, pcb->context.node.objptr);
        }
    }

    result = new_result(instance, pcb, XP_RT_STRING);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    if (str) {
        result->r.str = xml_strdup(instance, str);
    } else {
        result->r.str = xml_strdup(instance, EMPTY_STRING);
    }

    if (!result->r.str) {
        *res = ERR_INTERNAL_MEM;
        free_result(instance, pcb, result);
        return NULL;
    }

    return result;

}  /* local_name_fn */


/********************************************************************
 * FUNCTION namespace_uri_fn
 *
 * string namespace-uri(nodeset?) function [4.1]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with optional node-set parm
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
namespace_uri_fn (ncx_instance_t *instance,
                  xpath_pcb_t *pcb,
                  dlq_hdr_t *parmQ,
                  status_t  *res)
{
    xpath_result_t  *parm, *result;
    xpath_resnode_t *resnode;
    const xmlChar   *str;
    uint32           parmcnt;
    xmlns_id_t       nsid;


    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    parmcnt = dlq_count(instance, parmQ);
    if (parmcnt > 1) {
        *res = ERR_NCX_EXTRA_PARM;
        wrong_parmcnt_error(instance, pcb, parmcnt, *res);
        return NULL;
    }

    nsid = 0;
    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (parm && parm->restype != XP_RT_NODESET) {
        ;
    } else if (parm) {
        resnode = (xpath_resnode_t *)
                  dlq_firstEntry(instance, &parm->r.nodeQ);
        if (resnode) {
            if (pcb->val) {
                nsid = obj_get_nsid(instance, resnode->node.valptr->obj);
            } else {
                nsid = obj_get_nsid(instance, resnode->node.objptr);
            }
        }
    } else {
        if (pcb->val) {
            nsid = obj_get_nsid(instance, pcb->context.node.valptr->obj);
        } else {
            nsid = obj_get_nsid(instance, pcb->context.node.objptr);
        }
    }

    result = new_result(instance, pcb, XP_RT_STRING);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    if (nsid) {
        str = xmlns_get_ns_name(instance, nsid);
        result->r.str = xml_strdup(instance, str);
    } else {
        result->r.str = xml_strdup(instance, EMPTY_STRING);
    }

    if (!result->r.str) {
        *res = ERR_INTERNAL_MEM;
        free_result(instance, pcb, result);
        return NULL;
    }

    return result;

}  /* namespace_uri_fn */


/********************************************************************
 * FUNCTION name_fn
 *
 * string name(nodeset?) function [4.1]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with optional node-set parm
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
name_fn (ncx_instance_t *instance,
         xpath_pcb_t *pcb,
         dlq_hdr_t *parmQ,
         status_t  *res)
{
    xpath_result_t  *parm, *result;
    xpath_resnode_t *resnode;
    const xmlChar   *prefix, *name;
    xmlChar         *str;
    uint32           len, parmcnt;
    xmlns_id_t       nsid;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    parmcnt = dlq_count(instance, parmQ);
    if (parmcnt > 1) {
        *res = ERR_NCX_EXTRA_PARM;
        wrong_parmcnt_error(instance, pcb, parmcnt, *res);
        return NULL;
    }

    nsid = 0;
    name = NULL;
    prefix = NULL;

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (parm && parm->restype != XP_RT_NODESET) {
        ;
    } else if (parm) {
        resnode = (xpath_resnode_t *)
                  dlq_firstEntry(instance, &parm->r.nodeQ);
        if (resnode) {
            if (pcb->val) {
                nsid = obj_get_nsid(instance, resnode->node.valptr->obj);
                name = resnode->node.valptr->name;
            } else {
                nsid = obj_get_nsid(instance, resnode->node.objptr);
                name = obj_get_name(instance, resnode->node.objptr);
            }
        }
    } else {
        if (pcb->val) {
            nsid = obj_get_nsid(instance, pcb->context.node.valptr->obj);
            name = pcb->context.node.valptr->name;
        } else {
            nsid = obj_get_nsid(instance, pcb->context.node.objptr);
            name = obj_get_name(instance, pcb->context.node.objptr);
        }
    }

    result = new_result(instance, pcb, XP_RT_STRING);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    if (nsid) {
        prefix = xmlns_get_ns_prefix(instance, nsid);
    }

    len = 0;
    if (prefix) {
        len += xml_strlen(instance, prefix);
        len++;
    }
    if (name) {
        len += xml_strlen(instance, name);
    } else {
        name = EMPTY_STRING;
    }

    result->r.str = m__getMem(instance, len+1);
    if (!result->r.str) {
        *res = ERR_INTERNAL_MEM;
        free_result(instance, pcb, result);
        return NULL;
    }

    str = result->r.str;
    if (prefix) {
        str += xml_strcpy(instance, str, prefix);
        *str++ = ':';
    }
    xml_strcpy(instance, str, name);

    return result;

}  /* name_fn */


/********************************************************************
 * FUNCTION normalize_space_fn
 *
 * string normalize-space(string?) function [4.2]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with optional string to convert to normalized
 *             string
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
normalize_space_fn (ncx_instance_t *instance,
                    xpath_pcb_t *pcb,
                    dlq_hdr_t *parmQ,
                    status_t  *res)
{
    xpath_result_t  *parm, *result, *tempresult;
    const xmlChar   *teststr;
    xmlChar         *mallocstr, *str;
    uint32           parmcnt;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    parmcnt = dlq_count(instance, parmQ);
    if (parmcnt > 1) {
        *res = ERR_NCX_EXTRA_PARM;
        wrong_parmcnt_error(instance, pcb, parmcnt, *res);
        return NULL;
    }

    teststr = NULL;
    str = NULL;
    mallocstr = NULL;

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (parm && parm->restype != XP_RT_STRING) {
        ;
    } else if (parm) {
        teststr = parm->r.str;
    } else {
        tempresult= new_nodeset(instance,
                                pcb,
                                pcb->context.node.objptr,
                                pcb->context.node.valptr,
                                1,
                                FALSE);

        if (!tempresult) {
            *res = ERR_INTERNAL_MEM;
            return NULL;
        } else {
            *res = xpath_cvt_string(instance,
                                    pcb,
                                    tempresult,
                                    &mallocstr);
            if (*res == NO_ERR) {
                teststr = mallocstr;
            }
            free_result(instance, pcb, tempresult);
        }
    }

    if (*res != NO_ERR) {
        return NULL;
    }

    result = new_result(instance, pcb, XP_RT_STRING);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        if (mallocstr) {
            m__free(instance, mallocstr);
        }
        return NULL;
    }

    if (!teststr) {
        teststr = EMPTY_STRING;
    }

    result->r.str = m__getMem(instance, xml_strlen(instance, teststr)+1);
    if (!result->r.str) {
        *res = ERR_INTERNAL_MEM;
        free_result(instance, pcb, result);
        if (mallocstr) {
            m__free(instance, mallocstr);
        }
        return NULL;
    }

    str = result->r.str;

    while (*teststr && xml_isspace(*teststr)) {
        teststr++;
    }

    while (*teststr) {
        if (xml_isspace(*teststr)) {
            *str++ = ' ';
            teststr++;
            while (*teststr && xml_isspace(*teststr)) {
                teststr++;
            }

        } else {
            *str++ = *teststr++;
        }
    }

    if (str > result->r.str && *(str-1) == ' ') {
        str--;
    }

    *str = 0;

    if (mallocstr) {
        m__free(instance, mallocstr);
    }

    return result;

}  /* normalize_space_fn */


/********************************************************************
 * FUNCTION not_fn
 *
 * boolean not(object) function [4.3]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 1 object to perform NOT boolean conversion
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
not_fn (ncx_instance_t *instance,
        xpath_pcb_t *pcb,
        dlq_hdr_t *parmQ,
        status_t  *res)
{
    xpath_result_t  *parm, *result;
    boolean          boolresult;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (parm->restype != XP_RT_BOOLEAN) {
        boolresult = xpath_cvt_boolean(instance, parm);
    } else {
        boolresult = parm->r.boo;
    }

    result = new_result(instance, pcb, XP_RT_BOOLEAN);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
    } else {
        result->r.boo = !boolresult;
        *res = NO_ERR;
    }

    return result;

}  /* not_fn */


/********************************************************************
 * FUNCTION number_fn
 *
 * number number(object?) function [4.4]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 1 optional object to convert to a number
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
number_fn (ncx_instance_t *instance,
           xpath_pcb_t *pcb,
           dlq_hdr_t *parmQ,
           status_t  *res)
{
    xpath_result_t  *parm, *result, *tempresult;
    uint32           parmcnt;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    parmcnt = dlq_count(instance, parmQ);
    if (parmcnt > 1) {
        *res = ERR_NCX_EXTRA_PARM;
        wrong_parmcnt_error(instance, pcb, parmcnt, *res);
        return NULL;
    }

    result = new_result(instance, pcb, XP_RT_NUMBER);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    *res = NO_ERR;

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (parm) {
        xpath_cvt_number(instance, parm, &result->r.num);
    } else {
        /* convert the context node value */
        if (pcb->val) {
            /* get the value of the context node
             * and convert it to a string first
             */
            tempresult= new_nodeset(instance,
                                    pcb,
                                    pcb->context.node.objptr,
                                    pcb->context.node.valptr,
                                    1,
                                    FALSE);

            if (!tempresult) {
                *res = ERR_INTERNAL_MEM;
            } else {
                xpath_cvt_number(instance, tempresult, &result->r.num);
                free_result(instance, pcb, tempresult);
            }
        } else {
            /* nothing to check; conversion to NaN is
             * not an XPath error
             */
            ncx_set_num_zero(instance, &result->r.num, NCX_BT_FLOAT64);
        }
    }

    if (*res != NO_ERR) {
        free_result(instance, pcb, result);
        result = NULL;
    }

    return result;

}  /* number_fn */


/********************************************************************
 * FUNCTION position_fn
 *
 * number position() function [4.1]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == empty parmQ
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
position_fn (ncx_instance_t *instance,
             xpath_pcb_t *pcb,
             dlq_hdr_t *parmQ,
             status_t  *res)
{
    xpath_result_t *result;
    ncx_num_t       tempnum;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    (void)parmQ;
    *res = NO_ERR;
    result = new_result(instance, pcb, XP_RT_NUMBER);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
    } else {
        ncx_init_num(instance, &tempnum);
        tempnum.l = pcb->context.position;
        *res = ncx_cast_num(instance,
                            &tempnum,
                            NCX_BT_INT64,
                            &result->r.num,
                            NCX_BT_FLOAT64);

        ncx_clean_num(instance, NCX_BT_INT64, &tempnum);

        if (*res != NO_ERR) {
            free_result(instance, pcb, result);
            result = NULL;
        }
    }

    return result;

}  /* position_fn */


/********************************************************************
 * FUNCTION round_fn
 *
 * number round(number) function [4.4]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 1 number to convert to round(number)
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
round_fn (ncx_instance_t *instance,
          xpath_pcb_t *pcb,
          dlq_hdr_t *parmQ,
          status_t  *res)
{
    xpath_result_t  *parm, *result;
    ncx_num_t        num;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (!parm) {
        *res = ERR_NCX_MISSING_PARM;
        return NULL;
    }

    result = new_result(instance, pcb, XP_RT_NUMBER);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    if (parm->restype == XP_RT_NUMBER) {
        *res = ncx_round_num(instance,
                             &parm->r.num,
                             &result->r.num,
                             NCX_BT_FLOAT64);
    } else {
        ncx_init_num(instance, &num);
        xpath_cvt_number(instance, parm, &num);
        *res = ncx_round_num(instance,
                             &num,
                             &result->r.num,
                             NCX_BT_FLOAT64);
        ncx_clean_num(instance, NCX_BT_FLOAT64, &num);
    }

    if (*res != NO_ERR) {
        free_result(instance, pcb, result);
        result = NULL;
    }

    return result;

}  /* round_fn */


/********************************************************************
 * FUNCTION starts_with_fn
 *
 * boolean starts-with(string, string) function [4.2]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 2 strings
 *             returns true if the 1st string starts with the 2nd string
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
starts_with_fn (ncx_instance_t *instance,
                xpath_pcb_t *pcb,
                dlq_hdr_t *parmQ,
                status_t  *res)
{
    xpath_result_t  *parm1, *parm2, *result;
    xmlChar         *str1, *str2;
    uint32           len1, len2;
    boolean          malloc1, malloc2;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    *res = NO_ERR;
    str1 = NULL;
    str2 = NULL;
    malloc1 = FALSE;
    malloc2 = FALSE;
    len1 = 0;
    len2= 0;

    parm1 = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    parm2 = (xpath_result_t *)dlq_nextEntry(instance, parm1);

    if (parm1->restype != XP_RT_STRING) {
        *res = xpath_cvt_string(instance, pcb, parm1, &str1);
        malloc1 = TRUE;
    } else {
        str1 = parm1->r.str;
    }
    if (*res != NO_ERR) {
        return NULL;
    }

    if (parm2->restype != XP_RT_STRING) {
        *res = xpath_cvt_string(instance, pcb, parm2, &str2);
        malloc2 = TRUE;
    } else {
        str2 = parm2->r.str;
    }
    if (*res != NO_ERR) {
        if (malloc1) {
            m__free(instance, str1);
        }
        return NULL;
    }

    result = new_result(instance, pcb, XP_RT_BOOLEAN);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
    } else {
        len1 = xml_strlen(instance, str1);
        len2 = xml_strlen(instance, str2);

        if (len2 > len1) {
            result->r.boo = FALSE;
        } else if (!xml_strncmp(instance, str1, str2, len2)) {
            result->r.boo = TRUE;
        } else {
            result->r.boo = FALSE;
        }
        *res = NO_ERR;
    }

    if (malloc1) {
        m__free(instance, str1);
    }
    if (malloc2) {
        m__free(instance, str2);
    }

    return result;

}  /* starts_with_fn */


/********************************************************************
 * FUNCTION string_fn
 *
 * string string(object?) function [4.2]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with optional object to convert to string
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
string_fn (ncx_instance_t *instance,
           xpath_pcb_t *pcb,
           dlq_hdr_t *parmQ,
           status_t  *res)
{
    xpath_result_t  *parm, *result, *tempresult;
    uint32           parmcnt;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    parmcnt = dlq_count(instance, parmQ);
    if (parmcnt > 1) {
        *res = ERR_NCX_EXTRA_PARM;
        wrong_parmcnt_error(instance, pcb, parmcnt, *res);
        return NULL;
    }

    result = new_result(instance, pcb, XP_RT_STRING);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    *res = NO_ERR;

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (parm) {
        xpath_cvt_string(instance, pcb, parm, &result->r.str);
    } else {
        /* convert the context node value */
        if (pcb->val) {
            /* get the value of the context node
             * and convert it to a string first
             */
            tempresult= new_nodeset(instance,
                                    pcb,
                                    pcb->context.node.objptr,
                                    pcb->context.node.valptr,
                                    1,
                                    FALSE);

            if (!tempresult) {
                *res = ERR_INTERNAL_MEM;
            } else {
                *res = xpath_cvt_string(instance,
                                        pcb,
                                        tempresult,
                                        &result->r.str);
                free_result(instance, pcb, tempresult);
            }
        } else {
            result->r.str = xml_strdup(instance, EMPTY_STRING);
            if (!result->r.str) {
                *res = ERR_INTERNAL_MEM;
            }
        }
    }

    if (*res != NO_ERR) {
        free_result(instance, pcb, result);
        result = NULL;
    }

    return result;

}  /* string_fn */


/********************************************************************
 * FUNCTION string_length_fn
 *
 * number string-length(string?) function [4.2]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with optional string to check length
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
string_length_fn (ncx_instance_t *instance,
                  xpath_pcb_t *pcb,
                  dlq_hdr_t *parmQ,
                  status_t  *res)
{
    xpath_result_t  *parm, *result, *tempresult;
    xmlChar         *tempstr;
    uint32           parmcnt, len;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    parmcnt = dlq_count(instance, parmQ);
    if (parmcnt > 1) {
        *res = ERR_NCX_EXTRA_PARM;
        wrong_parmcnt_error(instance, pcb, parmcnt, *res);
        return NULL;
    }

    result = new_result(instance, pcb, XP_RT_NUMBER);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    *res = NO_ERR;
    len = 0;
    tempstr = NULL;

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (parm && parm->restype != XP_RT_STRING) {
        *res = xpath_cvt_string(instance, pcb, parm, &tempstr);
        if (*res == NO_ERR) {
            len = xml_strlen(instance, tempstr);
            m__free(instance, tempstr);
        }
    } else if (parm) {
        len = xml_strlen(instance, parm->r.str);
    } else {
        /* convert the context node value */
        if (pcb->val) {
            /* get the value of the context node
             * and convert it to a string first
             */
            tempresult= new_nodeset(instance,
                                    pcb,
                                    pcb->context.node.objptr,
                                    pcb->context.node.valptr,
                                    1,
                                    FALSE);

            if (!tempresult) {
                *res = ERR_INTERNAL_MEM;
            } else {
                tempstr = NULL;
                *res = xpath_cvt_string(instance,
                                        pcb,
                                        tempresult,
                                        &tempstr);
                free_result(instance, pcb, tempresult);
                if (*res == NO_ERR) {
                    len = xml_strlen(instance, tempstr);
                    m__free(instance, tempstr);
                }
            }
        }
    }

    if (*res == NO_ERR) {
        set_uint32_num(len, &result->r.num);
    } else {
        free_result(instance, pcb, result);
        result = NULL;
    }

    return result;

}  /* string_length_fn */


/********************************************************************
 * FUNCTION substring_fn
 *
 * string substring(string, number, number?) function [4.2]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 2 or 3 parms
 *             returns substring of 1st string starting at the
 *             position indicated by the first number; copies
 *             only N chars if the 2nd number is present
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
substring_fn (ncx_instance_t *instance,
              xpath_pcb_t *pcb,
              dlq_hdr_t *parmQ,
              status_t  *res)
{
    xpath_result_t  *parm1, *parm2, *parm3, *result;
    xmlChar         *str1;
    ncx_num_t        tempnum, num2, num3;
    uint32           parmcnt, maxlen;
    int64            startpos, copylen, slen;
    status_t         myres;
    boolean          copylenset, malloc1;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    *res = NO_ERR;

    str1 = NULL;
    malloc1 = FALSE;
    ncx_init_num(instance, &num2);
    ncx_init_num(instance, &num3);

    /* check at least 2 strings to concat */
    parmcnt = dlq_count(instance, parmQ);
    if (parmcnt < 2) {
        *res = ERR_NCX_MISSING_PARM;
        wrong_parmcnt_error(instance, pcb, parmcnt, *res);
        return NULL;
    } else if (parmcnt > 3) {
        *res = ERR_NCX_EXTRA_PARM;
        wrong_parmcnt_error(instance, pcb, parmcnt, *res);
        return NULL;
    }

    parm1 = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    parm2 = (xpath_result_t *)dlq_nextEntry(instance, parm1);
    parm3 = (xpath_result_t *)dlq_nextEntry(instance, parm2);

    if (parm1->restype != XP_RT_STRING) {
        *res = xpath_cvt_string(instance, pcb, parm1, &str1);
        if (*res != NO_ERR) {
            return NULL;
        } else {
            malloc1 = TRUE;
        }
    } else {
        str1 = parm1->r.str;
    }

    xpath_cvt_number(instance, parm2, &num2);
    if (parm3) {
        xpath_cvt_number(instance, parm3, &num3);
    }

    startpos = 0;
    ncx_init_num(instance, &tempnum);
    myres = ncx_round_num(instance, &num2, &tempnum, NCX_BT_FLOAT64);
    if (myres == NO_ERR) {
        startpos = ncx_cvt_to_int64(instance, &tempnum, NCX_BT_FLOAT64);
    }
    ncx_clean_num(instance, NCX_BT_FLOAT64, &tempnum);

    copylenset = FALSE;
    copylen = 0;
    if (parm3) {
        copylenset = TRUE;
        myres = ncx_round_num(instance, &num3, &tempnum, NCX_BT_FLOAT64);
        if (myres == NO_ERR) {
            copylen = ncx_cvt_to_int64(instance, &tempnum, NCX_BT_FLOAT64);

        }
        ncx_clean_num(instance, NCX_BT_FLOAT64, &tempnum);
    }

    slen = (int64)xml_strlen(instance, str1);

    result = new_result(instance, pcb, XP_RT_STRING);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    if (startpos > (slen+1) || (copylenset && copylen <= 0)) {
        /* starting after EOS */
        result->r.str = xml_strdup(instance, EMPTY_STRING);
    } else if (copylenset) {
        if (startpos < 1) {
            /* adjust startpos, then copy N chars */
            copylen += (startpos-1);
            startpos = 1;
        }
        if (copylen >= 1) {
            if (copylen >= NCX_MAX_UINT) {
                /* internal max of strings way less than 4G
                 * so copy the whole string
                 */
                result->r.str = xml_strdup(instance, &str1[startpos-1]);
            } else {
                /* copy at most N chars of whole string */
                maxlen = (uint32)copylen;
                result->r.str = xml_strndup(instance,
                                            &str1[startpos-1],
                                            maxlen);
            }
        } else {
            result->r.str = xml_strdup(instance, EMPTY_STRING);
        }
    } else if (startpos <= 1) {
        /* copying whole string */
        result->r.str = xml_strdup(instance, str1);
    } else {
        /* copying rest of string */
        result->r.str = xml_strdup(instance, &str1[startpos-1]);
    }

    if (!result->r.str) {
        *res = ERR_INTERNAL_MEM;
        free_result(instance, pcb, result);
        result = NULL;
    }

    if (malloc1) {
        m__free(instance, str1);
    }
    ncx_clean_num(instance, NCX_BT_FLOAT64, &num2);
    ncx_clean_num(instance, NCX_BT_FLOAT64, &num3);

    return result;

}  /* substring_fn */


/********************************************************************
 * FUNCTION substring_after_fn
 *
 * string substring-after(string, string) function [4.2]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 2 strings
 *             returns substring of 1st string after
 *             the occurance of the 2nd string
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
substring_after_fn (ncx_instance_t *instance,
                    xpath_pcb_t *pcb,
                    dlq_hdr_t *parmQ,
                    status_t  *res)
{
    xpath_result_t  *parm1, *parm2, *result;
    const xmlChar   *str, *retstr;
    xmlChar         *str1, *str2;
    boolean          malloc1, malloc2;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    str1 = NULL;
    str2 = NULL;
    malloc1 = FALSE;
    malloc2 = FALSE;

    parm1 = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    parm2 = (xpath_result_t *)dlq_nextEntry(instance, parm1);

    if (parm1->restype != XP_RT_STRING) {
        *res = xpath_cvt_string(instance, pcb, parm1, &str1);
        if (*res != NO_ERR) {
            return NULL;
        }
        malloc1 = TRUE;
    } else {
        str1 = parm1->r.str;
    }

    if (parm2->restype != XP_RT_STRING) {
        *res = xpath_cvt_string(instance, pcb, parm2, &str2);
        if (*res != NO_ERR) {
            if (malloc1) {
                m__free(instance, str1);
            }
            return NULL;
        }
        malloc2 = TRUE;
    } else {
        str2 = parm2->r.str;
    }

    result = new_result(instance, pcb, XP_RT_STRING);
    if (result) {
        str = (const xmlChar *)
              strstr((const char *)str1,
                     (const char *)str2);
        if (str) {
            retstr = str + xml_strlen(instance, str2);
            result->r.str = m__getMem(instance, xml_strlen(instance, retstr)+1);
            if (result->r.str) {
                xml_strcpy(instance, result->r.str, retstr);
            }
        } else {
            result->r.str = xml_strdup(instance, EMPTY_STRING);
        }

        if (!result->r.str) {
            *res = ERR_INTERNAL_MEM;
        }

        if (*res != NO_ERR) {
            free_result(instance, pcb, result);
            result = NULL;
        }
    } else {
        *res = ERR_INTERNAL_MEM;
    }

    if (malloc1) {
        m__free(instance, str1);
    }
    if (malloc2) {
        m__free(instance, str2);
    }

    return result;

}  /* substring_after_fn */


/********************************************************************
 * FUNCTION substring_before_fn
 *
 * string substring-before(string, string) function [4.2]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 2 strings
 *             returns substring of 1st string that precedes
 *             the occurance of the 2nd string
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
substring_before_fn (ncx_instance_t *instance,
                     xpath_pcb_t *pcb,
                     dlq_hdr_t *parmQ,
                     status_t  *res)
{
    xpath_result_t  *parm1, *parm2, *result;
    const xmlChar   *str;
    xmlChar         *str1, *str2;
    uint32           retlen;
    boolean          malloc1, malloc2;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    malloc1 = FALSE;
    malloc2 = FALSE;

    parm1 = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    parm2 = (xpath_result_t *)dlq_nextEntry(instance, parm1);

    if (parm1->restype != XP_RT_STRING) {
        *res = xpath_cvt_string(instance, pcb, parm1, &str1);
        if (*res != NO_ERR) {
            return NULL;
        }
        malloc1 = TRUE;
    } else {
        str1 = parm1->r.str;
    }

    if (parm2->restype != XP_RT_STRING) {
        *res = xpath_cvt_string(instance, pcb, parm2, &str2);
        if (*res != NO_ERR) {
            if (malloc1) {
                m__free(instance, str1);
            }
            return NULL;
        }
        malloc2 = TRUE;
    } else {
        str2 = parm2->r.str;
    }

    result = new_result(instance, pcb, XP_RT_STRING);
    if (result) {
        str = (const xmlChar *)
              strstr((const char *)str1,
                     (const char *)str2);
        if (str) {
            retlen = (uint32)(str - str1);
            result->r.str = m__getMem(instance, retlen+1);
            if (result->r.str) {
                xml_strncpy(instance,
                            result->r.str,
                            str1,
                            retlen);
            }
        } else {
            result->r.str = xml_strdup(instance, EMPTY_STRING);
        }

        if (!result->r.str) {
            *res = ERR_INTERNAL_MEM;
        }

        if (*res != NO_ERR) {
            free_result(instance, pcb, result);
            result = NULL;
        }
    } else {
        *res = ERR_INTERNAL_MEM;
    }

    if (malloc1) {
        m__free(instance, str1);
    }
    if (malloc2) {
        m__free(instance, str2);
    }

    return result;

}  /* substring_before_fn */


/********************************************************************
 * FUNCTION sum_fn
 *
 * number sum(nodeset) function [4.4]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 1 nodeset to convert to numbers
 *             and add together to resurn the total
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
sum_fn (ncx_instance_t *instance,
        xpath_pcb_t *pcb,
        dlq_hdr_t *parmQ,
        status_t  *res)
{
    xpath_result_t   *parm, *result;
    xpath_resnode_t  *resnode;
    val_value_t      *val;
    ncx_num_t         tempnum, mysum;
    status_t          myres;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    result = new_result(instance, pcb, XP_RT_NUMBER);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (!parm) {
        *res = ERR_NCX_MISSING_PARM;
        return NULL;
    }
    if (parm->restype != XP_RT_NODESET || !pcb->val) {
        ncx_set_num_zero(instance, &result->r.num, NCX_BT_FLOAT64);
    } else {
        ncx_init_num(instance, &tempnum);
        ncx_set_num_zero(instance, &tempnum, NCX_BT_FLOAT64);
        ncx_init_num(instance, &mysum);
        ncx_set_num_zero(instance, &mysum, NCX_BT_FLOAT64);
        myres = NO_ERR;

        for (resnode = (xpath_resnode_t *)
                       dlq_firstEntry(instance, &parm->r.nodeQ);
             resnode != NULL && myres == NO_ERR;
             resnode = (xpath_resnode_t *)
                       dlq_nextEntry(instance, resnode)) {

            val = val_get_first_leaf(instance, resnode->node.valptr);
            if (val && typ_is_number(val->btyp)) {
                myres = ncx_cast_num(instance,
                                     &val->v.num,
                                     val->btyp,
                                     &tempnum,
                                     NCX_BT_FLOAT64);
                if (myres == NO_ERR) {
                    mysum.d += tempnum.d;
                }
            } else {
                myres = ERR_NCX_INVALID_VALUE;
            }
        }

        if (myres == NO_ERR) {
            result->r.num.d = mysum.d;
        } else {
            ncx_set_num_nan(instance, &result->r.num, NCX_BT_FLOAT64);
        }
    }

    return result;

}  /* sum_fn */

/********************************************************************
 * FUNCTION rift_min_fn
 *
 * number min(nodeset)
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 1 nodeset to convert to numbers
 *             and add together to resurn the total
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t*
rift_min_fn(struct ncx_instance_t_ *instance,
            xpath_pcb_t *pcb,
            dlq_hdr_t *parmQ,
            status_t *res)
{
    xpath_result_t   *parm, *result;
    xpath_resnode_t  *resnode;
    val_value_t      *val;
    ncx_num_t         tempnum, mymin;
    status_t          myres;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    result = new_result(instance, pcb, XP_RT_NUMBER);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (!parm) {
        *res = ERR_NCX_MISSING_PARM;
        return NULL;
    }
   if (parm->restype != XP_RT_NODESET || !pcb->val) {
        ncx_set_num_zero(instance, &result->r.num, NCX_BT_FLOAT64);
    } else {
        ncx_init_num(instance, &tempnum);
        ncx_set_num_zero(instance, &tempnum, NCX_BT_FLOAT64);
        ncx_init_num(instance, &mymin);
        ncx_set_num_zero(instance, &mymin, NCX_BT_FLOAT64);
        myres = NO_ERR;

        int32_t count = 0;
        for (resnode = (xpath_resnode_t *)
                       dlq_firstEntry(instance, &parm->r.nodeQ);
             resnode != NULL && myres == NO_ERR;
             resnode = (xpath_resnode_t *)
                       dlq_nextEntry(instance, resnode)) {

            val = val_get_first_leaf(instance, resnode->node.valptr);
            if (val && typ_is_number(val->btyp)) {
                myres = ncx_cast_num(instance,
                                     &val->v.num,
                                     val->btyp,
                                     &tempnum,
                                     NCX_BT_FLOAT64);
                if (myres == NO_ERR) {
                    if (count && (tempnum.d < mymin.d)) {
                        mymin.d = tempnum.d;
                    }
                    else {
                        mymin.d = tempnum.d;
                    }
                    count++;
                }
            } else {
                myres = ERR_NCX_INVALID_VALUE;
            }
        }

        if (myres == NO_ERR) {
            result->r.num.d = mymin.d;
        } else {
            ncx_set_num_nan(instance, &result->r.num, NCX_BT_FLOAT64);
        }
    }

    return result;

}  /* rift_min_fn */

/********************************************************************
 * FUNCTION rift_max_fn
 *
 * number max(nodeset)
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 1 nodeset to convert to numbers
 *             and add together to resurn the total
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t*
rift_max_fn(struct ncx_instance_t_ *instance,
            xpath_pcb_t *pcb,
            dlq_hdr_t *parmQ,
            status_t *res)
{
    xpath_result_t   *parm, *result;
    xpath_resnode_t  *resnode;
    val_value_t      *val;
    ncx_num_t         tempnum, mymax;
    status_t          myres;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    result = new_result(instance, pcb, XP_RT_NUMBER);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (!parm) {
        *res = ERR_NCX_INVALID_VALUE;
        return NULL;
    }
    if (parm->restype != XP_RT_NODESET || !pcb->val) {
        ncx_set_num_zero(instance, &result->r.num, NCX_BT_FLOAT64);
    } else {
        ncx_init_num(instance, &tempnum);
        ncx_set_num_zero(instance, &tempnum, NCX_BT_FLOAT64);
        ncx_init_num(instance, &mymax);
        ncx_set_num_zero(instance, &mymax, NCX_BT_FLOAT64);
        myres = NO_ERR;

        int32_t count = 0;
        for (resnode = (xpath_resnode_t *)
                       dlq_firstEntry(instance, &parm->r.nodeQ);
             resnode != NULL && myres == NO_ERR;
             resnode = (xpath_resnode_t *)
                       dlq_nextEntry(instance, resnode)) {

            val = val_get_first_leaf(instance, resnode->node.valptr);
            if (val && typ_is_number(val->btyp)) {
                myres = ncx_cast_num(instance,
                                     &val->v.num,
                                     val->btyp,
                                     &tempnum,
                                     NCX_BT_FLOAT64);
                if (myres == NO_ERR) {
                    if (count && (tempnum.d > mymax.d)) {
                        mymax.d = tempnum.d;
                    }
                    else {
                        mymax.d = tempnum.d;
                    }
                    count++;
                }
            } else {
                myres = ERR_NCX_INVALID_VALUE;
            }
        }

        if (myres == NO_ERR) {
            result->r.num.d = mymax.d;
        } else {
            ncx_set_num_nan(instance, &result->r.num, NCX_BT_FLOAT64);
        }
    }

    return result;

}  /* rift_max_fn */

/********************************************************************
 * FUNCTION rift_avg_fn
 *
 * number avg(nodeset)
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 1 nodeset to convert to numbers
 *             and add together to resurn the total
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t*
rift_avg_fn(struct ncx_instance_t_ *instance,
            xpath_pcb_t *pcb,
            dlq_hdr_t *parmQ,
            status_t *res)
{
    xpath_result_t   *parm, *result;
    xpath_resnode_t  *resnode;
    val_value_t      *val;
    ncx_num_t         tempnum, myavg;
    status_t          myres;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    result = new_result(instance, pcb, XP_RT_NUMBER);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (!parm) {
        *res = ERR_NCX_MISSING_PARM;
        return NULL;
    }
    if (parm->restype != XP_RT_NODESET || !pcb->val) {
        ncx_set_num_zero(instance, &result->r.num, NCX_BT_FLOAT64);
    } else {
        ncx_init_num(instance, &tempnum);
        ncx_set_num_zero(instance, &tempnum, NCX_BT_FLOAT64);
        ncx_init_num(instance, &myavg);
        ncx_set_num_zero(instance, &myavg, NCX_BT_FLOAT64);
        myres = NO_ERR;

        int32_t count = 0;
        for (resnode = (xpath_resnode_t *)
                       dlq_firstEntry(instance, &parm->r.nodeQ);
             resnode != NULL && myres == NO_ERR;
             resnode = (xpath_resnode_t *)
                       dlq_nextEntry(instance, resnode)) {

            val = val_get_first_leaf(instance, resnode->node.valptr);
            if (val && typ_is_number(val->btyp)) {
                myres = ncx_cast_num(instance,
                                     &val->v.num,
                                     val->btyp,
                                     &tempnum,
                                     NCX_BT_FLOAT64);
                if (myres == NO_ERR) {
                    if (count) {
                        myavg.d = ((myavg.d*count)+tempnum.d)/(count+1);
                    }
                    else {
                        myavg.d = tempnum.d;
                    }
                    count++;
                }
            } else {
                myres = ERR_NCX_INVALID_VALUE;
            }
        }

        if (myres == NO_ERR) {
            result->r.num.d = myavg.d;
        } else {
            ncx_set_num_nan(instance, &result->r.num, NCX_BT_FLOAT64);
        }
    }

    return result;

}  /* sum_fn */

/********************************************************************
 * FUNCTION translate_fn
 *
 * string translate(string, string, string) function [4.2]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 3 strings to translate
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
translate_fn (ncx_instance_t *instance,
              xpath_pcb_t *pcb,
              dlq_hdr_t *parmQ,
              status_t  *res)
{
    xpath_result_t  *parm1, *parm2, *parm3, *result;
    xmlChar         *p1str, *p2str, *p3str, *writestr;
    uint32           parmcnt, p1len, p2len, p3len, curpos;
    boolean          done, malloc1, malloc2, malloc3;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    malloc1 = FALSE;
    malloc2 = FALSE;
    malloc3 = FALSE;

    *res = NO_ERR;

    parm1 = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    parm2 = (xpath_result_t *)dlq_nextEntry(instance, parm1);
    parm3 = (xpath_result_t *)dlq_nextEntry(instance, parm2);

    /* check at least 3 parameters */
    parmcnt = dlq_count(instance, parmQ);
    if (parmcnt < 3)
    {
        *res = ERR_NCX_MISSING_PARM;
        return NULL;
    }

    if (parm1->restype != XP_RT_STRING) {
        *res = xpath_cvt_string(instance, pcb, parm1, &p1str);
        if (*res != NO_ERR) {
            return NULL;
        }
        malloc1 = TRUE;
    } else {
        p1str = parm1->r.str;
    }
    p1len = xml_strlen(instance, p1str);

    if (parm2->restype != XP_RT_STRING) {
        *res = xpath_cvt_string(instance, pcb, parm2, &p2str);
        if (*res != NO_ERR) {
            if (malloc1) {
                m__free(instance, p1str);
            }
            return NULL;
        }
        malloc2 = TRUE;
    } else {
        p2str = parm2->r.str;
    }
    p2len = xml_strlen(instance, p2str);

    if (parm3->restype != XP_RT_STRING) {
        *res = xpath_cvt_string(instance, pcb, parm3, &p3str);
        if (*res != NO_ERR) {
            if (malloc1) {
                m__free(instance, p1str);
            }
            if (malloc2) {
                m__free(instance, p2str);
            }
            return NULL;
        }
        malloc3 = TRUE;
    } else {
        p3str = parm3->r.str;
    }
    p3len = xml_strlen(instance, p3str);

    result = new_result(instance, pcb, XP_RT_STRING);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
    } else {
        result->r.str = m__getMem(instance, p1len+1);
        if (!result->r.str) {
            *res = ERR_INTERNAL_MEM;
            free_result(instance, pcb, result);
            result = NULL;
        } else {
            writestr = result->r.str;

            /* translate p1str into the result string */
            while (*p1str) {
                curpos = 0;
                done = FALSE;

                /* look for a match char in p2str */
                while (!done && curpos < p2len) {
                    if (p2str[curpos] == *p1str) {
                        done = TRUE;
                    } else {
                        curpos++;
                    }
                }

                if (done) {
                    if (curpos < p3len) {
                        /* replace p1char with p3str translation char */
                        *writestr++ = p3str[curpos];
                    } /* else drop p1char from result */
                } else {
                    /* copy p1char to result */
                    *writestr++ = *p1str;
                }

                p1str++;
            }

            *writestr = 0;
        }
    }

    if (malloc1) {
        m__free(instance, p1str);
    }
    if (malloc2) {
        m__free(instance, p2str);
    }
    if (malloc3) {
        m__free(instance, p3str);
    }

    return result;

}  /* translate_fn */


/********************************************************************
 * FUNCTION true_fn
 *
 * boolean true() function [4.3]
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == empty parmQ
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
true_fn (ncx_instance_t *instance,
         xpath_pcb_t *pcb,
         dlq_hdr_t *parmQ,
         status_t  *res)
{
    xpath_result_t *result;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    (void)parmQ;
    result = new_result(instance, pcb, XP_RT_BOOLEAN);
    if (!result) {
        *res = ERR_INTERNAL_MEM;
    } else {
        result->r.boo = TRUE;
        *res = NO_ERR;
    }
    return result;

}  /* true_fn */


/********************************************************************
 * FUNCTION module_loaded_fn
 *
 * boolean module-loaded(string [,string])
 *   parm1 == module name
 *   parm2 == optional revision-date
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 1 object to convert to boolean
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
module_loaded_fn (ncx_instance_t *instance,
                  xpath_pcb_t *pcb,
                  dlq_hdr_t *parmQ,
                  status_t  *res)
{
    xpath_result_t  *parm, *parm2, *result;
    xmlChar         *modstr, *revstr, *str1, *str2;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    *res = NO_ERR;

    modstr = NULL;
    revstr = NULL;
    str1 = NULL;
    str2 = NULL;

    if (dlq_count(instance, parmQ) > 2) {
        /* should already be reported in obj mode */
        *res = ERR_NCX_EXTRA_PARM;
        return NULL;
    }

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (parm == NULL) {
        /* should already be reported in obj mode */
        *res = ERR_NCX_MISSING_PARM;
        return NULL;
    }

    parm2 = (xpath_result_t *)dlq_nextEntry(instance, parm);

    result = new_result(instance, pcb, XP_RT_BOOLEAN);
    if (result == NULL) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }
    result->r.boo = FALSE;

    if (parm->restype != XP_RT_STRING) {
        *res = xpath_cvt_string(instance, pcb, parm, &str1);
        if (*res == NO_ERR) {
            modstr = str1;
        }
    } else {
        modstr = parm->r.str;
    }

    if (*res == NO_ERR && parm2 != NULL) {
        if (parm2->restype != XP_RT_STRING) {
            *res = xpath_cvt_string(instance, pcb, parm2, &str2);
            if (*res == NO_ERR) {
                revstr = str2;
            }
        } else {
            revstr = parm2->r.str;
        }
    }

    if (*res == NO_ERR) {
        if (ncx_valid_name2(instance, modstr)) {
            if (ncx_find_module(instance, modstr, revstr)) {
                result->r.boo = TRUE;
            }
        }
    }

    if (str1 != NULL) {
        m__free(instance, str1);
    }

    if (str2 != NULL) {
        m__free(instance, str2);
    }

    return result;

}  /* module_loaded_fn */


/********************************************************************
 * FUNCTION feature_enabled_fn
 *
 * boolean feature-enabled(string ,string)
 *   parm1 == module name
 *   parm2 == feature name
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    parmQ == parmQ with 1 object to convert to boolean
 *    res == address of return status
 *
 * OUTPUTS:
 *    *res == return status
 *
 * RETURNS:
 *    malloced xpath_result_t if no error and results being processed
 *    NULL if error
 *********************************************************************/
static xpath_result_t *
feature_enabled_fn (ncx_instance_t *instance,
                    xpath_pcb_t *pcb,
                    dlq_hdr_t *parmQ,
                    status_t  *res)
{
    xpath_result_t  *parm, *parm2, *result;
    xmlChar         *modstr, *featstr, *str1, *str2;
    ncx_module_t    *mod;
    ncx_feature_t   *feat;

    if (!pcb->val && !pcb->obj) {
        return NULL;
    }

    *res = NO_ERR;

    modstr = NULL;
    featstr = NULL;
    str1 = NULL;
    str2 = NULL;

    parm = (xpath_result_t *)dlq_firstEntry(instance, parmQ);
    if (parm == NULL) {
        /* should already be reported in obj mode */
        *res = ERR_NCX_MISSING_PARM;
        return NULL;
    }

    parm2 = (xpath_result_t *)dlq_nextEntry(instance, parm);

    result = new_result(instance, pcb, XP_RT_BOOLEAN);
    if (result == NULL) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }
    result->r.boo = FALSE;

    if (parm->restype != XP_RT_STRING) {
        *res = xpath_cvt_string(instance, pcb, parm, &str1);
        if (*res == NO_ERR) {
            modstr = str1;
        }
    } else {
        modstr = parm->r.str;
    }

    if (*res == NO_ERR) {
        if (parm2->restype != XP_RT_STRING) {
            *res = xpath_cvt_string(instance, pcb, parm2, &str2);
            if (*res == NO_ERR) {
                featstr = str2;
            }
        } else {
            featstr = parm2->r.str;
        }
    }

    if (*res == NO_ERR) {
        if (ncx_valid_name2(instance, modstr)) {
            mod = ncx_find_module(instance, modstr, NULL);
            if (mod != NULL) {
                feat = ncx_find_feature(instance, mod, featstr);
                if (feat != NULL && ncx_feature_enabled(instance, feat)) {
                    result->r.boo = TRUE;
                }
            }
        }
    }

    if (str1 != NULL) {
        m__free(instance, str1);
    }

    if (str2 != NULL) {
        m__free(instance, str2);
    }

    return result;

}  /* feature_enabled_fn */


/****************   U T I L I T Y    F U N C T I O N S   ***********/



/********************************************************************
 * FUNCTION find_fncb
 *
 * Find an XPath function control block
 *
 * INPUTS:
 *    pcb == parser control block to check
 *    name == name string to check
 *
 * RETURNS:
 *   pointer to found control block
 *   NULL if not found
 *********************************************************************/
static const xpath_fncb_t *
find_fncb (ncx_instance_t *instance,
           xpath_pcb_t *pcb,
           const xmlChar *name)
{
    const xpath_fncb_t  *fncb;
    uint32               i;

    i = 0;
    fncb = &pcb->functions[i];
    while (fncb && fncb->name) {
        if (!xml_strcmp(instance, name, fncb->name)) {
            return fncb;
        } else {
            fncb = &pcb->functions[++i];
        }
    }
    return NULL;

} /* find_fncb */


/********************************************************************
 * FUNCTION get_varbind
 *
 * Get the specified variable binding
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    prefix == prefix string of module with the varbind
 *           == NULL for current module (pcb->tkerr.mod)
 *    prefixlen == length of prefix string
 *    name == variable name string
 *     res == address of return status
 *
 * OUTPUTS:
 *   *res == resturn status
 *
 * RETURNS:
 *   pointer to found varbind, NULL if not found
 *********************************************************************/
static ncx_var_t *
get_varbind (ncx_instance_t *instance,
             xpath_pcb_t *pcb,
             const xmlChar *prefix,
             uint32 prefixlen,
             const xmlChar *name,
             status_t  *res)
{
    ncx_var_t    *var;

    var = NULL;
    *res = NO_ERR;

    /*
     * varbinds with prefixes are not supported in NETCONF
     * there is no way to define a varbind in a YANG module
     * so they do not correlate to modules.
     *
     * They are name to value node bindings in yuma
     * and the pcb needs to be pre-configured with
     * a queue of varbinds or a callback function to get the
     * requested variable binding
     */
    if (prefix && prefixlen) {
        /* no variables with prefixes allowed !!! */
        *res = ERR_NCX_DEF_NOT_FOUND;
    } else {
        /* try to find the variable */
        if (pcb->getvar_fn) {
            var = (*pcb->getvar_fn)(pcb, name, res);
        } else {
            var = var_get_que_raw(instance, &pcb->varbindQ, 0, name);
            if (!var) {
                *res = ERR_NCX_DEF_NOT_FOUND;
            }
        }
    }

    return var;

}  /* get_varbind */


/********************************************************************
 * FUNCTION match_next_token
 *
 * Match the next token in the chain with a type and possibly value
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    tktyp == token type to match
 *    tkval == string val to match (or NULL to just match tktyp)
 *
 * RETURNS:
 *   TRUE if the type and value (if non-NULL) match
 *   FALSE if next token is not a match
 *********************************************************************/
static boolean
match_next_token (ncx_instance_t *instance,
                  xpath_pcb_t *pcb,
                  tk_type_t tktyp,
                  const xmlChar *tkval)
{
    const xmlChar  *nextval;
    tk_type_t       nexttyp;

    nexttyp = tk_next_typ(instance, pcb->tkc);
    if (nexttyp == tktyp) {
        if (tkval) {
            nextval = tk_next_val(instance, pcb->tkc);
            if (nextval && !xml_strcmp(instance, tkval, nextval)) {
                return TRUE;
            } else {
                return FALSE;
            }
        } else {
            return TRUE;
        }
    } else {
        return FALSE;
    }
    /*NOTREACHED*/

}  /* match_next_token */


/********************************************************************
 * FUNCTION check_qname_prefix
 *
 * Check the prefix for the current node against the proper context
 * and report any unresolved prefix errors
 *
 * Do not check the local-name part of the QName because
 * the module is still being parsed and out of order
 * nodes will not be found yet
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    prefix == prefix string to use (may be NULL)
 *    prefixlen == length of prefix
 *    nsid == address of return NS ID (may be NULL)
 *
 * OUTPUTS:
 *   if non-NULL:
 *    *nsid == namespace ID for the prefix, if found
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
check_qname_prefix (ncx_instance_t *instance,
                    xpath_pcb_t *pcb,
                    const xmlChar *prefix,
                    uint32 prefixlen,
                    xmlns_id_t *nsid)
{
    ncx_module_t   *targmod;
    status_t        res;

    res = NO_ERR;

    if (pcb->reader) {
        res = xml_get_namespace_id(instance,
                                   pcb->reader,
                                   prefix,
                                   prefixlen,
                                   nsid);
    } else {
        res = xpath_get_curmod_from_prefix_str(instance,
                                               prefix,
                                               prefixlen,
                                               pcb->tkerr.mod,
                                               &targmod);
        if (res == NO_ERR) {
            *nsid = targmod->nsid;
        }
    }

    if (res != NO_ERR) {
        if (pcb->logerrors) {
            log_error(instance,
                      "\nError: Module for prefix '%s' not found",
                      (prefix) ? prefix : EMPTY_STRING);
            ncx_print_errormsg(instance, pcb->tkc, pcb->tkerr.mod, res);
        }
    }

    return res;

}  /* check_qname_prefix */


/********************************************************************
 * FUNCTION cvt_from_value
 *
 * Convert a val_value_t into an XPath result
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    val == value struct to convert
 *
 * RETURNS:
 *   malloced and filled in xpath_result_t or NULL if malloc error
 *********************************************************************/
static xpath_result_t *
cvt_from_value (ncx_instance_t *instance,
                xpath_pcb_t *pcb,
                val_value_t *val)
{
    xpath_result_t    *result;
    val_value_t       *newval;
    const val_value_t *useval;
    status_t           res;
    uint32             len;

    result = NULL;
    newval = NULL;
    res = NO_ERR;

    if (val_is_virtual(instance, val)) {
        newval = val_get_virtual_value(instance, NULL, val, &res);
        if (newval == NULL) {
            return NULL;
        } else {
            useval = newval;
        }
    } else {
        useval = val;
    }

    if (typ_is_string(useval->btyp)) {
        result = new_result(instance, pcb, XP_RT_STRING);
        if (result) {
            if (VAL_STR(useval)) {
                result->r.str = xml_strdup(instance, VAL_STR(useval));
            } else {
                result->r.str = xml_strdup(instance, EMPTY_STRING);
            }
            if (!result->r.str) {
                free_result(instance, pcb, result);
                result = NULL;
            }
        }
    } else if (typ_is_number(useval->btyp)) {
        result = new_result(instance, pcb, XP_RT_NUMBER);
        if (result) {
            res = ncx_cast_num(instance,
                               &useval->v.num,
                               useval->btyp,
                               &result->r.num,
                               NCX_BT_FLOAT64);
            if (res != NO_ERR) {
                free_result(instance, pcb, result);
                result = NULL;
            }
        }
    } else if (typ_is_simple(instance, useval->btyp)) {
        len = 0;
        res = val_sprintf_simval_nc(instance, NULL, useval, &len);
        if (res == NO_ERR) {
            result = new_result(instance, pcb, XP_RT_STRING);
            if (result) {
                result->r.str = m__getMem(instance, len+1);
                if (!result->r.str) {
                    free_result(instance, pcb, result);
                    result = NULL;
                } else {
                    res = val_sprintf_simval_nc(instance,
                                                result->r.str,
                                                useval,
                                                &len);
                    if (res != NO_ERR) {
                        free_result(instance, pcb, result);
                        result = NULL;
                    }
                }
            }
        }
    }

    return result;

}  /* cvt_from_value */


/********************************************************************
 * FUNCTION find_resnode
 *
 * Check if the specified resnode ptr is already in the Q
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    resultQ == Q of xpath_resnode_t structs to check
 *               DOES NOT HAVE TO BE WITHIN A RESULT NODE Q
 *    ptr   == pointer value to find
 *
 * RETURNS:
 *    found resnode or NULL if not found
 *********************************************************************/
static xpath_resnode_t *
find_resnode (ncx_instance_t *instance,
              xpath_pcb_t *pcb,
              dlq_hdr_t *resultQ,
              const void *ptr)
{
    xpath_resnode_t  *resnode;
    (void)instance;

    for (resnode = (xpath_resnode_t *)dlq_firstEntry(instance, resultQ);
         resnode != NULL;
         resnode = (xpath_resnode_t *)dlq_nextEntry(instance, resnode)) {

        if (pcb->val) {
            if (resnode->node.valptr == ptr) {
                return resnode;
            }
        } else {
            if (resnode->node.objptr == ptr) {
                return resnode;
            }
        }
    }
    return NULL;

}  /* find_resnode */


/********************************************************************
 * FUNCTION merge_nodeset
 *
 * Add the nodes from val1 into val2
 * that are not already there
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    result == address of return XPath result nodeset
 *
 * OUTPUTS:
 *    result->nodeQ contents adjusted
 *
 *********************************************************************/
static void
merge_nodeset (ncx_instance_t *instance,
               xpath_pcb_t *pcb,
               xpath_result_t *val1,
               xpath_result_t *val2)
{
    xpath_resnode_t        *resnode, *findnode;

    if (!pcb->val && !pcb->obj) {
        return;
    }

    if (val1->restype != XP_RT_NODESET ||
        val2->restype != XP_RT_NODESET) {
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return;
    }

    while (!dlq_empty(instance, &val1->r.nodeQ)) {
        resnode = (xpath_resnode_t *)
                  dlq_deque(instance, &val1->r.nodeQ);

        if (pcb->val) {
            findnode = find_resnode(instance,
                                    pcb,
                                    &val2->r.nodeQ,
                                    resnode->node.valptr);
        } else {
            findnode = find_resnode(instance,
                                    pcb,
                                    &val2->r.nodeQ,
                                    resnode->node.objptr);
        }
        if (findnode) {
            if (resnode->dblslash) {
                findnode->dblslash = TRUE;
            }
            findnode->position = resnode->position;
            free_resnode(instance, pcb, resnode);
        } else {
            dlq_enque(instance, resnode, &val2->r.nodeQ);
        }
    }

}  /* merge_nodeset */


/********************************************************************
 * FUNCTION set_nodeset_dblslash
 *
 * Set the current nodes dblslash flag
 * to indicate that all unchecked descendants
 * are also represented by this node, for
 * further node-test or predicate testing
 *
 * INPUTS:
 *    pcb == parser control block to use
 *    result == address of return XPath result nodeset
 *
 * OUTPUTS:
 *    result->nodeQ contents adjusted
 *
 *********************************************************************/
static void
set_nodeset_dblslash (rwdts_xpath_pcb_t *rwpcb,
                      xpath_pcb_t *pcb,
                      xpath_result_t *result)
{
    xpath_resnode_t        *resnode;
    ncx_instance_t *instance = (ncx_instance_t *)rwpcb->lib_inst;
    (void)instance;

    if (!pcb->val && !pcb->obj) {
        return;
    }

    for (resnode = (xpath_resnode_t *)
                   dlq_firstEntry(instance, &result->r.nodeQ);
         resnode != NULL;
         resnode = (xpath_resnode_t *)dlq_nextEntry(instance, resnode)) {

        resnode->dblslash = TRUE;
    }

}  /* set_nodeset_dblslash */


/********************************************************************
 * FUNCTION value_walker_fn
 *
 * Check the current found value node, based on the
 * criteria passed to the search function
 *
 * Matches val_walker_fn_t template in val.h
 *
 * INPUTS:
 *    val == value node found in the search
 *    cookie1 == xpath_pcb_t * : parser control block to use
 *    cookie2 == xpath_walkerparms_t *: walker parms to use
 *
 * OUTPUTS:
 *    result->nodeQ contents adjusted or replaced
 *
 * RETURNS:
 *    TRUE to keep walk going
 *    FALSE to terminate walk
 *********************************************************************/
static boolean
value_walker_fn (ncx_instance_t *instance,
                 val_value_t *val,
                 void *cookie1,
                 void *cookie2)
{
    xpath_pcb_t          *pcb;
    xpath_walkerparms_t  *parms;
    xpath_resnode_t      *newresnode;
    val_value_t          *child;
    int64                 position;
    boolean               done;

    pcb = (xpath_pcb_t *)cookie1;
    parms = (xpath_walkerparms_t *)cookie2;

    /* check if this node is already in the result */
    if (find_resnode(instance, pcb, parms->resnodeQ, val)) {
        return TRUE;
    }

    if (obj_is_root(val->obj) || val->parent==NULL) {
        position = 1;
    } else {
        position = 0;
        done = FALSE;
        for (child = val_get_first_child(instance, val->parent);
             child != NULL && !done;
             child = val_get_next_child(instance, child)) {
            position++;
            if (child == val) {
                done = TRUE;
            }
        }
    }

    ++parms->callcount;

    /* need to add this node */
    newresnode = new_val_resnode(instance,
                                 pcb,
                                 position,
                                 FALSE,
                                 val);
    if (!newresnode) {
        parms->res = ERR_INTERNAL_MEM;
        return FALSE;
    }

    dlq_enque(instance, newresnode, parms->resnodeQ);
    return TRUE;

}  /* value_walker_fn */


/********************************************************************
 * FUNCTION object_walker_fn
 *
 * Check the current found object node, based on the
 * criteria passed to the search function
 *
 * Matches obj_walker_fn_t template in obj.h
 *
 * INPUTS:
 *    val == value node found in the search
 *    cookie1 == xpath_pcb_t * : parser control block to use
 *    cookie2 == xpath_walkerparms_t *: walker parms to use
 *
 * OUTPUTS:
 *    result->nodeQ contents adjusted or replaced
 *
 * RETURNS:
 *    TRUE to keep walk going
 *    FALSE to terminate walk
 *********************************************************************/
static boolean
object_walker_fn (ncx_instance_t *instance,
                  obj_template_t *obj,
                  void *cookie1,
                  void *cookie2)
{
    xpath_pcb_t          *pcb;
    xpath_walkerparms_t  *parms;
    xpath_resnode_t      *newresnode;

    pcb = (xpath_pcb_t *)cookie1;
    parms = (xpath_walkerparms_t *)cookie2;

    /* check if this node is already in the result */
    if (find_resnode(instance, pcb, parms->resnodeQ, obj)) {
        return TRUE;
    }

    /* need to add this child node
     * the position is wrong but it will not really matter
     */
    newresnode = new_obj_resnode(instance,
                                 pcb,
                                 ++parms->callcount,
                                 FALSE,
                                 obj);
    if (!newresnode) {
        parms->res = ERR_INTERNAL_MEM;
        return FALSE;
    }

    dlq_enque(instance, newresnode, parms->resnodeQ);
    return TRUE;

}  /* object_walker_fn */


/********************************************************************
 * FUNCTION set_nodeset_self
 *
 * Check the current result nodeset and keep each
 * node, or remove it if filter tests fail
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    result == address of return XPath result nodeset
 *    nsid == 0 if any namespace is OK
 *              else only this namespace will be checked
 *    name == name of parent to find
 *         == NULL to find any parent name
 *    textmode == TRUE if just selecting text() nodes
 *                FALSE if ignored
 *
 * OUTPUTS:
 *    result->nodeQ contents adjusted or removed
 *
 * RETURNS:
 *    status
 *********************************************************************/
static status_t
set_nodeset_self (rwdts_xpath_pcb_t *rwpcb,
                  xpath_pcb_t *pcb,
                  xpath_result_t *result,
                  xmlns_id_t nsid,
                  const xmlChar *name,
                  boolean textmode)
{
    xpath_resnode_t        *resnode, *findnode;
    obj_template_t         *testobj;
    const xmlChar           *modname;
    val_value_t            *testval;
    boolean                 keep, cfgonly, fnresult, fncalled, useroot;
    dlq_hdr_t               resnodeQ;
    status_t                res;
    xpath_walkerparms_t     walkerparms;
    ncx_instance_t *instance = (ncx_instance_t *)rwpcb->lib_inst;

    if (!pcb->val && !pcb->obj) {
        return NO_ERR;
    }

    if (dlq_empty(instance, &result->r.nodeQ)) {
        return NO_ERR;
    }

    dlq_createSQue(instance, &resnodeQ);

    walkerparms.resnodeQ = &resnodeQ;
    walkerparms.res = NO_ERR;
    walkerparms.callcount = 0;

    modname = (nsid) ? xmlns_get_module(instance, nsid) : NULL;
    useroot = (pcb->flags & XP_FL_USEROOT) ? TRUE : FALSE;

    res = NO_ERR;

    /* the resnodes need to be deleted or moved to a tempQ
     * to correctly track duplicates and remove them
     */
    while (!dlq_empty(instance, &result->r.nodeQ) && res == NO_ERR) {

        resnode = (xpath_resnode_t *)dlq_deque(instance, &result->r.nodeQ);

        if (resnode->dblslash) {
            if (pcb->val) {
                testval = resnode->node.valptr;
                cfgonly = obj_is_config(instance, testval->obj);

                fnresult =
                        val_find_all_descendants(instance,
                                                 value_walker_fn,
                                                 pcb,
                                                 &walkerparms,
                                                 testval,
                                                 modname,
                                                 name,
                                                 cfgonly,
                                                 textmode,
                                                 TRUE,
                                                 FALSE);
            } else {
                testobj = resnode->node.objptr;
                cfgonly = obj_is_config(instance, testobj);

                fnresult =
                        obj_find_all_descendants(instance,
                                                 pcb->objmod,
                                                 object_walker_fn,
                                                 pcb,
                                                 &walkerparms,
                                                 testobj,
                                                 modname,
                                                 name,
                                                 cfgonly,
                                                 textmode,
                                                 useroot,
                                                 TRUE,
                                                 &fncalled);
            }

            if (!fnresult || walkerparms.res != NO_ERR) {
                res = walkerparms.res;
            }
            free_resnode(instance, pcb, resnode);
        } else {
            keep = FALSE;

            if (pcb->val) {
                testval = resnode->node.valptr;

                if (textmode) {
                    if (obj_has_text_content(instance, testval->obj)) {
                        keep = TRUE;
                    }
                } else if (modname && name) {
                    if (!xml_strcmp(instance,
                                    modname,
                                    val_get_mod_name(instance, testval)) &&
                        !xml_strcmp(instance, name, testval->name)) {
                        keep = TRUE;
                    }

                } else if (modname) {
                    if (!xml_strcmp(instance,
                                    modname,
                                    val_get_mod_name(instance, testval))) {
                        keep = TRUE;
                    }
                } else if (name) {
                    if (!xml_strcmp(instance, name, testval->name)) {
                        keep = TRUE;
                    }
                } else {
                    keep = TRUE;
                }

                if (keep) {
                    findnode = find_resnode(instance,
                                            pcb,
                                            &resnodeQ,
                                            testval);
                    if (findnode) {
                        if (resnode->dblslash) {
                            findnode->dblslash = TRUE;
                            findnode->position =
                                    ++walkerparms.callcount;
                        }
                        free_resnode(instance, pcb, resnode);
                    } else {
                        /* set the resnode to its parent */
                        resnode->node.valptr = testval;
                        resnode->position =
                                ++walkerparms.callcount;
                        dlq_enque(instance, resnode, &resnodeQ);
                    }
                } else {
                    free_resnode(instance, pcb, resnode);
                }
            } else {
                testobj = resnode->node.objptr;

                if (textmode) {
                    if (obj_has_text_content(instance, testobj)) {
                        keep = TRUE;
                    }
                } else if (modname && name) {
                    if (!xml_strcmp(instance,
                                    modname,
                                    obj_get_mod_name(instance, testobj)) &&
                        !xml_strcmp(instance, name, obj_get_name(instance, testobj))) {
                        keep = TRUE;
                    }
                } else if (modname) {
                    if (!xml_strcmp(instance,
                                    modname,
                                    obj_get_mod_name(instance, testobj))) {
                        keep = TRUE;
                    }
                } else if (name) {
                    if (!xml_strcmp(instance, name, obj_get_name(instance, testobj))) {
                        keep = TRUE;
                    }
                } else {
                    keep = TRUE;
                }

                if (keep) {
                    findnode = find_resnode(instance,
                                            pcb,
                                            &resnodeQ,
                                            testobj);
                    if (findnode) {
                        if (resnode->dblslash) {
                            findnode->dblslash = TRUE;
                            findnode->position =
                                    ++walkerparms.callcount;
                        }
                        free_resnode(instance, pcb, resnode);
                    } else {
                        /* set the resnode to its parent */
                        resnode->node.objptr = testobj;
                        resnode->position =
                                ++walkerparms.callcount;
                        dlq_enque(instance, resnode, &resnodeQ);
                    }
                } else {
                    if (pcb->logerrors &&
                        ncx_warning_enabled(instance, ERR_NCX_NO_XPATH_NODES)) {
                        log_warn(instance,
                                 "\nWarning: no self node found "
                                 "in XPath expr '%s'",
                                 pcb->exprstr);
                        ncx_print_errormsg(instance,
                                           pcb->tkc,
                                           pcb->objmod,
                                           ERR_NCX_NO_XPATH_NODES);
                    } else if (pcb->objmod != NULL) {
                        ncx_inc_warnings(pcb->objmod);
                    }
                    free_resnode(instance, pcb, resnode);
                }
            }
        }
    }

    /* put the resnode entries back where they belong */
    if (!dlq_empty(instance, &resnodeQ)) {
        dlq_block_enque(instance, &resnodeQ, &result->r.nodeQ);
    }

    result->last = walkerparms.callcount;

    return res;

}  /* set_nodeset_self */


/********************************************************************
 * FUNCTION set_nodeset_parent
 *
 * Check the current result nodeset and move each
 * node to its parent, or remove it if no parent
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    result == address of return XPath result nodeset
 *    nsid == 0 if any namespace is OK
 *              else only this namespace will be checked
 *    name == name of parent to find
 *         == NULL to find any parent name
 *
 * OUTPUTS:
 *    result->nodeQ contents adjusted or removed
 *
 * RETURNS:
 *    status
 *********************************************************************/
static status_t
set_nodeset_parent (rwdts_xpath_pcb_t *rwpcb,
                    xpath_pcb_t *pcb,
                    xpath_result_t *result,
                    xmlns_id_t nsid,
                    const xmlChar *name)
{
    xpath_resnode_t        *resnode, *findnode;
    obj_template_t         *testobj, *useobj;
    const xmlChar           *modname;
    val_value_t            *testval;
    boolean                 keep, cfgonly, fnresult, fncalled, useroot;
    dlq_hdr_t               resnodeQ;
    status_t                res;
    xpath_walkerparms_t     walkerparms;
    int64                   position;
    ncx_instance_t *instance = (ncx_instance_t *)rwpcb->lib_inst;


    if (!pcb->val && !pcb->obj) {
        return NO_ERR;
    }

    if (dlq_empty(instance, &result->r.nodeQ)) {
        return NO_ERR;
    }

    dlq_createSQue(instance, &resnodeQ);

    walkerparms.resnodeQ = &resnodeQ;
    walkerparms.res = NO_ERR;
    walkerparms.callcount = 0;

    modname = (nsid) ? xmlns_get_module(instance, nsid) : NULL;
    useroot = (pcb->flags & XP_FL_USEROOT) ? TRUE : FALSE;

    res = NO_ERR;
    position = 0;

    /* the resnodes need to be deleted or moved to a tempQ
     * to correctly track duplicates and remove them
     */
    while (!dlq_empty(instance, &result->r.nodeQ) && res == NO_ERR) {

        resnode = (xpath_resnode_t *)dlq_deque(instance, &result->r.nodeQ);

        if (resnode->dblslash) {
            if (pcb->val) {
                testval = resnode->node.valptr;
                cfgonly = obj_is_config(instance, testval->obj);

                fnresult =
                        val_find_all_descendants(instance,
                                                 value_walker_fn,
                                                 pcb,
                                                 &walkerparms,
                                                 testval,
                                                 modname,
                                                 name,
                                                 cfgonly,
                                                 FALSE,
                                                 TRUE,
                                                 FALSE);
            } else {
                testobj = resnode->node.objptr;
                cfgonly = obj_is_config(instance, testobj);

                fnresult =
                        obj_find_all_descendants(instance,
                                                 pcb->objmod,
                                                 object_walker_fn,
                                                 pcb,
                                                 &walkerparms,
                                                 testobj,
                                                 modname,
                                                 name,
                                                 cfgonly,
                                                 FALSE,
                                                 useroot,
                                                 TRUE,
                                                 &fncalled);
            }

            if (!fnresult || walkerparms.res != NO_ERR) {
                res = walkerparms.res;
            }
            free_resnode(instance, pcb, resnode);
        } else {
            keep = FALSE;

            if (pcb->val) {
                testval = resnode->node.valptr;

                if (testval == pcb->val_docroot) {
                    if (resnode->dblslash) {
                        /* just move this node to the result */
                        resnode->position = ++position;
                        dlq_enque(instance, resnode, &resnodeQ);
                    } else {
                        /* no parent available error
                         * remove node from result
                         */
                        no_parent_warning(instance, pcb);
                        free_resnode(instance, pcb, resnode);
                    }
                } else {
                    testval = testval->parent;

                    if (modname && name) {
                        if (!xml_strcmp(instance,
                                        modname,
                                        val_get_mod_name(instance, testval)) &&
                            !xml_strcmp(instance, name, testval->name)) {
                            keep = TRUE;
                        }
                    } else if (modname) {
                        if (!xml_strcmp(instance,
                                        modname,
                                        val_get_mod_name(instance, testval))) {
                            keep = TRUE;
                        }
                    } else if (name) {
                        if (!xml_strcmp(instance, name, testval->name)) {
                            keep = TRUE;
                        }
                    } else {
                        keep = TRUE;
                    }

                    if (keep) {
                        findnode = find_resnode(instance,
                                                pcb,
                                                &resnodeQ,
                                                testval);
                        if (findnode) {
                            /* parent already in the Q
                             * remove node from result
                             */
                            if (resnode->dblslash) {
                                findnode->position = ++position;
                                findnode->dblslash = TRUE;
                            }
                            free_resnode(instance, pcb, resnode);
                        } else {
                            /* set the resnode to its parent */
                            resnode->position = ++position;
                            resnode->node.valptr = testval;
                            dlq_enque(instance, resnode, &resnodeQ);
                        }
                    } else {
                        /* no parent available error
                         * remove node from result
                         */
                        no_parent_warning(instance, pcb);
                        free_resnode(instance, pcb, resnode);
                    }
                }
            } else {
                testobj = resnode->node.objptr;
                if (testobj == pcb->docroot) {
                    resnode->position = ++position;
                    dlq_enque(instance, resnode, &resnodeQ);
                } else if (!testobj->parent) {
                    if (!resnode->dblslash && (modname || name)) {
                        no_parent_warning(instance, pcb);
                        free_resnode(instance, pcb, resnode);
                    } else {
                        /* this is a databd node */
                        findnode = find_resnode(instance,
                                                pcb,
                                                &resnodeQ,
                                                pcb->docroot);
                        if (findnode) {
                            if (resnode->dblslash) {
                                findnode->position = ++position;
                                findnode->dblslash = TRUE;
                            }
                            free_resnode(instance, pcb, resnode);
                        } else {
                            resnode->position = ++position;
                            resnode->node.objptr = pcb->docroot;
                            dlq_enque(instance, resnode, &resnodeQ);
                        }
                    }
                } else {
                    /* find a parent but not a case or choice */
                    testobj = testobj->parent;
                    while (testobj && testobj->parent &&
                           !obj_is_root(testobj->parent) &&
                           (testobj->objtype==OBJ_TYP_CHOICE ||
                            testobj->objtype==OBJ_TYP_CASE)) {
                        testobj = testobj->parent;
                    }

                    /* check if stopped on top-level choice
                     * a top-level case should not happen
                     * but check it anyway
                     */
                    if (testobj->objtype==OBJ_TYP_CHOICE ||
                        testobj->objtype==OBJ_TYP_CASE) {
                        useobj = pcb->docroot;
                    } else {
                        useobj = testobj;
                    }

                    if (modname && name) {
                        if (!xml_strcmp(instance,
                                        modname,
                                        obj_get_mod_name(instance, useobj)) &&
                            !xml_strcmp(instance, name, obj_get_name(instance, useobj))) {
                            keep = TRUE;
                        }
                    } else if (modname) {
                        if (!xml_strcmp(instance,
                                        modname,
                                        obj_get_mod_name(instance, useobj))) {
                            keep = TRUE;
                        }
                    } else if (name) {
                        if (!xml_strcmp(instance, name, obj_get_name(instance, useobj))) {
                            keep = TRUE;
                        }
                    } else {
                        keep = TRUE;
                    }

                    if (keep) {
                        /* replace this node with the useobj */
                        findnode = find_resnode(instance, pcb, &resnodeQ, useobj);
                        if (findnode) {
                            if (resnode->dblslash) {
                                findnode->position = ++position;
                                findnode->dblslash = TRUE;
                            }
                            free_resnode(instance, pcb, resnode);
                        } else {
                            resnode->node.objptr = useobj;
                            resnode->position = ++position;
                            dlq_enque(instance, resnode, &resnodeQ);
                        }
                    } else {
                        no_parent_warning(instance, pcb);
                        free_resnode(instance, pcb, resnode);
                    }
                }
            }
        }
    }

    /* put the resnode entries back where they belong */
    if (!dlq_empty(instance, &resnodeQ)) {
        dlq_block_enque(instance, &resnodeQ, &result->r.nodeQ);
    }

    result->last = (position) ? position : walkerparms.callcount;

    return res;

}  /* set_nodeset_parent */


/********************************************************************
 * FUNCTION set_nodeset_child
 *
 * Check the current result nodeset and replace
 * each node with a node for every child instead
 *
 * Handles child and descendant nodes
 *
 * If a child is specified then any node not
 * containing this child will be removed
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    result == address of return XPath result nodeset
 *    childnsid == 0 if any namespace is OK
 *                 else only this namespace will be checked
 *    childname == name of child to find
 *              == NULL to find all child nodes
 *                 In this mode, if the context object
 *                 is config=true, then config=false
 *                 children will be skipped
 *    textmode == TRUE if just selecting text() nodes
 *                FALSE if ignored
 *     axis == actual axis used
 *
 * OUTPUTS:
 *    result->nodeQ contents adjusted or replaced
 *
 * RETURNS:
 *    status
 *********************************************************************/
static status_t
set_nodeset_child (rwdts_xpath_pcb_t *rwpcb,
                   xpath_pcb_t *pcb,
                   xpath_result_t *result,
                   xmlns_id_t  childnsid,
                   const xmlChar *childname,
                   boolean textmode,
                   ncx_xpath_axis_t axis)
{
    xpath_resnode_t      *resnode;
    obj_template_t       *testobj;
    val_value_t          *testval;
    const xmlChar        *modname;
    status_t              res;
    boolean               fnresult, fncalled, cfgonly;
    boolean               orself, myorself, useroot;
    dlq_hdr_t             resnodeQ;
    xpath_walkerparms_t   walkerparms;
    ncx_instance_t *instance = (ncx_instance_t *)rwpcb->lib_inst;

    if (!pcb->val && !pcb->obj) {
        return NO_ERR;
    }

    if (dlq_empty(instance, &result->r.nodeQ)) {
        return NO_ERR;
    }

    dlq_createSQue(instance, &resnodeQ);
    res = NO_ERR;
    cfgonly = (pcb->flags & XP_FL_CONFIGONLY) ? TRUE : FALSE;
    orself = (axis == XP_AX_DESCENDANT_OR_SELF) ? TRUE : FALSE;
    useroot = (pcb->flags & XP_FL_USEROOT) ? TRUE : FALSE;

    if (childnsid) {
        modname = xmlns_get_module(instance, childnsid);
    } else {
        modname = NULL;
    }

    walkerparms.resnodeQ = &resnodeQ;
    walkerparms.res = NO_ERR;
    walkerparms.callcount = 0;

    /* the resnodes need to be deleted or moved to a tempQ
     * to correctly track duplicates and remove them
     */
    while (!dlq_empty(instance, &result->r.nodeQ) && res == NO_ERR) {

        resnode = (xpath_resnode_t *)dlq_deque(instance, &result->r.nodeQ);
        myorself = (orself || resnode->dblslash) ? TRUE : FALSE;

        /* select 1 or all children of the resnode
         * special YANG support; skip over nodes that
         * are not config nodes if they were selected
         * by the wildcard '*' operator
         */
        if (pcb->val) {

            testval = resnode->node.valptr;

            if (axis != XP_AX_CHILD || resnode->dblslash) {
                fnresult =
                        val_find_all_descendants(instance,
                                                 value_walker_fn,
                                                 pcb,
                                                 &walkerparms,
                                                 testval,
                                                 modname,
                                                 childname,
                                                 cfgonly,
                                                 textmode,
                                                 myorself,
                                                 resnode->dblslash);
                if (!fnresult || walkerparms.res != NO_ERR) {
                    res = walkerparms.res;
                }
            } else {
                fnresult =
                        val_find_all_children(instance,
                                              value_walker_fn,
                                              pcb,
                                              &walkerparms,
                                              testval,
                                              modname,
                                              childname,
                                              cfgonly,
                                              textmode);
                if (!fnresult || walkerparms.res != NO_ERR) {
                    res = walkerparms.res;
                }
            }
        } else {
            testobj = resnode->node.objptr;

            if (axis != XP_AX_CHILD || resnode->dblslash) {
                fnresult =
                        obj_find_all_descendants(instance,
                                                 pcb->objmod,
                                                 object_walker_fn,
                                                 pcb,
                                                 &walkerparms,
                                                 testobj,
                                                 modname,
                                                 childname,
                                                 cfgonly,
                                                 textmode,
                                                 useroot,
                                                 myorself,
                                                 &fncalled);
                if (!fnresult || walkerparms.res != NO_ERR) {
                    res = walkerparms.res;
                }
            } else {
                fnresult =
                        obj_find_all_children(instance,
                                              pcb->objmod,
                                              object_walker_fn,
                                              pcb,
                                              &walkerparms,
                                              testobj,
                                              modname,
                                              childname,
                                              cfgonly,
                                              textmode,
                                              useroot);
                if (!fnresult || walkerparms.res != NO_ERR) {
                    res = walkerparms.res;
                }
            }
        }

        free_resnode(instance, pcb, resnode);
    }

    /* put the resnode entries back where they belong */
    if (!dlq_empty(instance, &resnodeQ)) {
        dlq_block_enque(instance, &resnodeQ, &result->r.nodeQ);
    } else if (!pcb->val && pcb->obj) {
        if (pcb->logerrors) {
            /* hack: the val_gen_instance_id code will generate
             * instance IDs that start with /nc:rpc QName.
             * The <rpc> node is currently unsupported in
             * node searches since all YANG content starts
             * from the RPC operation node;
             * check for the <nc:rpc> node and suppress the warning
             * if this is the node that is not found  */
            if ((childnsid == 0 || childnsid == xmlns_nc_id(instance)) &&
                !xml_strcmp(instance, childname, NCX_EL_RPC)) {
                ;  // assume this is an error-path XPath string
            } else if (axis != XP_AX_CHILD) {
                res = ERR_NCX_NO_XPATH_DESCENDANT;
                if (ncx_warning_enabled(instance, res)) {
                    log_warn(instance,
                             "\nWarning: no descendant nodes found "
                             "in XPath expr '%s'",
                             pcb->exprstr);
                    ncx_print_errormsg(instance, pcb->tkc, pcb->objmod, res);
                } else if (pcb->objmod) {
                    ncx_inc_warnings(pcb->objmod);
                }
            } else {
                res = ERR_NCX_NO_XPATH_CHILD;
                if (ncx_warning_enabled(instance, res)) {
                    log_warn(instance,
                             "\nWarning: no child nodes found "
                             "in XPath expr '%s'",
                             pcb->exprstr);
                    ncx_print_errormsg(instance, pcb->tkc, pcb->objmod, res);
                } else if (pcb->objmod) {
                    ncx_inc_warnings(pcb->objmod);
                }
            }
            res = NO_ERR;
        }
    }

    result->last = walkerparms.callcount;

    return res;

}  /* set_nodeset_child */


/********************************************************************
 * FUNCTION set_nodeset_pfaxis
 *
 * Handles these axis node tests
 *
 *   preceding::*
 *   preceding-sibling::*
 *   following::*
 *   following-sibling::*
 *
 * Combinations with '//' are handled as well
 *
 * Check the current result nodeset and replace
 * each node with all the requested nodes instead,
 * based on the axis used
 *
 * The result set is changed to the selected axis
 * Since the current node is no longer going to
 * be part of the result set
 *
 * After this initial step, the desired filtering
 * is performed on each of the result nodes (if any)
 *
 * At this point, if a 'nsid' and/or 'name' is
 * specified then any selected node not
 * containing this namespace and/or node name
 * will be removed
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    result == address of return XPath result nodeset
 *    nsid == 0 if any namespace is OK
 *            else only this namespace will be checked
 *    name == name of preceding node to find
 *            == NULL to find all child nodes
 *               In this mode, if the context object
 *               is config=true, then config=false
 *               preceding nodes will be skipped
 *    textmode == TRUE if just selecting text() nodes
 *                FALSE if ignored
 *    axis == axis in use for this current step
 *
 * OUTPUTS:
 *    result->nodeQ contents adjusted or replaced
 *
 * RETURNS:
 *    status
 *********************************************************************/
static status_t
set_nodeset_pfaxis (rwdts_xpath_pcb_t *rwpcb,
                    xpath_pcb_t *pcb,
                    xpath_result_t *result,
                    xmlns_id_t  nsid,
                    const xmlChar *name,
                    boolean textmode,
                    ncx_xpath_axis_t axis)
{
    xpath_resnode_t      *resnode;
    obj_template_t       *testobj;
    val_value_t          *testval;
    const xmlChar        *modname;
    status_t              res;
    boolean               fnresult, fncalled, cfgonly, useroot;
    dlq_hdr_t             resnodeQ;
    xpath_walkerparms_t   walkerparms;
    ncx_instance_t *instance = (ncx_instance_t *)rwpcb->lib_inst;

    if (!pcb->val && !pcb->obj) {
        return NO_ERR;
    }

    if (dlq_empty(instance, &result->r.nodeQ)) {
        return NO_ERR;
    }

    dlq_createSQue(instance, &resnodeQ);
    res = NO_ERR;
    cfgonly = (pcb->flags & XP_FL_CONFIGONLY) ? TRUE : FALSE;

    modname = (nsid) ? xmlns_get_module(instance, nsid) : NULL;
    useroot = (pcb->flags & XP_FL_USEROOT) ? TRUE : FALSE;

    walkerparms.resnodeQ = &resnodeQ;
    walkerparms.res = NO_ERR;
    walkerparms.callcount = 0;

    /* the resnodes need to be deleted or moved to a tempQ
     * to correctly track duplicates and remove them
     */
    while (!dlq_empty(instance, &result->r.nodeQ) && res == NO_ERR) {

        resnode = (xpath_resnode_t *)dlq_deque(instance, &result->r.nodeQ);

        /* select 1 or all children of the resnode
         * special YANG support; skip over nodes that
         * are not config nodes if they were selected
         * by the wildcard '*' operator
         */
        if (pcb->val) {
            testval = resnode->node.valptr;

            if (axis==XP_AX_PRECEDING || axis==XP_AX_FOLLOWING) {
                fnresult =
                        val_find_all_pfaxis(instance,
                                            value_walker_fn,
                                            pcb,
                                            &walkerparms,
                                            testval,
                                            modname,
                                            name,
                                            cfgonly,
                                            resnode->dblslash,
                                            textmode,
                                            axis);
            } else {
                fnresult =
                        val_find_all_pfsibling_axis(instance,
                                                    value_walker_fn,
                                                    pcb,
                                                    &walkerparms,
                                                    testval,
                                                    modname,
                                                    name,
                                                    cfgonly,
                                                    resnode->dblslash,
                                                    textmode,
                                                    axis);

            }
            if (!fnresult || walkerparms.res != NO_ERR) {
                res = walkerparms.res;
            }
        } else {
            testobj = resnode->node.objptr;
            fnresult = obj_find_all_pfaxis(instance,
                                           pcb->objmod,
                                           object_walker_fn,
                                           pcb,
                                           &walkerparms,
                                           testobj,
                                           modname,
                                           name,
                                           cfgonly,
                                           resnode->dblslash,
                                           textmode,
                                           useroot,
                                           axis,
                                           &fncalled);
            if (!fnresult || walkerparms.res != NO_ERR) {
                res = walkerparms.res;
            }
        }
        free_resnode(instance, pcb, resnode);
    }

    /* put the resnode entries back where they belong */
    if (!dlq_empty(instance, &resnodeQ)) {
        dlq_block_enque(instance, &resnodeQ, &result->r.nodeQ);
    } else if (!pcb->val && pcb->obj) {
        res = ERR_NCX_NO_XPATH_NODES;
        if (pcb->logerrors && ncx_warning_enabled(instance, res)) {
            log_warn(instance,
                     "\nWarning: no axis nodes found "
                     "in XPath expr '%s'",
                     pcb->exprstr);
            ncx_print_errormsg(instance, pcb->tkc, pcb->objmod, res);
        } else if (pcb->objmod != NULL) {
            ncx_inc_warnings(pcb->objmod);
        }
        res = NO_ERR;
    }

    result->last = walkerparms.callcount;

    return res;

}  /* set_nodeset_pfaxis */


/********************************************************************
 * FUNCTION set_nodeset_ancestor
 *
 * Check the current result nodeset and move each
 * node to the specified ancestor, or remove it
 * if none found that matches the filter criteria
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    result == address of return XPath result nodeset
 *    nsid == 0 if any namespace is OK
 *                 else only this namespace will be checked
 *    name == name of ancestor to find
 *              == NULL to find any ancestor nodes
 *                 In this mode, if the context object
 *                 is config=true, then config=false
 *                 children will be skipped
 *    textmode == TRUE if just selecting text() nodes
 *                FALSE if ignored
 *     axis == axis in use for this current step
 *             XP_AX_ANCESTOR or XP_AX_ANCESTOR_OR_SELF
 *
 * OUTPUTS:
 *    result->nodeQ contents adjusted or removed
 *
 * RETURNS:
 *    status
 *********************************************************************/
static status_t
set_nodeset_ancestor (rwdts_xpath_pcb_t *rwpcb,
                      xpath_pcb_t *pcb,
                      xpath_result_t *result,
                      xmlns_id_t  nsid,
                      const xmlChar *name,
                      boolean textmode,
                      ncx_xpath_axis_t axis)
{
    xpath_resnode_t        *resnode, *testnode;
    obj_template_t         *testobj;
    val_value_t            *testval;
    const xmlChar          *modname;
    xpath_result_t         *dummy;
    status_t                res;
    boolean                 cfgonly, fnresult, fncalled, orself, useroot;
    dlq_hdr_t               resnodeQ;
    xpath_walkerparms_t     walkerparms;
    ncx_instance_t *instance = (ncx_instance_t *)rwpcb->lib_inst;

    if (!pcb->val && !pcb->obj) {
        return NO_ERR;
    }

    if (dlq_empty(instance, &result->r.nodeQ)) {
        return NO_ERR;
    }

    dlq_createSQue(instance, &resnodeQ);

    res = NO_ERR;
    testval = NULL;
    testobj = NULL;

    modname = (nsid) ? xmlns_get_module(instance, nsid) : NULL;

    cfgonly = (pcb->flags & XP_FL_CONFIGONLY) ? TRUE : FALSE;
    useroot = (pcb->flags & XP_FL_USEROOT) ? TRUE : FALSE;
    orself = (axis == XP_AX_ANCESTOR_OR_SELF) ? TRUE : FALSE;

    walkerparms.resnodeQ = &resnodeQ;
    walkerparms.res = NO_ERR;

    /* the resnodes need to be deleted or moved to a tempQ
     * to correctly track duplicates and remove them
     */
    while (!dlq_empty(instance, &result->r.nodeQ) && res == NO_ERR) {

        resnode = (xpath_resnode_t *)dlq_deque(instance, &result->r.nodeQ);

        walkerparms.callcount = 0;

        if (pcb->val) {
            testval = resnode->node.valptr;
            fnresult = val_find_all_ancestors(instance,
                                              value_walker_fn,
                                              pcb,
                                              &walkerparms,
                                              testval,
                                              modname,
                                              name,
                                              cfgonly,
                                              orself,
                                              textmode);
        } else {
            testobj = resnode->node.objptr;
            fnresult = obj_find_all_ancestors(instance,
                                              pcb->objmod,
                                              object_walker_fn,
                                              pcb,
                                              &walkerparms,
                                              testobj,
                                              modname,
                                              name,
                                              cfgonly,
                                              textmode,
                                              useroot,
                                              orself,
                                              &fncalled);

        }
        if (walkerparms.res != NO_ERR) {
            res = walkerparms.res;
        } else if (!fnresult) {
            res = ERR_NCX_OPERATION_FAILED;
        } else if (!walkerparms.callcount && resnode->dblslash) {
            dummy = new_result(instance, pcb, XP_RT_NODESET);
            if (!dummy) {
                res = ERR_INTERNAL_MEM;
                continue;
            }

            walkerparms.resnodeQ = &dummy->r.nodeQ;
            if (pcb->val) {
                fnresult = val_find_all_descendants(instance,
                                                    value_walker_fn,
                                                    pcb,
                                                    &walkerparms,
                                                    testval,
                                                    modname,
                                                    name,
                                                    cfgonly,
                                                    textmode,
                                                    TRUE,
                                                    FALSE);

            } else {
                fnresult = obj_find_all_descendants(instance,
                                                    pcb->objmod,
                                                    object_walker_fn,
                                                    pcb,
                                                    &walkerparms,
                                                    testobj,
                                                    modname,
                                                    name,
                                                    cfgonly,
                                                    textmode,
                                                    useroot,
                                                    TRUE,
                                                    &fncalled);
            }
            walkerparms.resnodeQ = &resnodeQ;

            if (walkerparms.res != NO_ERR) {
                res = walkerparms.res;
            } else if (!fnresult) {
                res = ERR_NCX_OPERATION_FAILED;
            } else {
                res = set_nodeset_ancestor(rwpcb,
                                           pcb,
                                           dummy,
                                           nsid,
                                           name,
                                           textmode,
                                           axis);
                while (!dlq_empty(instance, &dummy->r.nodeQ)) {
                    testnode = (xpath_resnode_t *)
                               dlq_deque(instance, &dummy->r.nodeQ);

                    /* It is assumed that testnode cannot NULL because the call
                     * to dlq_empty returned false. */
                    if (find_resnode(instance, pcb, &resnodeQ,
                                     (const void *)testnode->node.valptr)) {
                        free_resnode(instance, pcb, testnode);
                    } else {
                        dlq_enque(instance, testnode, &resnodeQ);
                    }
                }
                free_result(instance, pcb, dummy);
            }
        }

        free_resnode(instance, pcb, resnode);
    }

    /* put the resnode entries back where they belong */
    if (!dlq_empty(instance, &resnodeQ)) {
        dlq_block_enque(instance, &resnodeQ, &result->r.nodeQ);
    } else {
        res = ERR_NCX_NO_XPATH_ANCESTOR;
        if (pcb->logerrors && ncx_warning_enabled(instance, res)) {
            if (orself) {
                log_warn(instance, "\nWarning: no ancestor-or-self nodes found "
                         "in XPath expr '%s'", pcb->exprstr);
            } else {
                log_warn(instance, "\nWarning: no ancestor nodes found "
                         "in XPath expr '%s'", pcb->exprstr);
            }
            ncx_print_errormsg(instance, pcb->tkc, pcb->objmod, res);
        } else if (pcb->objmod != NULL) {
            ncx_inc_warnings(pcb->objmod);
        }
        res = NO_ERR;
    }

    result->last = walkerparms.callcount;

    return res;

}  /* set_nodeset_ancestor */


/***********   B E G I N    E B N F    F U N C T I O N S *************/


/********************************************************************
 * FUNCTION parse_node_test
 *
 * Parse the XPath NodeTest sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [7] NodeTest ::= NameTest
 *                  | NodeType '(' ')'
 *                  | 'processing-instruction' '(' Literal ')'
 *
 * [37] NameTest ::= '*'
 *                  | NCName ':' '*'
 *                  | QName
 *
 * [38] NodeType ::= 'comment'
 *                    | 'text'
 *                    | 'processing-instruction'
 *                    | 'node'
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    axis  == current axis from first part of Step
 *    result == address of pointer to result struct in progress
 *
 * OUTPUTS:
 *   *result is modified
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
parse_node_test (rwdts_xpath_pcb_t *rwpcb,
                 xpath_pcb_t *pcb,
                 ncx_xpath_axis_t axis,
                 xpath_result_t **result)
{
    const xmlChar     *name;
    tk_type_t          nexttyp;
    xpath_nodetype_t   nodetyp;
    status_t           res;
    xmlns_id_t         nsid;
    boolean            emptyresult, textmode;
    ncx_instance_t *instance = (ncx_instance_t *)rwpcb->lib_inst;

    nsid = 0;
    name = NULL;
    textmode = FALSE;

    res = TK_ADV(instance, pcb->tkc);
    if (res != NO_ERR) {
        res = ERR_NCX_INVALID_XPATH_EXPR;
        if (pcb->logerrors) {
            log_error(instance, "\nError: token expected in XPath "
                      "expression '%s'", pcb->exprstr);
            ncx_print_errormsg(instance, pcb->tkc, pcb->tkerr.mod, res);
        } else {
            /*** handle agent error ***/
        }
        return res;
    }

    /* process the tokens but not the result yet */
    switch (TK_CUR_TYP(pcb->tkc)) {
        case TK_TT_STAR:
            if (pcb->flags & XP_FL_INSTANCEID) {
                invalid_instanceid_error(instance, pcb);
                return ERR_NCX_INVALID_INSTANCEID;
            }
            break;
        case TK_TT_NCNAME_STAR:
            if (pcb->flags & XP_FL_INSTANCEID) {
                invalid_instanceid_error(instance, pcb);
                return ERR_NCX_INVALID_INSTANCEID;
            }
            /* match all nodes in the namespace w/ specified prefix */
            if (!pcb->tkc->cur->nsid) {
                res = check_qname_prefix(instance,
                                         pcb,
                                         TK_CUR_VAL(pcb->tkc),
                                         xml_strlen(instance, TK_CUR_VAL(pcb->tkc)),
                                         &pcb->tkc->cur->nsid);
            }
            if (res == NO_ERR) {
                nsid = pcb->tkc->cur->nsid;
            }
            break;
        case TK_TT_MSTRING:
            /* match all nodes in the namespace w/ specified prefix */
            if (!pcb->tkc->cur->nsid) {
                res = check_qname_prefix(instance,
                                         pcb,
                                         TK_CUR_MOD(pcb->tkc),
                                         TK_CUR_MODLEN(pcb->tkc),
                                         &pcb->tkc->cur->nsid);
            }
            if (res == NO_ERR) {
                nsid = pcb->tkc->cur->nsid;
                name = TK_CUR_VAL(pcb->tkc);
            }
            break;
        case TK_TT_TSTRING:
            /* check the ID token for a NodeType name */
            nodetyp = get_nodetype_id(instance, TK_CUR_VAL(pcb->tkc));
            if (nodetyp == XP_EXNT_NONE ||
                (tk_next_typ(instance, pcb->tkc) != TK_TT_LPAREN)) {
                name = TK_CUR_VAL(pcb->tkc);
                break;
            }

            /* get the node test left paren */
            res = xpath_parse_token(instance, pcb, TK_TT_LPAREN);
            if (res != NO_ERR) {
                return res;
            }

            /* check if a literal param can be present */
            if (nodetyp == XP_EXNT_PROC_INST) {
                /* check if a literal param is present */
                nexttyp = tk_next_typ(instance, pcb->tkc);
                if (nexttyp==TK_TT_QSTRING ||
                    nexttyp==TK_TT_SQSTRING) {
                    /* temp save the literal string */
                    res = xpath_parse_token(instance, pcb, nexttyp);
                    if (res != NO_ERR) {
                        return res;
                    }
                }
            }

            /* get the node test right paren */
            res = xpath_parse_token(instance, pcb, TK_TT_RPAREN);
            if (res != NO_ERR) {
                return res;
            }

            if (pcb->flags & XP_FL_INSTANCEID) {
                invalid_instanceid_error(instance, pcb);
                return ERR_NCX_INVALID_INSTANCEID;
            }

            /* process the result based on the node type test */
            switch (nodetyp) {
                case XP_EXNT_COMMENT:
                    /* no comments to match */
                    emptyresult = TRUE;
                    if (pcb->obj &&
                        pcb->logerrors &&
                        ncx_warning_enabled(instance, ERR_NCX_EMPTY_XPATH_RESULT)) {
                        log_warn(instance,
                                 "\nWarning: no comment nodes available in "
                                 "XPath expr '%s'",
                                 pcb->exprstr);
                        ncx_print_errormsg(instance,
                                           pcb->tkc,
                                           pcb->tkerr.mod,
                                           ERR_NCX_EMPTY_XPATH_RESULT);
                    } else if (pcb->objmod != NULL) {
                        ncx_inc_warnings(pcb->objmod);
                    }

                    break;
                case XP_EXNT_TEXT:
                    /* match all leaf of leaf-list content */
                    emptyresult = FALSE;
                    textmode = TRUE;
                    break;
                case XP_EXNT_PROC_INST:
                    /* no processing instructions to match */
                    emptyresult = TRUE;
                    if (pcb->obj &&
                        pcb->logerrors &&
                        ncx_warning_enabled(instance, ERR_NCX_EMPTY_XPATH_RESULT)) {
                        log_warn(instance,
                                 "\nWarning: no processing instruction "
                                 "nodes available in "
                                 "XPath expr '%s'",
                                 pcb->exprstr);
                        ncx_print_errormsg(instance,
                                           pcb->tkc,
                                           pcb->tkerr.mod,
                                           ERR_NCX_EMPTY_XPATH_RESULT);
                    } else if (pcb->objmod != NULL) {
                        ncx_inc_warnings(pcb->objmod);
                    }
                    break;
                case XP_EXNT_NODE:
                    /* match any node */
                    emptyresult = FALSE;
                    break;
                default:
                    emptyresult = TRUE;
                    res = SET_ERROR(instance, ERR_INTERNAL_VAL);
            }

            if (emptyresult) {
                if (*result) {
                    free_result(instance, pcb, *result);
                }
                *result = new_result(instance, pcb, XP_RT_NODESET);
                if (!*result) {
                    res = ERR_INTERNAL_MEM;
                }
                return res;
            }  /* else go on to the text() or node() test */
            break;
        default:
            /* wrong token type found */
            res = ERR_NCX_WRONG_TKTYPE;
            unexpected_error(instance, pcb);
    }

    /* do not care about result if fatal error occurred */
    if (res != NO_ERR) {
        return res;
    } else if (!pcb->val && !pcb->obj) {
        /* nothing to do in first pass except create
         * dummy result to flag that a location step
         * has already started
         */
        if (!*result) {
            *result = new_result(instance, pcb, XP_RT_NODESET);
            if (!*result) {
                res = ERR_INTERNAL_MEM;
            }
        }
        return res;
    }


    /* if we get here, then the axis and name fields are set
     * or a texttest is needed
     */
    switch (axis) {
        case XP_AX_ANCESTOR:
        case XP_AX_ANCESTOR_OR_SELF:
            if (!*result) {
                *result = new_nodeset(instance,
                                      pcb,
                                      pcb->context.node.objptr,
                                      pcb->context.node.valptr,
                                      1,
                                      FALSE);
                if (!*result) {
                    res = ERR_INTERNAL_MEM;
                }
            }
            if (res == NO_ERR) {
                res = set_nodeset_ancestor(rwpcb,
                                           pcb,
                                           *result,
                                           nsid,
                                           name,
                                           textmode,
                                           axis);
            }
            break;
        case XP_AX_ATTRIBUTE:
            /* Attribute support in XPath is TBD
             * YANG does not define them and the ncx:metadata
             * extension is not fully supported within the
             * the edit-config code yet anyway
             *
             * just set the result to the empty nodeset
             */
            if (pcb->obj &&
                pcb->logerrors &&
                ncx_warning_enabled(instance, ERR_NCX_EMPTY_XPATH_RESULT)) {
                log_warn(instance, "\nWarning: attribute axis is empty in "
                         "XPath expr '%s'", pcb->exprstr);
                ncx_print_errormsg(instance,
                                   pcb->tkc,
                                   pcb->tkerr.mod,
                                   ERR_NCX_EMPTY_XPATH_RESULT);
            } else if (pcb->objmod != NULL) {
                ncx_inc_warnings(pcb->objmod);
            }

            if (*result) {
                free_result(instance, pcb, *result);
            }
            *result = new_result(instance, pcb, XP_RT_NODESET);
            if (!*result) {
                res = ERR_INTERNAL_MEM;
            }
            break;
        case XP_AX_DESCENDANT:
        case XP_AX_DESCENDANT_OR_SELF:
        case XP_AX_CHILD:
            /* select all the child nodes of each node in
             * the result node set.
             * ALSO select all the descendant nodes of each node in
             * the result node set. (they are the same in NETCONF)
             */
            if (!*result) {
                /* first step is child::* or descendant::*    */
                *result = new_nodeset(instance,
                                      pcb,
                                      pcb->context.node.objptr,
                                      pcb->context.node.valptr,
                                      1,
                                      FALSE);
                if (!*result) {
                    res = ERR_INTERNAL_MEM;
                }
            }
            if (res == NO_ERR) {
                res = set_nodeset_child(rwpcb,
                                        pcb,
                                        *result,
                                        nsid,
                                        name,
                                        textmode,
                                        axis);
            }
            break;
        case XP_AX_FOLLOWING:
        case XP_AX_PRECEDING:
        case XP_AX_FOLLOWING_SIBLING:
        case XP_AX_PRECEDING_SIBLING:
            /* need to set the result to all the objects
             * or all instances of all value nodes
             * preceding or following the context node
             */
            if (!*result) {
                /* first step is following::* or preceding::* */
                *result = new_nodeset(instance,
                                      pcb,
                                      pcb->context.node.objptr,
                                      pcb->context.node.valptr,
                                      1,
                                      FALSE);
                if (!*result) {
                    res = ERR_INTERNAL_MEM;
                }
            }

            if (res == NO_ERR) {
                res = set_nodeset_pfaxis(rwpcb,
                                         pcb,
                                         *result,
                                         nsid,
                                         name,
                                         textmode,
                                         axis);
            }
            break;
        case XP_AX_NAMESPACE:
            /* for NETCONF purposes, there is no need to
             * provide access to namespace xmlns declarations
             * within the object or value tree
             * This can be added later!
             *
             *
             * For now, just turn the result into the empty set
             */
            if (pcb->obj &&
                pcb->logerrors &&
                ncx_warning_enabled(instance, ERR_NCX_EMPTY_XPATH_RESULT)) {
                log_warn(instance,
                         "Warning: namespace axis is empty in "
                         "XPath expr '%s'",
                         pcb->exprstr);
                ncx_print_errormsg(instance,
                                   pcb->tkc,
                                   pcb->tkerr.mod,
                                   ERR_NCX_EMPTY_XPATH_RESULT);
            } else if (pcb->objmod != NULL) {
                ncx_inc_warnings(pcb->objmod);
            }

            if (*result) {
                free_result(instance, pcb, *result);
            }
            *result = new_result(instance, pcb, XP_RT_NODESET);
            if (!*result) {
                res = ERR_INTERNAL_MEM;
            }
            break;
        case XP_AX_PARENT:
            /* step is parent::*  -- same as .. for nodes  */
            if (textmode) {
                if (pcb->obj &&
                    pcb->logerrors &&
                    ncx_warning_enabled(instance, ERR_NCX_EMPTY_XPATH_RESULT)) {
                    log_warn(instance,
                             "Warning: parent axis contains no text nodes in "
                             "XPath expr '%s'",
                             pcb->exprstr);
                    ncx_print_errormsg(instance,
                                       pcb->tkc,
                                       pcb->tkerr.mod,
                                       ERR_NCX_EMPTY_XPATH_RESULT);
                } else if (pcb->objmod != NULL) {
                    ncx_inc_warnings(pcb->objmod);
                }

                if (*result) {
                    free_result(instance, pcb, *result);
                }
                *result = new_result(instance, pcb, XP_RT_NODESET);
                if (!*result) {
                    res = ERR_INTERNAL_MEM;
                }
                break;
            }

            if (!*result) {
                *result = new_nodeset(instance,
                                      pcb,
                                      pcb->context.node.objptr,
                                      pcb->context.node.valptr,
                                      1,
                                      FALSE);
                if (!*result) {
                    res = ERR_INTERNAL_MEM;
                }
            }

            if (res == NO_ERR) {
                res = set_nodeset_parent(rwpcb,
                                         pcb,
                                         *result,
                                         nsid,
                                         name);
            }
            break;
        case XP_AX_SELF:
            /* keep the same context node */
            if (!*result) {
                /* first step is self::*   */
                *result = new_nodeset(instance,
                                      pcb,
                                      pcb->context.node.objptr,
                                      pcb->context.node.valptr,
                                      1,
                                      FALSE);
                if (!*result) {
                    res = ERR_INTERNAL_MEM;
                }
            }
            if (res == NO_ERR) {
                res = set_nodeset_self(rwpcb,
                                       pcb,
                                       *result,
                                       nsid,
                                       name,
                                       textmode);
            }
            break;
        case XP_AX_NONE:
        default:
            res = SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    return res;

}  /* parse_node_test */


/********************************************************************
 * FUNCTION parse_predicate
 *
 * Parse an XPath Predicate sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [8] Predicate ::= '[' PredicateExpr ']'
 * [9] PredicateExpr ::= Expr
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    result == address of result in progress to filter
 *
 * OUTPUTS:
 *   *result may be pruned based on filter matches
 *            nodes may be updated if descendants are
 *            checked and matches are found
 *
 * RETURNS:
 *   malloced result of predicate expression
 *********************************************************************/
static status_t
parse_predicate (rwdts_xpath_pcb_t *rwpcb,
                 xpath_pcb_t *pcb,
                 xpath_result_t **result)
{
    xpath_result_t  *val1, *contextset;
    xpath_resnode_t  lastcontext, *resnode, *nextnode;
    tk_token_t      *leftbrack;
    boolean          boo;
    status_t         res;
    int64            position;
    ncx_instance_t *instance = (ncx_instance_t *)rwpcb->lib_inst;

    res = xpath_parse_token(instance, pcb, TK_TT_LBRACK);
    if (res != NO_ERR) {
        return res;
    }

    boo = FALSE;
    if (pcb->val || pcb->obj) {
        leftbrack = TK_CUR(pcb->tkc);
        contextset = *result;
        if (!contextset) {
            return SET_ERROR(instance, ERR_INTERNAL_VAL);
        } else if (contextset->restype == XP_RT_NODESET) {
            if (dlq_empty(instance, &contextset->r.nodeQ)) {
                /* always one pass; do not care about result */
                val1 = parse_expr(rwpcb, pcb, &res);
                if (res == NO_ERR) {
                    res = xpath_parse_token(instance, pcb, TK_TT_RBRACK);
                }
                if (val1) {
                    free_result(instance, pcb, val1);
                }
                return res;
            }

            lastcontext.node.valptr =
                    pcb->context.node.valptr;
            lastcontext.position = pcb->context.position;
            lastcontext.last = pcb->context.last;
            lastcontext.dblslash = pcb->context.dblslash;

            for (resnode = (xpath_resnode_t *)
                           dlq_firstEntry(instance, &contextset->r.nodeQ);
                 resnode != NULL;
                 resnode = nextnode) {

                /* go over and over the predicate expression
                 * with the resnode as the current context node
                 */
                TK_CUR(pcb->tkc) = leftbrack;
                boo = FALSE;

                nextnode = (xpath_resnode_t *)
                           dlq_nextEntry(instance, resnode);

                pcb->context.node.valptr = resnode->node.valptr;
                pcb->context.position = resnode->position;
                pcb->context.last = contextset->last;
                pcb->context.dblslash = resnode->dblslash;

#ifdef EXTRA_DEBUG
                if (LOGDEBUG3 && pcb->val) {
                    log_debug3(instance,
                               "\nXPath setting context node: %s",
                               pcb->context.node.valptr->name);
                }
#endif

                int32_t path_id = rwpcb->loc_path_id;
                val1 = parse_expr(rwpcb, pcb, &res);
                if (res != NO_ERR) {
                    if (val1) {
                        free_result(instance, pcb, val1);
                    }
                    return res;
                }
                rw_status_t rs = rwdts_xpath_build_predicate(rwpcb, path_id);
                if (RW_STATUS_SUCCESS != rs) {
                    res = ERR_INTERNAL_MEM;
                }

                res = xpath_parse_token(instance, pcb, TK_TT_RBRACK);
                if (res != NO_ERR) {
                    if (val1) {
                        free_result(instance, pcb, val1);
                    }
                    return res;
                }

                if (val1->restype == XP_RT_NUMBER) {
                    /* the predicate specifies a context
                     * position and this resnode is
                     * only selected if it is the Nth
                     * instance within the current context
                     */
                    if (ncx_num_is_integral(instance,
                                            &val1->r.num,
                                            NCX_BT_FLOAT64)) {
                        position =
                                ncx_cvt_to_int64(instance,
                                                 &val1->r.num,
                                                 NCX_BT_FLOAT64);
                        /* check if the proximity position
                         * of this node matches the position
                         * value from this expression
                         */
                        boo = (position == resnode->position) ?
                              TRUE : FALSE;
                    } else {
                        boo = FALSE;
                    }
                } else {
                    boo = xpath_cvt_boolean(instance, val1);
                }
                free_result(instance, pcb, val1);

                if (!boo) {
                    /* predicate expression evaluated to false
                     * so delete this resnode from the result
                     */
                    dlq_remove(instance, resnode);
                    free_resnode(instance, pcb, resnode);
                }
            }

            pcb->context.node.valptr =
                    lastcontext.node.valptr;
            pcb->context.position = lastcontext.position;
            pcb->context.last = lastcontext.last;
            pcb->context.dblslash = lastcontext.dblslash;

#ifdef EXTRA_DEBUG
            if (LOGDEBUG3 && pcb->val) {
                log_debug3(instance,
                           "\nXPath set context node to last: %s",
                           pcb->context.node.valptr->name);
            }
#endif

        } else {
            /* result is from a primary expression and
             * is not a nodeset.  It will get cleared
             * if the predicate evaluates to false
             */
            int32_t path_id = rwpcb->loc_path_id;
            val1 = parse_expr(rwpcb, pcb, &res);
            if (res != NO_ERR) {
                if (val1) {
                    free_result(instance, pcb, val1);
                }
                return res;
            }
            rw_status_t rs = rwdts_xpath_build_predicate(rwpcb, path_id);
            if (RW_STATUS_SUCCESS != rs) {
                res = ERR_INTERNAL_MEM;
            }

            res = xpath_parse_token(instance, pcb, TK_TT_RBRACK);
            if (res != NO_ERR) {
                if (val1) {
                    free_result(instance, pcb, val1);
                }
                return res;
            }

            if (val1 && pcb->val) {
                boo = xpath_cvt_boolean(instance, val1);
            }
            if (val1) {
                free_result(instance, pcb, val1);
            }
            if (pcb->val && !boo && *result) {
                xpath_clean_result(instance, *result);
                xpath_init_result(instance, *result, XP_RT_NONE);
            }
        }
    } else {
        /* always one pass; do not care about result */
        int32_t path_id = rwpcb->loc_path_id;
        val1 = parse_expr(rwpcb, pcb, &res);
        if (res == NO_ERR) {
            res = xpath_parse_token(instance, pcb, TK_TT_RBRACK);
        }
        if (res == NO_ERR) {
            rw_status_t rs = rwdts_xpath_build_predicate(rwpcb, path_id);
            if (RW_STATUS_SUCCESS != rs) {
                res = ERR_INTERNAL_MEM;
            }
        }
        if (val1) {
            free_result(instance, pcb, val1);
        }
    }

    return res;

} /* parse_predicate */


/********************************************************************
 * FUNCTION parse_step
 *
 * Parse the XPath Step sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [4] Step  ::=  AxisSpecifier NodeTest Predicate*
 *                | AbbreviatedStep
 *
 * [12] AbbreviatedStep ::= '.'        | '..'
 *
 * [5] AxisSpecifier ::= AxisName '::'
 *                | AbbreviatedAxisSpecifier
 *
 * [13] AbbreviatedAxisSpecifier ::= '@'?
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    result == address of erturn XPath result nodeset
 *

 * OUTPUTS:
 *   *result pointer is set to malloced result struct
 *    if it is NULL, or used if it is non-NULL;
 *    MUST be a XP_RT_NODESET result

 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
parse_step (rwdts_xpath_pcb_t *rwpcb,
            xpath_pcb_t *pcb,
            xpath_result_t **result)
{
    const xmlChar    *nextval;
    tk_type_t         nexttyp, nexttyp2;
    ncx_xpath_axis_t  axis;
    status_t          res;
    ncx_instance_t *instance = (ncx_instance_t *)rwpcb->lib_inst;

    nexttyp = tk_next_typ(instance, pcb->tkc);
    axis = XP_AX_CHILD;

    /* check start token '/' or '//' */
    if (nexttyp == TK_TT_DBLFSLASH) {
        res = xpath_parse_token(instance, pcb, TK_TT_DBLFSLASH);
        if (res != NO_ERR) {
            return res;
        }

        if (pcb->flags & XP_FL_INSTANCEID) {
            invalid_instanceid_error(instance, pcb);
            return ERR_NCX_INVALID_INSTANCEID;
        }

        if (!*result) {
            *result = new_nodeset(instance,
                                  pcb,
                                  pcb->context.node.objptr,
                                  pcb->context.node.valptr,
                                  1,
                                  TRUE);
            if (!*result) {
                return ERR_INTERNAL_MEM;
            }
        }
        set_nodeset_dblslash(rwpcb, pcb, *result);
    } else if (nexttyp == TK_TT_FSLASH) {
        res = xpath_parse_token(instance, pcb, TK_TT_FSLASH);
        if (res != NO_ERR) {
            return res;
        }

        if (!*result) {
            /* this is the first call */
            *result = new_nodeset(instance,
                                  pcb,
                                  pcb->docroot,
                                  pcb->val_docroot,
                                  1,
                                  FALSE);
            if (!*result) {
                return ERR_INTERNAL_MEM;
            }

            /* check corner-case path '/' */
            if (location_path_end(instance, pcb)) {
                /* exprstr is simply docroot '/' */
                rwpcb->loc_path_id = -1;
                return NO_ERR;
            }
        }
    } else if (*result) {
        /* should not happen */
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return ERR_NCX_INVALID_XPATH_EXPR;
    }

    /* handle an abbreviated step (. or ..) or
     * handle the axis-specifier for the full form step
     */
    nexttyp = tk_next_typ(instance, pcb->tkc);
    switch (nexttyp) {
        case TK_TT_PERIOD:
            /* abbreviated step '.':
             * current target node stays the same unless
             * this is the first token in the location path,
             * then the context result needs to be initialized
             *
             * first consume the period token
             */
            res = xpath_parse_token(instance, pcb, TK_TT_PERIOD);
            if (res != NO_ERR) {
                return res;
            }

            /* first step is simply . */
            if (!*result) {
                *result = new_nodeset(instance,
                                      pcb,
                                      pcb->context.node.objptr,
                                      pcb->context.node.valptr,
                                      1,
                                      FALSE);
                if (!*result) {
                    return ERR_INTERNAL_MEM;
                }
            } /* else leave current result alone */
            return NO_ERR;
        case TK_TT_RANGESEP:
            /* abbrev step '..':
             * matches parent of current context */
            res = xpath_parse_token(instance, pcb, TK_TT_RANGESEP);
            if (res != NO_ERR) {
                return res;
            }

            if (pcb->flags & XP_FL_INSTANCEID) {
                invalid_instanceid_error(instance, pcb);
                return ERR_NCX_INVALID_INSTANCEID;
            }

            /* step is .. or  //.. */
            if (!*result) {
                /* first step is .. or //..     */
                *result = new_nodeset(instance,
                                      pcb,
                                      pcb->context.node.objptr,
                                      pcb->context.node.valptr,
                                      1,
                                      FALSE);
                if (!*result) {
                    res = ERR_INTERNAL_MEM;
                }
            }
            if (res == NO_ERR) {
                res = set_nodeset_parent(rwpcb,
                                         pcb,
                                         *result,
                                         0,
                                         NULL);
            }
            return res;
        case TK_TT_ATSIGN:
            axis = XP_AX_ATTRIBUTE;
            res = xpath_parse_token(instance, pcb, TK_TT_ATSIGN);
            if (res != NO_ERR) {
                return res;
            }

            if (pcb->flags & XP_FL_INSTANCEID) {
                invalid_instanceid_error(instance, pcb);
                return ERR_NCX_INVALID_INSTANCEID;
            }
            break;
        case TK_TT_STAR:
        case TK_TT_NCNAME_STAR:
        case TK_TT_MSTRING:
            /* set the axis to default child, hit node test */
            axis = XP_AX_CHILD;
            break;
        case TK_TT_TSTRING:
            /* check the ID token for an axis name */
            nexttyp2 = tk_next_typ2(instance, pcb->tkc);
            nextval = tk_next_val(instance, pcb->tkc);
            axis = get_axis_id(instance, nextval);
            if (axis != XP_AX_NONE && nexttyp2==TK_TT_DBLCOLON) {
                /* correct axis-name :: sequence */
                res = xpath_parse_token(instance, pcb, TK_TT_TSTRING);
                if (res != NO_ERR) {
                    return res;
                }
                res = xpath_parse_token(instance, pcb, TK_TT_DBLCOLON);
                if (res != NO_ERR) {
                    return res;
                }

                if (pcb->flags & XP_FL_INSTANCEID) {
                    invalid_instanceid_error(instance, pcb);
                    return ERR_NCX_INVALID_INSTANCEID;
                }
            } else if (axis == XP_AX_NONE && nexttyp2==TK_TT_DBLCOLON) {
                /* incorrect axis-name :: sequence */
                (void)TK_ADV(instance, pcb->tkc);
                res = ERR_NCX_INVALID_XPATH_EXPR;
                if (pcb->logerrors) {
                    log_error(instance,
                              "\nError: invalid axis name '%s' in "
                              "XPath expression '%s'",
                              (TK_CUR_VAL(pcb->tkc) != NULL) ?
                              TK_CUR_VAL(pcb->tkc) : EMPTY_STRING,
                              pcb->exprstr);
                    ncx_print_errormsg(instance, pcb->tkc, pcb->tkerr.mod, res);
                } else {
                    /*** log agent error ***/
                }
                (void)TK_ADV(instance, pcb->tkc);
                return res;
            } else {
                axis = XP_AX_CHILD;
            }
            break;
        default:
            /* wrong token type found */
            (void)TK_ADV(instance, pcb->tkc);
            res = ERR_NCX_WRONG_TKTYPE;
            unexpected_error(instance, pcb);
            return res;
    }

    /* axis or default child parsed OK, get node test */
    res = parse_node_test(rwpcb, pcb, axis, result);
    rw_status_t rs = rwdts_xpath_build_path(rwpcb, rwdts_xpath_get_path_ncx(rwpcb, *result), rwpcb->loc_path_id);
    if (RW_STATUS_SUCCESS != rs) {
       res = ERR_INTERNAL_MEM;
    }
    if (res == NO_ERR) {
        nexttyp = tk_next_typ(instance, pcb->tkc);
        while (nexttyp == TK_TT_LBRACK && res==NO_ERR) {
            res = parse_predicate(rwpcb, pcb, result);
            nexttyp = tk_next_typ(instance, pcb->tkc);
        }
    }

    return res;

}  /* parse_step */


/********************************************************************
 * FUNCTION parse_location_path
 *
 * Parse the Location-Path sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [1] LocationPath  ::=  RelativeLocationPath
 *                       | AbsoluteLocationPath
 *
 * [2] AbsoluteLocationPath ::= '/' RelativeLocationPath?
 *                     | AbbreviatedAbsoluteLocationPath
 *
 * [10] AbbreviatedAbsoluteLocationPath ::= '//' RelativeLocationPath
 *
 * [3] RelativeLocationPath ::= Step
 *                       | RelativeLocationPath '/' Step
 *                       | AbbreviatedRelativeLocationPath
 *
 * [11] AbbreviatedRelativeLocationPath ::=
 *               RelativeLocationPath '//' Step
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    result == result struct in progress or NULL if none
 *    res == address of result status
 *
 * OUTPUTS:
 *   *res == function result status
 *
 * RETURNS:
 *   pointer to malloced result struct or NULL if no
 *   result processing in effect
 *********************************************************************/
static xpath_result_t *
parse_location_path (rwdts_xpath_pcb_t *rwpcb,
                     xpath_pcb_t *pcb,
                     xpath_result_t *result,
                     status_t *res)
{
    xpath_result_t     *val1;
    tk_type_t           nexttyp;
    boolean             done;
    ncx_instance_t *instance = (ncx_instance_t *)rwpcb->lib_inst;

    val1 = result;
    rwpcb->loc_path_id = -1;

    done = FALSE;
    while (!done && *res == NO_ERR) {
        *res = parse_step(rwpcb, pcb, &val1);
        if (*res == NO_ERR) {
            nexttyp = tk_next_typ(instance, pcb->tkc);
            if (!(nexttyp == TK_TT_FSLASH ||
                  nexttyp == TK_TT_DBLFSLASH)) {
                done = TRUE;
            }
        }
    }

    return val1;

}  /* parse_location_path */


/********************************************************************
 * FUNCTION parse_function_call
 *
 * Parse an XPath FunctionCall sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [16] FunctionCall ::= FunctionName
 *                        '(' ( Argument ( ',' Argument )* )? ')'
 * [17] Argument ::= Expr
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    res == address of result status
 *
 * OUTPUTS:
 *   *res == function result status
 *
 * RETURNS:
 *   pointer to malloced result struct or NULL if no
 *   result processing in effect
 *********************************************************************/
static xpath_result_t *
parse_function_call (rwdts_xpath_pcb_t *rwpcb,
                     xpath_pcb_t *pcb,
                     status_t *res)
{
    xpath_result_t          *val1, *val2;
    const xpath_fncb_t      *fncb;
    dlq_hdr_t               parmQ;
    tk_type_t               nexttyp;
    int32                   parmcnt;
    boolean                 done;
    ncx_instance_t          *instance = (ncx_instance_t *)rwpcb->lib_inst;
    int32_t                 ids[RWDTS_XPATH_FN_MAX_PARAMS];

    val1 = NULL;
    parmcnt = 0;
    dlq_createSQue(instance, &parmQ);

    /* get the function name */
    *res = xpath_parse_token(instance, pcb, TK_TT_TSTRING);
    if (*res != NO_ERR) {
        return NULL;
    }

    if (pcb->flags & XP_FL_INSTANCEID) {
        invalid_instanceid_error(instance, pcb);
        *res = ERR_NCX_INVALID_INSTANCEID;
        return NULL;
    }

    /* find the function in the library */
    fncb = NULL;
    if (TK_CUR_VAL(pcb->tkc) != NULL) {
        fncb = find_fncb(instance, pcb, TK_CUR_VAL(pcb->tkc));
    }
    if (fncb) {
        /* get the mandatory left paren */
        *res = xpath_parse_token(instance, pcb, TK_TT_LPAREN);
        if (*res != NO_ERR) {
            return NULL;
        }

        /* get parms until a matching right paren is reached */
        nexttyp = tk_next_typ(instance, pcb->tkc);
        done = (nexttyp == TK_TT_RPAREN) ? TRUE : FALSE;
        while (!done && *res == NO_ERR) {
            val1 = parse_expr(rwpcb, pcb, res);
            if (*res == NO_ERR) {
                parmcnt++;
                if (parmcnt <= RWDTS_XPATH_FN_MAX_PARAMS) {
                    ids[parmcnt-1] = rwpcb->next_id - 1;
                }
                if (val1) {
                    dlq_enque(instance, val1, &parmQ);
                    val1 = NULL;
                }

                /* check for right paren or else should be comma */
                nexttyp = tk_next_typ(instance, pcb->tkc);
                if (nexttyp == TK_TT_RPAREN) {
                    done = TRUE;
                } else {
                    *res = xpath_parse_token(instance, pcb, TK_TT_COMMA);
                }
            }
        }

        /* get closing right paren */
        if (*res == NO_ERR) {
            *res = xpath_parse_token(instance, pcb, TK_TT_RPAREN);
        }

        /* check parameter count */
        if (fncb->parmcnt >= 0 && fncb->parmcnt != parmcnt) {
            *res = (parmcnt > fncb->parmcnt) ?
                   ERR_NCX_EXTRA_PARM : ERR_NCX_MISSING_PARM;

            if (pcb->logerrors) {
                log_error(instance,
                          "\nError: wrong number of "
                          "parameters got %d, need %d"
                          " for function '%s'",
                          parmcnt, fncb->parmcnt,
                          fncb->name);
                ncx_print_errormsg(instance, pcb->tkc, pcb->tkerr.mod, *res);
            } else {
                /*** log agent error ***/
            }
        } else {
            // Add to the expression list
            rwpcb->loc_path_id = -1;
            rw_status_t rs = rwdts_xpath_build_oper_ids(rwpcb,
                                                        rwdts_xpath_map_fn((const char*)fncb->name),
                                                        ids, parmcnt);
            if (RW_STATUS_SUCCESS != rs) {
                *res = ERR_INTERNAL_MEM;
            }
            /* make the function call */
            val1 = (*fncb->fn)(instance, pcb, &parmQ, res);

            if (LOGDEBUG3) {
                if (val1) {
                    log_debug3(instance,
                               "\nXPath fn %s result:",
                               fncb->name);
                    dump_result(instance, pcb, val1, NULL);
                    if (pcb->val && pcb->context.node.valptr->name) {
                        log_debug3(instance,
                                   "\nXPath context val name: %s",
                                   pcb->context.node.valptr->name);
                    }
                }
            }
        }
    } else {
        *res = ERR_NCX_UNKNOWN_PARM;

        if (pcb->logerrors) {
            log_error(instance,
                      "\nError: Invalid XPath function name '%s'",
                      (TK_CUR_VAL(pcb->tkc) != NULL) ?
                      TK_CUR_VAL(pcb->tkc) : EMPTY_STRING);
            ncx_print_errormsg(instance, pcb->tkc, pcb->tkerr.mod, *res);
        } else {
            /*** log agent error ***/
        }
    }

    /* clean up any function parameters */
    while (!dlq_empty(instance, &parmQ)) {
        val2 = (xpath_result_t *)dlq_deque(instance, &parmQ);
        free_result(instance, pcb, val2);
    }

    return val1;

} /* parse_function_call */


/********************************************************************
 * FUNCTION parse_primary_expr
 *
 * Parse an XPath PrimaryExpr sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [15] PrimaryExpr ::= VariableReference
 *                       | '(' Expr ')'
 *                       | Literal
 *                       | Number
 *                       | FunctionCall
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    res == address of result status
 *
 * OUTPUTS:
 *   *res == function result status
 *
 * RETURNS:
 *   pointer to malloced result struct or NULL if no
 *   result processing in effect
 *********************************************************************/
static xpath_result_t *
parse_primary_expr (rwdts_xpath_pcb_t *rwpcb,
                    xpath_pcb_t *pcb,
                    status_t *res)
{
    xpath_result_t         *val1;
    ncx_var_t              *varbind;
    const xmlChar          *errstr;
    tk_type_t               nexttyp;
    ncx_numfmt_t            numfmt;
    ncx_instance_t *instance = (ncx_instance_t *)rwpcb->lib_inst;

    val1 = NULL;
    nexttyp = tk_next_typ(instance, pcb->tkc);

    switch (nexttyp) {
        case TK_TT_VARBIND:
        case TK_TT_QVARBIND:
            *res = xpath_parse_token(instance, pcb, nexttyp);

            if (pcb->flags & XP_FL_INSTANCEID) {
                invalid_instanceid_error(instance, pcb);
                *res = ERR_NCX_INVALID_INSTANCEID;
                return NULL;
            }

            /* get QName or NCName variable reference
             * but only if this get is a real one
             */
            if (*res == NO_ERR && pcb->val) {
                if (TK_CUR_TYP(pcb->tkc) == TK_TT_VARBIND) {
                    varbind = get_varbind(instance,
                                          pcb,
                                          NULL,
                                          0,
                                          TK_CUR_VAL(pcb->tkc), res);
                    errstr = TK_CUR_VAL(pcb->tkc);
                } else {
                    varbind = get_varbind(instance,
                                          pcb,
                                          TK_CUR_MOD(pcb->tkc),
                                          TK_CUR_MODLEN(pcb->tkc),
                                          TK_CUR_VAL(pcb->tkc), res);
                    errstr = TK_CUR_MOD(pcb->tkc);
                }
                if (!varbind || *res != NO_ERR) {
                    if (pcb->logerrors) {
                        if (*res == ERR_NCX_DEF_NOT_FOUND) {
                            log_error(instance,
                                      "\nError: unknown variable binding '%s'",
                                      errstr);
                            ncx_print_errormsg(instance, pcb->tkc, pcb->tkerr.mod, *res);
                        } else {
                            log_error(instance,
                                      "\nError: error in variable binding '%s'",
                                      errstr);
                            ncx_print_errormsg(instance, pcb->tkc, pcb->tkerr.mod, *res);
                        }
                    }
                } else {
                    /* OK: found the variable binding */
                    val1 = cvt_from_value(instance, pcb, varbind->val);
                    if (!val1) {
                        *res = ERR_INTERNAL_MEM;
                    }
                }
            }
            break;
        case TK_TT_LPAREN:
            /* get ( expr ) */
            *res = xpath_parse_token(instance, pcb, TK_TT_LPAREN);
            if (*res == NO_ERR) {

                if (pcb->flags & XP_FL_INSTANCEID) {
                    invalid_instanceid_error(instance, pcb);
                    *res = ERR_NCX_INVALID_INSTANCEID;
                    return NULL;
                }

                val1 = parse_expr(rwpcb, pcb, res);
                if (*res == NO_ERR) {
                    *res = xpath_parse_token(instance, pcb, TK_TT_RPAREN);
                }
            }
            break;
        case TK_TT_DNUM:
        case TK_TT_RNUM:
            *res = xpath_parse_token(instance, pcb, nexttyp);

            if (pcb->flags & XP_FL_INSTANCEID) {
                invalid_instanceid_error(instance, pcb);
                *res = ERR_NCX_INVALID_INSTANCEID;
                return NULL;
            }

            /* get the Number token */
            if (*res == NO_ERR) {
                val1 = new_result(instance, pcb, XP_RT_NUMBER);
                if (!val1) {
                    *res = ERR_INTERNAL_MEM;
                    return NULL;
                }

                numfmt = ncx_get_numfmt(instance, TK_CUR_VAL(pcb->tkc));
                if (numfmt == NCX_NF_OCTAL) {
                    numfmt = NCX_NF_DEC;
                }
                if (numfmt == NCX_NF_DEC || numfmt == NCX_NF_REAL) {
                    *res = ncx_convert_num(instance,
                                           TK_CUR_VAL(pcb->tkc),
                                           numfmt,
                                           NCX_BT_FLOAT64,
                                           &val1->r.num);
                    rw_status_t rs = rwdts_xpath_build_expr_float(rwpcb, val1->r.num.d);
                    if (RW_STATUS_SUCCESS != rs) {
                        *res = ERR_INTERNAL_MEM;
                    }
                } else if (numfmt == NCX_NF_NONE) {
                    *res = ERR_NCX_INVALID_VALUE;
                } else {
                    *res = ERR_NCX_WRONG_NUMTYP;
                }
            }
            break;
        case TK_TT_QSTRING:             /* double quoted string */
        case TK_TT_SQSTRING:            /* single quoted string */
            /* get the literal token */
            *res = xpath_parse_token(instance, pcb, nexttyp);

            if (*res == NO_ERR) {
                val1 = new_result(instance, pcb, XP_RT_STRING);
                if (!val1) {
                    *res = ERR_INTERNAL_MEM;
                    return NULL;
                }

                if (TK_CUR_VAL(pcb->tkc) != NULL) {
                    val1->r.str = xml_strdup(instance, TK_CUR_VAL(pcb->tkc));
                } else {
                    val1->r.str = xml_strdup(instance, EMPTY_STRING);
                }
                if (!val1->r.str) {
                    *res = ERR_INTERNAL_MEM;
                    malloc_failed_error(instance, pcb);
                    xpath_free_result(instance, val1);
                    val1 = NULL;
                }
                else {
                    rw_status_t rs = rwdts_xpath_build_expr_str(rwpcb, (char*)val1->r.str);
                    if (RW_STATUS_SUCCESS != rs) {
                        *res = ERR_INTERNAL_MEM;
                    }
                }
            }
            break;
        case TK_TT_TSTRING:                    /* NCName string */
            /* get the string ID token */
            nexttyp = tk_next_typ2(instance, pcb->tkc);
            if (nexttyp == TK_TT_LPAREN) {
                val1 = parse_function_call(rwpcb, pcb, res);
            } else {
                *res = SET_ERROR(instance, ERR_INTERNAL_VAL);
            }
            break;
        case TK_TT_NONE:
            /* unexpected end of token chain */
            (void)TK_ADV(instance, pcb->tkc);
            *res = ERR_NCX_INVALID_XPATH_EXPR;
            if (pcb->logerrors) {
                log_error(instance,
                          "\nError: token expected in XPath expression '%s'",
                          pcb->exprstr);
                /* hack to get correct error token to print */
                pcb->tkc->curerr = &pcb->tkerr;
                ncx_print_errormsg(instance, pcb->tkc, pcb->tkerr.mod, *res);
            } else {
                ;  /**** log agent error ****/
            }
            break;
        default:
            /* unexpected token error */
            (void)TK_ADV(instance, pcb->tkc);
            *res = ERR_NCX_WRONG_TKTYPE;
            unexpected_error(instance, pcb);
    }

    return val1;

} /* parse_primary_expr */


/********************************************************************
 * FUNCTION parse_filter_expr
 *
 * Parse an XPath FilterExpr sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [20] FilterExpr ::= PrimaryExpr
 *                     | FilterExpr Predicate
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    res == address of result status
 *
 * OUTPUTS:
 *   *res == function result status
 *
 * RETURNS:
 *   pointer to malloced result struct or NULL if no
 *   result processing in effect
 *********************************************************************/
static xpath_result_t *
parse_filter_expr (rwdts_xpath_pcb_t *rwpcb,
                   xpath_pcb_t *pcb,
                   status_t *res)
{
    xpath_result_t  *val1, *dummy;
    tk_type_t        nexttyp;
    ncx_instance_t *instance = (ncx_instance_t *)rwpcb->lib_inst;

    val1 = parse_primary_expr(rwpcb, pcb, res);
    if (*res != NO_ERR) {
        return val1;
    }

    /* peek ahead to check the possible next chars */
    nexttyp = tk_next_typ(instance, pcb->tkc);

    if (val1) {
        while (nexttyp == TK_TT_LBRACK && *res==NO_ERR) {
            *res = parse_predicate(rwpcb, pcb, &val1);
            nexttyp = tk_next_typ(instance, pcb->tkc);
        }
    } else if (nexttyp==TK_TT_LBRACK) {
        dummy = new_result(instance, pcb, XP_RT_NODESET);
        if (dummy) {
            while (nexttyp == TK_TT_LBRACK && *res==NO_ERR) {
                *res = parse_predicate(rwpcb, pcb, &dummy);
                nexttyp = tk_next_typ(instance, pcb->tkc);
            }
            free_result(instance, pcb, dummy);
        } else {
            *res = ERR_INTERNAL_MEM;
            return NULL;
        }
    }

    return val1;

} /* parse_filter_expr */


/********************************************************************
 * FUNCTION parse_path_expr
 *
 * Parse an XPath PathExpr sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [19] PathExpr ::= LocationPath
 *                   | FilterExpr
 *                   | FilterExpr '/' RelativeLocationPath
 *                   | FilterExpr '//' RelativeLocationPath
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    res == address of result status
 *
 * OUTPUTS:
 *   *res == function result status
 *
 * RETURNS:
 *   pointer to malloced result struct or NULL if no
 *   result processing in effect
 *********************************************************************/
static xpath_result_t *
parse_path_expr (rwdts_xpath_pcb_t *rwpcb,
                 xpath_pcb_t *pcb,
                 status_t *res)
{
    xpath_result_t  *val1, *val2;
    const xmlChar   *nextval;
    tk_type_t        nexttyp, nexttyp2;
    xpath_exop_t     curop;
    ncx_instance_t *instance = (ncx_instance_t *)rwpcb->lib_inst;

    val1 = NULL;
    val2 = NULL;

    /* peek ahead to check the possible next sequence */
    nexttyp = tk_next_typ(instance, pcb->tkc);
    switch (nexttyp) {
        case TK_TT_FSLASH:                /* abs location path */
        case TK_TT_DBLFSLASH:     /* abbrev. abs location path */
        case TK_TT_PERIOD:                      /* abbrev step */
        case TK_TT_RANGESEP:                    /* abbrev step */
        case TK_TT_ATSIGN:                 /* abbrev axis name */
        case TK_TT_STAR:              /* rel, step, node, name */
        case TK_TT_NCNAME_STAR:       /* rel, step, node, name */
        case TK_TT_MSTRING:          /* rel, step, node, QName */
            return parse_location_path(rwpcb, pcb, NULL, res);
        case TK_TT_TSTRING:
            /* some sort of identifier string to check
             * get the value of the string and the following token type
             */
            nexttyp2 = tk_next_typ2(instance, pcb->tkc);
            nextval = tk_next_val(instance, pcb->tkc);

            /* check 'axis-name ::' sequence */
            if (nexttyp2==TK_TT_DBLCOLON && get_axis_id(instance, nextval)) {
                /* this is an axis name */
                return parse_location_path(rwpcb, pcb, NULL, res);
            }

            /* check 'NodeType (' sequence */
            if (nexttyp2==TK_TT_LPAREN && get_nodetype_id(instance, nextval)) {
                /* this is an nodetype name */
                return parse_location_path(rwpcb, pcb, NULL, res);
            }

            /* check not a function call, so must be a QName */
            if (nexttyp2 != TK_TT_LPAREN) {
                /* this is an NameTest QName w/o a prefix */
                return parse_location_path(rwpcb, pcb, NULL, res);
            }
            break;
        default:
            ;
    }

    /* if we get here, then a filter expression is expected */
    val1 = parse_filter_expr(rwpcb, pcb, res);

    if (*res == NO_ERR) {
        nexttyp = tk_next_typ(instance, pcb->tkc);
        switch (nexttyp) {
            case TK_TT_FSLASH:
                curop = XP_EXOP_FILTER1;
                break;
            case TK_TT_DBLFSLASH:
                curop = XP_EXOP_FILTER2;
                break;
            default:
                curop = XP_EXOP_NONE;
        }

        if (curop != XP_EXOP_NONE) {
            val2 = parse_location_path(rwpcb, pcb, val1, res);
        } else {
            val2 = val1;
        }
        val1 = NULL;
    }

    if (val1) {
        free_result(instance, pcb, val1);
    }

    return val2;

} /* parse_path_expr */


/********************************************************************
 * FUNCTION parse_union_expr
 *
 * Parse an XPath UnionExpr sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [18] UnionExpr ::= PathExpr
 *                    | UnionExpr '|' PathExpr
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    res == address of result status
 *
 * OUTPUTS:
 *   *res == function result status
 *
 * RETURNS:
 *   pointer to malloced result struct or NULL if no
 *   result processing in effect
 *********************************************************************/
static xpath_result_t *
parse_union_expr (rwdts_xpath_pcb_t *rwpcb,
                  xpath_pcb_t *pcb,
                  status_t *res)
{
    xpath_result_t   *val1, *val2;
    boolean          done;
    tk_type_t        nexttyp;
    ncx_instance_t   *instance = (ncx_instance_t *)rwpcb->lib_inst;
    uint64_t         id1;

    val1 = NULL;
    val2 = NULL;
    done = FALSE;

    while (!done && *res == NO_ERR) {
        val1 = parse_path_expr(rwpcb, pcb, res);

        if (*res == NO_ERR) {
            if (val2) {
                // Add the union expression to the expr list
                rw_status_t rs = rwdts_xpath_build_oper_2_ids(rwpcb,
                                                              RWDTS_XPATH_ACTION_EXOP_UNION,
                                                              id1, rwpcb->next_id-1);
                if (rs != RW_STATUS_SUCCESS) {
                    *res = ERR_INTERNAL_MEM;
                    return NULL;
                }

                if (pcb->val || pcb->obj) {
                    /* add all the nodes from val1 into val2
                     * that are not already present
                     */
                    merge_nodeset(instance, pcb, val1, val2);

                    if (val1) {
                        free_result(instance, pcb, val1);
                        val1 = NULL;
                    }
                }
            } else {
                val2 = val1;
                val1 = NULL;
            }

            if (*res != NO_ERR) {
                continue;
            }

            nexttyp = tk_next_typ(instance, pcb->tkc);
            if (nexttyp != TK_TT_BAR) {
                done = TRUE;
            } else {
                id1 = rwpcb->next_id - 1;
                rwpcb->loc_path_id = -1;
               *res = xpath_parse_token(instance, pcb, TK_TT_BAR);
                if (*res == NO_ERR) {
                    if (pcb->flags & XP_FL_INSTANCEID) {
                        invalid_instanceid_error(instance, pcb);
                        *res = ERR_NCX_INVALID_INSTANCEID;
                    }
                }
            }
        }
    }

    if (val1) {
        free_result(instance, pcb, val1);
    }

    return val2;

} /* parse_union_expr */


/********************************************************************
 * FUNCTION parse_unary_expr
 *
 * Parse an XPath UnaryExpr sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [27] UnaryExpr ::= UnionExpr
 *                    | '-' UnaryExpr
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    res == address of result status
 *
 * OUTPUTS:
 *   *res == function result status
 *
 * RETURNS:
 *   pointer to malloced result struct or NULL if no
 *   result processing in effect
 *********************************************************************/
static xpath_result_t *
parse_unary_expr (rwdts_xpath_pcb_t *rwpcb,
                  xpath_pcb_t *pcb,
                  status_t *res)
{
    xpath_result_t  *val1, *result;
    tk_type_t        nexttyp;
    uint32           minuscnt;
    ncx_instance_t *instance = (ncx_instance_t *)rwpcb->lib_inst;

    val1 = NULL;
    minuscnt = 0;

    nexttyp = tk_next_typ(instance, pcb->tkc);
    while (nexttyp == TK_TT_MINUS) {
        *res = xpath_parse_token(instance, pcb, TK_TT_MINUS);
        if (*res != NO_ERR) {
            return NULL;
        } else {
            nexttyp = tk_next_typ(instance, pcb->tkc);
            minuscnt++;
        }
    }

    val1 = parse_union_expr(rwpcb, pcb, res);

    if (*res == NO_ERR && (minuscnt & 1)) {
        // Add the unary expression to the expr list
        rwpcb->loc_path_id = -1;
       rw_status_t rs = rwdts_xpath_build_oper_1_id(rwpcb,
                                                     RWDTS_XPATH_ACTION_EXOP_NEGATE,
                                                     rwpcb->next_id-1);
        if (rs != RW_STATUS_SUCCESS) {
            *res = ERR_INTERNAL_MEM;
            return NULL;
        }

        if (pcb->val || pcb->obj) {
            /* odd number of negate ops requested */

            if (val1->restype == XP_RT_NUMBER) {
                val1->r.num.d *= -1;
                return val1;
            } else {
                result = new_result(instance, pcb, XP_RT_NUMBER);
                if (!result) {
                    *res = ERR_INTERNAL_MEM;
                    return NULL;
                }
                xpath_cvt_number(instance, val1, &result->r.num);
                result->r.num.d *= -1;
                free_result(instance, pcb, val1);
                return result;
            }
        }
    }

    return val1;

} /* parse_unary_expr */


/********************************************************************
 * FUNCTION parse_multiplicative_expr
 *
 * Parse an XPath MultiplicativeExpr sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [26] MultiplicativeExpr ::= UnaryExpr
 *              | MultiplicativeExpr MultiplyOperator UnaryExpr
 *              | MultiplicativeExpr 'div' UnaryExpr
 *              | MultiplicativeExpr 'mod' UnaryExpr
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    res == address of result status
 *
 * OUTPUTS:
 *   *res == function result status
 *
 * RETURNS:
 *   pointer to malloced result struct or NULL if no
 *   result processing in effect
 *********************************************************************/
static xpath_result_t *
parse_multiplicative_expr (rwdts_xpath_pcb_t *rwpcb,
                           xpath_pcb_t *pcb,
                           status_t *res)
{
    xpath_result_t   *val1, *val2, *result;
    ncx_num_t        num1, num2;
    xpath_exop_t     curop;
    boolean          done;
    tk_type_t        nexttyp;
    ncx_instance_t   *instance = (ncx_instance_t *)rwpcb->lib_inst;
    uint64_t         id1;

    val1 = NULL;
    val2 = NULL;
    result = NULL;
    curop = XP_EXOP_NONE;
    done = FALSE;

    ncx_init_num(instance, &num1);
    ncx_init_num(instance, &num2);

    while (!done && *res == NO_ERR) {
        val1 = parse_unary_expr(rwpcb, pcb, res);

        if (*res == NO_ERR) {
            if (val2) {
                // Add the multiplicative expression to the expr list
                rw_status_t rs = rwdts_xpath_build_oper_2_ids(rwpcb,
                                                              rwdts_xpath_map_oper_ncx(curop),
                                                              id1, rwpcb->next_id-1);
                if (rs != RW_STATUS_SUCCESS) {
                    *res = ERR_INTERNAL_MEM;
                    return NULL;
                }

               if (pcb->val || pcb->obj) {
                    /* val2 holds the 1st operand
                     * val1 holds the 2nd operand
                     */
                    if (val1->restype != XP_RT_NUMBER) {
                        xpath_cvt_number(instance, val1, &num1);
                    } else {
                        *res = ncx_copy_num(instance,
                                            &val1->r.num,
                                            &num1,
                                            NCX_BT_FLOAT64);
                    }

                    if (val2->restype != XP_RT_NUMBER) {
                        xpath_cvt_number(instance, val2, &num2);
                    } else {
                        *res = ncx_copy_num(instance,
                                            &val2->r.num,
                                            &num2,
                                            NCX_BT_FLOAT64);
                    }

                    if (*res == NO_ERR) {
                        result = new_result(instance, pcb, XP_RT_NUMBER);
                        if (!result) {
                            *res = ERR_INTERNAL_MEM;
                        } else {
                            switch (curop) {
                                case XP_EXOP_MULTIPLY:
                                    result->r.num.d = num2.d * num1.d;
                                    break;
                                case XP_EXOP_DIV:
                                    if (ncx_num_zero(instance,
                                                     &num2,
                                                     NCX_BT_FLOAT64)) {
                                        ncx_set_num_max(instance,
                                                        &result->r.num,
                                                        NCX_BT_FLOAT64);
                                    } else {
                                        result->r.num.d = num2.d / num1.d;
                                    }
                                    break;
                                case XP_EXOP_MOD:
                                    result->r.num.d = num2.d / num1.d;
#ifdef HAS_FLOAT
                                    result->r.num.d = trunc(result->r.num.d);
#endif
                                    break;
                                default:
                                    *res = SET_ERROR(instance, ERR_INTERNAL_VAL);
                            }
                        }
                    }
                }

                if (val1) {
                    free_result(instance, pcb, val1);
                    val1 = NULL;
                }
                if (val2) {
                    free_result(instance, pcb, val2);
                    val2 = NULL;
                }

                if (result) {
                    val2 = result;
                    result = NULL;
                }
            } else {
                val2 = val1;
                val1 = NULL;
            }

            if (*res != NO_ERR) {
                continue;
            }

            nexttyp = tk_next_typ(instance, pcb->tkc);
            switch (nexttyp) {
                case TK_TT_STAR:
                    curop = XP_EXOP_MULTIPLY;
                    break;
                case TK_TT_TSTRING:
                    if (match_next_token(instance, pcb, TK_TT_TSTRING,
                                         XP_OP_DIV)) {
                        curop = XP_EXOP_DIV;
                    } else if (match_next_token(instance,
                                                pcb,
                                                TK_TT_TSTRING,
                                                XP_OP_MOD)) {
                        curop = XP_EXOP_MOD;
                    } else {
                        done = TRUE;
                    }
                    break;
                default:
                    done = TRUE;
            }
            if (!done) {
                id1 = rwpcb->next_id - 1;
                rwpcb->loc_path_id = -1;
               *res = xpath_parse_token(instance, pcb, nexttyp);
                if (*res == NO_ERR) {
                    if (pcb->flags & XP_FL_INSTANCEID) {
                        invalid_instanceid_error(instance, pcb);
                        *res = ERR_NCX_INVALID_INSTANCEID;
                    }
                }
            }
        }
    }

    if (val1) {
        free_result(instance, pcb, val1);
    }

    ncx_clean_num(instance, NCX_BT_FLOAT64, &num1);
    ncx_clean_num(instance, NCX_BT_FLOAT64, &num2);

    return val2;

} /* parse_multiplicative_expr */


/********************************************************************
 * FUNCTION parse_additive_expr
 *
 * Parse an XPath AdditiveExpr sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [25] AdditiveExpr ::= MultiplicativeExpr
 *                | AdditiveExpr '+' MultiplicativeExpr
 *                | AdditiveExpr '-' MultiplicativeExpr
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    res == address of result status
 *
 * OUTPUTS:
 *   *res == function result status
 *
 * RETURNS:
 *   pointer to malloced result struct or NULL if no
 *   result processing in effect
 *********************************************************************/
static xpath_result_t *
parse_additive_expr (rwdts_xpath_pcb_t *rwpcb,
                     xpath_pcb_t *pcb,
                     status_t *res)
{
    xpath_result_t   *val1, *val2, *result;
    ncx_num_t        num1, num2;
    xpath_exop_t     curop;
    boolean          done;
    tk_type_t        nexttyp;
    ncx_instance_t   *instance = (ncx_instance_t *)rwpcb->lib_inst;
    uint64_t         id1;

    val1 = NULL;
    val2 = NULL;
    result = NULL;
    curop = XP_EXOP_NONE;
    done = FALSE;

    while (!done && *res == NO_ERR) {
        val1 = parse_multiplicative_expr(rwpcb, pcb, res);

        if (*res == NO_ERR) {
            if (val2) {
                // Add the additive expression to the expr list
                rw_status_t rs = rwdts_xpath_build_oper_2_ids(rwpcb,
                                                              rwdts_xpath_map_oper_ncx(curop),
                                                              id1, rwpcb->next_id-1);
                if (rs != RW_STATUS_SUCCESS) {
                    *res = ERR_INTERNAL_MEM;
                    return NULL;
                }

                /* val2 holds the 1st operand
                 * val1 holds the 2nd operand
                 */
                if (pcb->val || pcb->val) {
                    if (val1->restype != XP_RT_NUMBER) {
                        xpath_cvt_number(instance, val1, &num1);
                    } else {
                        *res = ncx_copy_num(instance,
                                            &val1->r.num,
                                            &num1,
                                            NCX_BT_FLOAT64);
                    }

                    if (val2->restype != XP_RT_NUMBER) {
                        xpath_cvt_number(instance, val2, &num2);
                    } else {
                        *res = ncx_copy_num(instance,
                                            &val2->r.num,
                                            &num2,
                                            NCX_BT_FLOAT64);
                    }

                    if (*res == NO_ERR) {
                        result = new_result(instance, pcb, XP_RT_NUMBER);
                        if (!result) {
                            *res = ERR_INTERNAL_MEM;
                        } else {
                            switch (curop) {
                                case XP_EXOP_ADD:
                                    result->r.num.d = num2.d + num1.d;
                                    break;
                                case XP_EXOP_SUBTRACT:
                                    result->r.num.d = num2.d - num1.d;
                                    break;
                                default:
                                    *res = SET_ERROR(instance, ERR_INTERNAL_VAL);
                            }
                        }
                    }
                }

                if (val1) {
                    free_result(instance, pcb, val1);
                    val1 = NULL;
                }
                if (val2) {
                    free_result(instance, pcb, val2);
                    val2 = NULL;
                }

                if (result) {
                    val2 = result;
                    result = NULL;
                }
            } else {
                val2 = val1;
                val1 = NULL;
            }

            if (*res != NO_ERR) {
                continue;
            }

            nexttyp = tk_next_typ(instance, pcb->tkc);
            switch (nexttyp) {
                case TK_TT_PLUS:
                    curop = XP_EXOP_ADD;
                    break;
                case TK_TT_MINUS:
                    curop = XP_EXOP_SUBTRACT;
                    break;
                default:
                    done = TRUE;
            }
            if (!done) {
                id1 = rwpcb->next_id - 1;
                rwpcb->loc_path_id = -1;
               *res = xpath_parse_token(instance, pcb, nexttyp);
                if (*res == NO_ERR) {
                    if (pcb->flags & XP_FL_INSTANCEID) {
                        invalid_instanceid_error(instance, pcb);
                        *res = ERR_NCX_INVALID_INSTANCEID;
                    }
                }
            }
        }
    }

    if (val1) {
        free_result(instance, pcb, val1);
    }

    ncx_clean_num(instance, NCX_BT_FLOAT64, &num1);
    ncx_clean_num(instance, NCX_BT_FLOAT64, &num2);

    return val2;

} /* parse_additive_expr */


/********************************************************************
 * FUNCTION parse_relational_expr
 *
 * Parse an XPath RelationalExpr sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [24]  RelationalExpr ::= AdditiveExpr
 *             | RelationalExpr '<' AdditiveExpr
 *             | RelationalExpr '>' AdditiveExpr
 *             | RelationalExpr '<=' AdditiveExpr
 *             | RelationalExpr '>=' AdditiveExpr
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    res == address of result status
 *
 * OUTPUTS:
 *   *res == function result status
 *
 * RETURNS:
 *   pointer to malloced result struct or NULL if no
 *   result processing in effect
 *********************************************************************/
static xpath_result_t *
parse_relational_expr (rwdts_xpath_pcb_t *rwpcb,
                       xpath_pcb_t *pcb,
                       status_t *res)
{
    xpath_result_t   *val1, *val2, *result;
    xpath_exop_t     curop;
    boolean          done, cmpresult;
    tk_type_t        nexttyp;
    ncx_instance_t   *instance = (ncx_instance_t *)rwpcb->lib_inst;
    uint64_t         id1 = 0;

    val1 = NULL;
    val2 = NULL;
    result = NULL;
    curop = XP_EXOP_NONE;
    done = FALSE;

    while (!done && *res == NO_ERR) {
        val1 = parse_additive_expr(rwpcb, pcb, res);

        if (*res == NO_ERR) {
            if (val2) {
              // Add the relational expression to the expr list
              rw_status_t rs = rwdts_xpath_build_oper_2_ids(rwpcb,
                                                            rwdts_xpath_map_oper_ncx(curop),
                                                            id1, rwpcb->next_id-1);
              if (rs != RW_STATUS_SUCCESS) {
                  *res = ERR_INTERNAL_MEM;
                  return NULL;
              }

              if (pcb->val || pcb->obj) {
                    /* val2 holds the 1st operand
                     * val1 holds the 2nd operand
                     */
                    cmpresult = compare_results(instance,
                                                pcb,
                                                val2,
                                                val1,
                                                curop,
                                                res);

                    if (*res == NO_ERR) {
                        result = new_result(instance, pcb, XP_RT_BOOLEAN);
                        if (!result) {
                            *res = ERR_INTERNAL_MEM;
                        } else {
                            result->r.boo = cmpresult;
                        }
                    }
                }

                if (val1) {
                    free_result(instance, pcb, val1);
                    val1 = NULL;
                }

                if (val2) {
                    free_result(instance, pcb, val2);
                    val2 = NULL;
                }

                if (result) {
                    val2 = result;
                    result = NULL;
                }
            } else {
                val2 = val1;
                val1 = NULL;
            }

            if (*res != NO_ERR) {
                continue;
            }

            nexttyp = tk_next_typ(instance, pcb->tkc);
            switch (nexttyp) {
                case TK_TT_LT:
                    curop = XP_EXOP_LT;
                    break;
                case TK_TT_GT:
                    curop = XP_EXOP_GT;
                    break;
                case TK_TT_LEQUAL:
                    curop = XP_EXOP_LEQUAL;
                    break;
                case TK_TT_GEQUAL:
                    curop = XP_EXOP_GEQUAL;
                    break;
                default:
                    done = TRUE;
            }
            if (!done) {
                id1 = rwpcb->next_id - 1;
                rwpcb->loc_path_id = -1;
               *res = xpath_parse_token(instance, pcb, nexttyp);
                if (*res == NO_ERR) {
                    if (pcb->flags & XP_FL_INSTANCEID) {
                        invalid_instanceid_error(instance, pcb);
                        *res = ERR_NCX_INVALID_INSTANCEID;
                    }
                }
            }
        }
    }

    if (val1) {
        free_result(instance, pcb, val1);
    }

    return val2;

}  /* parse_relational_expr */


/********************************************************************
 * FUNCTION parse_equality_expr
 *
 * Parse an XPath EqualityExpr sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [23] EqualityExpr  ::=   RelationalExpr
 *                | EqualityExpr '=' RelationalExpr
 *                | EqualityExpr '!=' RelationalExpr
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    res == address of result status
 *
 * OUTPUTS:
 *   *res == function result status
 *
 * RETURNS:
 *   pointer to malloced result struct or NULL if no
 *   result processing in effect
 *********************************************************************/
static xpath_result_t *
parse_equality_expr (rwdts_xpath_pcb_t *rwpcb,
                     xpath_pcb_t *pcb,
                     status_t *res)
{
    xpath_result_t   *val1, *val2, *result;
    xpath_exop_t     curop;
    boolean          done, cmpresult, equalsdone;
    ncx_instance_t   *instance = (ncx_instance_t *)rwpcb->lib_inst;
    uint64_t         id1 = 0;

    val1 = NULL;
    val2 = NULL;
    result = NULL;
    curop = XP_EXOP_NONE;
    equalsdone = FALSE;
    done = FALSE;

    while (!done && *res == NO_ERR) {
        val1 = parse_relational_expr(rwpcb, pcb, res);

        if (*res == NO_ERR) {
            if (val2) {
              // Add the equality expression to the expr list
              rw_status_t rs = rwdts_xpath_build_oper_2_ids(rwpcb,
                                                            rwdts_xpath_map_oper_ncx(curop),
                                                            id1, rwpcb->next_id-1);
              if (rs != RW_STATUS_SUCCESS) {
                  *res = ERR_INTERNAL_MEM;
                  return NULL;
              }

              if (pcb->val || pcb->obj) {
                    /* val2 holds the 1st operand
                     * val1 holds the 2nd operand
                     */
                    if (pcb->flags & XP_FL_INSTANCEID) {
                        *res = check_instanceid_expr(instance,
                                                     pcb,
                                                     val2,
                                                     val1);
                    }

                    if (*res == NO_ERR) {
                        cmpresult = compare_results(instance,
                                                    pcb,
                                                    val2,
                                                    val1,
                                                    curop,
                                                    res);

                        if (*res == NO_ERR) {
                            result = new_result(instance, pcb, XP_RT_BOOLEAN);
                            if (!result) {
                                *res = ERR_INTERNAL_MEM;
                            } else {
                                result->r.boo = cmpresult;
                            }
                        }
                    }
                }

                if (val1) {
                    free_result(instance, pcb, val1);
                    val1 = NULL;
                }

                if (val2) {
                    free_result(instance, pcb, val2);
                    val2 = NULL;
                }

                if (result) {
                    val2 = result;
                    result = NULL;
                }
            } else {
                val2 = val1;
                val1 = NULL;
            }

            if (*res != NO_ERR) {
                continue;
            }

            if (match_next_token(instance, pcb, TK_TT_EQUAL, NULL)) {
                id1 = rwpcb->next_id - 1;
                rwpcb->loc_path_id = -1;
               *res = xpath_parse_token(instance, pcb, TK_TT_EQUAL);
                if (*res == NO_ERR) {
                    if (equalsdone) {
                        if (pcb->flags & XP_FL_INSTANCEID) {
                            invalid_instanceid_error(instance, pcb);
                            *res = ERR_NCX_INVALID_INSTANCEID;
                        }
                    } else {
                        equalsdone = TRUE;
                    }
                }
                curop = XP_EXOP_EQUAL;
            } else if (match_next_token(instance, pcb, TK_TT_NOTEQUAL, NULL)) {
                id1 = rwpcb->next_id - 1;
                rwpcb->loc_path_id = -1;
                *res = xpath_parse_token(instance, pcb, TK_TT_NOTEQUAL);
                if (*res == NO_ERR) {
                    if (pcb->flags & XP_FL_INSTANCEID) {
                        invalid_instanceid_error(instance, pcb);
                        *res = ERR_NCX_INVALID_INSTANCEID;
                    }
                }
                curop = XP_EXOP_NOTEQUAL;
            } else {
                done = TRUE;
            }
        }
    }

    if (val1) {
        free_result(instance, pcb, val1);
    }

    return val2;

}  /* parse_equality_expr */


/********************************************************************
 * FUNCTION parse_and_expr
 *
 * Parse an XPath AndExpr sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [22] AndExpr  ::= EqualityExpr
 *                   | AndExpr 'and' EqualityExpr
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    res == address of result status
 *
 * OUTPUTS:
 *   *res == function result status
 *
 * RETURNS:
 *   pointer to malloced result struct or NULL if no
 *   result processing in effect
 *********************************************************************/
static xpath_result_t *
parse_and_expr (rwdts_xpath_pcb_t *rwpcb,
                xpath_pcb_t *pcb,
                status_t *res)
{
    xpath_result_t   *val1, *val2, *result;
    boolean          done, bool1, bool2;
    ncx_instance_t   *instance = (ncx_instance_t *)rwpcb->lib_inst;
    uint64_t         id1 = 0;

    val1 = NULL;
    val2 = NULL;
    result = NULL;
    done = FALSE;

    while (!done && *res == NO_ERR) {
        val1 = parse_equality_expr(rwpcb, pcb, res);

        if (*res == NO_ERR) {
            if (val2) {
               // Add the AND expression to the expr list
               rw_status_t rs = rwdts_xpath_build_oper_2_ids(rwpcb,
                                                             RWDTS_XPATH_ACTION_EXOP_AND,
                                                             id1, rwpcb->next_id-1);
               if (rs != RW_STATUS_SUCCESS) {
                   *res = ERR_INTERNAL_MEM;
                   return NULL;
               }

               if (pcb->val || pcb->obj) {
                    /* val2 holds the 1st operand
                     * val1 holds the 2nd operand
                     */
                    bool1 = xpath_cvt_boolean(instance, val1);
                    bool2 = xpath_cvt_boolean(instance, val2);

                    result = new_result(instance, pcb, XP_RT_BOOLEAN);
                    if (!result) {
                        *res = ERR_INTERNAL_MEM;
                    } else {
                        result->r.boo = (bool1 && bool2)
                                        ? TRUE : FALSE;
                    }
                }

                if (val1) {
                    free_result(instance, pcb, val1);
                    val1 = NULL;
                }

                if (val2) {
                    free_result(instance, pcb, val2);
                    val2 = NULL;
                }

                if (result) {
                    val2 = result;
                    result = NULL;
                }
            } else {
                val2 = val1;
                val1 = NULL;
            }

            if (*res != NO_ERR) {
                continue;
            }

            if (match_next_token(instance, pcb, TK_TT_TSTRING, XP_OP_AND)) {
                id1 = rwpcb->next_id - 1;
                rwpcb->loc_path_id = -1;
               *res = xpath_parse_token(instance, pcb, TK_TT_TSTRING);
            } else {
                done = TRUE;
            }
        }
    }

    if (val1) {
        free_result(instance, pcb, val1);
    }

    return val2;

}  /* parse_and_expr */


/********************************************************************
 * FUNCTION parse_or_expr
 *
 * Parse an XPath OrExpr sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [21] OrExpr ::= AndExpr
 *                 | OrExpr 'or' AndExpr
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    res == address of result status
 *
 * OUTPUTS:
 *   *res == function result status
 *
 * RETURNS:
 *   pointer to malloced result struct or NULL if no
 *   result processing in effect
 *********************************************************************/
static xpath_result_t *
parse_or_expr (rwdts_xpath_pcb_t *rwpcb,
               xpath_pcb_t *pcb,
               status_t *res)
{
    xpath_result_t   *val1, *val2, *result;
    boolean          done, bool1, bool2;
    ncx_instance_t   *instance = (ncx_instance_t *)rwpcb->lib_inst;
    uint64_t         id1 = 0;

    val1 = NULL;
    val2 = NULL;
    result = NULL;
    done = FALSE;

    while (!done && *res == NO_ERR) {
        val1 = parse_and_expr(rwpcb, pcb, res);

        if (*res == NO_ERR) {
            if (val2) {
                // Add the OR expression to the expr list
                rw_status_t rs = rwdts_xpath_build_oper_2_ids(rwpcb,
                                                              RWDTS_XPATH_ACTION_EXOP_OR,
                                                              id1, rwpcb->next_id-1);
                if (rs != RW_STATUS_SUCCESS) {
                    *res = ERR_INTERNAL_MEM;
                    return NULL;
                }

                if (pcb->val || pcb->obj) {
                    /* val2 holds the 1st operand
                     * val1 holds the 2nd operand
                     */
                    bool1 = xpath_cvt_boolean(instance, val1);
                    bool2 = xpath_cvt_boolean(instance, val2);

                    result = new_result(instance, pcb, XP_RT_BOOLEAN);
                    if (!result) {
                        *res = ERR_INTERNAL_MEM;
                    } else {
                        result->r.boo = (bool1 || bool2)
                                        ? TRUE : FALSE;
                    }
                }

                if (val1) {
                    free_result(instance, pcb, val1);
                    val1 = NULL;
                }

                if (val2) {
                    free_result(instance, pcb, val2);
                    val2 = NULL;
                }

                if (result) {
                    val2 = result;
                    result = NULL;
                }
            } else {
                val2 = val1;
                val1 = NULL;
            }

            if (*res != NO_ERR) {
                continue;
            }

            if (match_next_token(instance, pcb, TK_TT_TSTRING, XP_OP_OR)) {
                // Track the expr id for the first part of the OR expr
                id1 = rwpcb->next_id-1;
                rwpcb->loc_path_id = -1;
                *res = xpath_parse_token(instance, pcb, TK_TT_TSTRING);
            } else {
                done = TRUE;
            }
        }
    }

    if (val1) {
        free_result(instance, pcb, val1);
    }

    return val2;

}  /* parse_or_expr */


/********************************************************************
 * FUNCTION parse_expr
 *
 * Parse an XPath Expr sequence
 * It has already been tokenized
 *
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 *
 * [14] Expr ::= OrExpr
 *
 * INPUTS:
 *    pcb == parser control block in progress
 *    res == address of result status
 *
 * OUTPUTS:
 *   *res == function result status
 *
 * RETURNS:
 *   pointer to malloced result struct or NULL if no
 *   result processing in effect
 *********************************************************************/
static xpath_result_t *
parse_expr (rwdts_xpath_pcb_t *rwpcb,
            xpath_pcb_t *pcb,
            status_t  *res)
{

    return parse_or_expr(rwpcb, pcb, res);

}  /* parse_expr */



/************    E X T E R N A L   F U N C T I O N S    ************/


/********************************************************************
 * FUNCTION rwdts_ncx_xpath1_parse_expr
 *
 * Parse the XPATH 1.0 expression string.
 *
 * parse initial expr with YANG prefixes: must/when
 * the object is left out in case it is in a grouping
 *
 * This is just a first pass done when the
 * XPath string is consumed.  If this is a
 * YANG file source then the prefixes will be
 * checked against the 'mod' import Q
 *
 * The expression is parsed into XPath tokens
 * and checked for well-formed syntax and function
 * invocations. Any variable
 *
 * INPUTS:
 *    rwpcb == DTS pcb with ncx instance
 *    xpath == expression to parse
 *
 * OUTPUTS:
 *   rwpcb->pcb->tkc is filled and then partially validated
 *   rwpcb->pcb->parseres is set
 *
 * RETURNS:
 *   status
 *********************************************************************/
rw_status_t
rwdts_ncx_xpath1_parse_expr(rwdts_xpath_pcb_t *rwpcb,
                             const xmlChar *xpath)
{
    RW_ASSERT(rwpcb);
    RW_ASSERT(rwpcb->lib_inst);
    ncx_instance_t *instance = (ncx_instance_t*)rwpcb->lib_inst;
    xpath_pcb_t *pcb = (xpath_pcb_t*)rwpcb->pcb;
    xpath_result_t  *result = NULL;
    status_t         res;
    uint32           linenum, linepos;


    if (!pcb) {
        RW_ASSERT(xpath);
        pcb = xpath_new_pcb(instance, xpath, NULL);
        RW_ASSERT(pcb);
        rwpcb->pcb = pcb;
        pcb->tkc = tk_tokenize_xpath_string(instance,
                                            NULL,
                                            pcb->exprstr,
                                            linenum,
                                            linepos,
                                            &res);
        if (!pcb->tkc || res != NO_ERR) {
            log_error(instance,
                      "\nError: Invalid XPath string '%s'",
                      pcb->exprstr);
            ncx_print_errormsg(instance, pcb->tkc, NULL, res);
            return rwdts_ncx_status(res);
        }

        pcb->functions = functions;
        //return SET_ERROR(instance, ERR_INTERNAL_PTR);
    }


    /* the module that contains the XPath expr is the one
     * that will always be used to resolve prefixes
     */
    obj_template_t *obj = ncx_get_gen_root(instance);
    RW_ASSERT(obj);
    pcb->tkerr.mod = NULL;
    pcb->source = XP_SRC_SCHEMA_INSTANCEID;
    pcb->logerrors = TRUE;
    pcb->obj = obj;
    pcb->objmod = obj->tkerr.mod;
    pcb->docroot = NULL;
    pcb->doctype = XP_DOC_NONE;
    pcb->val = NULL;
    pcb->val_docroot = NULL;
    pcb->context.node.objptr = NULL;
    pcb->orig_context.node.objptr = NULL;
    pcb->parseres = NO_ERR;

    boolean rootdone = FALSE;
    if (obj_is_root(obj) ||
        obj_is_data_db(instance, obj) ||
        obj_is_cli(instance, obj)) {
        rootdone = TRUE;
        pcb->doctype = XP_DOC_DATABASE;
        pcb->docroot = ncx_get_gen_root(instance);
        if (!pcb->docroot) {
            return SET_ERROR(instance, ERR_INTERNAL_VAL);
        }
    } else if (obj_in_notif(obj)) {
        pcb->doctype = XP_DOC_NOTIFICATION;
    } else if (obj_in_rpc(instance, obj)) {
        pcb->doctype = XP_DOC_RPC;
    } else if (obj_in_rpc_reply(instance, obj)) {
        pcb->doctype = XP_DOC_RPC_REPLY;
    } else {
        return RW_STATUS_FAILURE; //SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    if (!rootdone) {
        /* get the rpc/input, rpc/output, or /notif node */
        obj_template_t *rootobj = obj;
        while (rootobj->parent && !obj_is_root(rootobj->parent) &&
               rootobj->objtype != OBJ_TYP_RPCIO) {
            rootobj = rootobj->parent;
        }
        pcb->docroot = rootobj;
    }


    result = parse_expr(rwpcb, pcb, &pcb->parseres);

    if (pcb->parseres == NO_ERR && pcb->tkc->cur) {
        res = TK_ADV(instance, pcb->tkc);
        if (res == NO_ERR) {
            /* should not get more tokens at this point
             * have something like '7 + 4 4 + 7'
             * and the parser stopped after the first complete
             * expression
             */
                pcb->parseres = ERR_NCX_INVALID_XPATH_EXPR;
                if (pcb->logerrors) {
                    log_error(instance,
                              "\nError: extra tokens in "
                              "XPath expression '%s'",
                              pcb->exprstr);
                    ncx_print_errormsg(instance,
                                       pcb->tkc,
                                       pcb->tkerr.mod,
                                       pcb->parseres);
                }
        }
    }

    /* if (pcb->tkc) { */
    /*     log_debug_t dbg = log_get_debug_level(instance); */
    /*     log_set_debug_level (instance, LOG_DEBUG_DEBUG3); */
    /*     log_debug3(instance, */
    /*                "\n\nParse chain for XPath '%s':\n", */
    /*                pcb->exprstr); */
    /*     tk_dump_chain(instance, pcb->tkc); */
    /*     log_debug3(instance, "\n"); */
    /*     log_set_debug_level (instance, dbg); */
    /* } */

    /* if (result) { */
    /*     log_debug_t dbg = log_get_debug_level(instance); */
    /*     log_set_debug_level (instance, LOG_DEBUG_DEBUG3); */
    /*     dump_result(instance, pcb, result, "parse_expr"); */
    /*     log_set_debug_level (instance, dbg); */
    /* } */

    /* since the pcb->obj is not set, this validation
     * phase will skip identifier tests, predicate tests
     * and completeness tests
     */
    if (result) {
        free_result(instance, pcb, result);
    }

    /* the expression will not be processed further if the
     * parseres is other than NO_ERR
     */
    return rwdts_ncx_status(pcb->parseres);

}  /* rwdts_ncx_xpath1_parse_expr */


rw_status_t
rwdts_ncx_status (status_t status) {
    switch (status) {
        case NO_ERR:
            return RW_STATUS_SUCCESS;

        default:
            return RW_STATUS_FAILURE;
    }
} /* rwdts_ncx_status */

ncx_instance_t*
rwdts_xpath_lib_init_ncx(const rw_yang_pb_schema_t* schema)
{

  ncx_instance_t *ncx = NULL;
  if (schema && schema->schema_module && schema->schema_module->module_name) {
    ncx = ncx_alloc();
    RW_ASSERT(ncx);

    status_t ncxst = ncx_init(ncx,
                              TRUE/*savestr*/,
                              LOG_DEBUG_ERROR, /* change to LOG_DEBUG_OFF later */
                              FALSE, /*logtstamps*/
                              "",
                              0,
                              NULL);
    RW_ASSERT(NO_ERR == ncxst);

    ncx_module_t* new_mod = NULL;
    ncxst = ncxmod_load_module( ncx, (xmlChar*)schema->schema_module->module_name, NULL, NULL, &new_mod );
    if (NO_ERR != ncxst) {
      ncx_cleanup(ncx);
      ncx = NULL;
    }
  }

  return ncx;
}

rw_status_t
rwdts_xpath_load_schema_ncx(void *instance,
                            const rw_yang_pb_schema_t* schema)
{
    rw_status_t rs = RW_STATUS_FAILURE;
    if (schema && schema->schema_module && schema->schema_module->module_name) {
        ncx_module_t* new_mod = NULL;
        status_t ncxst = ncxmod_load_module((ncx_instance_t*)instance,
                                            (xmlChar*)schema->schema_module->module_name,
                                            NULL, NULL, &new_mod );
        if (NO_ERR == ncxst) {
            rs = RW_STATUS_SUCCESS;
        }
    }
    return rs;
}

rw_status_t
rwdts_xpath_lib_inst_free_ncx(void *instance)
{
    RW_ASSERT(instance);
    ncx_cleanup (instance);
    return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_xpath_pcb_free_ncx (rwdts_xpath_pcb_t *rwpcb)
{
  RW_ASSERT(rwpcb);
  RW_ASSERT(rwpcb->lib_inst);

  ncx_instance_t *instance = (ncx_instance_t*)rwpcb->lib_inst;

  if (rwpcb->pcb) {
    xpath_pcb_t *pcb = (xpath_pcb_t *)rwpcb->pcb;
    xpath_free_pcb(instance, pcb);
    rwpcb->pcb = NULL;
  }

  if (rwpcb->local_inst) {
    ncx_cleanup (instance);
  }
  rwpcb->lib_inst = NULL;
  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_xpath_parse_ncx(rwdts_xpath_pcb_t *rwpcb,
                      const char* xpath)
{
  rw_status_t rs = RW_STATUS_FAILURE;
  RW_ASSERT(rwpcb);

  rs = rwdts_ncx_xpath1_parse_expr(rwpcb, (const xmlChar*)xpath);

  return rs;
}
