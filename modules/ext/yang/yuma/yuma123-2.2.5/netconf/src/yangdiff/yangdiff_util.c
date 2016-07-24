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
/*  FILE: yangdiff_util.c

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
10-jul-08    abb      split out from yangdiff.c

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <unistd.h>

#ifndef _H_procdefs
#include  "procdefs.h"
#endif

#ifndef _H_ncx
#include  "ncx.h"
#endif

#ifndef _H_ncx_feature
#include  "ncx_feature.h"
#endif

#ifndef _H_ncxconst
#include  "ncxconst.h"
#endif

#ifndef _H_ncxtypes
#include  "ncxtypes.h"
#endif

#ifndef _H_ses
#include  "ses.h"
#endif

#ifndef _H_status
#include  "status.h"
#endif

#ifndef _H_xml_util
#include  "xml_util.h"
#endif

#ifndef _H_yangconst
#include  "yangconst.h"
#endif

#ifndef _H_yangdiff
#include  "yangdiff.h"
#endif

#ifndef _H_yangdiff_util
#include  "yangdiff_util.h"
#endif


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/
#define YANGDIFF_UTIL_DEBUG   1


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/


/********************************************************************
* FUNCTION indent_in
*
* Move curindent to the right
*
* INPUTS:
*   cp == compare paraeters to use
*
*********************************************************************/
void
    indent_in (ncx_instance_t *instance, yangdiff_diffparms_t *cp)
{
    if (cp->indent) {
        cp->curindent += cp->indent;
        ses_set_indent(instance, cp->scb, cp->curindent);
    }
    
} /* indent_in */


/********************************************************************
* FUNCTION indent_out
*
* Move curindent to the left
*
* INPUTS:
*   cp == compare paraeters to use
*
*********************************************************************/
void
    indent_out (ncx_instance_t *instance, yangdiff_diffparms_t *cp)
{
    if (cp->indent) {
        if (cp->curindent >= cp->indent) {
            cp->curindent -= cp->indent;
        } else {
            cp->curindent = 0;
        }
        ses_set_indent(instance, cp->scb, cp->curindent);
    }
    
} /* indent_out */


/********************************************************************
* FUNCTION str_field_changed
*
* Check if the string field changed at all
*
* INPUTS:
*   fieldname == fieldname to use (may be NULL)
*   oldstr == old string to check (may be NULL)
*   newstr == new string to check (may be NULL)
*   isrev == TRUE if revision mode, FALSE otherwise
*   cdb == pointer to change description block to fill in (may be NULL)
*
* OUTPUTS:
*   *cdb is filled in, if cdb is non-NULL
*
* RETURNS:
*   1 if field changed
*   0 if field not changed
*   
*********************************************************************/
uint32
    str_field_changed (ncx_instance_t *instance,
                       const xmlChar *fieldname,
                       const xmlChar *oldstr,
                       const xmlChar *newstr,
                       boolean isrev,
                       yangdiff_cdb_t *cdb)
{
    uint32  ret;

    ret = 0;

    if (cdb) {
        cdb->fieldname = fieldname;
        cdb->oldval = oldstr;
        cdb->newval = newstr;
    }

    if (!oldstr && newstr) {
        ret = 1;
        if (cdb) {
            cdb->changed = TRUE;
            cdb->useval = newstr;
            cdb->chtyp = (isrev) ? ADD_STR : A_STR;
        }
    } else if (oldstr && !newstr) {
        ret = 1;
        if (cdb) {
            cdb->changed = TRUE;
            cdb->useval = oldstr;
            cdb->chtyp = (isrev) ? DEL_STR : D_STR;
        }
    } else if (oldstr && newstr && 
               xml_strcmp_nosp(instance, oldstr, newstr)) {
        ret = 1;
        if (cdb) {
            cdb->changed = TRUE;
            cdb->useval = newstr;
            cdb->chtyp = (isrev) ? MOD_STR : M_STR;
        }
    } else if (cdb) {
        cdb->changed = FALSE;
        cdb->useval = NULL;
        cdb->chtyp = NULL;
    }
    return ret;

} /* str_field_changed */


/********************************************************************
* FUNCTION bool_field_changed
*
* Check if the boolean field changed at all
*
* INPUTS:
*   oldbool == old boolean to check
*   newbool == new boolean to check
*   isrev == TRUE if revision mode, FALSE otherwise
*   cdb == pointer to change description block to fill in
*
* OUTPUTS:
*   *cdb is filled in
*
* RETURNS:
*   1 if field changed
*   0 if field not changed
*   
*********************************************************************/
uint32
    bool_field_changed (const xmlChar *fieldname,
                       boolean oldbool,
                       boolean newbool,
                       boolean isrev,
                       yangdiff_cdb_t *cdb)
{
    uint32 ret;

    ret = 0;
    if (cdb) {
        cdb->fieldname = fieldname;
        cdb->oldval = oldbool ? NCX_EL_TRUE : NCX_EL_FALSE;
        cdb->newval = newbool ? NCX_EL_TRUE : NCX_EL_FALSE;
    }

    if (oldbool != newbool) {
        ret = 1;
        if (cdb) {
            cdb->changed = TRUE;
            cdb->useval = cdb->newval;
            cdb->chtyp = (isrev) ? MOD_STR : M_STR;
        }
    } else if (cdb) {
        cdb->changed = FALSE;
        cdb->useval = NULL;
        cdb->chtyp = NULL;
    }
    return ret;

} /* bool_field_changed */


/********************************************************************
* FUNCTION status_field_changed
*
* Check if the status field changed at all
*
* INPUTS:
*   oldstat == old status to check
*   newstat == new status to check
*   isrev == TRUE if revision mode, FALSE otherwise
*   cdb == pointer to change description block to fill in
*
* OUTPUTS:
*   *cdb is filled in
*
* RETURNS:
*   1 if field changed
*   0 if field not changed
*********************************************************************/
uint32
    status_field_changed (ncx_instance_t *instance,
                          const xmlChar *fieldname,
                          ncx_status_t oldstat,
                          ncx_status_t newstat,
                          boolean isrev,
                          yangdiff_cdb_t *cdb)
{
    uint32 ret;

    ret = 0;

    if (cdb) {
        cdb->fieldname = fieldname;
        cdb->oldval = ncx_get_status_string(instance, oldstat);
        cdb->newval = ncx_get_status_string(instance, newstat);
    }
    if (oldstat != newstat) {
        ret = 1;
        if (cdb) {
            cdb->changed = TRUE;
            cdb->useval = cdb->newval;
            cdb->chtyp = (isrev) ? MOD_STR : M_STR;
        }
    } else if (cdb) {
        cdb->changed = FALSE;
        cdb->useval = NULL;
        cdb->chtyp = NULL;
    }
    return ret;

} /* status_field_changed */


/********************************************************************
* FUNCTION prefix_field_changed
*
* Check if the status field changed at all
*
* INPUTS:
*   oldmod == old module to check
*   newmod == new module to check
*   oldprefix == old prefix to check
*   newprefix == new prefix to check
*
* RETURNS:
*   1 if field changed
*   0 if field not changed
*********************************************************************/
uint32
    prefix_field_changed (ncx_instance_t *instance,
                          const ncx_module_t *oldmod,
                          const ncx_module_t *newmod,
                          const xmlChar *oldprefix,
                          const xmlChar *newprefix)
{
    const ncx_import_t *oldimp, *newimp;

    if (str_field_changed(instance, NULL, oldprefix, 
                          newprefix, FALSE, NULL)) {
        if (oldprefix && newprefix) {
            /* check if the different prefix values reference
             * the same module name
             */
            oldimp = ncx_find_pre_import(instance, oldmod, oldprefix);
            newimp = ncx_find_pre_import(instance, newmod, newprefix);
            if (oldimp && newimp &&
                !xml_strcmp(instance, oldimp->module, newimp->module)) {
                return 0;
            } else {
                return 1;
            }
        } else if (oldprefix && 
                   !xml_strcmp(instance, oldprefix, oldmod->prefix)) {
            return 0;  /* using own module prefix */
        } else if (newprefix && 
                   !xml_strcmp(instance, newprefix, newmod->prefix)) {
            return 0;  /* using own module prefix */
        } else {
            return 1;
        }
    } else {
        return 0;
    }
    /*NOTREACHED*/

} /* prefix_field_changed */


/********************************************************************
 * FUNCTION output_val
 * 
 *  Output one value; will add truncate!
 *
 * INPUTS:
 *    cp == parameter block to use
 *    strval == str value to output
 *********************************************************************/
void
    output_val (ncx_instance_t *instance,
                yangdiff_diffparms_t *cp,
                const xmlChar *strval)
{
    uint32 i;

    ses_putchar(instance, cp->scb, '\'');
    if (xml_strlen(instance, strval) > cp->maxlen) {
        for (i=0; i<cp->maxlen; i++) {
            ses_putchar(instance, cp->scb, strval[i]);
        }
        ses_putstr(instance, cp->scb, (const xmlChar *)"...");
    } else {
        ses_putstr(instance, cp->scb, strval);
    }
    ses_putchar(instance, cp->scb, '\'');

}  /* output_val */


/********************************************************************
 * FUNCTION output_mstart_line
 * 
 *  Output one line for a comparison where the change is 'modify'
 *  Just print the field name and identifier
 *
 * INPUTS:
 *    cp == parameter block to use
 *    fieldname == fieldname that is different to print
 *    val == string value to print (may be NULL)
 *    isid == TRUE if val is an ID
 *         == FALSE if val should be single-quoted
 *         (ignored in val == NULL)
 *********************************************************************/
void
    output_mstart_line (ncx_instance_t *instance,
                        yangdiff_diffparms_t *cp,
                        const xmlChar *fieldname,
                        const xmlChar *val,
                        boolean isid)
{
    ses_putstr_indent(instance, 
                      cp->scb, 
                      (cp->edifftype==YANGDIFF_DT_REVISION) 
                      ? MOD_STR : M_STR, cp->curindent);
    ses_putstr(instance, cp->scb, fieldname);
    if (val) {
        ses_putchar(instance, cp->scb, ' ');
        if (isid) {
            ses_putstr(instance, cp->scb, val);
        } else {
            output_val(instance, cp, val);
        }
    }

}  /* output_mstart_line */


/********************************************************************
 * FUNCTION output_cdb_line
 * 
 *  Output one line for a comparison, from a change description block
 *
 * INPUTS:
 *    cp == parameter block to use
 *    cdb == change descriptor block to use
 *********************************************************************/
void
    output_cdb_line (ncx_instance_t *instance,
                     yangdiff_diffparms_t *cp,
                     const yangdiff_cdb_t *cdb)
{
    boolean showval;

    if (!cdb->changed) {
        return;
    }

    showval = (cp->edifftype == YANGDIFF_DT_TERSE) ? FALSE : TRUE;

    ses_putstr_indent(instance, cp->scb, cdb->chtyp, cp->curindent);
    ses_putstr(instance, cp->scb, cdb->fieldname);
    if (showval) {
        ses_putchar(instance, cp->scb, ' ');
        if (cdb->oldval && cdb->newval) {
            if ((xml_strlen(instance, cdb->oldval) < cp->maxlen) &&
                (xml_strlen(instance, cdb->newval) < cp->maxlen)) {
                ses_putstr(instance, cp->scb, FROM_STR);
                output_val(instance, cp, cdb->oldval);
                ses_putchar(instance, cp->scb, ' ');
                ses_putstr(instance, cp->scb, TO_STR);
                output_val(instance, cp, cdb->newval);
            }
        } else if (cdb->useval) {
            output_val(instance, cp, cdb->useval);
        } else {
            ses_putstr(instance, cp->scb, (const xmlChar *)" ''");
        }
    }

}  /* output_cdb_line */


/********************************************************************
 * FUNCTION output_diff
 * 
 *  Output one detailed difference
 *
 * INPUTS:
 *    cp == parameter block to use
 *    fieldname == fieldname that is different to print
 *    oldval == string value for old rev (may be NULL)
 *    newval == string value for new rev (may be NULL)
 *    isid == TRUE if value is really an identifier, not just a string
 *
 *********************************************************************/
void
    output_diff (ncx_instance_t *instance,
                 yangdiff_diffparms_t *cp,
                 const xmlChar *fieldname,
                 const xmlChar *oldval,
                 const xmlChar *newval,
                 boolean isid)
{
    const xmlChar *useval, *astr, *dstr, *mstr;
    boolean finish, useto;

    useval = NULL;
    useto = FALSE;
    finish = TRUE;

    if (cp->edifftype==YANGDIFF_DT_REVISION) {
        astr = ADD_STR;
        dstr = DEL_STR;
        mstr = MOD_STR;
    } else {
        astr = A_STR;
        dstr = D_STR;
        mstr = M_STR;
    }

    if (!oldval && newval) {
        ses_putstr_indent(instance, cp->scb, astr, cp->curindent);
        useval = newval;
    } else if (oldval && !newval) {
        ses_putstr_indent(instance, cp->scb, dstr, cp->curindent);
        useval = oldval;
    } else if (oldval && newval) {
        if (xml_strcmp(instance, oldval, newval)) {
            ses_putstr_indent(instance, cp->scb, mstr, cp->curindent);
            useto = TRUE;
        } else {
            finish = FALSE;
        }
    } else {
        finish = FALSE;
    }

    if (finish) {
        ses_putstr(instance, cp->scb, fieldname);
        if (useto) {
            if ((xml_strlen(instance, oldval) < cp->maxlen) &&
                (xml_strlen(instance, newval) < cp->maxlen)) {
                ses_putchar(instance, cp->scb, ' ');
                ses_putstr(instance, cp->scb, FROM_STR);
                output_val(instance, cp, oldval);
                ses_putchar(instance, cp->scb, ' ');
                ses_putstr(instance, cp->scb, TO_STR);
                output_val(instance, cp, newval);
            }
        } else {
            ses_putchar(instance, cp->scb, ' ');
            if (isid) {
                ses_putstr(instance, cp->scb, useval);
            } else {
                output_val(instance, cp, useval);
            }
        }
    }

}  /* output_diff */


/********************************************************************
 * FUNCTION output_errinfo_diff
 * 
 *  Output the differences report for one ncx_errinfo_t struct
 *  Used for may clauses, such as must, range, length, pattern;
 *  within a leaf, leaf-list, or typedef definition
 *
 * INPUTS:
 *    cp == parameter block to use
 *    olderr == old err info struct (may be NULL)
 *    newerr == new err info struct (may be NULL)
 *
 *********************************************************************/
void
    output_errinfo_diff (ncx_instance_t *instance,
                         yangdiff_diffparms_t *cp,
                         const ncx_errinfo_t *olderr,
                         const ncx_errinfo_t *newerr)
{
    if (!olderr && newerr) {
        /* errinfo added in new revision */
        output_diff(instance, 
                    cp, 
                    YANG_K_ERROR_MESSAGE, 
                    NULL,
                    newerr->error_message, 
                    FALSE);
        output_diff(instance, 
                    cp, 
                    YANG_K_ERROR_APP_TAG, 
                    NULL, 
                    newerr->error_app_tag, 
                    FALSE);
        output_diff(instance, 
                    cp, 
                    YANG_K_DESCRIPTION, 
                    NULL, 
                    newerr->descr, 
                    FALSE);
        output_diff(instance, 
                    cp, 
                    YANG_K_REFERENCE, 
                    NULL, 
                    newerr->ref, 
                    FALSE);
    } else if (olderr && !newerr) {
        /* errinfo removed in new revision */
        output_diff(instance, 
                    cp, 
                    YANG_K_ERROR_MESSAGE,
                    olderr->error_message, 
                    NULL, 
                    FALSE);
        output_diff(instance, 
                    cp, 
                    YANG_K_ERROR_APP_TAG,
                    olderr->error_app_tag, 
                    NULL, 
                    FALSE);
        output_diff(instance, 
                    cp, 
                    YANG_K_DESCRIPTION,
                    olderr->descr, 
                    NULL, 
                    FALSE);
        output_diff(instance, 
                    cp, 
                    YANG_K_REFERENCE,
                    olderr->ref, 
                    NULL, 
                    FALSE);
    } else if (olderr && newerr) {
        /* errinfo maybe changed in new revision */
        output_diff(instance, 
                    cp, 
                    YANG_K_ERROR_MESSAGE,
                    olderr->error_message,
                    newerr->error_message, 
                    FALSE);
        output_diff(instance, 
                    cp, 
                    YANG_K_ERROR_APP_TAG,
                    olderr->error_app_tag,
                    newerr->error_app_tag, 
                    FALSE);
        output_diff(instance, 
                    cp, 
                    YANG_K_DESCRIPTION,
                    olderr->descr, 
                    newerr->descr, 
                    FALSE);
        output_diff(instance, 
                    cp, 
                    YANG_K_REFERENCE,
                    olderr->ref, 
                    newerr->ref, 
                    FALSE);
    }
} /* output_errinfo_diff */


/********************************************************************
* FUNCTION errinfo_changed
*
* Check if the ncx_errinfo_t struct changed at all
*
* INPUTS:
*   olderr == old error struct to check
*   newerr == new error struct to check
*
* RETURNS:
*   1 if field changed
*   0 if field not changed
*********************************************************************/
uint32
    errinfo_changed (ncx_instance_t *instance,
                     const ncx_errinfo_t *olderr,
                     const ncx_errinfo_t *newerr)
{
    if ((!olderr && newerr) || (olderr && !newerr)) {
        return 1;
    } else if (olderr && newerr) {
        if (str_field_changed(instance,
                              YANG_K_DESCRIPTION,
                              olderr->descr, 
                              newerr->descr,
                              FALSE, 
                              NULL)) {
            return 1;
        }
        if (str_field_changed(instance,
                              YANG_K_REFERENCE,
                              olderr->ref, 
                              newerr->ref,
                              FALSE, 
                              NULL)) {
            return 1;
        }
        if (str_field_changed(instance,
                              YANG_K_ERROR_APP_TAG,
                              olderr->error_app_tag,
                              newerr->error_app_tag,
                              FALSE, 
                              NULL)) {
            return 1;
        }
        if (str_field_changed(instance,
                              YANG_K_ERROR_MESSAGE,
                              olderr->error_message,
                              newerr->error_message,
                              FALSE, 
                              NULL)) {
            return 1;
        }
        return 0;
    } else {
        return 0;
    }
    /*NOTREACHED*/
    
}  /* errinfo_changed */


/********************************************************************
* FUNCTION iffeature_changed
*
* Check if the ncx_iffeature_t struct changed at all
*
* INPUTS:
*   modprefix == module prefix value to use as default
*   oldif == old if-feature struct to check
*   newif == new if-feature struct to check
*
* RETURNS:
*   1 if field changed
*   0 if field not changed
*********************************************************************/
uint32
    iffeature_changed (ncx_instance_t *instance,
                       const xmlChar *modprefix,
                       const ncx_iffeature_t *oldif,
                       const ncx_iffeature_t *newif)
{
    if ((!oldif && newif) || (oldif && !newif)) {
        return 1;
    } else if (oldif && newif) {
        if (ncx_prefix_different(instance, 
                                 oldif->prefix, 
                                 newif->prefix,
                                 modprefix)) {
            return 1;
        }
        if (str_field_changed(instance,
                              YANG_K_IF_FEATURE,
                              oldif->name, 
                              newif->name,
                              FALSE,
                              NULL)) {
            return 1;
        }
    }

    return 0;    

}  /* iffeature_changed */


/********************************************************************
 * FUNCTION iffeatureQ_changed
 * 
 *  Check if a Q of must-stmt definitions changed
 *
 * INPUTS:
 *    modprefix == module prefix in use
 *    oldQ == Q of old ncx_iffeature_t to use
 *    newQ == Q of new ncx_iffeature_t to use
 *
 * RETURNS:
 *    1 if field changed
 *    0 if field not changed
 *********************************************************************/
uint32
    iffeatureQ_changed (ncx_instance_t *instance,
                        const xmlChar *modprefix,
                        dlq_hdr_t *oldQ,
                        dlq_hdr_t *newQ)
{
    ncx_iffeature_t *oldif, *newif;
    uint32           oldcnt, newcnt;

#ifdef DEBUG
    if (oldQ == NULL || newQ == NULL) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return 0;
    }
#endif

    oldcnt = dlq_count(instance, oldQ);
    newcnt = dlq_count(instance, newQ);

    if (oldcnt != newcnt) {
        return 1;
    }

    if (oldcnt == 0) {
        return 0;
    }

    for (newif = (ncx_iffeature_t *)dlq_firstEntry(instance, newQ);
         newif != NULL;
         newif = (ncx_iffeature_t *)dlq_nextEntry(instance, newif)) {
        newif->seen = FALSE;
    }

    /* look through the old Q for matching entries in the new Q */
    for (oldif = (ncx_iffeature_t *)dlq_firstEntry(instance, oldQ);
         oldif != NULL;
         oldif = (ncx_iffeature_t *)dlq_nextEntry(instance, oldif)) {

        newif = ncx_find_iffeature(instance, 
                                   newQ, 
                                   oldif->prefix,
                                   oldif->name,
                                   modprefix);
        if (newif) {
            if (iffeature_changed(instance, modprefix, oldif, newif)) {
                return 1;
            } else {
                newif->seen = TRUE;
            }
        } else {
            return 1;
        }
    }

    /* look for iffeature-stmts that were added in the new module */
    for (newif = (ncx_iffeature_t *)dlq_firstEntry(instance, newQ);
         newif != NULL;
         newif = (ncx_iffeature_t *)dlq_nextEntry(instance, newif)) {
        if (!newif->seen) {
            return 1;
        }
    }

    return 0;

}  /* iffeatureQ_changed */


/********************************************************************
 * FUNCTION output_iffeatureQ_diff
 * 
 *  Output any changes in a Q of if-feature-stmt definitions
 *
 * INPUTS:
 *    cp == comparison paraeters to use
 *    modprefix == module prefix in use
 *    oldQ == Q of old ncx_iffeature_t to use
 *    newQ == Q of new ncx_iffeature_t to use
 *
 *********************************************************************/
void
    output_iffeatureQ_diff (ncx_instance_t *instance,
                            yangdiff_diffparms_t *cp,
                            const xmlChar *modprefix,
                            dlq_hdr_t *oldQ,
                            dlq_hdr_t *newQ)
{
    ncx_iffeature_t *oldif, *newif;

#ifdef DEBUG
    if (cp == NULL || oldQ == NULL || newQ == NULL) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    for (newif = (ncx_iffeature_t *)dlq_firstEntry(instance, newQ);
         newif != NULL;
         newif = (ncx_iffeature_t *)dlq_nextEntry(instance, newif)) {
        newif->seen = FALSE;
    }

    /* look through the old Q for matching entries in the new Q */
    for (oldif = (ncx_iffeature_t *)dlq_firstEntry(instance, oldQ);
         oldif != NULL;
         oldif = (ncx_iffeature_t *)dlq_nextEntry(instance, oldif)) {

        newif = ncx_find_iffeature(instance, 
                                   newQ, 
                                   oldif->prefix,
                                   oldif->name,
                                   modprefix);
        if (newif) {
            newif->seen = TRUE;
            if (iffeature_changed(instance, modprefix, oldif, newif)) {
                output_mstart_line(instance, 
                                   cp, 
                                   YANG_K_IF_FEATURE, 
                                   oldif->name, 
                                   FALSE);
                if (cp->edifftype != YANGDIFF_DT_TERSE) {
                    indent_in(instance, cp);
                    output_diff(instance, 
                                cp, 
                                YANG_K_IF_FEATURE,
                                oldif->name, 
                                newif->name, 
                                TRUE);
                    indent_out(instance, cp);
                }
            }
        } else {
            /* if-feature-stmt was removed from the new module */
            output_diff(instance, 
                        cp, 
                        YANG_K_IF_FEATURE,  
                        oldif->name, 
                        NULL, 
                        FALSE);
        }
    }

    /* look for must-stmts that were added in the new module */
    for (newif = (ncx_iffeature_t *)dlq_firstEntry(instance, newQ);
         newif != NULL;
         newif = (ncx_iffeature_t *)dlq_nextEntry(instance, newif)) {

        if (!newif->seen) {
            /* must-stmt was added in the new module */
            output_diff(instance, 
                        cp, 
                        YANG_K_IF_FEATURE,  
                        NULL, 
                        newif->name,
                        FALSE);
        }
    }

}  /* output_iffeatureQ_diff */


/* END yangdiff_util.c */
