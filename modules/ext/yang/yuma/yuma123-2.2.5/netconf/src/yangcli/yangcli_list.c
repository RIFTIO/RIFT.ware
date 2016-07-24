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
/*  FILE: yangcli_list.c

   NETCONF YANG-based CLI Tool

   list command

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
13-aug-09    abb      begun; started from yangcli_cmd.c

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libssh2.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include "libtecla.h"

#ifndef _H_procdefs
#include "procdefs.h"
#endif

#ifndef _H_log
#include "log.h"
#endif

#ifndef _H_mgr
#include "mgr.h"
#endif

#ifndef _H_mgr_ses
#include "mgr_ses.h"
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

#ifndef _H_op
#include "op.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_val_util
#include "val_util.h"
#endif

#ifndef _H_var
#include "var.h"
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

#ifndef _H_yangconst
#include "yangconst.h"
#endif

#ifndef _H_yangcli
#include "yangcli.h"
#endif

#ifndef _H_yangcli_list
#include "yangcli_list.h"
#endif

#ifndef _H_yangcli_cmd
#include "yangcli_cmd.h"
#endif

#ifndef _H_yangcli_util
#include "yangcli_util.h"
#endif



/********************************************************************
 * FUNCTION do_list_one_oid (sub-mode of list oids RPC)
 * 
 * list oids: 1 of N
 *
 * INPUTS:
 *    obj == the object to use
 *    help_mode == verbosity level to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_list_one_oid (ncx_instance_t *instance,
                     obj_template_t *obj,
                     help_mode_t help_mode)
{
    xmlChar      *buffer;
    boolean       imode;
    status_t      res;

    res = NO_ERR;

    if (obj_is_data_db(instance, obj) && 
        obj_has_name(instance, obj) &&
        !obj_is_hidden(instance, obj) && 
        !obj_is_abstract(instance, obj)) {

        imode = interactive_mode();
        buffer = NULL;
        res = obj_gen_object_id(instance, obj, &buffer);
        if (res != NO_ERR) {
            log_error(instance,
                      "\nError: list OID failed (%s)",
                      get_error_string(res));
        } else if (help_mode == HELP_MODE_FULL) {
            if (imode) {
                log_stdout(instance, 
                           "\n   %s %s", 
                           obj_get_typestr(instance, obj),
                           buffer);
            } else {
                log_write(instance, 
                          "\n   %s %s", 
                          obj_get_typestr(instance, obj),
                          buffer);
            }
        } else {
            if (imode) {
                log_stdout(instance, "\n   %s", buffer);
            } else {
                log_write(instance, "\n   %s", buffer);
            }
        }
        if (buffer) {
            m__free(instance, buffer);
        }
    }

    return res;

} /* do_list_one_oid */


/********************************************************************
 * FUNCTION do_list_oid (sub-mode of local RPC)
 * 
 * list oids
 *
 * INPUTS:
 *    obj == object to use
 *    nestlevel to stop at
 *    help_mode == verbosity level to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_list_oid (ncx_instance_t *instance,
                 obj_template_t *obj,
                 uint32 level,
                 help_mode_t  help_mode)
    
{
    obj_template_t  *chobj;
    status_t         res;

    res = NO_ERR;

    if (obj_get_level(instance, obj) <= level) {
        res = do_list_one_oid(instance, obj, help_mode);
        for (chobj = obj_first_child(instance, obj);
             chobj != NULL && res == NO_ERR;
             chobj = obj_next_child(instance, chobj)) {
            res = do_list_oid(instance, chobj, level, help_mode);
        }
    }

    return res;

} /* do_list_oid */


/********************************************************************
 * FUNCTION do_list_oids (sub-mode of local RPC)
 * 
 * list oids
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    mod == the 1 module to use
 *           NULL to use all available modules instead
 *    mode == requested help mode
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_list_oids (ncx_instance_t *instance,
                  server_cb_t *server_cb,
                  ncx_module_t *mod,
                  help_mode_t mode)
{
    modptr_t        *modptr;
    obj_template_t  *obj;
    boolean          imode;
    uint32           level;
    status_t         res;

    res = NO_ERR;

    switch (mode) {
    case HELP_MODE_NONE:
        return res;
    case HELP_MODE_BRIEF:
        level = 3;
        break;
    case HELP_MODE_NORMAL:
        level = 10;
        break;
    case HELP_MODE_FULL:
        level = 999;
        break;
    default:
        return SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    imode = interactive_mode();

    if (mod) {
        obj = ncx_get_first_object(instance, mod);
        while (obj && res == NO_ERR) {
            res = do_list_oid(instance, obj, level, mode);
            obj = ncx_get_next_object(instance, mod, obj);
        }
    } else if (use_servercb(instance, server_cb)) {
        for (modptr = (modptr_t *)
                 dlq_firstEntry(instance, &server_cb->modptrQ);
             modptr != NULL;
             modptr = (modptr_t *)dlq_nextEntry(instance, modptr)) {

            obj = ncx_get_first_object(instance, modptr->mod);
            while (obj && res == NO_ERR) {
                res = do_list_oid(instance, obj, level, mode);
                obj = ncx_get_next_object(instance, modptr->mod, obj);
            }
        }
    } else {
        return res;
    }
    if (imode) {
        log_stdout(instance, "\n");
    } else {
        log_write(instance, "\n");
    }

    return res;

} /* do_list_oids */


/********************************************************************
 * FUNCTION do_list_one_command (sub-mode of list command RPC)
 * 
 * list commands: 1 of N
 *
 * INPUTS:
 *    obj == object template for the RPC command to use
 *    mode == requested help mode
 *
 * RETURNS:
 *    status
 *********************************************************************/
static status_t
    do_list_one_command (ncx_instance_t *instance,
                         obj_template_t *obj,
                         help_mode_t mode)
{
    if (interactive_mode()) {
        if (mode == HELP_MODE_BRIEF) {
            log_stdout(instance, "\n   %s", obj_get_name(instance, obj));
        } else if (mode == HELP_MODE_NORMAL) {
            log_stdout(instance,
                       "\n   %s:%s",
                       obj_get_mod_xmlprefix(obj),
                       obj_get_name(instance, obj));
        } else {
            log_stdout(instance,
                       "\n   %s:%s",
                       obj_get_mod_name(instance, obj),
                       obj_get_name(instance, obj));
        }
    } else {
        if (mode == HELP_MODE_BRIEF) {
            log_write(instance, "\n   %s", obj_get_name(instance, obj));
        } else if (mode == HELP_MODE_NORMAL) {
            log_write(instance,
                      "\n   %s:%s",
                      obj_get_mod_xmlprefix(obj),
                      obj_get_name(instance, obj));
        } else {
            log_write(instance,
                      "\n   %s:%s",
                      obj_get_mod_name(instance, obj),
                      obj_get_name(instance, obj));
        }
    }

    return NO_ERR;

} /* do_list_one_command */


/********************************************************************
 * FUNCTION do_list_objects (sub-mode of local RPC)
 * 
 * list objects
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    mod == the 1 module to use
 *           NULL to use all available modules instead
 *    mode == requested help mode
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_list_objects (ncx_instance_t *instance,
                     server_cb_t *server_cb,
                     ncx_module_t *mod,
                     help_mode_t mode)
{
    modptr_t             *modptr;
    obj_template_t       *obj;
    logfn_t               logfn;
    boolean               anyout;
    status_t              res;

    res = NO_ERR;
    anyout = FALSE;

    if (interactive_mode()) {
        logfn = log_stdout;
    } else {
        logfn = log_write;
    }

    if (mod) {
        obj = ncx_get_first_object(instance, mod);
        while (obj && res == NO_ERR) {
            if (obj_is_data_db(instance, obj) && 
                obj_has_name(instance, obj) &&
                !obj_is_hidden(instance, obj) && 
                !obj_is_abstract(instance, obj)) {
                anyout = TRUE;
                res = do_list_one_command(instance, obj, mode);
            }
            obj = ncx_get_next_object(instance, mod, obj);
        }
    } else {
        if (use_servercb(instance, server_cb)) {
            for (modptr = (modptr_t *)
                     dlq_firstEntry(instance, &server_cb->modptrQ);
                 modptr != NULL;
                 modptr = (modptr_t *)dlq_nextEntry(instance, modptr)) {

                obj = ncx_get_first_object(instance, modptr->mod);
                while (obj && res == NO_ERR) {
                    if (obj_is_data_db(instance, obj) && 
                        obj_has_name(instance, obj) &&
                        !obj_is_hidden(instance, obj) && 
                        !obj_is_abstract(instance, obj)) {
                        anyout = TRUE;                  
                        res = do_list_one_command(instance, obj, mode);
                    }
                    obj = ncx_get_next_object(instance, modptr->mod, obj);
                }
            }
        }

        for (modptr = (modptr_t *)
                 dlq_firstEntry(instance, get_mgrloadQ());
             modptr != NULL && res == NO_ERR;
             modptr = (modptr_t *)dlq_nextEntry(instance, modptr)) {

            obj = ncx_get_first_object(instance, modptr->mod);
            while (obj && res == NO_ERR) {
                if (obj_is_data_db(instance, obj) && 
                    obj_has_name(instance, obj) &&
                    !obj_is_hidden(instance, obj) && 
                    !obj_is_abstract(instance, obj)) {
                    anyout = TRUE;                  
                    res = do_list_one_command(instance, obj, mode);
                }
                obj = ncx_get_next_object(instance, modptr->mod, obj);
            }
        }
    }

    if (!anyout) {
        (*logfn)(instance, "\nNo objects found to list");
    }

    (*logfn)(instance, "\n");

    return res;

} /* do_list_objects */


/********************************************************************
 * FUNCTION do_list_commands (sub-mode of local RPC)
 * 
 * list commands
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    mod == the 1 module to use
 *           NULL to use all available modules instead
 *    mode == requested help mode
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_list_commands (ncx_instance_t *instance,
                      server_cb_t *server_cb,
                      ncx_module_t *mod,
                      help_mode_t mode)
{
    modptr_t              *modptr;
    obj_template_t        *obj;
    logfn_t                logfn;
    boolean                imode, anyout;
    status_t               res;

    res = NO_ERR;
    imode = interactive_mode();
    if (imode) {
        logfn = log_stdout;
    } else {
        logfn = log_write;
    }

    if (mod) {
        anyout = FALSE;
        obj = ncx_get_first_object(instance, mod);
        while (obj && res == NO_ERR) {
            if (obj_is_rpc(instance, obj)) {
                res = do_list_one_command(instance, obj, mode);
                anyout = TRUE;
            }
            obj = ncx_get_next_object(instance, mod, obj);
        }
        if (!anyout) {
            (*logfn)(instance, "\nNo commands found in module '%s'",
                     mod->name);
        }
    } else {
        if (use_servercb(instance, server_cb)) {
            (*logfn)(instance, "\nServer Commands:");
        
            for (modptr = (modptr_t *)
                     dlq_firstEntry(instance, &server_cb->modptrQ);
                 modptr != NULL && res == NO_ERR;
                 modptr = (modptr_t *)dlq_nextEntry(instance, modptr)) {

                obj = ncx_get_first_object(instance, modptr->mod);
                while (obj) {
                    if (obj_is_rpc(instance, obj)) {
                        res = do_list_one_command(instance, obj, mode);
                    }
                    obj = ncx_get_next_object(instance, modptr->mod, obj);
                }
            }
        }

        (*logfn)(instance, "\n\nLocal Commands:");

        obj = ncx_get_first_object(instance, get_yangcli_mod());
        while (obj && res == NO_ERR) {
            if (obj_is_rpc(instance, obj)) {
                if (use_servercb(instance, server_cb)) {
                    /* list a local command */
                    res = do_list_one_command(instance, obj, mode);
                } else {
                    /* session not active so filter out
                     * all the commands except top command
                     */
                    if (is_top_command(instance, obj_get_name(instance, obj))) {
                        res = do_list_one_command(instance, obj, mode);
                    }
                }
            }
            obj = ncx_get_next_object(instance, get_yangcli_mod(), obj);
        }
    }

    (*logfn)(instance, "\n");

    return res;

} /* do_list_commands */


/********************************************************************
 * FUNCTION do_list (local RPC)
 * 
 * list objects   [module=mod-name]
 *      ids
 *      commands
 *
 * List the specified information based on the parameters
 *
 * INPUTS:
 * server_cb == server control block to use
 *    rpc == RPC method for the show command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_list (ncx_instance_t *instance,
             server_cb_t *server_cb,
             obj_template_t *rpc,
             const xmlChar *line,
             uint32  len)
{
    val_value_t        *valset, *parm;
    ncx_module_t       *mod;
    status_t            res;
    boolean             imode, done;
    help_mode_t         mode;

    mod = NULL;
    done = FALSE;
    res = NO_ERR;
    imode = interactive_mode();

    valset = get_valset(instance, server_cb, rpc, &line[len], &res);
    if (valset && res == NO_ERR) {
        mode = HELP_MODE_NORMAL;

        /* check if the 'brief' flag is set first */
        parm = val_find_child(instance, 
                              valset, 
                              YANGCLI_MOD, 
                              YANGCLI_BRIEF);
        if (parm && parm->res == NO_ERR) {
            mode = HELP_MODE_BRIEF;
        } else {
            parm = val_find_child(instance, 
                                  valset, 
                                  YANGCLI_MOD, 
                                  YANGCLI_FULL);
            if (parm && parm->res == NO_ERR) {
                mode = HELP_MODE_FULL;
            }
        }

        parm = val_find_child(instance, 
                              valset, 
                              YANGCLI_MOD, 
                              YANGCLI_MODULE);
        if (parm && parm->res == NO_ERR) {
            mod = find_module(instance, server_cb, VAL_STR(parm));
            if (!mod) {
                if (imode) {
                    log_stdout(instance,
                               "\nError: no module found named '%s'",
                               VAL_STR(parm)); 
                } else {
                    log_write(instance,
                              "\nError: no module found named '%s'",
                              VAL_STR(parm)); 
                }
                done = TRUE;
            }
        }

        /* find the 1 of N choice */
        if (!done) {
            parm = val_find_child(instance, 
                                  valset, 
                                  YANGCLI_MOD, 
                                  YANGCLI_COMMANDS);
            if (parm) {
                /* do list commands */
                res = do_list_commands(instance, server_cb, mod, mode);
                done = TRUE;
            }
        }

        if (!done) {
            parm = val_find_child(instance, 
                                  valset, 
                                  YANGCLI_MOD, 
                                  YANGCLI_FILES);
            if (parm) {
                /* list the data files */
                res = ncxmod_list_data_files(instance, mode, imode);
                done = TRUE;
            }
        }

        if (!done) {
            parm = val_find_child(instance, 
                                  valset, 
                                  YANGCLI_MOD, 
                                  YANGCLI_OBJECTS);
            if (parm) {
                /* do list objects */
                res = do_list_objects(instance, server_cb, mod, mode);
                done = TRUE;
            }
        }

        if (!done) {
            parm = val_find_child(instance, 
                                  valset, 
                                  YANGCLI_MOD, 
                                  YANGCLI_OIDS);
            if (parm) {
                /* do list oids */
                res = do_list_oids(instance, server_cb, mod, mode);
                done = TRUE;
            }
        }

        if (!done) {
            parm = val_find_child(instance, 
                                  valset, 
                                  YANGCLI_MOD, 
                                  YANGCLI_MODULES);
            if (parm) {
                /* list the YANG files */
                res = ncxmod_list_yang_files(instance, mode, imode);
                done = TRUE;
            }
        }

        if (!done) {
            parm = val_find_child(instance, 
                                  valset, 
                                  YANGCLI_MOD, 
                                  YANGCLI_SCRIPTS);
            if (parm) {
                /* list the script files */
                res = ncxmod_list_script_files(instance, mode, imode);
                done = TRUE;
            }
        }

    }

    if (valset) {
        val_free_value(instance, valset);
    }

    return res;

}  /* do_list */


/* END yangcli_list.c */
