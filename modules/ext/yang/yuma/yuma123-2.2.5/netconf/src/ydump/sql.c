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
/*  FILE: sql.c

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
19feb08      abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <memory.h>
#include <ctype.h>

#include <xmlstring.h>
#include <xmlreader.h>

#ifndef _H_procdefs
#include  "procdefs.h"
#endif

#ifndef _H_dlq
#include "dlq.h"
#endif

#ifndef _H_ext
#include "ext.h"
#endif

#ifndef _H_grp
#include "grp.h"
#endif

#ifndef _H_ncx
#include "ncx.h"
#endif

#ifndef _H_ncxconst
#include "ncxconst.h"
#endif

#ifndef _H_ncxmod
#include "ncxmod.h"
#endif

#ifndef _H_obj
#include "obj.h"
#endif

#ifndef _H_ses
#include "ses.h"
#endif

#ifndef _H_sql
#include "sql.h"
#endif

#ifndef _H_status
#include  "status.h"
#endif

#ifndef _H_tstamp
#include "tstamp.h"
#endif

#ifndef _H_typ
#include "typ.h"
#endif

#ifndef _H_xmlns
#include "xmlns.h"
#endif

#ifndef _H_xml_util
#include "xml_util.h"
#endif

#ifndef _H_xml_val
#include "xml_val.h"
#endif

#ifndef _H_xpath
#include "xpath.h"
#endif

#ifndef _H_xsd_util
#include "xsd_util.h"
#endif

#ifndef _H_yang
#include "yang.h"
#endif

#ifndef _H_yangconst
#include "yangconst.h"
#endif

#ifndef _H_yangdump
#include "yangdump.h"
#endif


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/
#define SQL_BUFFSIZE        32768
#define EMPTY_COLUMN        (const xmlChar *)"'', "
#define EMPTY_COLUMN_LAST   (const xmlChar *)"''"
#define SEP_BAR             (const xmlChar *)\
    "\n#################################################"

#define MAX_ABSTRACT_LEN    128

/********************************************************************
* FUNCTION write_empty_col
* 
* Write an empty column entry
*
* INPUTS:
*   scb == session control block to use 
*********************************************************************/
static void
    write_empty_col (ncx_instance_t *instance, ses_cb_t *scb)
{
    ses_putstr(instance, scb, (const xmlChar *)"\n    '',");

} /* write_empty_col */


/********************************************************************
* FUNCTION write_cstring
* 
* Write a user content string which might have a single quote
* Need to escape any single quote found
*
* INPUTS:
*   scb == session control block to use for writing
*   cstr == content string to write
*********************************************************************/
static void
    write_cstring (ncx_instance_t *instance,
                   ses_cb_t *scb,
                   const xmlChar *cstr)
{
    while (*cstr) {
        if (*cstr == '\'') {
            ses_putchar(instance, scb, '\\');
        }
        ses_putchar(instance, scb, *cstr);
        cstr++;
    }

} /* write_cstring */


/********************************************************************
* FUNCTION write_cstring_n
* 
* Write a user content string which might have a single quote
* Need to escape any single quote found
*
* INPUTS:
*   scb == session control block to use for writing
*   cstr == content string to write
*   count == max number of characters to write
*********************************************************************/
static void
    write_cstring_n (ncx_instance_t *instance,
                     ses_cb_t *scb,
                     const xmlChar *cstr,
                     uint32 count)
{
    while (*cstr && count) {
        if (*cstr == '\'') {
            ses_putchar(instance, scb, '\\');
        }
        ses_putchar(instance, scb, *cstr);
        cstr++;
        count--;
    }

} /* write_cstring_n */


/********************************************************************
* FUNCTION write_end_tstamps
* 
* Write the created_on and updated_on tstamp values
*
* INPUTS:
*   scb == session control block to use for writing
*   buff == malloced buff to use
*********************************************************************/
static void
    write_end_tstamps (ncx_instance_t *instance,
                       ses_cb_t *scb,
                       char *buff)
{

    /* column: created_on */
    ses_putstr(instance, scb, (const xmlChar *)"\n    '");
    tstamp_datetime_sql(instance, (xmlChar *)buff);
    ses_putstr(instance, scb, (const xmlChar *)buff);
    ses_putstr(instance, scb, (const xmlChar *)"', ");

    /* column: updated_on */
    ses_putstr(instance, scb, (const xmlChar *)"\n    '");
    ses_putstr(instance, scb, (const xmlChar *)buff);
    ses_putchar(instance, scb, '\'');

} /* write_end_tstamps */


/********************************************************************
* FUNCTION write_docurl
* 
* Write the docurl URL for some type of entry
*
* INPUTS:
*   scb == session control block to use for writing
*   mod == module that will be used for the URL
*   cp == conversion parameters to use
*   fragname == name of fragment to use (NULL == none)
*   linenum == line number to add to the fragment because
*              many symbol types can overlap maning scopes,
*              and the [name, linenum] tuple is needed
*              to make the URLs unique (instead of broken)
*********************************************************************/
static void
    write_docurl (ncx_instance_t *instance,
                  ses_cb_t *scb,
                  const ncx_module_t *mod,
                  const yangdump_cvtparms_t *cp,
                  const xmlChar *fragname,
                  uint32 linenum)
{
    char           numbuff[NCX_MAX_NUMLEN];

    /* column: docurl */
    if (cp->urlstart && *cp->urlstart) {
        ses_putstr(instance, scb, (const xmlChar *)"\n    '/modules/");
    } else {
        ses_putstr(instance, scb, (const xmlChar *)"\n    ");
    }
    if (cp->unified && !mod->ismod) {
        ses_putstr(instance, scb, mod->belongs);
    } else {
        ses_putstr(instance, scb, mod->name);
    }
    if (cp->versionnames && mod->version) {
        if (cp->simurls) {
            ses_putchar(instance, scb, NCXMOD_PSCHAR);
        } else {            
            ses_putchar(instance, scb, YANG_FILE_SEPCHAR);
        }
        ses_putstr(instance, scb, mod->version);
    }
    if (!cp->simurls) {
        ses_putstr(instance, scb, (const xmlChar *)".html");
    }
    if (fragname) {
        ses_putchar(instance, scb, '#');
        if (cp->unified && !mod->ismod) {
            /* add the submodule to the fragment ID since
             * the line numbers may collide with another definition
             * on the same line in 2 different submodules
             */
            ses_putstr(instance, scb, mod->name);
            ses_putchar(instance, scb, '.');
        }
        ses_putstr(instance, scb, fragname);
        if (linenum) {
            sprintf(numbuff, ".%u", linenum);
            ses_putstr(instance, scb, (const xmlChar *)numbuff);
        }
    }
    ses_putstr(instance, scb, (const xmlChar *)"', ");

} /* write_docurl */


/********************************************************************
* FUNCTION write_first_tuple
* 
* Write the columns for the 
*   [modname, submodname, version, name, linenum] tuple
*
* INPUTS:
*   scb == session control block to use for writing
*   mod == module that will be used for the URL
*   name == myname field value to write
*   linenum == line number for 'name'
*   buff == scratch buff to use
*********************************************************************/
static void
    write_first_tuple (ncx_instance_t *instance,
                       ses_cb_t *scb,
                       const ncx_module_t *mod,
                       const xmlChar *name,
                       uint32 linenum,
                       char *buff)
{
    /* columns: ID, modname, submodname, version, name, linenum */
    sprintf(buff, "\n    '', '%s', '%s', '%s', '%s', '%u',",
            (mod->ismod) ? mod->name : mod->belongs,
            mod->name, 
            (mod->version) ? mod->version : NCX_EL_NONE,
            name, 
            linenum);
    ses_putstr(instance, scb, (const xmlChar *)buff);

} /* write_first_tuple */


/********************************************************************
* FUNCTION write_descr_ref
* 
* Write the description, reference columns
*
* INPUTS:
*   scb == session control block to use for writing
*   descr == description string to use (may be NULL)
*   ref == reference string to use (may be NULL)
*********************************************************************/
static void
    write_descr_ref (ncx_instance_t *instance,
                     ses_cb_t *scb,
                     const xmlChar *descr,
                     const xmlChar *ref)
{

    /* column: description */
    ses_putstr(instance, scb, (const xmlChar *)"\n    '");
    write_cstring(instance, scb, (descr) ? descr : EMPTY_STRING);
    ses_putstr(instance, scb, (const xmlChar *)"',");

    /* column: reference */
    ses_putstr(instance, scb, (const xmlChar *)"\n    '");
    write_cstring(instance, scb, (ref) ? ref : EMPTY_STRING);
    ses_putstr(instance, scb, (const xmlChar *)"',");

} /* write_descr_ref */


/********************************************************************
* FUNCTION write_object_list
* 
* Write an object list  column
*    'objname objname ...'
*
* INPUTS:
*   scb == session control block to use for writing
*   datadefQ == Q of obj_template_t to use
*
*********************************************************************/
static void
    write_object_list (ncx_instance_t *instance,
                       ses_cb_t *scb,
                       const dlq_hdr_t *datadefQ)
{
    obj_template_t   *obj, *nextobj;

    /* column: object list */
    ses_putstr(instance, scb, (const xmlChar *)"\n    '");
    for (obj = (obj_template_t *)dlq_firstEntry(instance, datadefQ);
         obj != NULL; obj = nextobj) {
        nextobj = (obj_template_t *)dlq_nextEntry(instance, obj);
        if (obj_has_name(instance, obj)) {
            ses_putstr(instance, scb, obj_get_name(instance, obj));
            if (nextobj && obj_has_name(instance, nextobj)) {
                ses_putchar(instance, scb, ' ');
            }
        }
    }

    ses_putstr(instance, scb, (const xmlChar *)"',");

} /* write_object_list */


/********************************************************************
* FUNCTION write_object_id
* 
* Write an object ID (Xpath absolute expression
*    '/obj1/chobj/chobj ...'
*
* INPUTS:
*   scb == session control block to use for writing
*   obj == obj_template_t to write
*   buff == scratch buff to use
*
*********************************************************************/
static void
    write_object_id (ncx_instance_t *instance,
                     ses_cb_t *scb,
                     const obj_template_t *obj,
                     char *buff)
{
    status_t   res;
    uint32     retlen;

    res = obj_copy_object_id(instance, obj, (xmlChar *)buff, 
                             SQL_BUFFSIZE, &retlen);
    if (res != NO_ERR) {
        /* may ruin the SQL output to SSTDOUT */
        log_error(instance,
                  "\nError: Write SQL Object ID %s failed",
                  obj_get_name(instance, obj));
        return;
    }

    if (retlen > MAX_OBJECTID_LEN) {
        log_error(instance,
                  "\nError: Write SQL Object ID %s too big (%u)",
                  obj_get_name(instance, obj), retlen);
        return;
    }

    ses_putstr(instance, scb, (const xmlChar *)"\n    '");
    ses_putstr(instance, scb, (const xmlChar *)buff);
    ses_putstr(instance, scb, (const xmlChar *)"',");    

} /* write_object_id */


/********************************************************************
* FUNCTION write_banner
* 
* Generate the SQL comments to start the file
*
* INPUTS:
*   mod == module in progress
*   scb == session control block to use for writing
*
*********************************************************************/
static void
    write_banner (ncx_instance_t *instance,
                  const ncx_module_t *mod,
                  ses_cb_t *scb)
{
    status_t       res;
    xmlChar        buffer[NCX_VERSION_BUFFSIZE];

    ses_putstr(instance, scb, (const xmlChar *)"\n#");
    ses_putstr(instance, scb, (const xmlChar *)"\n# Generated by yangdump version ");
    res = ncx_get_version(instance, buffer, NCX_VERSION_BUFFSIZE);
    if (res == NO_ERR) {
        ses_putstr(instance, scb, buffer);
    } else {
        SET_ERROR(instance, res);
    }

    ses_putstr(instance, scb, (const xmlChar *)"\n#");
    ses_putstr(instance, scb, (const xmlChar *)"\n# mySQL database: netconfcentral");
    ses_putstr(instance, scb, (const xmlChar *)"\n# source: ");
    ses_putstr(instance, scb, mod->source);

    if (mod->ismod) {
        ses_putstr(instance, scb, (const xmlChar *)"\n# module: ");
    } else {
        ses_putstr(instance, scb, (const xmlChar *)"\n# submodule: ");
    }
    ses_putstr(instance, scb, mod->name);

    if (!mod->ismod) {
        ses_putstr(instance, scb, (const xmlChar *)"\n# belongs-to: ");
        ses_putstr(instance, scb, mod->belongs);
    }

    ses_putstr(instance, scb, (const xmlChar *)"\n# version: ");
    if (mod->version) {
        write_cstring(instance, scb, mod->version);
    } else {
        write_cstring(instance, scb, NCX_EL_NONE);
    }
    ses_putstr(instance, scb, (const xmlChar *)"\n#");
    ses_putstr(instance, scb, SEP_BAR);

}  /* write_banner */


/********************************************************************
* FUNCTION write_quick_module_entry
* 
* Generate the SQL code to add the module to 
* the 'ncquickmod' table
*
* INPUTS:
*   mod == module in progress
*   scb == session control block to use for writing
*   buff == scratch buff to use
*********************************************************************/
static void
    write_quick_module_entry (ncx_instance_t *instance,
                              const ncx_module_t *mod,
                              ses_cb_t *scb,
                              char *buff)

{

    if (!mod->ismod || !mod->defaultrev) {
        return;
    }

    ses_putstr(instance, scb, (const xmlChar *)"\n\nINSERT INTO ncquickmod VALUES (");

    /* columns: ID, modname, submodname */
    sprintf(buff, "\n    '', '%s');", mod->name);
    ses_putstr(instance, scb, (const xmlChar *)buff);

}  /* write_quick_module_entry */


/********************************************************************
* FUNCTION write_module_entry
* 
* Generate the SQL code to add the module to 
* the 'ncmodule' table
*
* INPUTS:
*   mod == module in progress
*   cp == conversion parameters to use
*   scb == session control block to use for writing
*   buff == scratch buff to use
*
*********************************************************************/
static void
    write_module_entry (ncx_instance_t *instance,
                        const ncx_module_t *mod,
                        const yangdump_cvtparms_t *cp,
                        ses_cb_t *scb,
                        char *buff)
{
    ncx_revhist_t *revhist;

    ses_putstr(instance, scb, (const xmlChar *)"\n\nINSERT INTO ncmodule VALUES (");

    /* columns: ID, modname, submodname */
    sprintf(buff, "\n    '', '%s', '%s', ",
            (mod->ismod) ? mod->name : mod->belongs, mod->name);
    ses_putstr(instance, scb, (const xmlChar *)buff);

    /* columns: version, modprefix */
    ses_putchar(instance, scb, '\'');
    if (mod->version) {
        write_cstring(instance, scb, mod->version);
    } else {
        write_cstring(instance, scb, NCX_EL_NONE);
    }
    ses_putstr(instance, scb, (const xmlChar *)"', ");
    sprintf(buff, "'%s',", (mod->prefix) ? mod->prefix : mod->name);
    ses_putstr(instance, scb, (const xmlChar *)buff);

    /* column: namespace */
    ses_putstr(instance, scb, (const xmlChar *)"\n    '");
    if (mod->nsid) {
        write_cstring(instance, scb, xmlns_get_ns_name(instance, mod->nsid));
    } else {
        write_cstring(instance, scb, EMPTY_STRING);
    }
    ses_putstr(instance, scb, (const xmlChar *)"',");

    /* column: organization */
    ses_putstr(instance, scb, (const xmlChar *)"\n    '");
    write_cstring(instance, scb, (mod->organization) 
                  ? mod->organization : EMPTY_STRING);
    ses_putstr(instance, scb, (const xmlChar *)"',");

    /* column: abstract -- need to use different abstract!!! */
    ses_putstr(instance, scb, (const xmlChar *)"\n    '");
    write_cstring_n(instance, scb, (mod->descr) 
                    ? mod->descr : EMPTY_STRING, MAX_ABSTRACT_LEN);
    if (mod->descr && (xml_strlen(instance, mod->descr) > MAX_ABSTRACT_LEN)) {
        ses_putstr(instance, scb, (const xmlChar *)"...");
    }
    ses_putstr(instance, scb, (const xmlChar *)"',");

    /* columns: description, reference */
    write_descr_ref(instance, scb, mod->descr, mod->ref);

    /* column: contact */
    ses_putstr(instance, scb, (const xmlChar *)"\n    '");
    write_cstring(instance, scb, (mod->contact_info) 
                  ? mod->contact_info : EMPTY_STRING);
    ses_putstr(instance, scb, (const xmlChar *)"',");

    /* column: revcomment */
    revhist = NULL;
    if (mod->version) {
        revhist = ncx_find_revhist(instance, mod, mod->version);
    }
    if (revhist && revhist->descr) {
        ses_putstr(instance, scb, (const xmlChar *)"\n    '");
        write_cstring(instance, scb, revhist->descr);
        ses_putstr(instance, scb, (const xmlChar *)"',");
    } else {
        write_empty_col(instance, scb);
    }

    /* column: xsdurl
     * HARD-WIRED TO
     *   /xsd/modname.xsd OR /xsd/modname@modversion.xsd
     */
    ses_putstr(instance, scb, (const xmlChar *)"\n    '/xsd/");
    if (cp->unified && !mod->ismod) {
        ses_putstr(instance, scb, mod->belongs);
    } else {
        ses_putstr(instance, scb, mod->name);
    }
    if (cp->versionnames && mod->version) {
        ses_putchar(instance, scb, YANG_FILE_SEPCHAR);
        ses_putstr(instance, scb, mod->version);
    }
    ses_putstr(instance, scb, (const xmlChar *)".xsd',");

    /* column: docurl -- not hard-wired, based on cp */
    write_docurl(instance, scb, mod, cp, NULL, 0);

    /* column: srcurl 
     * HARD-WIRED TO
     *   /src/modname.yang
     *  OR
     *   /src/modname@modversion.yang 
     */
    ses_putstr(instance, scb, (const xmlChar *)"\n    '/src/");
    if (cp->unified && !mod->ismod) {
        ses_putstr(instance, scb, mod->belongs);
    } else {
        ses_putstr(instance, scb, mod->name);
    }
    if (cp->versionnames && mod->version) {
        ses_putchar(instance,  scb,  YANG_FILE_SEPCHAR);
        ses_putstr(instance, scb, mod->version);
    }
    ses_putstr(instance, scb, (const xmlChar *)".yang',");

    /* column: sourcespec */
    sprintf(buff, "\n    '%s',", mod->source);
    ses_putstr(instance, scb, (const xmlChar *)buff);

    /* column: sourcename */
    sprintf(buff, "\n    '%s',", mod->sourcefn);
    ses_putstr(instance, scb, (const xmlChar *)buff);

    /* columns: iscurrent, islatest **** !!!! ****/
    ses_putstr(instance, scb, (const xmlChar *)" '1', '1',");

    /* columns: ismod, isyang */
    sprintf(buff, " '%u', '1',",
            (mod->ismod) ? 1 : 0);
    ses_putstr(instance, scb, (const xmlChar *)buff);

    /* columns: created_on, updated_on */
    write_end_tstamps(instance, scb, buff);

    /* end the VALUES clause */
    ses_putstr(instance, scb, (const xmlChar *)");");

}  /* write_module_entry */


/********************************************************************
* FUNCTION write_typedef_entry
* 
* Generate the SQL code to add the typ_template_t to 
* the 'typedef' table
*
* INPUTS:
*   mod == module in progress
*   typ == type template to process
*   cp == conversion parameters to use
*   scb == session control block to use for writing
*   buff == scratch buff to use
*
*********************************************************************/
static void
    write_typedef_entry (ncx_instance_t *instance,
                         const ncx_module_t *mod,
                         const typ_template_t *typ,
                         const yangdump_cvtparms_t *cp,
                         ses_cb_t *scb,
                         char *buff)
{
    const typ_template_t  *parenttyp;

    ses_putstr(instance, scb, (const xmlChar *)"\n\nINSERT INTO nctypedef VALUES (");

    /* columns: ID, modname, submodname, version, name, linenum */
    write_first_tuple(instance, scb, mod, typ->name, typ->tkerr.linenum, buff);

    /* columns: description, reference */
    write_descr_ref(instance, scb, typ->descr, typ->ref);

    /* column: docurl */
    write_docurl(instance, scb, mod, cp, typ->name, typ->tkerr.linenum);

    /* column: basetypename */
    sprintf(buff, "'%s', ", (const char *)typ_get_basetype_name(instance, typ));
    ses_putstr(instance, scb, (const xmlChar *)buff);

    /* column: parenttypename */
    sprintf(buff, "'%s', ", (const char *)typ_get_parenttype_name(instance, typ));
    ses_putstr(instance, scb, (const xmlChar *)buff);

    /* columns: parentmodname, parentlinenum */
    parenttyp = typ_get_parent_type(instance, typ);
    if (parenttyp) {
        sprintf(buff, "\n    '%s', '%u', ", 
                parenttyp->tkerr.mod->name, 
                parenttyp->tkerr.linenum);
        ses_putstr(instance, scb, (const xmlChar *)buff);
    } else {
        write_empty_col(instance, scb);
        write_empty_col(instance, scb);
    }

    /* columns: iscurrent, islatest **** !!!! ****/
    ses_putstr(instance, scb, (const xmlChar *)"\n    '1', '1',");

    /* columns: created_on, updated_on */
    write_end_tstamps(instance, scb, buff);

    /* end the VALUES clause */
    ses_putstr(instance, scb, (const xmlChar *)");");

}  /* write_typedef_entry */


/********************************************************************
* FUNCTION write_grouping_entry
* 
* Generate the SQL code to add the grp_template_t to 
* the 'ncgrouping' table
*
* INPUTS:
*   mod == module in progress
*   grp == grouping template to process
*   cp == conversion parameters to use
*   scb == session control block to use for writing
*   buff == scratch buff to use
*
*********************************************************************/
static void
    write_grouping_entry (ncx_instance_t *instance,
                          const ncx_module_t *mod,
                          const grp_template_t *grp,
                          const yangdump_cvtparms_t *cp,
                          ses_cb_t *scb,
                          char *buff)
{
    ses_putstr(instance, scb, (const xmlChar *)"\n\nINSERT INTO ncgrouping VALUES (");

    /* columns: ID, modname, submodname, version, name, linenum */
    write_first_tuple(instance, scb, mod, grp->name, grp->tkerr.linenum, buff);

    /* columns: description, reference */
    write_descr_ref(instance, scb, grp->descr, grp->ref);

    /* column: docurl */
    write_docurl(instance, scb, mod, cp, grp->name, grp->tkerr.linenum);

    /* column: object list */
    write_object_list(instance, scb, &grp->datadefQ);

    /* columns: iscurrent, islatest **** !!!! ****/
    ses_putstr(instance, scb, (const xmlChar *)"\n    '1', '1',");

    /* columns: created_on, updated_on */
    write_end_tstamps(instance, scb, buff);

    /* end the VALUES clause */
    ses_putstr(instance, scb, (const xmlChar *)");");

}  /* write_grouping_entry */


/********************************************************************
* FUNCTION write_object_entry
* 
* Generate the SQL code to add the obj_template_t to 
* the 'ncobject' table
*
* INPUTS:
*   mod == module in progress
*   obj == object template to process
*   cp == conversion parameters to use
*   scb == session control block to use for writing
*   buff == scratch buff to use
*
*********************************************************************/
static void
    write_object_entry (ncx_instance_t *instance,
                        const ncx_module_t *mod,
                        obj_template_t *obj,
                        const yangdump_cvtparms_t *cp,
                        ses_cb_t *scb,
                        char *buff)
{
    dlq_hdr_t              *datadefQ;
    obj_template_t         *chobj;
    const xmlChar          *name, *defval;

    if (!obj_has_name(instance, obj)) {
        return;
    }

    name = obj_get_name(instance, obj);
    datadefQ = obj_get_datadefQ(instance, obj);

    ses_putstr(instance, scb, (const xmlChar *)"\n\nINSERT INTO ncobject VALUES (");

    /* columns: ID, modname, submodname, version, name, linenum */
    write_first_tuple(instance, scb, mod, name, obj->tkerr.linenum, buff);

    /* column: objectid */
    write_object_id(instance, scb, obj, buff);

    /* columns: description, reference */
    write_descr_ref(instance, scb, obj_get_description(instance, obj),
                    obj_get_reference(instance, obj));

    /* column: docurl */
    write_docurl(instance, scb, mod, cp, name, obj->tkerr.linenum);

    /* column: objtyp */
    sprintf(buff, "\n    '%s', ", (const char *)obj_get_typestr(instance, obj));
    ses_putstr(instance, scb, (const xmlChar *)buff);

    /* column: parentid */
    if (obj->parent && !obj_is_root(obj->parent)) {
        write_object_id(instance, scb, obj->parent, buff);
    } else {
        write_empty_col(instance, scb);
    }

    /* column: istop */
    sprintf(buff, 
            "\n    '%u',",  
            (obj->parent && !obj_is_root(obj->parent)) ? 0 : 1);
    ses_putstr(instance, scb, (const xmlChar *)buff);

    /* column: isdata -- config DB data only */
    sprintf(buff, "\n    '%u',", obj_is_data_db(instance, obj) ? 1 : 0);
    ses_putstr(instance, scb, (const xmlChar *)buff);

    /* column: typename, or groupname if uses */
    switch (obj->objtype) {
    case OBJ_TYP_LEAF:
        sprintf(buff, "\n    '%s',", 
                typ_get_name(instance, obj->def.leaf->typdef));
        break;
    case OBJ_TYP_LEAF_LIST:
        sprintf(buff, "\n    '%s',", 
                typ_get_name(instance, obj->def.leaflist->typdef));
        break;
    case OBJ_TYP_USES:
        sprintf(buff, "\n    '%s',", obj->def.uses->grp->name);
        break;
    default:
        sprintf(buff, "\n    '',");
    }
    ses_putstr(instance, scb, (const xmlChar *)buff);

    /* column: augwhen */
    if (obj->augobj && obj->augobj->when &&
        obj->augobj->when->exprstr) {
        ses_putstr(instance, scb, (const xmlChar *)"\n    '");
        write_cstring(instance, scb, obj->augobj->when->exprstr);
        ses_putstr(instance, scb, (const xmlChar *)"',");
    } else {
        write_empty_col(instance, scb);
    }

    /* column: childlist */
    if (datadefQ) {
        write_object_list(instance, scb, datadefQ);
    } else {
        write_empty_col(instance, scb);
    }

    /* column: defval */
    defval = obj_get_default(instance, obj);
    if (defval) {
        sprintf(buff, "\n    '%s',",  defval);  
        ses_putstr(instance, scb, (const xmlChar *)buff);
    } else {
        write_empty_col(instance, scb);
    }

    /* column: listkey */
    if (obj_get_keystr(instance, obj) != NULL) {
        sprintf(buff, "\n    '%s',", obj_get_keystr(instance, obj));
        ses_putstr(instance, scb, (const xmlChar *)buff);
    } else {
        write_empty_col(instance, scb);
    }   
   
    /* columns: config, mandatory, level */
    sprintf(buff, "\n    '%u', '%u', '%u',",
            obj_get_config_flag(instance, obj) ? 1 : 0,
            obj_is_mandatory(instance, obj) ? 1 : 0,
            obj_get_level(instance, obj));
    ses_putstr(instance, scb, (const xmlChar *)buff);

    /* columns: minelements, maxelements */
    if (obj->objtype == OBJ_TYP_LEAF_LIST) {
        sprintf(buff, "\n    '%u', '%u',",
                obj->def.leaflist->minelems,
                obj->def.leaflist->maxelems);
        ses_putstr(instance, scb, (const xmlChar *)buff);
    } else if (obj->objtype == OBJ_TYP_LIST) {
        sprintf(buff, "\n    '%u', '%u',",
                obj->def.list->minelems,
                obj->def.list->maxelems);
        ses_putstr(instance, scb, (const xmlChar *)buff);
    } else {
        ses_putstr(instance, scb, (const xmlChar *)"\n    '0', '0',");
    }
    
    /* columns: iscurrent, islatest **** !!!! ****/
    ses_putstr(instance, scb, (const xmlChar *)"\n    '1', '1',");

    /* columns: created_on, updated_on */
    write_end_tstamps(instance, scb, buff);

    /* end the VALUES clause */
    ses_putstr(instance, scb, (const xmlChar *)");");

    /* write an entry for each child node */
    if (datadefQ) {
        for (chobj = (obj_template_t *)dlq_firstEntry(instance, datadefQ);
             chobj != NULL;
             chobj = (obj_template_t *)dlq_nextEntry(instance, chobj)) {
            write_object_entry(instance, mod, chobj, cp, scb, buff);
        }
    }

}  /* write_object_entry */


/********************************************************************
* FUNCTION write_extension_entry
* 
* Generate the SQL code to add the ext_template_t to 
* the 'ncextension' table
*
* INPUTS:
*   mod == module in progress
*   ext == extension template to process
*   cp == conversion parameters to use
*   scb == session control block to use for writing
*   buff == scratch buff to use
*
*********************************************************************/
static void
    write_extension_entry (ncx_instance_t *instance,
                           const ncx_module_t *mod,
                           const ext_template_t *ext,
                           const yangdump_cvtparms_t *cp,
                           ses_cb_t *scb,
                           char *buff)
{
    ses_putstr(instance, scb, (const xmlChar *)"\n\nINSERT INTO ncextension VALUES (");

    /* columns: ID, modname, submodname, version, myid, myname, linenum */
    write_first_tuple(instance, scb, mod, ext->name, ext->tkerr.linenum, buff);

    /* columns: description, reference */
    write_descr_ref(instance, scb, ext->descr, ext->ref);

    /* column: docurl */
    write_docurl(instance, scb, mod, cp, ext->name, ext->tkerr.linenum);

    /* columns: argument, yinelement */
    if (ext->arg) {
        sprintf(buff, "\n    '%s', '%u',",
                (const char *)ext->arg, 
                (ext->argel) ? 1 : 0);
        ses_putstr(instance, scb, (const xmlChar *)buff); 
    } else {    
        ses_putstr(instance, scb, (const xmlChar *)"\n    '', '0',");
    }

    /* columns: iscurrent, islatest **** !!!! ****/
    ses_putstr(instance, scb, (const xmlChar *)"\n    '1', '1',");

    /* columns: created_on, updated_on */
    write_end_tstamps(instance, scb, buff);

    /* end the VALUES clause */
    ses_putstr(instance, scb, (const xmlChar *)");");

}  /* write_extension_entry */


/********************************************************************
* FUNCTION convert_one_module
* 
*  Convert the YANG or NCX module to SQL for input to
*  the netconfcentral object dictionary database
*
* INPUTS:
*   mod == module to convert
*   cp == convert parms struct to use
*   scb == session to use for output
*
*********************************************************************/
static void
    convert_one_module (ncx_instance_t *instance,
                        const ncx_module_t *mod,
                        const yangdump_cvtparms_t *cp,
                        ses_cb_t *scb)
{
    const typ_template_t  *typ;
    const grp_template_t  *grp;
    obj_template_t        *obj;
    const ext_template_t  *ext;

#ifdef DEBUG
    /* copy namespace ID if this is a submodule */
    if (!mod->ismod && !mod->nsid) {
        SET_ERROR(instance, ERR_INTERNAL_VAL);
    }
#endif

    /* write the application banner */
    write_banner(instance, mod, scb);

    /* write the ncquickmod table entry */
    write_quick_module_entry(instance, mod, scb, cp->buff);

    /* write the ncmodule table entry */
    write_module_entry(instance, mod, cp, scb, cp->buff);

    /* write the typedef table entries */
    for (typ = (const typ_template_t *)dlq_firstEntry(instance, &mod->typeQ);
         typ != NULL;
         typ = (const typ_template_t *)dlq_nextEntry(instance, typ)) {
        write_typedef_entry(instance, mod, typ, cp, scb, cp->buff);
    }

    /* write the ncgrouping table entries */
    for (grp = (const grp_template_t *)dlq_firstEntry(instance, &mod->groupingQ);
         grp != NULL;
         grp = (const grp_template_t *)dlq_nextEntry(instance, grp)) {
        write_grouping_entry(instance, mod, grp, cp, scb, cp->buff);
    }

    /* write the ncobject table entries */
    for (obj = (obj_template_t *)dlq_firstEntry(instance, &mod->datadefQ);
         obj != NULL;
         obj = (obj_template_t *)dlq_nextEntry(instance, obj)) {

        if (obj_is_hidden(instance, obj)) {
            continue;
        }

        write_object_entry(instance, mod, obj, cp, scb, cp->buff);
    }

    /* write the ncextension table entries */
    for (ext = (const ext_template_t *)dlq_firstEntry(instance, &mod->extensionQ);
         ext != NULL;
         ext = (const ext_template_t *)dlq_nextEntry(instance, ext)) {
        write_extension_entry(instance, mod, ext, cp, scb, cp->buff);
    }

    ses_putchar(instance, scb, '\n');

}   /* convert_one_module */


/*********     E X P O R T E D   F U N C T I O N S    **************/


/********************************************************************
* FUNCTION sql_convert_module
* 
*  Convert the YANG module to SQL for input to
*  the netconfcentral object dictionary database
*
* INPUTS:
*   pcb == parser control block of module to convert
*          This is returned from ncxmod_load_module_ex
*   cp == convert parms struct to use
*   scb == session to use for output
*
* RETURNS:
*   status
*********************************************************************/
status_t
    sql_convert_module (ncx_instance_t *instance,
                        const yang_pcb_t *pcb,
                        const yangdump_cvtparms_t *cp,
                        ses_cb_t *scb)
{
    const ncx_module_t    *mod;
    const yang_node_t     *node;

    /* the module should already be parsed and loaded */
    mod = pcb->top;
    if (!mod) {
        return SET_ERROR(instance, ERR_NCX_MOD_NOT_FOUND);
    }

    convert_one_module(instance, mod, cp, scb);

    if (cp->unified && mod->ismod) {
        for (node = (const yang_node_t *)dlq_firstEntry(instance, &mod->allincQ);
             node != NULL;
             node = (const yang_node_t *)dlq_nextEntry(instance, node)) {
            if (node->submod) {
                convert_one_module(instance, node->submod, cp, scb);
            }
        }
    }

    return NO_ERR;

}   /* sql_convert_module */


/* END file sql.c */
