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
/*  FILE: help.c

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
05oct07      abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xmlstring.h>
#include <xmlreader.h>

#ifndef _H_procdefs
#include  "procdefs.h"
#endif

#ifndef _H_help
#include "help.h"
#endif

#ifndef _H_log
#include "log.h"
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

#ifndef _H_obj_help
#include "obj_help.h"
#endif

#ifndef _H_status
#include  "status.h"
#endif

#ifndef _H_val
#include "val.h"
#endif


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

#ifdef DEBUG
#define HELP_DEBUG 1
#endif



/********************************************************************
 * FUNCTION get_nestlevel
 * 
 * Get the appropriate nestlevel for the help mode
 *
 * INPUTS:
 *    mode == help mode requested
 *
 * RETURNS:
 *   nestlevel to use for obj_dump_template
 *********************************************************************/
static uint32
    get_nestlevel (ncx_instance_t *instance, help_mode_t mode)
{
    /* these are arbitrary numbers; need better plan */
    switch (mode) {
    case HELP_MODE_BRIEF:
        return 9;
    case HELP_MODE_NORMAL:
        return 31;
    case HELP_MODE_FULL:
        return 0;
    case HELP_MODE_NONE:
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return 1;
    }
    /*NOTREACHED*/

} /* get_nestlevel */


/********************************************************************
 * FUNCTION dump_appinfoQ (sub-mode of show RPC)
 * 
 * show module=mod-name
 *
 * INPUTS:
 *    hdr == appinfo Q to show (initialized but may be empty)
 *    indent == start indent count
 *********************************************************************/
static void
    dump_appinfoQ (ncx_instance_t *instance,
                   const dlq_hdr_t *hdr,
                   uint32 indent)
{
    const ncx_appinfo_t *appinfo;
    boolean   first;

    first = TRUE;
    for (appinfo = (const ncx_appinfo_t *)dlq_firstEntry(instance, hdr);
         appinfo != NULL;
         appinfo = (const ncx_appinfo_t *)dlq_nextEntry(instance, appinfo)) {

        if (first) {
            help_write_lines(instance, 
                             (const xmlChar *)"Appinfo Queue:\n", 
                             indent, TRUE);
            first = FALSE;
        }

        help_write_lines(instance, EMPTY_STRING, indent+2, TRUE);
        if (appinfo->value) {
            log_stdout(instance, "%s = %s", appinfo->name, appinfo->value);
        } else {
            log_stdout(instance, "%s", appinfo->name);
        }
    }
    if (!first) {
        log_stdout(instance, "\n");
    }

} /* dump_appinfoQ */


/********************************************************************
 * FUNCTION dump_rpcQ (sub-mode of show RPC)
 * 
 * List all the RPC methods defined in the specified module
 *
 * INPUTS:
 *    hdr == Queue header of RPC methods to show
 *    mode == help mode requisted
 *            HELP_MODE_FULL for full dump of each RPC
 *            HELP_MODE_NORMAL for normal dump of each RPC
 *            HELP_MODE_BRIEF for 1 line report on each RPC
 *    indent == start indent count
 *********************************************************************/
static void
    dump_rpcQ (ncx_instance_t *instance,
               const dlq_hdr_t *hdr,
               help_mode_t mode,
               uint32 indent)
{

    obj_template_t       *rpc;
    boolean               anyout;
    uint32                nestlevel;

    nestlevel = get_nestlevel(instance, mode);
    anyout = FALSE;
    for (rpc = (obj_template_t *)dlq_firstEntry(instance, hdr);
         rpc != NULL;
         rpc = (obj_template_t *)dlq_nextEntry(instance, rpc)) {

        if (rpc->objtype == OBJ_TYP_RPC) {
            anyout = TRUE;
            obj_dump_template(instance, rpc, mode, nestlevel, indent);
        }
    }

    if (anyout) {
        log_stdout(instance, "\n");
    }

} /* dump_rpcQ */


/********************************************************************
 * FUNCTION dump_mod_hdr (sub-mode of do_show_module(s)
 * 
 * show module=mod-name header info
 *
 * INPUTS:
 *    mod == module to show
 *
 *********************************************************************/
static void
    dump_mod_hdr  (ncx_instance_t *instance, const ncx_module_t *mod)
{

    /* dump some header info */
    log_stdout(instance, "\n\nModule: %s", mod->name);
    if (mod->version) {
        log_stdout(instance, " (%s)", mod->version);
    }
    log_stdout(instance, "\nPrefix: %s", mod->prefix);
    if (mod->xmlprefix) {
        log_stdout(instance, "\nXML prefix: %s", mod->xmlprefix);
    }
    log_stdout(instance, "\nNamespace: %s", (mod->ns) ?
               (const char *)mod->ns : "(none)");
    log_stdout(instance, "\nSource: %s", mod->source);

} /* dump_mod_hdr */


/**********  E X T E R N A L   F U N C T I O N S  ****************/


/********************************************************************
* FUNCTION help_program_module
*
* Print the full help text for an entire program module to STDOUT
*
* INPUTS:
*    modname == module name without file suffix
*    cliname == name of CLI parmset within the modname module
*
*********************************************************************/
void
    help_program_module (ncx_instance_t *instance,
                         const xmlChar *modname,
                         const xmlChar *cliname,
                         help_mode_t mode)
{
    ncx_module_t         *mod;
    obj_template_t       *cli;
    uint32                nestlevel;
    help_mode_t           usemode;

#ifdef DEBUG
    if (!modname) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
    if (mode == HELP_MODE_NONE || mode > HELP_MODE_FULL) {
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return;
    }
#endif

    nestlevel = get_nestlevel(instance, mode);

    mod = ncx_find_module(instance, modname, NULL);
    if (!mod) {
        log_error(instance, "\nhelp: Module '%s' not found", modname);
        SET_ERROR(instance, ERR_NCX_MOD_NOT_FOUND);
        return;
    }

    log_stdout(instance, "\n\n  Program %s", mod->name);
    log_stdout(instance, "\n\n  Usage:");
    log_stdout(instance, "\n\n    %s [parameters]", mod->name);
    if (mode != HELP_MODE_BRIEF) {
        log_stdout(instance, "\n\n  Parameters can be entered in any order, and have ");
        log_stdout(instance, "the form:");
        log_stdout(instance, "\n\n    [start] name separator [value]");
        log_stdout(instance, "\n\n  where:");
        log_stdout(instance, "\n\n    start == 0, 1, or 2 dashes (foo, -foo, --foo)");
        log_stdout(instance, "\n\n    name == parameter name (foo)"
                   "\n\n  Parameter name completion "
                   "will be attempted "
                   "\n  if a partial name is entered.");
        log_stdout(instance, "\n\n    separator == whitespace or equals sign "
                   "(foo=bar, foo bar)");
        log_stdout(instance, "\n\n    value == string value for the parameter");
        log_stdout(instance, "\n\n Strings with whitespace need to be "
                   "double quoted."
                   "\n    (--foo=\"some string\")");
    }

    if (mode == HELP_MODE_FULL && mod->descr) {
        log_stdout(instance, "\n\n  Description:");
        help_write_lines(instance, mod->descr, 4, TRUE);
    }

    if (cliname) {
        cli = ncx_find_object(instance, mod, cliname);
        if (!cli) {
            log_error(instance, "\nhelp: CLI Object %s not found", cliname);
            SET_ERROR(instance, ERR_NCX_DEF_NOT_FOUND);
            return;
        } else if (cli->objtype == OBJ_TYP_CONTAINER) {
            log_stdout(instance, "\n\n Command Line Parameters");
            log_stdout(instance, "\n\n Key:  parm-name [built-in-type] [d:default]\n");


            if (mode == HELP_MODE_BRIEF) {
                usemode = HELP_MODE_NORMAL;
            } else {
                usemode = HELP_MODE_FULL;
            }
                
            obj_dump_datadefQ(instance, 
                              obj_get_datadefQ(instance, cli), 
                              usemode, 
                              nestlevel, 
                              4);
            log_stdout(instance, "\n");
        }
    }

    if (obj_any_rpcs(instance, &mod->datadefQ) && mode == HELP_MODE_FULL) {
        log_stdout(instance, "\n\n  Local Commands\n");
        dump_rpcQ(instance, &mod->datadefQ, mode, 4);
    }

}  /* help_program_module */


/********************************************************************
* FUNCTION help_data_module
*
* Print the full help text for an entire data module to STDOUT
*
* INPUTS:
*    mod     == data module struct
*    mode == help mode requested
*********************************************************************/
void
    help_data_module (ncx_instance_t *instance,
                      const ncx_module_t *mod,
                      help_mode_t mode)
{
#ifdef DEBUG
    if (mod == NULL) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    dump_mod_hdr(instance, mod);

    if (mode == HELP_MODE_BRIEF) {
        return;
    }

    if (mode == HELP_MODE_FULL && mod->descr) {
        log_stdout(instance, "\nDescription:\n %s", mod->descr);
    }

    dump_rpcQ(instance, &mod->datadefQ, mode, 2);

    if (mode == HELP_MODE_FULL) {
        dump_appinfoQ(instance, &mod->appinfoQ, 2);
    }

}  /* help_data_module */


/********************************************************************
* FUNCTION help_type
*
* Print the full help text for a data type to STDOUT
*
* INPUTS:
*    typ     == type template struct
*    mode == help mode requested
*********************************************************************/
void
    help_type (ncx_instance_t *instance,
               const typ_template_t *typ,
               help_mode_t mode)
{
#ifdef DEBUG
    if (typ == NULL) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    log_stdout(instance, "\n  Type: %s", typ->name);
    log_stdout(instance,
               " (%s)",
               tk_get_btype_sym(typ_get_basetype
                                (instance, (const typ_def_t *)&typ->typdef)));

    if (mode > HELP_MODE_BRIEF && typ->descr) {
        log_stdout(instance, "\n Description: %s", typ->descr);
    }

    if (typ->defval) {
        log_stdout(instance, "\n Default: %s", typ->defval);
    }

    if (typ->units) {
        log_stdout(instance, "\n Units: %s", typ->units);
    }

    if (mode == HELP_MODE_FULL) {
        dump_appinfoQ(instance, &typ->typdef.appinfoQ, 1);
    }

}  /* help_type */


/********************************************************************
* FUNCTION help_object
*
* Print the full help text for a RPC method template to STDOUT
*
* INPUTS:
*    obj     == object template
*    mode == help mode requested
*********************************************************************/
void
    help_object (ncx_instance_t *instance,
                 obj_template_t *obj,
                 help_mode_t mode)
{
#ifdef DEBUG
    if (obj == NULL) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    obj_dump_template(instance, obj, mode, get_nestlevel(instance, mode), 0);

}  /* help_object */


/********************************************************************
 * FUNCTION help_write_lines
 * 
 * write some indented output to STDOUT
 *
 * INPUTS:
 *    str == string to print; 'indent' number of spaces
 *           will be added to each new line
 *    indent == indent count
 *    startnl == TRUE if start with a newline, FALSE otherwise
 *********************************************************************/
void
    help_write_lines (ncx_instance_t *instance,
                      const xmlChar *str,
                      uint32 indent,
                      boolean startnl)
{
    uint32  i;

    if (startnl) {
        log_stdout(instance, "\n");
        for (i=0; i<indent; i++) {
            log_stdout(instance, " ");
        }
    }

    if (str) {
        while (*str) {
            log_stdout(instance, "%c", *str);
            if (*str++ == '\n') {
                for (i=0; i<indent; i++) {
                    log_stdout(instance, " ");
                }
            }
        }
    }

} /* help_write_lines */


/********************************************************************
 * FUNCTION help_write_lines_max
 * 
 * write some indented output to STDOUT
 *
 * INPUTS:
 *    str == string to print; 'indent' number of spaces
 *           will be added to each new line
 *    indent == indent count
 *    startnl == TRUE if start with a newline, FALSE otherwise
 *    maxlen == 0..N max number of chars to output
 *********************************************************************/
void
    help_write_lines_max (ncx_instance_t *instance,
                          const xmlChar *str,
                          uint32 indent,
                          boolean startnl,
                          uint32 maxlen)
{
    uint32  i, cnt;

    cnt = 0;

    if (maxlen==0) {
        return;
    }

    if (startnl) {
        log_stdout(instance, "\n");
        if (++cnt > maxlen) {
            log_stdout(instance, "..."); 
            return;
        }
        for (i=0; i<indent; i++) {
            log_stdout(instance, " ");
            if (++cnt > maxlen) {
                log_stdout(instance, "..."); 
                return;
            }
        }
    }

    if (str) {
        while (*str) {
            log_stdout(instance, "%c", *str);
            if (++cnt > maxlen) {
                log_stdout(instance, "..."); 
                return;
            }
            
            if (*str++ == '\n') {
                for (i=0; i<indent; i++) {
                    log_stdout(instance, " ");
                    if (++cnt > maxlen) {
                        log_stdout(instance, "..."); 
                        return;
                    }
                }
            }
        }
    }

} /* help_write_lines_max */


/* END file help.c */
