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
/*  FILE: c_util.c

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
24-oct-09    abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <xmlstring.h>

#include "procdefs.h"
#include "c_util.h"
#include "log.h"
#include "ncx.h"
#include "ncx_str.h"
#include "ncxmod.h"
#include "ncxtypes.h"
#include "ses.h"
#include "status.h"
#include "yangconst.h"
#include "yangdump.h"
#include "yangdump_util.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/
#ifdef DEBUG
/* #define C_UTIL_DEBUG 1 */
#endif


/********************************************************************
*								    *
*			     T Y P E S				    *
*								    *
*********************************************************************/

/* used by write_key_parm */
typedef struct c_keywalker_parms_t_ {
    ses_cb_t    *scb;
    dlq_hdr_t   *objnameQ;
    const xmlChar *parmname;
    uint32       keycount;
    uint32       keynum;
    int32        startindent;
    boolean      done;
} c_keywalker_parms_t;


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/



/********************************************************************
* FUNCTION new_id_string
* 
* Allocate and fill in a constructed identifier to string binding
* for an object struct of some kind
*
* INPUTS:
*   modname == module name to use
*   defname == definition name to use
*   cdefmode == c definition mode
*********************************************************************/
static xmlChar *
    new_id_string (ncx_instance_t *instance,
                   const xmlChar *modname,
                   const xmlChar *defname,
                   c_mode_t cmode)
{
    xmlChar     *buffer, *p;
    uint32       len = 0;

    /* get the idstr length */
    switch (cmode) {
    case  C_MODE_CALLBACK:
        break;
    case C_MODE_VARNAME:
        len++;
        break;
    default:
        len += xml_strlen(instance, Y_PREFIX);
    }

    if (cmode != C_MODE_VARNAME) {
        len += xml_strlen(instance, modname);
    }

    switch (cmode) {
    case C_MODE_OID:
        len += 3;  /* _N_ */
        break;
    case C_MODE_TYPEDEF:
        len += 2;  /* _T */
        break;
    case C_MODE_CALLBACK:
    case C_MODE_VARNAME:
        break;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return NULL;
    }

    len += xml_strlen(instance, defname);

    buffer = m__getMem(instance, len+1);
    if (buffer == NULL) {
        return NULL;
    }

    /* fill in the idstr buffer */
    p = buffer;

    switch (cmode) {
    case  C_MODE_CALLBACK:
        break;
    case C_MODE_VARNAME:
        *p++ = 'k';
        break;
    default:
        p += xml_strcpy(instance, p, Y_PREFIX);
    }

    if (cmode != C_MODE_VARNAME) {
        p += ncx_copy_c_safe_str(p, modname);
    }

    switch (cmode) {
    case C_MODE_OID:
        p += xml_strcpy(instance, p, (const xmlChar *)"_N_");
        break;
    case C_MODE_TYPEDEF:
        p += xml_strcpy(instance, p, (const xmlChar *)"_T");
        break;
    case C_MODE_CALLBACK:
    case C_MODE_VARNAME:
        break;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
    }
    p += ncx_copy_c_safe_str(p, defname);    

#ifdef C_UTIL_DEBUG
    if (LOGDEBUG4) {
        log_debug4(instance,
                   "\nnew-id(%s, %s, %d) = %s",
                   modname, defname, cmode, buffer);
    }
#endif

    return buffer;    /* transfer buffer memory here */

}  /* new_id_string */


/********************************************************************
* FUNCTION free_c_define
* 
* Free an identifier to string binding
*
* INPUTS:
*   cdef == struct to free
*********************************************************************/
static void
    free_c_define (ncx_instance_t *instance, c_define_t *cdef)
{
    if (cdef->idstr != NULL) {
        m__free(instance, cdef->idstr);
    }
    if (cdef->typstr != NULL) {
        m__free(instance, cdef->typstr);
    }
    if (cdef->varstr != NULL) {
        m__free(instance, cdef->varstr);
    }
    if (cdef->valstr != NULL) {
        m__free(instance, cdef->valstr);
    }
    m__free(instance, cdef);

}  /* free_c_define */


/********************************************************************
* FUNCTION new_c_define
* 
* Allocate and fill in a constructed identifier to string binding
* for an object struct of some kind
*
* INPUTS:
*   modname == module name to use
*   defname == definition name to use
*   cdefmode == c definition mode
*********************************************************************/
static c_define_t *
    new_c_define (ncx_instance_t *instance,
                  const xmlChar *modname,
                  const xmlChar *defname,
                  c_mode_t cmode)
{
    c_define_t  *cdef;

    cdef = m__getObj(instance, c_define_t);
    if (cdef == NULL) {
        return NULL;
    }
    memset(cdef, 0x0, sizeof(c_define_t));

    cdef->idstr = new_id_string(instance, modname, defname, cmode);
    if (cdef->idstr == NULL) {
        free_c_define(instance, cdef);
        return NULL;
    }

    if (cmode == C_MODE_CALLBACK) {
        cdef->typstr = new_id_string(instance, modname, defname, C_MODE_TYPEDEF);
        if (cdef->typstr == NULL) {
            free_c_define(instance, cdef);
            return NULL;
        }
        cdef->varstr = new_id_string(instance, modname, defname, C_MODE_VARNAME);
        if (cdef->varstr == NULL) {
            free_c_define(instance, cdef);
            return NULL;
        }
    }
        
    cdef->valstr = xml_strdup(instance, defname);
    if (cdef->valstr == NULL) {
        free_c_define(instance, cdef);
        return NULL;
    }

    return cdef;

}  /* new_c_define */


/********************************************************************
* FUNCTION write_key_parm
* 
* Write the key parameter; walker function
*
* INPUTS:
*   obj == the key object
*   cookie1 == the key walker parameter block to use
*   cookie2 = not used
* RETURNS:
*   TRUE to keep walking
*********************************************************************/
static boolean
    write_key_parm (ncx_instance_t *instance,
                    obj_template_t *obj,
                    void *cookie1,
                    void *cookie2)
{
    c_keywalker_parms_t *parms = (c_keywalker_parms_t *)cookie1;
    xmlChar              endchar;
    boolean              isconst = FALSE;

    (void)cookie2;

    if (parms->done) {
        /* 'done' not used yet -- always walks until the end */
        return FALSE;
    }

    if (++parms->keynum == parms->keycount) {
        endchar = 0;
    } else {
        endchar = ',';
    }

    if (typ_is_string(obj_get_basetype(instance, obj))) {
        isconst = TRUE;
    }

    ses_indent(instance, parms->scb, parms->startindent);
    
    /* write the data type, name and then endchar */
    write_c_objtype_max(instance, parms->scb, obj, parms->objnameQ, endchar,
                        isconst,
                        FALSE, /* needstar */
                        FALSE, /* usename */
                        FALSE, /* useprefix */
                        FALSE, /* isuser */
                        TRUE); /* isvar */

    return TRUE;
} /* write_key_parm */


/********************************************************************
* FUNCTION write_key_var
* 
* Write the key variable; walker function
*
* INPUTS:
*   obj == the key object
*   cookie1 == the key walker parameter block to use
*   cookie2 = not used
* RETURNS:
*   TRUE to keep walking
*********************************************************************/
static boolean
    write_key_var (ncx_instance_t *instance,
                   obj_template_t *obj,
                   void *cookie1,
                   void *cookie2)
{
    c_keywalker_parms_t *parms = (c_keywalker_parms_t *)cookie1;
    xmlChar endchar = 0;
    boolean isconst = typ_is_string(obj_get_basetype(instance, obj)) ? TRUE : FALSE;
    boolean notunion = (obj_get_basetype(instance, obj) != NCX_BT_UNION);

    (void)cookie2;

    if (parms->done) {
        /* 'done' not used yet -- always walks until the end */
        return FALSE;
    }

    ses_indent(instance, parms->scb, parms->startindent);
    
    /* write the data type, name and then endchar */
    write_c_objtype_max(instance, parms->scb, obj, parms->objnameQ, endchar,
                        isconst,
                        FALSE, /* needstar */
                        FALSE, /* usename */
                        FALSE, /* useprefix */
                        FALSE, /* isuser */
                        TRUE); /* isvar */
    ses_putstr(instance, parms->scb, (const xmlChar *)" = ");
    if (notunion) {
        write_c_val_macro_type(instance, parms->scb, obj);
        ses_putchar(instance, parms->scb, '(');
    }
    ses_putstr(instance, parms->scb, (const xmlChar *)"agt_get_key_value(");
    ses_putstr(instance, parms->scb, parms->parmname);
    ses_putstr(instance, parms->scb, (const xmlChar *)", &lastkey)");
    if (notunion) {
        ses_putchar(instance, parms->scb, ')');
    }
    ses_putchar(instance, parms->scb, ';');

    return TRUE;
} /* write_key_var */


/********************************************************************
* FUNCTION write_key_value
* 
* Write the key value get-function-call; walker function
*
* INPUTS:
*   obj == the key object
*   cookie1 == the key walker parameter block to use
*   cookie2 = not used
* RETURNS:
*   TRUE to keep walking
*********************************************************************/
static boolean
    write_key_value (ncx_instance_t *instance,
                     obj_template_t *obj,
                     void *cookie1,
                     void *cookie2)
{
    c_keywalker_parms_t *parms = (c_keywalker_parms_t *)cookie1;
    const c_define_t    *cdef = NULL;    
    xmlChar              endchar;

    (void)cookie2;

    if (++parms->keynum == parms->keycount) {
        endchar = 0;
    } else {
        endchar = ',';
    }

    ses_indent(instance, parms->scb, parms->startindent);
    /* write the variable name and then endchar */
    cdef = find_path_cdefine(instance, parms->objnameQ, obj);
    if (cdef == NULL) {
        SET_ERROR(instance, ERR_INTERNAL_VAL);
    } else {
        ses_putstr(instance, parms->scb, cdef->varstr);
        if (endchar) {
            ses_putchar(instance, parms->scb, endchar);
        }
    }

    return TRUE;
} /* write_key_value */


/**************    E X T E R N A L   F U N C T I O N S **********/


/********************************************************************
* FUNCTION need_rpc_includes
* 
* Check if the include-stmts for RPC methods are needed
*
* INPUTS:
*   mod == module in progress
*   cp == conversion parameters to use
*
* RETURNS:
*  TRUE if RPCs found
*  FALSE if no RPCs found
*********************************************************************/
boolean
    need_rpc_includes (ncx_instance_t *instance,
                       const ncx_module_t *mod,
                       const yangdump_cvtparms_t *cp)
{
    const ncx_include_t *inc;

    if (obj_any_rpcs(instance, &mod->datadefQ)) {
        return TRUE;
    }

    if (cp->unified) {
        for (inc = (const ncx_include_t *)
                 dlq_firstEntry(instance, &mod->includeQ);
             inc != NULL;
             inc = (const ncx_include_t *)dlq_nextEntry(instance, inc)) {

            if (inc->submod && obj_any_rpcs(instance, &inc->submod->datadefQ)) {
                return TRUE;
            }
        }
    }

    return FALSE;

} /* need_rpc_includes */


/********************************************************************
* FUNCTION need_notif_includes
* 
* Check if the include-stmts for notifications are needed
*
* INPUTS:
*   mod == module in progress
*   cp == conversion parameters to use
*
* RETURNS:
*   TRUE if notifcations found
*   FALSE if no notifications found
*********************************************************************/
boolean
    need_notif_includes (ncx_instance_t *instance,
                         const ncx_module_t *mod,
                         const yangdump_cvtparms_t *cp)
{
    const ncx_include_t *inc;

    if (obj_any_notifs(instance, &mod->datadefQ)) {
        return TRUE;
    }

    if (cp->unified) {
        for (inc = (const ncx_include_t *)
                 dlq_firstEntry(instance, &mod->includeQ);
             inc != NULL;
             inc = (const ncx_include_t *)dlq_nextEntry(instance, inc)) {

            if (inc->submod && obj_any_notifs(instance, &inc->submod->datadefQ)) {
                return TRUE;
            }
        }
    }

    return FALSE;

} /* need_notif_includes */


/********************************************************************
* FUNCTION write_c_safe_str
* 
* Generate a string token at the current line location
*
* INPUTS:
*   scb == session control block to use for writing
*   strval == string value
*********************************************************************/
void
    write_c_safe_str (ncx_instance_t *instance,
                      ses_cb_t *scb,
                      const xmlChar *strval)
{
    const xmlChar *s;

    s = strval;
    while (*s) {
        if (*s == '.' || *s == '-' || *s == NCXMOD_PSCHAR) {
            ses_putchar(instance, scb, '_');
        } else {
            ses_putchar(instance, scb, *s);
        }
        s++;
    }

}  /* write_c_safe_str */


/********************************************************************
* FUNCTION write_c_str
* 
* Generate a string token at the current line location
*
* INPUTS:
*   scb == session control block to use for writing
*   strval == string value
*   quotes == quotes style (0, 1, 2)
*********************************************************************/
void
    write_c_str (ncx_instance_t *instance,
                 ses_cb_t *scb,
                 const xmlChar *strval,
                 uint32 quotes)
{
    switch (quotes) {
    case 1:
        ses_putchar(instance, scb, '\'');
        break;
    case 2:
        ses_putchar(instance, scb, '"');
        break;
    default:
        ;
    }

    ses_putstr(instance, scb, strval);

    switch (quotes) {
    case 1:
        ses_putchar(instance, scb, '\'');
        break;
    case 2:
        ses_putchar(instance, scb, '"');
        break;
    default:
        ;
    }

}  /* write_c_str */


/********************************************************************
* FUNCTION write_c_simple_str
* 
* Generate a simple clause on 1 line
*
* INPUTS:
*   scb == session control block to use for writing
*   kwname == keyword name
*   strval == string value
*   indent == indent count to use
*   quotes == quotes style (0, 1, 2)
*********************************************************************/
void
    write_c_simple_str (ncx_instance_t *instance,
                        ses_cb_t *scb,
                        const xmlChar *kwname,
                        const xmlChar *strval,
                        int32 indent,
                        uint32 quotes)
{
    ses_putstr_indent(instance, scb, kwname, indent);
    if (strval) {
        ses_putchar(instance, scb, ' ');
        write_c_str(instance, scb, strval, quotes);
    }

}  /* write_c_simple_str */


/********************************************************************
*
* FUNCTION write_identifier
* 
* Generate an identifier
*
*  #module_DEFTYPE_idname
*
* INPUTS:
*   scb == session control block to use for writing
*   modname == module name start-string to use
*   defpart == internal string for deftype part (may be NULL)
*   idname == identifier name
*   isuser == TRUE if USER SIL file
*             FALSE if YUMA SIL file
*********************************************************************/
void
    write_identifier (ncx_instance_t *instance,
                      ses_cb_t *scb,
                      const xmlChar *modname,
                      const xmlChar *defpart,
                      const xmlChar *idname,
                      boolean isuser)
{
    if (isuser) {
        ses_putstr(instance, scb, U_PREFIX);
    } else {
        ses_putstr(instance, scb, Y_PREFIX);
    }
    write_c_safe_str(instance, scb, modname);
    ses_putchar(instance, scb, '_');
    if (defpart != NULL) {
        ses_putstr(instance, scb, defpart);
        ses_putchar(instance, scb, '_');
    }
    write_c_safe_str(instance, scb, idname); /***/

}  /* write_identifier */


/********************************************************************
* FUNCTION write_ext_include
* 
* Generate an include statement for an external file
*
*  #include <foo,h>
*
* INPUTS:
*   scb == session control block to use for writing
*   hfile == H file name == file name to include (foo.h)
*
*********************************************************************/
void
    write_ext_include (ncx_instance_t *instance,
                       ses_cb_t *scb,
                       const xmlChar *hfile)
{
    ses_putstr(instance, scb, POUND_INCLUDE);
    ses_putstr(instance, scb, (const xmlChar *)"<");
    ses_putstr(instance, scb, hfile);
    ses_putstr(instance, scb, (const xmlChar *)">\n");

}  /* write_ext_include */


/********************************************************************
* FUNCTION write_ncx_include
* 
* Generate an include statement for an NCX file
*
*  #ifndef _H_foo
*  #include "foo,h"
*
* INPUTS:
*   scb == session control block to use for writing
*   modname == module name to include (foo)
*
*********************************************************************/
void
    write_ncx_include (ncx_instance_t *instance,
                       ses_cb_t *scb,
                       const xmlChar *modname)
{
    ses_putstr(instance, scb, POUND_INCLUDE);
    ses_putchar(instance, scb, '"');
    ses_putstr(instance, scb, modname);
    ses_putstr(instance, scb, (const xmlChar *)".h\"");

}  /* write_ncx_include */


/********************************************************************
* FUNCTION write_cvt_include
* 
* Generate an include statement for an NCX split SIL file
* based on the format type
*
* INPUTS:
*   scb == session control block to use for writing
*   modname == module name to include (foo)
*   cvttyp == format enum to use
*********************************************************************/
void
    write_cvt_include (ncx_instance_t *instance,
                       ses_cb_t *scb,
                       const xmlChar *modname,
                       ncx_cvttyp_t cvttyp)
{
    ses_putstr(instance, scb, POUND_INCLUDE);
    ses_putchar(instance, scb, '"');
    switch (cvttyp) {
    case NCX_CVTTYP_C:
    case NCX_CVTTYP_CPP_TEST:
    case NCX_CVTTYP_H:
        break;
    case NCX_CVTTYP_UC:
    case NCX_CVTTYP_UH:
        ses_putstr(instance, scb, NCX_USER_SIL_PREFIX);
        break;
    case NCX_CVTTYP_YC:
    case NCX_CVTTYP_YH:
        ses_putstr(instance, scb, NCX_YUMA_SIL_PREFIX);
        break;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    ses_putstr(instance, scb, modname);
    ses_putstr(instance, scb, (const xmlChar *)".h\"");

}  /* write_cvt_include */


/********************************************************************
* FUNCTION write_qheader
* 
* Generate a QHEADER with indentation
*
*  \n{indentcnt}QHEADER
*
* INPUTS:
*   scb == session control block to use for writing
*
*********************************************************************/
void
    write_qheader (ncx_instance_t *instance, ses_cb_t *scb)
{
    ses_indent(instance, scb, ses_indent_count(scb));
    ses_putstr(instance, scb, QHEADER);

}  /* write_qheader */


/********************************************************************
* FUNCTION save_oid_cdefine
* 
* Generate a #define binding for a definition and save it in the
* specified Q of c_define_t structs
*
* INPUTS:
*   cdefineQ == Q of c_define_t structs to use
*   modname == module name to use
*   defname == object definition name to use
*
* OUTPUTS:
*   a new c_define_t is allocated and added to the cdefineQ
*   if returning NO_ERR;
*
* RETURNS:
*   status; duplicate C identifiers not supported yet
*      foo-1 -->  foo_1
*      foo.1 -->  foo_1
*   An error message will be generated if this type of error occurs
*********************************************************************/
status_t
    save_oid_cdefine (ncx_instance_t *instance,
                      dlq_hdr_t *cdefineQ,
                      const xmlChar *modname,
                      const xmlChar *defname)
{
    c_define_t    *newcdef, *testcdef;
    status_t       res;
    int32          retval;

#ifdef DEBUG
    if (cdefineQ == NULL || modname == NULL || defname == NULL) {
        return SET_ERROR(instance, ERR_INTERNAL_PTR);
    }
#endif

    newcdef = new_c_define(instance, modname, defname, C_MODE_OID);
    if (newcdef == NULL) {
        return ERR_INTERNAL_MEM;
    }

    /* keep the cdefineQ sorted by the idstr value */
    res = NO_ERR;
    for (testcdef = (c_define_t *)dlq_firstEntry(instance, cdefineQ);
         testcdef != NULL;
         testcdef = (c_define_t *)dlq_nextEntry(instance, testcdef)) {

        retval = xml_strcmp(instance, newcdef->idstr, testcdef->idstr);
        if (retval == 0) {
            if (xml_strcmp(instance, newcdef->valstr, testcdef->valstr)) {
                /* error - duplicate ID with different original value */
                res = ERR_NCX_INVALID_VALUE;
                log_error(instance,
                          "\nError: C idenitifer conflict between "
                          "'%s' and '%s'",
                          newcdef->valstr,
                          testcdef->valstr);
            } /* else duplicate is not a problem */
            free_c_define(instance, newcdef);
            return res;
        } else if (retval < 0) {
            dlq_insertAhead(instance, newcdef, testcdef);
            return NO_ERR;
        } /* else try next entry */
    }

    /* new last entry */
    dlq_enque(instance, newcdef, cdefineQ);
    return NO_ERR;

}  /* save_oid_cdefine */


/********************************************************************
* FUNCTION save_path_cdefine
* 
* Generate a #define binding for a definition and save it in the
* specified Q of c_define_t structs
*
* INPUTS:
*   cdefineQ == Q of c_define_t structs to use
*   modname == base module name to use
*   obj == object struct to use to generate path
*   cmode == mode to use
*
* OUTPUTS:
*   a new c_define_t is allocated and added to the cdefineQ
*   if returning NO_ERR;
*
* RETURNS:
*   status; duplicate C identifiers not supported yet
*      foo-1/a/b -->  foo_1_a_b
*      foo.1/a.2 -->  foo_1_a_b
*   An error message will be generated if this type of error occurs
*********************************************************************/
status_t
    save_path_cdefine (ncx_instance_t *instance,
                       dlq_hdr_t *cdefineQ,
                       const xmlChar *modname,
                       obj_template_t *obj,
                       c_mode_t cmode)
{
    c_define_t    *newcdef, *testcdef;
    xmlChar       *buffer;
    status_t       res;
    int32          retval;

#ifdef DEBUG
    if (cdefineQ == NULL || modname == NULL || obj == NULL) {
        return SET_ERROR(instance, ERR_INTERNAL_PTR);
    }
#endif

    buffer = NULL;
    res = obj_gen_object_id(instance, obj, &buffer);
    if (res != NO_ERR) {
        return res;
    }

    newcdef = new_c_define(instance, modname, buffer, cmode);
    m__free(instance, buffer);
    buffer = NULL;
    if (newcdef == NULL) {
        return ERR_INTERNAL_MEM;
    }
    newcdef->obj = obj;

    /* keep the cdefineQ sorted by the idstr value */
    res = NO_ERR;
    for (testcdef = (c_define_t *)dlq_firstEntry(instance, cdefineQ);
         testcdef != NULL;
         testcdef = (c_define_t *)dlq_nextEntry(instance, testcdef)) {

        retval = xml_strcmp(instance, newcdef->idstr, testcdef->idstr);
        if (retval == 0) {
            if (xml_strcmp(instance, newcdef->valstr, testcdef->valstr)) {
                /* error - duplicate ID with different original value */
                res = ERR_NCX_INVALID_VALUE;
                log_error(instance,
                          "\nError: C idenitifer conflict between "
                          "'%s' and '%s'",
                          newcdef->valstr,
                          testcdef->valstr);
            } /* else duplicate is not a problem */
            free_c_define(instance, newcdef);
            return res;
        } else if (retval < 0) {
            dlq_insertAhead(instance, newcdef, testcdef);
            return NO_ERR;
        } /* else try next entry */
    }

    /* new last entry */
    dlq_enque(instance, newcdef, cdefineQ);
    return NO_ERR;

}  /* save_path_cdefine */


/********************************************************************
* FUNCTION find_path_cdefine
* 
* Find a #define binding for a definition in the
* specified Q of c_define_t structs
*
* INPUTS:
*   cdefineQ == Q of c_define_t structs to use
*   obj == object struct to find
*
* RETURNS:
*   pointer to found entry
*   NULL if not found
*********************************************************************/
c_define_t *
    find_path_cdefine (ncx_instance_t *instance,
                       dlq_hdr_t *cdefineQ,
                       const obj_template_t *obj)
{
    c_define_t    *testcdef;

#ifdef DEBUG
    if (cdefineQ == NULL || obj == NULL) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return NULL;
    }
#endif

    /* find the cdefineQ sorted by the object struct back-ptr */
    for (testcdef = (c_define_t *)dlq_firstEntry(instance, cdefineQ);
         testcdef != NULL;
         testcdef = (c_define_t *)dlq_nextEntry(instance, testcdef)) {
        if (testcdef->obj == obj) {
            return testcdef;
        }
    }
    return NULL;

}  /* find_path_cdefine */


/********************************************************************
* FUNCTION clean_cdefineQ
* 
* Clean a Q of c_define_t structs
*
* INPUTS:
*   cdefineQ == Q of c_define_t structs to use
*********************************************************************/
void
    clean_cdefineQ (ncx_instance_t *instance, dlq_hdr_t *cdefineQ)
{
    c_define_t    *cdef;

#ifdef DEBUG
    if (cdefineQ == NULL) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    while (!dlq_empty(instance, cdefineQ)) {
        cdef = (c_define_t *)dlq_deque(instance, cdefineQ);
        free_c_define(instance, cdef);
    }

}  /* clean_cdefineQ */


/********************************************************************
* FUNCTION write_c_header
* 
* Write the C file header
*
* INPUTS:
*   scb == session control block to use for writing
*   mod == module in progress
*   cp == conversion parameters to use
*
*********************************************************************/
void
    write_c_header (ncx_instance_t *instance,
                    ses_cb_t *scb,
                    const ncx_module_t *mod,
                    const yangdump_cvtparms_t *cp)
{
    int32                     indent;

    indent = cp->indent;

    /* banner comments */
    ses_putstr(instance, scb, START_COMMENT);

    ses_putstr(instance, scb, COPYRIGHT_HEADER);

    /* generater tag */
    write_banner_session_ex(instance, scb, FALSE);

    /* module format */
    switch (cp->format) {
    case NCX_CVTTYP_C:
    case NCX_CVTTYP_CPP_TEST:
        ses_putstr_indent(instance, scb, (const xmlChar *)"Combined SIL module", 
                          indent);
        break;
    case NCX_CVTTYP_H:
        ses_putstr_indent(instance, scb, (const xmlChar *)"Combined SIL header", 
                          indent);
        break;
    case NCX_CVTTYP_UC:
        ses_putstr_indent(instance, scb, (const xmlChar *)"User SIL module", 
                          indent);
        break;
    case NCX_CVTTYP_UH:
        ses_putstr_indent(instance, scb, (const xmlChar *)"User SIL header", 
                          indent);
        break;
    case NCX_CVTTYP_YC:
        ses_putstr_indent(instance, scb, (const xmlChar *)"Yuma SIL module", 
                          indent);
        break;
    case NCX_CVTTYP_YH:
        ses_putstr_indent(instance, scb, (const xmlChar *)"Yuma SIL header", 
                          indent);
        break;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    /* module name */
    if (mod->ismod) {
        ses_putstr_indent(instance, scb, YANG_K_MODULE, indent);
    } else {
        ses_putstr_indent(instance, scb, YANG_K_SUBMODULE, indent);
    }
    ses_putchar(instance, scb, ' ');
    ses_putstr(instance, scb, mod->name);

    /* version */
    if (mod->version) {
        write_c_simple_str(instance, scb, YANG_K_REVISION,  mod->version,  indent, 0);
    }

    /* namespace or belongs-to */
    if (mod->ismod) {
        write_c_simple_str(instance, scb, YANG_K_NAMESPACE, mod->ns, indent, 0);
    } else {
        write_c_simple_str(instance, scb, YANG_K_BELONGS_TO, mod->belongs, indent, 0);
    }

    /* organization */
    if (mod->organization) {
        write_c_simple_str(instance, scb, YANG_K_ORGANIZATION,  mod->organization, 
                           indent, 0);
    }

    ses_putchar(instance, scb, '\n');
    ses_putchar(instance, scb, '\n');
    ses_putstr(instance, scb, END_COMMENT);
    ses_putchar(instance, scb, '\n');

} /* write_c_header */


/********************************************************************
* FUNCTION write_c_footer
* 
* Write the C file footer
*
* INPUTS:
*   scb == session control block to use for writing
*   mod == module in progress
*   cp == conversion parameters to use
*********************************************************************/
void
    write_c_footer (ncx_instance_t *instance,
                    ses_cb_t *scb,
                    const ncx_module_t *mod,
                    const yangdump_cvtparms_t *cp)
{
    ses_putstr(instance, scb, (const xmlChar *)"\n/* END ");
    switch (cp->format) {
    case NCX_CVTTYP_C:
    case NCX_CVTTYP_CPP_TEST:
    case NCX_CVTTYP_H:
        break;
    case NCX_CVTTYP_UC:
    case NCX_CVTTYP_UH:
        ses_putstr(instance, scb, U_PREFIX);
        break;
    case NCX_CVTTYP_YC:
    case NCX_CVTTYP_YH:
        ses_putstr(instance, scb, Y_PREFIX);
        break;
    default:
        ;
    }

    write_c_safe_str(instance, scb, ncx_get_modname(mod));
    ses_putstr(instance, scb, (const xmlChar *)".c */\n");

} /* write_c_footer */


/*******************************************************************
* FUNCTION write_c_objtype
* 
* Generate the C data type for the NCX data type
*
* INPUTS:
*   scb == session control block to use for writing
*   obj == object template to check
*
**********************************************************************/
void
    write_c_objtype (ncx_instance_t *instance,
                     ses_cb_t *scb,
                     const obj_template_t *obj)
{
    write_c_objtype_max(instance, scb, obj, NULL, ';', FALSE, FALSE,
                        TRUE, FALSE, FALSE, FALSE);

}  /* write_c_objtype */


/*******************************************************************
* FUNCTION write_c_objtype_ex
* 
* Generate the C data type for the NCX data type
*
* INPUTS:
*   scb == session control block to use for writing
*   obj == object template to check
*   cdefQ == Q of c_define_t to check for obj
*   endchar == char to use at end (semi-colon, comma, right-paren)
*   isconst == TRUE if a const pointer is needed
*              FALSE if pointers should not be 'const'
*   needstar == TRUE if this object reference is a reference
*               or a pointer to the data type
*               FALSE if this is a direct usage of the object data type
*               !! only affects complex types, not simple types
**********************************************************************/
void
    write_c_objtype_ex (ncx_instance_t *instance,
                        ses_cb_t *scb,
                        const obj_template_t *obj,
                        dlq_hdr_t *cdefQ,
                        xmlChar endchar,
                        boolean isconst,
                        boolean needstar)
{
    write_c_objtype_max(instance, scb, obj, cdefQ, endchar, isconst, needstar,
                        TRUE, FALSE, FALSE, FALSE);
}  /* write_c_objtype_ex */


/*******************************************************************
* FUNCTION write_c_objtype_max
* 
* Generate the C data type for the NCX data type
*
* INPUTS:
*   scb == session control block to use for writing
*   obj == object template to check
*   cdefQ == Q of c_define_t to check for obj
*   endchar == char to use at end (semi-colon, comma, right-paren)
*   isconst == TRUE if a const pointer is needed
*              FALSE if pointers should not be 'const'
*   needstar == TRUE if this object reference is a reference
*               or a pointer to the data type
*               FALSE if this is a direct usage of the object data type
*               !! only affects complex types, not simple types
*   usename == TRUE to use object name as the variable name
*              FALSE to use the idstr as the variable name
*   useprefix == TRUE to use object name as the variable name
*              FALSE to use the idstr as the variable name;
*              ignored if usename == TRUE
*   isuser == TRUE if format is NCX_CVTTYP_UC or NCX_CVTTYP_UH
*             FALSE otherwise;  ignored if useprefix == FALSE
*   isvar == TRUE if cdef->varstr should be used
*            FALSE if cdef->idstr should be used;
*            ignored if usename == TRUE
**********************************************************************/
void
    write_c_objtype_max (ncx_instance_t *instance,
                         ses_cb_t *scb,
                         const obj_template_t *obj,
                         dlq_hdr_t *cdefQ,
                         xmlChar endchar,
                         boolean isconst,
                         boolean needstar,
                         boolean usename,
                         boolean useprefix,
                         boolean isuser,
                         boolean isvar)
{
    const c_define_t *cdef = NULL;
    boolean        needspace;
    ncx_btype_t    btyp;


#ifdef DEBUG
    if (scb == NULL || obj == NULL) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    needspace = TRUE;

    btyp = obj_get_basetype(instance, obj);

    if (typ_is_simple(instance, btyp)) {
        needstar = FALSE;
    }

    switch (btyp) {
    case NCX_BT_EMPTY:
    case NCX_BT_BOOLEAN:
        ses_putstr(instance, scb, BOOLEAN);
        break;
    case NCX_BT_INT8:
        ses_putstr(instance, scb, INT8);
        break;
    case NCX_BT_INT16:
        ses_putstr(instance, scb, INT16);
        break;
    case NCX_BT_INT32:
        ses_putstr(instance, scb, INT32);
        break;
    case NCX_BT_INT64:
        ses_putstr(instance, scb, INT64);
        break;
    case NCX_BT_UINT8:
        ses_putstr(instance, scb, UINT8);
        break;
    case NCX_BT_UINT16:
        ses_putstr(instance, scb, UINT16);
        break;
    case NCX_BT_UINT32:
        ses_putstr(instance, scb, UINT32);
        break;
    case NCX_BT_UINT64:
        ses_putstr(instance, scb, UINT64);
        break;
    case NCX_BT_DECIMAL64:
        ses_putstr(instance, scb, INT64);
        break;
    case NCX_BT_FLOAT64:
        ses_putstr(instance, scb, DOUBLE);
        break;
    case NCX_BT_ENUM:
    case NCX_BT_STRING:
    case NCX_BT_BINARY:
    case NCX_BT_INSTANCE_ID:
    case NCX_BT_LEAFREF:
    case NCX_BT_SLIST:
        if (isconst) {
            ses_putstr(instance, scb, (const xmlChar *)"const ");
        }
        ses_putstr(instance, scb, STRING);
        needspace = FALSE;
        break;
    case NCX_BT_IDREF:
        if (isconst) {
            ses_putstr(instance, scb, (const xmlChar *)"const ");
        }
        ses_putstr(instance, scb, IDREF);
        needspace = FALSE;
        break;
    case NCX_BT_LIST:
        ses_putstr(instance, scb, QUEUE);
        break;
    case NCX_BT_UNION:
        if (isconst) {
            ses_putstr(instance, scb, (const xmlChar *)"const ");
        }
        ses_putstr(instance, scb, (const xmlChar *)"xmlChar");
        needstar = TRUE;
        break;
    case NCX_BT_BITS:
        ses_putstr(instance, scb, BITS);
        break;
    default:
        /* assume complex type */
        if (cdefQ == NULL) {
            SET_ERROR(instance, ERR_INTERNAL_VAL);
        } else {
            cdef = find_path_cdefine(instance, cdefQ, obj);
            if (cdef == NULL) {
                SET_ERROR(instance, ERR_INTERNAL_VAL);
            } else {
                if (cdef->typstr) {
                    ses_putstr(instance, scb, cdef->typstr);
                } else {
                    if (useprefix) {
                        if (isuser) {
                            ses_putstr(instance, scb, U_PREFIX);
                        } else {
                            ses_putstr(instance, scb, Y_PREFIX);
                        }
                    }
                    if (isvar) {
                        ses_putstr(instance, scb, cdef->varstr);
                    } else {
                        ses_putstr(instance, scb, cdef->idstr);
                    }
                }
            }
        }
    }

    if (needspace) {
        ses_putchar(instance, scb, ' ');
    }

    if (needstar) {
        ses_putchar(instance, scb, '*');
    }

    if (usename) {
        write_c_safe_str(instance, scb, obj_get_name(instance, obj));
    } else {
        if (cdef == NULL) {
            if (cdefQ == NULL) {
                SET_ERROR(instance, ERR_INTERNAL_VAL);
            } else {
                cdef = find_path_cdefine(instance, cdefQ, obj);
                if (cdef == NULL) {
                    SET_ERROR(instance, ERR_INTERNAL_VAL);
                }
            }
        }
        if (cdef != NULL) {
            if (useprefix) {
                if (isuser) {
                    ses_putstr(instance, scb, U_PREFIX);
                } else {
                    ses_putstr(instance, scb, Y_PREFIX);
                }
            }
            if (isvar) {
                ses_putstr(instance, scb, cdef->varstr);
            } else {
                ses_putstr(instance, scb, cdef->idstr);
            }
        } /* else generating a syntax error */
    }

    if (endchar != '\0') {
        ses_putchar(instance, scb, endchar);
    }

}  /* write_c_objtype_max */


/*******************************************************************
* FUNCTION write_c_val_macro_type
* 
* Generate the C VAL_FOO macro name for the data type
*
* INPUTS:
*   scb == session control block to use for writing
*   obj == object template to check
*
**********************************************************************/
void
    write_c_val_macro_type (ncx_instance_t *instance,
                            ses_cb_t *scb,
                            const obj_template_t *obj)
{
    ncx_btype_t    btyp;

    btyp = obj_get_basetype(instance, obj);

    switch (btyp) {
    case NCX_BT_BITS:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_BITS");
        break;
    case NCX_BT_EMPTY:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_EMPTY");
        break;
    case NCX_BT_BOOLEAN:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_BOOL");
        break;
    case NCX_BT_INT8:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_INT8");
        break;
    case NCX_BT_INT16:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_INT16");
        break;
    case NCX_BT_INT32:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_INT");
        break;
    case NCX_BT_INT64:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_LONG");
        break;
    case NCX_BT_UINT8:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_UINT8");
        break;
    case NCX_BT_UINT16:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_UINT16");
        break;
    case NCX_BT_UINT32:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_UINT");
        break;
    case NCX_BT_UINT64:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_ULONG");
        break;
    case NCX_BT_DECIMAL64:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_DEC64");
        break;
    case NCX_BT_FLOAT64:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_DOUBLE");
        break;
    case NCX_BT_ENUM:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_ENUM_NAME");
        break;
    case NCX_BT_STRING:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_STRING");
        break;
    case NCX_BT_UNION:
        //ses_putstr(scb, (const xmlChar *)"VAL_STRING");
        break;
    case NCX_BT_BINARY:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_BINARY");
        break;
    case NCX_BT_INSTANCE_ID:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_INSTANCE_ID");
        break;
    case NCX_BT_LEAFREF:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_STRING");
        break;
    case NCX_BT_IDREF:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_IDREF");
        break;
    case NCX_BT_SLIST:
        ses_putstr(instance, scb, (const xmlChar *)"VAL_LIST");
        break;
    case NCX_BT_LIST:
        ses_putstr(instance, scb, QUEUE);
        break;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

}  /* write_c_val_macro_type */


/*******************************************************************
* FUNCTION write_c_oid_comment
* 
* Generate the object OID as a comment line
*
* INPUTS:
*   scb == session control block to use for writing
*   obj == object template to check
*
**********************************************************************/
void
    write_c_oid_comment (ncx_instance_t *instance,
                         ses_cb_t *scb,
                         const obj_template_t *obj)
{
    xmlChar    *buffer;
    status_t    res;

    /* generate the YOID as a comment */
    res = obj_gen_object_id(instance, obj, &buffer);
    if (res != NO_ERR) {
        SET_ERROR(instance, res);
        return;
    }

    ses_putstr(instance, scb, START_COMMENT);
    ses_putstr(instance, scb, obj_get_typestr(instance, obj));
    ses_putchar(instance, scb, ' ');
    ses_putstr(instance, scb, buffer);
    ses_putstr(instance, scb, END_COMMENT);
    m__free(instance, buffer);

} /* write_c_oid_comment */


/********************************************************************
* FUNCTION save_c_objects
* 
* save the path name bindings for C typdefs
*
* INPUTS:
*   mod == module in progress
*   datadefQ == que of obj_template_t to use
*   savecdefQ == Q of c_define_t structs to use
*   cmode == C code generating mode to use
*
* OUTPUTS:
*   savecdefQ may get new structs added
*
* RETURNS:
*  status
*********************************************************************/
status_t
    save_c_objects (ncx_instance_t *instance,
                    ncx_module_t *mod,
                    dlq_hdr_t *datadefQ,
                    dlq_hdr_t *savecdefQ,
                    c_mode_t cmode)
{
    obj_template_t    *obj;
    dlq_hdr_t         *childdatadefQ;
    status_t           res;

    if (dlq_empty(instance, datadefQ)) {
        return NO_ERR;
    }

    for (obj = (obj_template_t *)dlq_firstEntry(instance, datadefQ);
         obj != NULL;
         obj = (obj_template_t *)dlq_nextEntry(instance, obj)) {

        if (!obj_has_name(instance, obj) || obj_is_cli(instance, obj) || obj_is_abstract(instance, obj)) {
            continue;
        }

        res = save_path_cdefine(instance, savecdefQ, ncx_get_modname(mod), obj, cmode);
        if (res != NO_ERR) {
            return res;
        }

        childdatadefQ = obj_get_datadefQ(instance, obj);
        if (childdatadefQ) {
            res = save_c_objects(instance, mod, childdatadefQ, savecdefQ, cmode);
            if (res != NO_ERR) {
                return res;
            }
        }
    }

    return NO_ERR;

}  /* save_c_objects */


/********************************************************************
* FUNCTION save_all_c_objects
* 
* save the path name bindings for C typdefs for mod and all submods
*
* INPUTS:
*   mod == module in progress
*   cp == conversion parameters to use
*   savecdefQ == Q of c_define_t structs to use
*   cmode == C code generating mode to use
*
* OUTPUTS:
*   savecdefQ may get new structs added
*
* RETURNS:
*  status
*********************************************************************/
status_t
    save_all_c_objects (ncx_instance_t *instance,
                        ncx_module_t *mod,
                        const yangdump_cvtparms_t *cp,
                        dlq_hdr_t *savecdefQ,
                        c_mode_t cmode)
{
    status_t res;

    res = save_c_objects(instance, mod, &mod->datadefQ, savecdefQ, cmode);
    if (res == NO_ERR) {
        if (cp->unified && mod->ismod) {
            yang_node_t *node;
            for (node = (yang_node_t *)
                     dlq_firstEntry(instance, &mod->allincQ);
                 node != NULL && res == NO_ERR;
                 node = (yang_node_t *)dlq_nextEntry(instance, node)) {
                if (node->submod) {
                    res = save_c_objects(instance,
                                         node->submod,
                                         &node->submod->datadefQ, 
                                         savecdefQ, cmode);
                }
            }
        }
    }
    return res;

} /* save_all_c_objects */


/********************************************************************
* FUNCTION skip_c_top_object
* 
* Check if a top-level object should be skipped
* in C file SIL code generation
*
* INPUTS:
*   obj == object to check
*
* RETURNS:
*  TRUE if object should be skipped
*  FALSE if object should not be skipped
*********************************************************************/
boolean
    skip_c_top_object (ncx_instance_t *instance, obj_template_t *obj)
{
    if (!obj_has_name(instance, obj) ||
        !obj_is_config(instance, obj) ||
        obj_is_cli(instance, obj) || 
        obj_is_abstract(instance, obj) ||
        obj_is_rpc(instance, obj) ||
        obj_is_notif(instance, obj)) {
        return TRUE;
    }
    return FALSE;
}  /* END skip_c_top_object */


/********************************************************************
* FUNCTION write_c_key_params
* 
* Write all the keys in C function parameter list format
*
* INPUTS:
*   scb == session to use
*   obj == object to start from (ancestor-or-self)
*   objnameQ == Q of name-to-idstr mappings
*   keycount == number of key leafs expected; used to
*               identify last key to suppress ending comma
*   startindent == start indent count
*
*********************************************************************/
void
    write_c_key_params (ncx_instance_t *instance, 
                        ses_cb_t *scb, 
                        obj_template_t *obj, 
                        dlq_hdr_t *objnameQ, 
                        uint32 keycount,
                        int32 startindent)
{
    c_keywalker_parms_t parms;

#ifdef DEBUG
    if (!scb || !obj || !objnameQ) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
    if (keycount == 0) {
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return;
    }
#endif

    parms.scb = scb;
    parms.objnameQ = objnameQ;
    parms.parmname = NULL;
    parms.keycount = keycount;
    parms.keynum = 0;
    parms.startindent = startindent;
    parms.done = FALSE;

    obj_traverse_keys(instance, obj, &parms, NULL, write_key_parm);

} /* write_c_key_params */


/********************************************************************
* FUNCTION write_c_key_vars
* 
* Write all the local key variables in the SIL C function
*
* INPUTS:
*   scb == session to use
*   obj == object to start from (ancestor-or-self)
*   objnameQ == Q of name-to-idstr mappings
*   parmname == name of parameter used in C code
*   keycount == number of key leafs expected; used to
*               identify last key to suppress ending comma
*   startindent == start indent count
*
*********************************************************************/
void
    write_c_key_vars (ncx_instance_t *instance, 
                      ses_cb_t *scb, 
                      obj_template_t *obj, 
                      dlq_hdr_t *objnameQ, 
                      const xmlChar *parmname,
                      uint32 keycount,
                      int32 startindent)
{
    c_keywalker_parms_t parms;

#ifdef DEBUG
    if (!scb || !obj || !objnameQ || !parmname) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
    if (keycount == 0) {
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return;
    }
#endif

    parms.scb = scb;
    parms.objnameQ = objnameQ;
    parms.parmname = parmname;
    parms.keycount = keycount;
    parms.keynum = 0;
    parms.startindent = startindent;
    parms.done = FALSE;

    obj_traverse_keys(instance, obj, &parms, NULL, write_key_var);

} /* write_c_key_vars */


/********************************************************************
* FUNCTION write_c_key_values
* 
* Write all the keys in call-C-function-to-get-key-value format
*
* INPUTS:
*   scb == session to use
*   obj == object to start from (ancestor-or-self)
*   objnameQ == Q of name-to-idstr mappings
*   keycount == number of key leafs expected; used to
*               identify last key to suppress ending comma
*   startindent == start indent count
*
*********************************************************************/
void
    write_c_key_values (ncx_instance_t *instance, 
                        ses_cb_t *scb, 
                        obj_template_t *obj, 
                        dlq_hdr_t *objnameQ, 
                        uint32 keycount,
                        int32 startindent)
{
    c_keywalker_parms_t parms;

#ifdef DEBUG
    if (!scb || !obj || !objnameQ) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
    if (keycount == 0) {
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return;
    }
#endif

    parms.scb = scb;
    parms.objnameQ = objnameQ;
    parms.parmname = NULL;
    parms.keycount = keycount;
    parms.keynum = 0;
    parms.startindent = startindent;
    parms.done = FALSE;

    obj_traverse_keys(instance, obj, &parms, NULL, write_key_value);

} /* write_c_key_params */


/********************************************************************
* FUNCTION write_h_iffeature_start
* 
* Generate the start C for 1 if-feature conditional;
*
* INPUTS:
*   scb == session control block to use for writing
*   iffeatureQ == Q of ncx_feature_t to use
*
*********************************************************************/
void
    write_h_iffeature_start (ncx_instance_t *instance,
                             ses_cb_t *scb,
                             const dlq_hdr_t *iffeatureQ)
{
    ncx_iffeature_t   *iffeature, *nextif;
    uint32             iffeaturecnt;

    iffeaturecnt = dlq_count(instance, iffeatureQ);

    /* check if conditional wrapper needed */
    if (iffeaturecnt == 1) {
        iffeature = (ncx_iffeature_t *)
            dlq_firstEntry(instance, iffeatureQ);

        ses_putchar(instance, scb, '\n');
        ses_putstr(instance, scb, POUND_IFDEF);
        write_identifier(instance, scb, iffeature->feature->tkerr.mod->name,
                         BAR_FEAT, iffeature->feature->name, TRUE);
    } else if (iffeaturecnt > 1) {
        ses_putchar(instance, scb, '\n');
        ses_putstr(instance, scb, POUND_IF);
        ses_putchar(instance, scb, '(');

        for (iffeature = (ncx_iffeature_t *)
                 dlq_firstEntry(instance, iffeatureQ);
             iffeature != NULL;
             iffeature = nextif) {

            nextif = (ncx_iffeature_t *)dlq_nextEntry(instance, iffeature);

            ses_putstr(instance, scb, START_DEFINED);
            write_identifier(instance, scb, iffeature->feature->tkerr.mod->name,
                             BAR_FEAT, iffeature->feature->name, TRUE);
            ses_putchar(instance, scb, ')');

            if (nextif) {
                ses_putstr(instance, scb, (const xmlChar *)" && ");
            }
        }
        ses_putchar(instance, scb, ')');
    }

}  /* write_h_iffeature_start */


/********************************************************************
* FUNCTION write_h_iffeature_end
* 
* Generate the end C for 1 if-feature conditiona;
*
* INPUTS:
*   scb == session control block to use for writing
*   iffeatureQ == Q of ncx_feature_t to use
*
*********************************************************************/
void
    write_h_iffeature_end (ncx_instance_t *instance,
                           ses_cb_t *scb,
                           const dlq_hdr_t *iffeatureQ)
{
    if (!dlq_empty(instance, iffeatureQ)) {
        ses_putstr(instance, scb, POUND_ENDIF);
        ses_putstr(instance, scb, (const xmlChar *)" /* ");

        ncx_iffeature_t *iffeature = 
            (ncx_iffeature_t *)dlq_firstEntry(instance, iffeatureQ);
        ncx_iffeature_t *nexif = NULL;

        for (; iffeature != NULL; iffeature = nexif) {
            write_identifier(instance, scb, iffeature->feature->tkerr.mod->name,
                             BAR_FEAT, iffeature->feature->name, TRUE);
            nexif = (ncx_iffeature_t *)dlq_nextEntry(instance, iffeature);
            if (nexif) {
                ses_putstr(instance, scb, (const xmlChar *)", ");
            }
        }

        ses_putstr(instance, scb, (const xmlChar *)" */");
    }

}  /* write_h_iffeature_end */


/* END c_util.c */




