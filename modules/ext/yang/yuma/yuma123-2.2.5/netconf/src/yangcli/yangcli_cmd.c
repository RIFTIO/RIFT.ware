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
/*  FILE: yangcli_cmd.c

   NETCONF YANG-based CLI Tool

   See ./README for details

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
11-apr-09    abb      begun; started from yangcli.c

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <assert.h>
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

#include "procdefs.h"
#include "cli.h"
#include "conf.h"
#include "help.h"
#include "log.h"
#include "mgr.h"
#include "mgr_io.h"
#include "mgr_load.h"
#include "mgr_not.h"
#include "mgr_rpc.h"
#include "mgr_ses.h"
#include "ncx.h"
#include "ncxconst.h"
#include "ncxmod.h"
#include "obj.h"
#include "obj_help.h"
#include "op.h"
#include "rpc.h"
#include "rpc_err.h"
#include "runstack.h"
#include "status.h"
#include "val.h"
#include "val_util.h"
#include "var.h"
#include "xmlns.h"
#include "xml_util.h"
#include "xml_val.h"
#include "xml_wr.h"
#include "xpath.h"
#include "xpath1.h"
#include "xpath_yang.h"
#include "yangconst.h"
#include "yangcli.h"
#include "yangcli_alias.h"
#include "yangcli_autolock.h"
#include "yangcli_cond.h"
#include "yangcli_cmd.h"
#include "yangcli_eval.h"
#include "yangcli_list.h"
#include "yangcli_save.h"
#include "yangcli_show.h"
#include "yangcli_tab.h"
#include "yangcli_timer.h"
#include "yangcli_uservars.h"
#include "yangcli_util.h"

/* get_case needs to recurse */
static status_t
    fill_valset (struct ncx_instance_t_ *instance,
                 server_cb_t *server_cb,
                 obj_template_t *rpc,
                 val_value_t *valset,
                 val_value_t *oldvalset,
                 boolean iswrite,
                 boolean isdelete);


/********************************************************************
* FUNCTION need_get_parm
* 
*  Check if the get_foo function should fill in this parameter
*  or not
* 
* INPUTS:
*   parm == object template to check
*
* RETURNS:
*    TRUE if value should be filled in
*    FALSE if it should be skipped
*********************************************************************/
static boolean
    need_get_parm (ncx_instance_t *instance, obj_template_t *parm)
{
    if (!obj_is_config(instance, parm) || 
        obj_is_abstract(instance, parm) ||
        obj_get_status(instance, parm) == NCX_STATUS_OBSOLETE) {
        return FALSE;
    }

    return TRUE;

}  /* need_get_parm */


/********************************************************************
* FUNCTION check_external_rpc_input
* 
* Check for the foo @parms.xml command form
* 
* INPUTS:
*   rpc == RPC to parse CLI for
*   line == input line to parse, starting with the parms to parse
*   res == pointer to status output
*
* OUTPUTS: 
*   *res == status
*        will be ERR_NCX_SKIPPED if not the external input form
*
* RETURNS:
*    pointer to malloced value set or NULL if none created,
*    may have errors, check *res
*    == NULL if *res == ERR_NCX_SKIPPED
*********************************************************************/
static val_value_t* 
   check_external_rpc_input (ncx_instance_t *instance,
                             obj_template_t *rpc,
                             const xmlChar *args,
                             status_t  *res)
{
    obj_template_t   *obj = obj_find_child(instance, rpc, NULL, YANG_K_INPUT);
    const xmlChar    *str = args;
    xmlChar          *sourcefile = NULL;
    xmlChar          *fname = NULL;
    val_value_t      *retval = NULL;

    if (obj == NULL) {
        *res = SET_ERROR(instance, ERR_INTERNAL_VAL);
        return NULL;
    }

    if (args == NULL) {
        *res = ERR_NCX_SKIPPED;
        return NULL;
    }

    while (*str && xml_isspace(*str)) {
        str++;
    }
    if (*str != '@') {
        *res = ERR_NCX_SKIPPED;
        return NULL;
    }

    /* stopped on the @ char.  Expecting @filespec
     * this is a NCX_BT_EXTERN value
     * find the file with the raw XML data
     * treat this as a source file name which
     * may need to be expanded (e.g., @~/filespec.xml)
     */
    *res = NO_ERR;
    sourcefile = ncx_get_source_ex(instance, &str[1], FALSE, res);
    if (*res == NO_ERR && sourcefile != NULL) {
        fname = ncxmod_find_data_file(instance, sourcefile, TRUE, res);
        if (fname != NULL && *res == NO_ERR) {
            retval = mgr_load_extern_file(instance, fname, obj, res);
        }
    }
    if (sourcefile != NULL) {
        m__free(instance, sourcefile);
    }
    if (fname != NULL) {
        m__free(instance, fname);
    }

    return retval;

}  /* check_external_rpc_input */


/********************************************************************
* FUNCTION parse_rpc_cli
* 
*  Call the cli_parse for an RPC input value set
* 
* INPUTS:
*   server_cb == server control block to use
*   rpc == RPC to parse CLI for
*   line == input line to parse, starting with the parms to parse
*   res == pointer to status output
*
* OUTPUTS: 
*   *res == status
*
* RETURNS:
*    pointer to malloced value set or NULL if none created,
*    may have errors, check *res
*********************************************************************/
static val_value_t* parse_rpc_cli (ncx_instance_t *instance,
                                     server_cb_t *server_cb,
                                    obj_template_t *rpc,
                                    const xmlChar *args,
                                    status_t  *res )
{
    obj_template_t   *obj;
    char             *myargv[2];
    val_value_t      *retval = NULL;

    /* construct an argv array, convert the CLI into a parmset */
    obj = obj_find_child(instance, rpc, NULL, YANG_K_INPUT);
    if (obj) {
        /* check if this is the special command form
         *  foo-command @parms.xml
         */
        if (args) {
            *res = NO_ERR;
            retval = check_external_rpc_input(instance, rpc, args, res);
            if (*res != ERR_NCX_SKIPPED) {
                return retval;
            }
            if (retval) {
                val_free_value(instance, retval);
                retval = NULL;
            }
        }

        myargv[0] = (char*)xml_strdup(instance,  obj_get_name(instance, rpc) );
        if ( myargv[0] ) {
            char* secondary_args = (char *)xml_strdup(instance, args);
            int secondary_args_flag = 0;
            while(strlen(secondary_args)>=strlen("--")) {
                if(memcmp(secondary_args, "--", strlen("--"))==0) {
                    secondary_args[0]=0;
                    secondary_args_flag = 1;
                    break;
                }
                secondary_args++;
            }

            myargv[1] = (char*)xml_strdup(instance,  args );
            if(secondary_args_flag) secondary_args[0] = '-';

            if ( myargv[1] ) {
                retval = cli_parse(instance,  server_cb->runstack_context, 2, myargv, obj,
                                    VALONLY, SCRIPTMODE, get_autocomp(), 
                                    CLI_MODE_COMMAND, res );
                m__free(instance,  myargv[1] );
            } else {
                *res = ERR_INTERNAL_MEM;
            }
            m__free(instance,  myargv[0] );
            if(*res==NO_ERR && secondary_args_flag) {
                secondary_args[0] = '-';
                myargv[0] = (char*)xml_strdup(instance, (const xmlChar *)(secondary_args + strlen("--")) );
            }
        } else {
            *res = ERR_INTERNAL_MEM;
        }
    } else {
        *res = SET_ERROR(instance, ERR_INTERNAL_VAL);
    }
    return retval;
}  /* parse_rpc_cli */


/********************************************************************
* FUNCTION get_prompt
* 
* Construct the CLI prompt for the current state
* 
* INPUTS:
*   buff == bufffer to hold prompt (zero-terminated string)
*   bufflen == length of buffer (max bufflen-1 chars can be printed
*
*********************************************************************/
static void
    get_prompt (ncx_instance_t *instance,
                server_cb_t *server_cb,
                xmlChar *buff,
                uint32 bufflen)
{
    xmlChar        *p;
    val_value_t    *parm;
    uint32          len, condlen;
    boolean         cond;

    if (server_cb->climore) {
        xml_strncpy(instance, buff, MORE_PROMPT, bufflen);
        return;
    }

    cond = runstack_get_cond_state(instance, server_cb->runstack_context);
    condlen = (cond) ? 0: xml_strlen(instance, FALSE_PROMPT);

    switch (server_cb->state) {
    case MGR_IO_ST_INIT:
    case MGR_IO_ST_IDLE:
    case MGR_IO_ST_CONNECT:
    case MGR_IO_ST_CONN_START:
    case MGR_IO_ST_SHUT:
        if (server_cb->cli_fn) {
            len = xml_strlen(instance, DEF_FN_PROMPT) 
                + xml_strlen(instance, server_cb->cli_fn) 
                + condlen + 2;
            if (len < bufflen) {
                p = buff;
                p += xml_strcpy(instance, p, DEF_FN_PROMPT);
                p += xml_strcpy(instance, p, server_cb->cli_fn);
                if (condlen) {
                    p += xml_strcpy(instance, p, FALSE_PROMPT);
                }
                xml_strcpy(instance, p, (const xmlChar *)"> ");
            } else if (cond) {
                xml_strncpy(instance, buff, DEF_PROMPT, bufflen);
            } else {
                xml_strncpy(instance, buff, DEF_FALSE_PROMPT, bufflen);
            }
        } else if (cond ) {
            xml_strncpy(instance, buff, DEF_PROMPT, bufflen);
        } else {
            xml_strncpy(instance, buff, DEF_FALSE_PROMPT, bufflen);
        }
        break;
    case MGR_IO_ST_CONN_IDLE:
    case MGR_IO_ST_CONN_RPYWAIT:
    case MGR_IO_ST_CONN_CANCELWAIT:
    case MGR_IO_ST_CONN_CLOSEWAIT:
    case MGR_IO_ST_CONN_SHUT:
        p = buff;
        len = xml_strncpy(instance, p, (const xmlChar *)"yangcli ", bufflen);
        p += len;
        bufflen -= len;

        if (bufflen == 0) {
            return;
        }

        parm = NULL;
        if (server_cb->connect_valset) {
            parm = val_find_child(instance, 
                                  server_cb->connect_valset, 
                                  YANGCLI_MOD, 
                                  YANGCLI_USER);
        }

        if (!parm && get_connect_valset()) {
            parm = val_find_child(instance, 
                                  get_connect_valset(), 
                                  YANGCLI_MOD, 
                                  YANGCLI_USER);
        }

        if (parm) {
            len = xml_strncpy(instance, p, VAL_STR(parm), bufflen);
            p += len;
            bufflen -= len;
            if (bufflen == 0) {
                return;
            }
            *p++ = NCX_AT_CH;
            --bufflen;
            if (bufflen == 0) {
                return;
            }
        }

        parm = NULL;
        if (server_cb->connect_valset) {
            parm = val_find_child(instance, 
                                  server_cb->connect_valset, 
                                  YANGCLI_MOD, 
                                  YANGCLI_SERVER);
        }

        if (!parm && get_connect_valset()) {
            parm= val_find_child(instance, 
                                 get_connect_valset(), 
                                 YANGCLI_MOD, 
                                 YANGCLI_SERVER);
        }

        if (parm) {
            len = xml_strncpy(instance, p, VAL_STR(parm), bufflen);
            p += len;
            bufflen -= len;
            if (bufflen == 0) {
                return;
            }
        }

        if (server_cb->cli_fn && bufflen > 3) {
            *p++ = ':';
            len = xml_strncpy(instance, p, server_cb->cli_fn, --bufflen);
            p += len;
            bufflen -= len;
        }

        if (!cond) {
            len = xml_strlen(instance, FALSE_PROMPT);
            if (bufflen > len) {
                p += xml_strcpy(instance, p, FALSE_PROMPT);
                bufflen -= len;
            }
        }

        if (bufflen > 2) {
            *p++ = '>';
            *p++ = ' ';
            *p = 0;
            bufflen -= 2;
        }
        break;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        xml_strncpy(instance, buff, DEF_PROMPT, bufflen);        
        break;
    }

}  /* get_prompt */


/********************************************************************
* get_line
*
* Generate a prompt based on the program state and
* use the tecla library read function to handle
* user keyboard input
*
*
* Do not free this line after use, just discard it
* It will be freed by the tecla lib
*
* INPUTS:
*   server_cb == server control block to use
*
* RETURNS:
*   static line from tecla read line, else NULL if some error
*********************************************************************/
static xmlChar *
    get_line (ncx_instance_t *instance, server_cb_t *server_cb)
{
    xmlChar          *line;
    xmlChar           prompt[MAX_PROMPT_LEN];
    GlReturnStatus    returnstatus;

    line = NULL;

    get_prompt(instance, server_cb, prompt, MAX_PROMPT_LEN-1);

    if (!server_cb->climore) {
        log_stdout(instance, "\n");
    }


    server_cb->returncode = 0;

    if (server_cb->history_line_active) {
        line = (xmlChar *)
            gl_get_line(server_cb->cli_gl,
                        (const char *)prompt,
                        (const char *)server_cb->history_line, 
                        -1);
        server_cb->history_line_active = FALSE;
    } else {
        line = (xmlChar *)
            gl_get_line(server_cb->cli_gl,
                        (const char *)prompt,
                        NULL, 
                        -1);
    }

    if (!line) {
        if (server_cb->returncode == MGR_IO_RC_DROPPED ||
            server_cb->returncode == MGR_IO_RC_DROPPED_NOW) {
            log_write(instance, "\nSession was dropped by the server");
            clear_server_cb_session(instance, server_cb);
            return NULL;
        }

        returnstatus = gl_return_status(server_cb->cli_gl);

        /* treat signal as control-C warning; about to exit */
        if (returnstatus == GLR_SIGNAL) {
            log_warn(instance, "\nWarning: Control-C exit\n\n");            
            return NULL;
        }

        /* skip ordinary line canceled code except if debug2 */
        if (returnstatus == GLR_TIMEOUT && !LOGDEBUG2) {
            return NULL;
        }

        log_error(instance, "\nError: GetLine returned ");
        switch (returnstatus) {
        case GLR_NEWLINE:
            log_error(instance, "NEWLINE");
            break;
        case GLR_BLOCKED:
            log_error(instance, "BLOCKED");
            break;
        case GLR_SIGNAL:
            log_error(instance, "SIGNAL");
            break;
        case GLR_TIMEOUT:
            log_error(instance, "TIMEOUT");
            break;
        case GLR_FDABORT:
            log_error(instance, "FDABORT");
            break;
        case GLR_EOF:
            log_error(instance, "EOF");
            break;
        case GLR_ERROR:
            log_error(instance, "ERROR");
            break;
        default:
            log_error(instance, "<unknown>");
        }
        log_error(instance, 
                  " (rt:%u errno:%u)", 
                  server_cb->returncode,
                  server_cb->errnocode);
    }

    return line;

} /* get_line */


/********************************************************************
* FUNCTION try_parse_def
* 
* Parse the possibly module-qualified definition (module:def)
* and find the template for the requested definition
*
* !!! DO NOT USE DIRECTLY !!!
* !!! CALLED ONLY BY parse_def !!!
*
* INPUTS:
*   server_cb == server control block to use
*   mymod == module struct if available (may be NULL)
*   modname == module name to try
*   defname == definition name to try
*   dtyp == definition type 
*
* OUTPUTS:
*    *dtyp is set if it started as NONE
*
* RETURNS:
*   pointer to the found definition template or NULL if not found
*********************************************************************/
static void *
    try_parse_def (ncx_instance_t *instance,
                   server_cb_t *server_cb,
                   ncx_module_t *mymod,
                   const xmlChar *modname,
                   const xmlChar *defname,
                   ncx_node_t *dtyp)

{
    void          *def;
    ncx_module_t  *mod;

    if (mymod != NULL) {
        mod = mymod;
    } else {
        mod = find_module(instance, server_cb, modname);
        if (!mod) {
            return NULL;
        }
    }

    def = NULL;
    switch (*dtyp) {
    case NCX_NT_NONE:
        def = ncx_find_object(instance, mod, defname);
        if (def) {
            *dtyp = NCX_NT_OBJ;
            break;
        }
        def = ncx_find_grouping(instance, mod, defname, FALSE);
        if (def) {
            *dtyp = NCX_NT_GRP;
            break;
        }
        def = ncx_find_type(instance, mod, defname, FALSE);
        if (def) {
            *dtyp = NCX_NT_TYP;
            break;
        }
        break;
    case NCX_NT_OBJ:
        def = ncx_find_object(instance, mod, defname);
        break;
    case NCX_NT_GRP:
        def = ncx_find_grouping(instance, mod, defname, FALSE);
        break;
    case NCX_NT_TYP:
        def = ncx_find_type(instance, mod, defname, FALSE);
        break;
    default:
        def = NULL;
        SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    return def;
    
} /* try_parse_def */


/********************************************************************
* FUNCTION get_yesno
* 
* Get the user-selected choice, yes or no
*
* INPUTS:
*   server_cb == server control block to use
*   prompt == prompt message
*   defcode == default answer code
*      0 == no def answer
*      1 == def answer is yes
*      2 == def answer is no
*   retcode == address of return code
*
* OUTPUTS:
*    *retcode is set if return NO_ERR
*       0 == cancel
*       1 == yes
*       2 == no
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    get_yesno (ncx_instance_t *instance,
               server_cb_t *server_cb,
               const xmlChar *prompt,
               uint32 defcode,
               uint32 *retcode)
{
    xmlChar                 *myline, *str;
    status_t                 res;
    boolean                  done;

    done = FALSE;
    res = NO_ERR;

    set_completion_state(instance,
                         &server_cb->completion_state,
                         NULL, NULL, CMD_STATE_YESNO);

    if (prompt) {
        log_stdout(instance, "\n%s", prompt);
    }
    log_stdout(instance, "\nEnter Y for yes, N for no, or C to cancel:");
    switch (defcode) {
    case YESNO_NODEF:
        break;
    case YESNO_YES:
        log_stdout(instance, " [default: Y]");
        break;
    case YESNO_NO:
        log_stdout(instance, " [default: N]");
        break;
    default:
        res = SET_ERROR(instance, ERR_INTERNAL_VAL);
        done = TRUE;
    }


    while (!done) {

        /* get input from the user STDIN */
        myline = get_cmd_line(instance, server_cb, &res);
        if (!myline) {
            done = TRUE;
            continue;
        }

        /* strip leading whitespace */
        str = myline;
        while (*str && xml_isspace(*str)) {
            str++;
        }

        /* convert to a number, check [ENTER] for default */
        if (*str) {
            if (*str == 'Y' || *str == 'y') {
                *retcode = YESNO_YES;
                done = TRUE;
            } else if (*str == 'N' || *str == 'n') {
                *retcode = YESNO_NO;
                done = TRUE;
            } else if (*str == 'C' || *str == 'c') {
                *retcode = YESNO_CANCEL;
                done = TRUE;
            }
        } else {
            /* default accepted */
            switch (defcode) {
            case YESNO_NODEF:
                break;
            case YESNO_YES:
                *retcode = YESNO_YES;
                done = TRUE;
                break;
            case YESNO_NO:
                *retcode = YESNO_NO;
                done = TRUE;
                break;
            default:
                res = SET_ERROR(instance, ERR_INTERNAL_VAL);
                done = TRUE;
            }
        }
        if (res == NO_ERR && !done) {
            log_stdout(instance, "\nError: invalid value '%s'\n", str);
        }
    }

    return res;

}  /* get_yesno */


/********************************************************************
* FUNCTION get_parm
* 
* Fill the specified parm
* Use the old value for the default if any
* This function will block on readline to get input from the user
*
* INPUTS:
*   server_cb == server control block to use
*   rpc == RPC method that is being called
*   parm == parm to get from the CLI
*   valset == value set being filled
*   oldvalset == last set of values (NULL if none)
*
* OUTPUTS:
*    new val_value_t node will be added to valset if NO_ERR
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    get_parm (ncx_instance_t *instance,
              server_cb_t *server_cb,
              obj_template_t *rpc,
              obj_template_t *parm,
              val_value_t *valset,
              val_value_t *oldvalset)
{
    const xmlChar    *def, *parmname, *str;
    val_value_t      *oldparm, *newparm;
    xmlChar          *line, *start, *objbuff, *buff;
    xmlChar          *line2, *start2, *saveline;
    status_t          res;
    ncx_btype_t       btyp;
    boolean           done, ispassword, iscomplex;
    uint32            len;

    if (!obj_is_mandatory(instance, parm) && !server_cb->get_optional) {
        return NO_ERR;
    }

    def = NULL;
    iscomplex = FALSE;
    btyp = obj_get_basetype(instance, parm);
    res = NO_ERR;
    ispassword = obj_is_password(instance, parm);

    if (obj_is_data_db(instance, parm) && 
        parm->objtype != OBJ_TYP_RPCIO) {
        objbuff = NULL;
        res = obj_gen_object_id(instance, parm, &objbuff);
        if (res != NO_ERR) {
            log_error(instance,
                      "\nError: generate object ID failed (%s)",
                      get_error_string(res));
            return res;
        }

        /* let the user know about the new nest level */
        if (obj_is_key(parm)) {
            str = YANG_K_KEY;
        } else if (ispassword) {
            str = YANGCLI_PASSWORD;
        } else if (obj_is_mandatory(instance, parm)) {
            str = YANG_K_MANDATORY;
        } else {
            str = YANGCLI_OPTIONAL;
        }

        log_stdout(instance, 
                   "\nFilling %s %s %s:", 
                   str,
                   obj_get_typestr(instance, parm), 
                   objbuff);
            
        m__free(instance, objbuff);
    }

    switch (btyp) {
    case NCX_BT_ANY:
    case NCX_BT_CONTAINER:
        iscomplex = TRUE;
        break;
        /* return get_complex_parm(server_cb, parm, valset); */
    default:
        ;
    }

    parmname = obj_get_name(instance, parm);
    btyp = obj_get_basetype(instance, parm);
    res = NO_ERR;
    oldparm = NULL;
  
    done = FALSE;
    while (!done) {
        if (btyp==NCX_BT_EMPTY) {
            log_stdout(instance, 
                       "\nShould flag %s be set? [Y, N]", 
                       parmname);
        } else if (iscomplex) {
            log_stdout(instance,
                       "\nEnter value for %s <%s>",
                       obj_get_typestr(instance, parm),
                       parmname);
        } else {
            log_stdout(instance,
                       "\nEnter %s value for %s <%s>",
                       (const xmlChar *)tk_get_btype_sym(btyp),
                       obj_get_typestr(instance, parm),
                       parmname);
        }
        if (oldvalset) {
            oldparm = val_find_child(instance, 
                                     oldvalset, 
                                     obj_get_mod_name(instance, parm),
                                     parmname);
        }

        /* pick a default value, either old value or default clause */
        if (oldparm) {
            /* use the old value for the default */
            if (btyp==NCX_BT_EMPTY) {
                log_stdout(instance, " [Y]");
            } else if (iscomplex) {
                log_stdout(instance,
                           " [%s contents not shown]",
                           obj_get_typestr(instance, parm));
            } else {
                res = val_sprintf_simval_nc(instance, NULL, oldparm, &len);
                if (res != NO_ERR) {
                    return SET_ERROR(instance, res);
                }
                buff = m__getMem(instance, len+1);
                if (!buff) {
                    return ERR_INTERNAL_MEM;
                }
                res = val_sprintf_simval_nc(instance, buff, oldparm, &len);
                if (res == NO_ERR) {
                    log_stdout(instance, " [%s]", buff);
                }
                m__free(instance, buff);
            }
        } else {
            def = NULL;

            /* try to get the defined default value */
            if (btyp != NCX_BT_EMPTY && !iscomplex) {
                def = obj_get_default(instance, parm);
                if (!def && (obj_get_nsid(instance, rpc) == xmlns_nc_id(instance) &&
                             (!xml_strcmp(instance, parmname, NCX_EL_TARGET) ||
                              !xml_strcmp(instance, parmname, NCX_EL_SOURCE)))) {
                    /* offer the default target for the NETCONF
                     * <source> and <target> parameters
                     */
                    def = server_cb->default_target;
                }
            }

            if (def) {
                log_stdout(instance, " [%s]\n", def);
            } else if (btyp==NCX_BT_EMPTY) {
                log_stdout(instance, " [N]\n");
            }
        }

        set_completion_state_curparm(instance,
                                     &server_cb->completion_state,
                                     parm);

        /* get a line of input from the user
         * but don't echo if this is a password parm
         */
        if (ispassword) {
            /* ignore return value which is previous mode */
            (void)gl_echo_mode(server_cb->cli_gl, 0);
        }
                                
        line = get_cmd_line(instance, server_cb, &res);

        if (ispassword) {
            /* ignore return value which is previous mode */
            (void)gl_echo_mode(server_cb->cli_gl, 1);
        }

        if (!line) {
            return res;
        }

        /* skip whitespace */
        start = line;
        while (*start && xml_isspace(*start)) {
            start++;
        }

        /* check for question-mark char sequences */
        if (*start == '?') {
            if (start[1] == '?') {
                /* ?? == full help */
                obj_dump_template(instance, parm, HELP_MODE_FULL, 0,
                                  server_cb->defindent);
            } else if (start[1] == 'C' || start[1] == 'c') {
                /* ?c or ?C == cancel the operation */
                log_stdout(instance,
                           "\n%s command canceled",
                           obj_get_name(instance, rpc));
                return ERR_NCX_CANCELED;
            } else if (start[1] == 'S' || start[1] == 's') {
                /* ?s or ?S == skip this parameter */
                log_stdout(instance,
                           "\n%s parameter skipped",
                           obj_get_name(instance, parm));
                return ERR_NCX_SKIPPED;
            } else {
                /* ? == normal help mode */
                obj_dump_template(instance, parm, HELP_MODE_NORMAL, 4,
                                  server_cb->defindent);
            }
            log_stdout(instance, "\n");
            continue;
        } else {
            /* top loop to get_parm is only executed once */
            done = TRUE;
        }
    }

    /* check if any non-whitespace chars entered */
    if (!*start) {
        /* no input, use default or old value or EMPTY_STRING */
        if (def) {
            /* use default */
            res = cli_parse_parm_ex(instance,
                                    server_cb->runstack_context,
                                    valset, 
                                    parm, 
                                    def, 
                                    SCRIPTMODE, 
                                    get_baddata());
        } else if (oldparm) {
            /* no default, try old value */
            if (btyp==NCX_BT_EMPTY) {
                res = cli_parse_parm_ex(instance,
                                        server_cb->runstack_context,
                                        valset, 
                                        parm, 
                                        NULL,
                                        SCRIPTMODE, 
                                        get_baddata());
            } else {
                /* use a copy of the last value */
                newparm = val_clone(instance, oldparm);
                if (!newparm) {
                    res = ERR_INTERNAL_MEM;
                } else {
                    val_add_child(instance, newparm, valset);
                }
            }
        } else if (btyp == NCX_BT_EMPTY) {
            res = cli_parse_parm_ex(instance,
                                    server_cb->runstack_context,
                                    valset, 
                                    parm, 
                                    NULL,
                                    SCRIPTMODE, 
                                    get_baddata());
        } else if (!iscomplex && 
                   (val_simval_ok(instance, 
                                  obj_get_typdef(parm), 
                                  EMPTY_STRING) == NO_ERR)) {
            res = cli_parse_parm_ex(instance,
                                    server_cb->runstack_context,
                                    valset, 
                                    parm, 
                                    EMPTY_STRING,
                                    SCRIPTMODE, 
                                    get_baddata());
        } else {
            /* data type requires some form of input */
            res = ERR_NCX_DATA_MISSING;
        }
    } else if (btyp==NCX_BT_EMPTY) {
        /* empty data type handled special Y: set, N: leave out */
        if (*start=='Y' || *start=='y') {
            res = cli_parse_parm_ex(instance,
                                    server_cb->runstack_context,
                                    valset, 
                                    parm, 
                                    NULL, 
                                    SCRIPTMODE, 
                                    get_baddata());
        } else if (*start=='N' || *start=='n') {
            ; /* skip; do not add the flag */
        } else if (oldparm) {
            /* previous value was set, so add this flag */
            res = cli_parse_parm_ex(instance,
                                    server_cb->runstack_context,
                                    valset, 
                                    parm, 
                                    NULL,
                                    SCRIPTMODE, 
                                    get_baddata());
        } else {
            /* some value was entered, other than Y or N */
            res = ERR_NCX_WRONG_VAL;
        }
    } else {
        /* normal case: input for regular data type */
        res = cli_parse_parm_ex(instance,
                                server_cb->runstack_context,
                                valset, 
                                parm, 
                                start,
                                SCRIPTMODE, 
                                get_baddata());
    }


    /* got some value, now check if it is OK
     * depending on the --bad-data CLI parameter,
     * the value may be entered over again, accepted,
     * or rejected with an invalid-value error
     */
    if (res != NO_ERR) {
        switch (get_baddata()) {
        case NCX_BAD_DATA_IGNORE:
        case NCX_BAD_DATA_WARN:
            /* if these modes had error return status then the
             * problem was not invalid value; maybe malloc error
             */
            break;
        case NCX_BAD_DATA_CHECK:
            if (NEED_EXIT(res)) {
                break;
            }

            saveline = (start) ? xml_strdup(instance, start) :
                xml_strdup(instance, EMPTY_STRING);
            if (!saveline) {
                res = ERR_INTERNAL_MEM;
                break;
            }

            done = FALSE;
            while (!done) {
                log_stdout(instance, 
                           "\nError: parameter '%s' value '%s' is invalid"
                           "\nShould this value be used anyway? [Y, N]"
                           " [N]", 
                           obj_get_name(instance, parm),
                           (start) ? start : EMPTY_STRING);

                /* save the previous value because it is about
                 * to get trashed by getting a new line
                 */

                /* get a line of input from the user */
                line2 = get_cmd_line(instance, server_cb, &res);
                if (!line2) {
                    m__free(instance, saveline);
                    return res;
                }

                /* skip whitespace */
                start2 = line2;
                while (*start2 && xml_isspace(*start2)) {
                    start2++;
                }

                /* check for question-mark char sequences */
                if (!*start2) {
                    /* default N: try again for a different input */
                    m__free(instance, saveline);
                    res = get_parm(instance, server_cb, rpc, parm, valset, oldvalset);
                    done = TRUE;
                } else if (*start2 == '?') {
                    if (start2[1] == '?') {
                        /* ?? == full help */
                        obj_dump_template(instance, parm, HELP_MODE_FULL, 0,
                                          server_cb->defindent);
                    } else if (start2[1] == 'C' || start2[1] == 'c') {
                        /* ?c or ?C == cancel the operation */
                        log_stdout(instance,
                                   "\n%s command canceled",
                                   obj_get_name(instance, rpc));
                        m__free(instance, saveline);
                        res = ERR_NCX_CANCELED;
                        done = TRUE;
                    } else if (start2[1] == 'S' || start2[1] == 's') {
                        /* ?s or ?S == skip this parameter */
                        log_stdout(instance,
                                   "\n%s parameter skipped",
                                   obj_get_name(instance, parm));
                        m__free(instance, saveline);
                        res = ERR_NCX_SKIPPED;
                        done = TRUE;
                    } else {
                        /* ? == normal help mode */
                        obj_dump_template(instance, parm, HELP_MODE_NORMAL, 4,
                                          server_cb->defindent);
                    }
                    log_stdout(instance, "\n");
                    continue;
                } else if (*start2 == 'Y' || *start2 == 'y') {
                    /* use the invalid value */
                    res = cli_parse_parm_ex(instance,
                                            server_cb->runstack_context,
                                            valset, 
                                            parm, 
                                            saveline, 
                                            SCRIPTMODE, 
                                            NCX_BAD_DATA_IGNORE);
                    m__free(instance, saveline);
                    done = TRUE;
                } else if (*start2 == 'N' || *start2 == 'n') {
                    /* recurse: try again for a different input */
                    m__free(instance, saveline);
                    res = get_parm(instance, server_cb, rpc, parm, valset, oldvalset);
                    done = TRUE;
                } else {
                    log_stdout(instance, "\nInvalid input.");
                }
            }
            break;
        case NCX_BAD_DATA_ERROR:
            log_stdout(instance,
                       "\nError: set parameter '%s' failed (%s)\n",
                       obj_get_name(instance, parm),
                       get_error_string(res));
            break;
        default:
            SET_ERROR(instance, ERR_INTERNAL_VAL);
        }
    }

    return res;
    
} /* get_parm */


/********************************************************************
* FUNCTION get_case
* 
* Get the user-selected case, which is not fully set
* Use values from the last set (if any) for defaults.
* This function will block on readline if mandatory parms
* are needed from the CLI
*
* INPUTS:
*   server_cb == server control block to use
*   cas == case object template header
*   valset == value set to fill
*   oldvalset == last set of values (or NULL if none)
*   iswrite == TRUE if write op; FALSE if not
*   isdelete == TRUE if delete op; FALSE if not
* OUTPUTS:
*    new val_value_t nodes may be added to valset
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    get_case (ncx_instance_t *instance,
              server_cb_t *server_cb,
              obj_template_t *cas,
              val_value_t *valset,
              val_value_t *oldvalset,
              boolean iswrite,
              boolean isdelete)
{
    status_t                 res = NO_ERR;
    boolean                  saveopt = server_cb->get_optional;

    /* make sure a case was selected or found */
    if (!obj_is_config(instance, cas) || obj_is_abstract(instance, cas)) {
        log_stdout(instance, "\nError: No writable objects to fill for this case");
        return ERR_NCX_SKIPPED;
    }

    if (obj_is_data_db(instance, cas)) {
        xmlChar *objbuff = NULL;
        const xmlChar *str = NULL;
        res = obj_gen_object_id(instance, cas, &objbuff);
        if (res != NO_ERR) {
            log_error(instance,
                      "\nError: generate object ID failed (%s)",
                      get_error_string(res));
            return res;
        }

        /* let the user know about the new nest level */
        if (obj_is_mandatory(instance, cas)) {
            str = YANG_K_MANDATORY;
        } else {
            str = (const xmlChar *)"optional";
        }

        log_stdout(instance, "\nFilling %s case %s:", str, objbuff);
            
        m__free(instance, objbuff);
    }

    /* corner-case: user selected a case, and that case has
     * one empty leaf in it; 
     * e.g., <source> and <target> parms
     */
    if (obj_get_child_count(instance, cas) == 1) {
        obj_template_t *parm = obj_first_child(instance, cas);
        if (parm && obj_get_basetype(instance, parm)==NCX_BT_EMPTY) {
            /* user picked a case that is an empty flag so
             * do not call fill_valset for this corner-case
             * because the user will be prompted again
             * 'should this flag be set? y/n'
             * Instead, call cli_parse_parm directly
             */
            val_value_t *parmval = val_find_child(instance,
                                                  valset,
                                                  obj_get_mod_name(instance, parm),
                                                  obj_get_name(instance, parm));
            if (parmval == NULL || 
                parmval->obj->objtype == OBJ_TYP_LEAF_LIST) {
                return cli_parse_parm(instance,
                                      server_cb->runstack_context,
                                      valset, parm, NULL, FALSE);
            }
        }
    }

    res = fill_valset(instance, server_cb, cas, valset, oldvalset, iswrite, isdelete);

    server_cb->get_optional = saveopt;
    return res;

} /* get_case */


/********************************************************************
* FUNCTION enabled_case_count
* 
* Get the number of enabled cases
*
* INPUTS:
*   server_cb == server control block to use
*   choic == choice object template header
*   iswrite == TRUE for write operation
* RETURNS:
*   number of enabled cases
*********************************************************************/
static uint32
    enabled_case_count (ncx_instance_t *instance,
                        server_cb_t *server_cb,
                        obj_template_t *choic,
                        boolean iswrite)
{
    uint32 case_count = 0;
    obj_template_t *cas = obj_first_child(instance, choic); 
    for (; cas != NULL; cas = obj_next_child(instance, cas)) {
        if ((!server_cb->get_optional && !iswrite) || !need_get_parm(instance, cas)) {
            continue;
        }
        obj_template_t *parm = obj_first_child(instance, cas);
        if (parm) {
            case_count++;
        }
    }
    return case_count;

}  /* enabled_case_count */


/********************************************************************
* FUNCTION first_enabled_case
* 
* Get the first enabled case
*
* INPUTS:
*   server_cb == server control block to use
*   choic == choice object template header
*   iswrite == TRUE for write operation
* RETURNS:
*   number of enabled cases
*********************************************************************/
static obj_template_t *
    first_enabled_case (ncx_instance_t *instance,
                        server_cb_t *server_cb,
                        obj_template_t *choic,
                        boolean iswrite)
{
    obj_template_t *cas = obj_first_child(instance, choic); 
    for (; cas != NULL; cas = obj_next_child(instance, cas)) {
        if ((!server_cb->get_optional && !iswrite) || !need_get_parm(instance, cas)) {
            continue;
        }
        obj_template_t *parm = obj_first_child(instance, cas);
        if (parm) {
            return cas;
        }
    }
    return NULL;

}  /* first_enabled_case */


/********************************************************************
* FUNCTION get_choice
* 
* Get the user-selected choice, which is not fully set
* Use values from the last set (if any) for defaults.
* This function will block on readline if mandatory parms
* are needed from the CLI
*
* INPUTS:
*   server_cb == server control block to use
*   rpc == RPC template in progress
*   choic == choice object template header
*   valset == value set to fill
*   oldvalset == last set of values (or NULL if none)
*   iswrite == TRUE if write op; FALSE if not
*   isdelete == TRUE if delete op; FALSE if not
*
* OUTPUTS:
*    new val_value_t nodes may be added to valset
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    get_choice (ncx_instance_t *instance,
                server_cb_t *server_cb,
                obj_template_t *rpc,
                obj_template_t *choic,
                val_value_t *valset,
                val_value_t *oldvalset,
                boolean iswrite,
                boolean isdelete)
{
    obj_template_t  *cas = NULL;
    status_t         res = NO_ERR;
    boolean          done = FALSE;

    if (!obj_is_config(instance, choic) || obj_is_abstract(instance, choic)) {
        log_stdout(instance,
                   "\nError: choice '%s' has no configurable parameters",
                   obj_get_name(instance, choic));
        return ERR_NCX_ACCESS_DENIED;
    }

    int casenum = 0;

    if (obj_is_data_db(instance, choic)) {
        xmlChar *objbuff = NULL;
        res = obj_gen_object_id(instance, choic, &objbuff);
        if (res != NO_ERR) {
            log_error(instance,
                      "\nError: generate object ID failed (%s)",
                      get_error_string(res));
            return res;
        }

        /* let the user know about the new nest level */
        log_stdout(instance, "\nFilling choice %s:", objbuff);
        m__free(instance, objbuff);
    }

    boolean saveopt = server_cb->get_optional;

    /* only force optional=true if this is a mandatory choice
     * otherwise, a mandatory choice of all optional cases
     * will just get skipped over;
     * only set to true for reads if --optional also set
     */
    if (((saveopt && !iswrite) && obj_is_mandatory(instance, choic)) ||
        (iswrite && obj_is_mandatory(instance, choic))) {
        server_cb->get_optional = TRUE;
    }

    /* first check the partial block corner case */
    val_value_t *pval = val_get_choice_first_set(instance, valset, choic);
    if (pval != NULL) {
        /* found something set from this choice, finish the case */
        log_stdout(instance, "\nEnter more parameters to complete the choice:");

        cas = pval->casobj;
        if (cas == NULL) {
            server_cb->get_optional = saveopt;
            return SET_ERROR(instance, ERR_INTERNAL_VAL);
        }
        obj_template_t *parm = obj_first_child(instance, cas); 
        for (; parm != NULL; parm = obj_next_child(instance, parm)) {

            if ((!server_cb->get_optional && !iswrite) ||
                !need_get_parm(instance, parm)) {
                continue;
            }

            pval = val_find_child(instance, valset, obj_get_mod_name(instance, parm),
                                  obj_get_name(instance, parm));
            if (pval) {
                continue;   /* node within case already set */
            }

            res = get_parm(instance, server_cb, rpc, parm, valset, oldvalset);
            switch (res) {
            case NO_ERR:
                break;
            case ERR_NCX_SKIPPED:
                res = NO_ERR;
                break;
            case ERR_NCX_CANCELED:
                server_cb->get_optional = saveopt;
                return res;
            default:
                server_cb->get_optional = saveopt;
                return res;
            }
        }
        server_cb->get_optional = saveopt;
        return NO_ERR;
    }

    /* get number of enabled cases */
    uint32 case_count = enabled_case_count(instance, server_cb, choic, iswrite);
    boolean usedef = FALSE;

    /* check corner-case -- choice with no cases defined */
    cas = obj_first_child(instance, choic);
    if (cas == NULL) {
        if (case_count) {
            log_stdout(instance,
                   "\nNo case nodes enabled for choice %s\n",
                   obj_get_name(instance, choic));
        } else {
            log_stdout(instance,
                       "\nNo case nodes defined for choice %s\n",
                       obj_get_name(instance, choic));
        }
        server_cb->get_optional = saveopt;
        return NO_ERR;
    }

    if (case_count == 1) {
        cas = first_enabled_case(instance, server_cb, choic, iswrite);
    } else {
        /* else not a partial block corner case but a normal
         * situation where no case has been selected at all
         */
        log_stdout(instance, "\nEnter the number of the selected case statement:\n");

        int num = 1;

        for (; cas != NULL; cas = obj_next_child(instance, cas)) {

            if ((!server_cb->get_optional && !iswrite) || !need_get_parm(instance, cas)) {
                continue;
            }

            boolean first = TRUE;
            obj_template_t *parm = obj_first_child(instance, cas);
            for (; parm != NULL; parm = obj_next_child(instance, parm)) {

                if (!obj_is_config(instance, parm) || obj_is_abstract(instance, parm)) {
                    continue;
                }

                if (first) {
                    log_stdout(instance, "\n  %d: case %s:", num++, obj_get_name(instance, cas));
                    first = FALSE;
                }

                log_stdout(instance, "\n       %s %s", obj_get_typestr(instance, parm),
                           obj_get_name(instance, parm));
            }
        }

        done = FALSE;
        log_stdout(instance, "\n");
        while (!done) {

            boolean redo = FALSE;

            /* Pick a prompt, depending on the choice default case */
            if (obj_get_default(instance, choic)) {
                log_stdout(instance,
                           "\nEnter choice number [%d - %d], "
                           "[ENTER] for default (%s): ",
                           1, num-1, obj_get_default(instance, choic));
            } else {
                log_stdout(instance, "\nEnter choice number [%d - %d]: ", 1, num-1);
            }

            /* get input from the user STDIN */
            xmlChar *myline = get_cmd_line(instance, server_cb, &res);
            if (!myline) {
                server_cb->get_optional = saveopt;
                return res;
            }

            /* strip leading whitespace */
            xmlChar *str = myline;
            while (*str && xml_isspace(*str)) {
                str++;
            }

            /* convert to a number, check [ENTER] for default */
            if (!*str) {
                usedef = TRUE;
            } else if (*str == '?') {
                redo = TRUE;
                if (str[1] == '?') {
                    obj_dump_template(instance, choic, HELP_MODE_FULL, 0,
                                      server_cb->defindent);
                } else if (str[1] == 'C' || str[1] == 'c') {
                    log_stdout(instance,
                               "\n%s command canceled\n",
                               obj_get_name(instance, rpc));
                    server_cb->get_optional = saveopt;
                    return ERR_NCX_CANCELED;
                } else if (str[1] == 'S' || str[1] == 's') {
                    log_stdout(instance, "\n%s choice skipped\n", obj_get_name(instance, choic));
                    server_cb->get_optional = saveopt;
                    return ERR_NCX_SKIPPED;
                } else {
                    obj_dump_template(instance, choic, HELP_MODE_NORMAL, 4,
                                      server_cb->defindent);
                }
                log_stdout(instance, "\n");
            } else {
                casenum = atoi((const char *)str);
                usedef = FALSE;
            }

            if (redo) {
                continue;
            }

            /* check if default requested */
            if (usedef) {
                if (obj_get_default(instance, choic)) {
                    done = TRUE;
                } else {
                    log_stdout(instance, "\nError: Choice does not have "
                               "a default case\n");
                    usedef = FALSE;
                }
            } else if (casenum < 0 || casenum >= num) {
                log_stdout(instance, "\nError: invalid value '%s'\n", str);
            } else {
                done = TRUE;
            }
        }
    }

    /* got a valid choice number or use the default case
     * now get the object template for the correct case 
     */
    if (usedef) {
        cas = obj_find_child(instance, choic, obj_get_mod_name(instance, choic),
                             obj_get_default(instance, choic));
    } else if (case_count > 1) {

        int num = 1;
        done = FALSE;
        obj_template_t *usecase = NULL;

        for (cas = obj_first_child(instance, choic); 
             cas != NULL && !done;
             cas = obj_next_child(instance, cas)) {

            if ((!server_cb->get_optional && !iswrite) ||
                !need_get_parm(instance, cas)) {
                continue;
            }

            if (obj_first_child(instance, cas) == NULL) {
                /* skip case if no enabled children */
                continue;
            }

            if (casenum == num) {
                done = TRUE;
                usecase = cas;
            } else {
                num++;
            }
        }

        cas = usecase;
    }

    /* make sure a case was selected or found */
    if (!cas) {
        log_stdout(instance, "\nError: No case to fill for this choice");
        server_cb->get_optional = saveopt;
        return ERR_NCX_SKIPPED;
    }

    res = get_case(instance, server_cb, cas, valset, oldvalset, iswrite, isdelete);
    switch (res) {
    case NO_ERR:
        break;
    case ERR_NCX_SKIPPED:
        res = NO_ERR;
        break;
    case ERR_NCX_CANCELED:
        break;
    default:
        ;
    }

    server_cb->get_optional = saveopt;

    return res;

} /* get_choice */


/********************************************************************
* FUNCTION fill_value
* 
* Malloc and fill the specified value
* Use the last value for the value if it is present and valid
* This function will block on readline for user input if no
* valid useval is given
*
* INPUTS:
*   server_cb == server control block to use
*   rpc == RPC method that is being called
*   parm == object for value to fill
*   useval == value to use (or NULL if none)
*   res == address of return status
*
* OUTPUTS:
*    *res == return status
*
* RETURNS:
*    malloced value, filled in or NULL if some error
*********************************************************************/
static val_value_t *
    fill_value (ncx_instance_t *instance,
                server_cb_t *server_cb,
                obj_template_t *rpc,
                obj_template_t *parm,
                val_value_t *useval,
                status_t  *res)
{
    obj_template_t        *parentobj;
    val_value_t           *dummy, *newval;
    boolean                saveopt;

    switch (parm->objtype) {
    case OBJ_TYP_ANYXML:
    case OBJ_TYP_LEAF:
    case OBJ_TYP_LEAF_LIST:
        break;
    default:
        *res = SET_ERROR(instance, ERR_INTERNAL_VAL);
        return NULL;
    }

    if (obj_is_abstract(instance, parm)) {
        *res = ERR_NCX_NO_ACCESS_MAX;
        log_error(instance, "\nError: no access to abstract objects");
        return NULL;
    }

    /* just copy the useval if it is given */
    if (useval) {
        newval = val_clone(instance, useval);
        if (!newval) {
            log_error(instance, "\nError: malloc failed");
            return NULL;
        }
        if (newval->obj != parm) {
            /* force the new value to have the QName of 'parm' */
            val_change_nsid(instance, newval, obj_get_nsid(instance, parm));
            val_set_name(instance, newval, obj_get_name(instance, parm),
                         xml_strlen(instance, obj_get_name(instance, parm)));
        }
        return newval;
    }        

    /* since the CLI and original NCX language were never designed
     * to support top-level leafs, a dummy container must be
     * created for the new and old leaf or leaf-list entries
     */
    if (parm->parent && !obj_is_root(parm->parent)) {
        parentobj = parm->parent;
    } else {
        parentobj = ncx_get_gen_container(instance);
    }

    dummy = val_new_value(instance);
    if (!dummy) {
        *res = ERR_INTERNAL_MEM;
        log_error(instance, "\nError: malloc failed");
        return NULL;
    }
    val_init_from_template(instance, dummy, parentobj);

    server_cb->cli_fn = obj_get_name(instance, rpc);

    newval = NULL;
    saveopt = server_cb->get_optional;
    server_cb->get_optional = TRUE;

    set_completion_state(instance,
                         &server_cb->completion_state,
                         rpc, parm, CMD_STATE_GETVAL);

    *res = get_parm(instance, server_cb, rpc, parm, dummy, NULL);
    server_cb->get_optional = saveopt;

    if (*res == NO_ERR) {
        newval = val_get_first_child(instance, dummy);
        if (newval) {
            val_remove_child(instance, newval);
        }
    }
    server_cb->cli_fn = NULL;
    val_free_value(instance, dummy);
    return newval;

} /* fill_value */


/********************************************************************
* FUNCTION fill_valset
* 
* Fill the specified value set with any missing parameters.
* Use values from the last set (if any) for defaults.
* This function will block on readline if mandatory parms
* are needed from the CLI
*
* INPUTS:
*   server_cb == current server control block (NULL if none)
*   rpc == RPC method that is being called
*   valset == value set to fill
*   oldvalset == last set of values (or NULL if none)
*   iswrite == TRUE if write command; FALSE if not
*   isdelete == TRUE if delete operation; FALSE if not
* OUTPUTS:
*    new val_value_t nodes may be added to valset
*
* RETURNS:
*    status,, valset may be partially filled if not NO_ERR
*********************************************************************/
static status_t
    fill_valset (ncx_instance_t *instance,
                 server_cb_t *server_cb,
                 obj_template_t *rpc,
                 val_value_t *valset,
                 val_value_t *oldvalset,
                 boolean iswrite,
                 boolean isdelete)
{
    obj_template_t *parm, *target;
    val_value_t    *val, *oldval;
    val_value_t    *firstchoice = NULL;
    status_t        res = NO_ERR;
    boolean         done;
    uint32          yesnocode;

    if (rpc->objtype == OBJ_TYP_CASE) {
        /* called by get_case */
        target = rpc;
    } else if (rpc->objtype == OBJ_TYP_RPC) {
        /* called by RPC invocation or fill data fn */
        server_cb->cli_fn = obj_get_name(instance, rpc);
        target = valset->obj;
    } else {
        /* called by fill data fn */
        target = valset->obj;
    }

    if (!(target->objtype == OBJ_TYP_RPC ||
          target->objtype == OBJ_TYP_RPCIO ||
          target->objtype == OBJ_TYP_CASE)) {
        xmlChar *objbuff = NULL;
        res = obj_gen_object_id(instance, target, &objbuff);
        if (res != NO_ERR) {
            log_error(instance,
                      "\nError: generate object ID failed (%s)",
                      get_error_string(res));
            return res;
        }

        /* let the user know about the new nest level */
        log_stdout(instance, "\nFilling %s %s:", obj_get_typestr(instance, target), objbuff);
        m__free(instance, objbuff);
    }

    for (parm = obj_first_child(instance, target);
         parm != NULL && res==NO_ERR;
         parm = obj_next_child(instance, parm)) {

        if ((!server_cb->get_optional && !iswrite) ||
            !need_get_parm(instance, parm)) {
            if (oldvalset != NULL) {
                res = clone_old_parm(instance, oldvalset, valset, parm);
            }
            continue;
        }

        if (isdelete && !obj_is_key(parm)) {
            continue;
        }

        if (!server_cb->get_optional) {
            if (!obj_is_mandatory(instance, parm)) {
                if (oldvalset != NULL) {
                    res = clone_old_parm(instance, oldvalset, valset, parm);
                }
                continue;
            }
            if (!iswrite && !obj_is_key(parm)) {
                if (oldvalset != NULL) {
                    res = clone_old_parm(instance, oldvalset, valset, parm);
                }
                continue;
            }
        }

        set_completion_state(instance, &server_cb->completion_state, rpc, parm,
                             CMD_STATE_GETVAL);

        switch (parm->objtype) {
        case OBJ_TYP_CHOICE:
            firstchoice = val_get_choice_first_set(instance, valset, parm);
            if (firstchoice) {
                assert( firstchoice->casobj && "case backptr is NULL!" );

                /* a case is already selected so try finishing that */
                res = get_case(instance, server_cb, firstchoice->casobj, valset,
                               oldvalset, iswrite, isdelete);
            } else {
                res = get_choice(instance, server_cb, rpc, parm, valset, oldvalset,
                                 iswrite, isdelete);
                switch (res) {
                case NO_ERR:
                    break;
                case ERR_NCX_SKIPPED:
                    res = NO_ERR;
                    break;
                case ERR_NCX_CANCELED:
                    break;
                default:
                    ;
                }
            }
            break;
        case OBJ_TYP_ANYXML:
        case OBJ_TYP_LEAF:
            val = val_find_child(instance, valset, obj_get_mod_name(instance, parm),
                                 obj_get_name(instance, parm));
            if (val == NULL) {
                res = get_parm(instance, server_cb, rpc, parm, valset, oldvalset);
                switch (res) {
                case NO_ERR:
                    break;
                case ERR_NCX_SKIPPED:
                    res = NO_ERR;
                    break;
                case ERR_NCX_CANCELED:
                    break;
                default:
                    ;
                }
            }
            break;
        case OBJ_TYP_LEAF_LIST:
            done = FALSE;
            while (!done && res == NO_ERR) {
                res = get_parm(instance, server_cb, rpc, parm, valset, oldvalset);
                switch (res) {
                case NO_ERR:
                    /* prompt for more leaf-list objects */
                    res = get_yesno(instance, server_cb, YANGCLI_PR_LLIST,
                                    YESNO_NO, &yesnocode);
                    if (res == NO_ERR) {
                        switch (yesnocode) {
                        case YESNO_CANCEL:
                            res = ERR_NCX_CANCELED;
                            break;
                        case YESNO_YES:
                            break;
                        case YESNO_NO:
                            done = TRUE;
                            break;
                        default:
                            res = SET_ERROR(instance, ERR_INTERNAL_VAL);
                        }
                    }
                    break;
                case ERR_NCX_SKIPPED:
                    done = TRUE;
                    res = NO_ERR;
                    break;
                case ERR_NCX_CANCELED:
                    break;
                default:
                    ;
                }
            }
            break;
        case OBJ_TYP_CONTAINER:
        case OBJ_TYP_NOTIF:
        case OBJ_TYP_RPCIO:
            /* if the parm is not already set and is not read-only
             * then try to get a value from the user at the CLI
             */
            val = val_find_child(instance, valset, obj_get_mod_name(instance, parm),
                                 obj_get_name(instance, parm));
            if (val) {
                break;
            }
                        
            if (oldvalset) {
                oldval = val_find_child(instance, oldvalset, obj_get_mod_name(instance, parm),
                                        obj_get_name(instance, parm));
            } else {
                oldval = NULL;
            }

            val = val_new_value(instance);
            if (!val) {
                res = ERR_INTERNAL_MEM;
                log_error(instance, "\nError: malloc of new value failed");
                break;
            } else {
                val_init_from_template(instance, val, parm);
                val_add_child(instance, val, valset);
            }

            /* recurse with the child nodes */
            res = fill_valset(instance, server_cb, parm, val, oldval, iswrite, isdelete);

            switch (res) {
            case NO_ERR:
                break;
            case ERR_NCX_SKIPPED:
                res = NO_ERR;
                break;
            case ERR_NCX_CANCELED:
                break;
            default:
                ;
            }
            break;
        case OBJ_TYP_LIST:
            done = FALSE;
            while (!done && res == NO_ERR) {
                val = val_new_value(instance);
                if (!val) {
                    res = ERR_INTERNAL_MEM;
                    log_error(instance, "\nError: malloc of new value failed");
                    continue;
                } else {
                    val_init_from_template(instance, val, parm);
                    val_add_child(instance, val, valset);
                }

                /* recurse with the child node -- NO OLD VALUE
                 * TBD: get keys, then look up old matching entry
                 */
                res = fill_valset(instance, server_cb, parm, val, NULL, iswrite, 
                                  isdelete);
                switch (res) {
                case NO_ERR:
                    /* prompt for more list entries */
                    res = get_yesno(instance, server_cb, YANGCLI_PR_LIST,
                                    YESNO_NO, &yesnocode);
                    if (res == NO_ERR) {
                        switch (yesnocode) {
                        case YESNO_CANCEL:
                            res = ERR_NCX_CANCELED;
                            break;
                        case YESNO_YES:
                            break;
                        case YESNO_NO:
                            done = TRUE;
                            break;
                        default:
                            res = SET_ERROR(instance, ERR_INTERNAL_VAL);
                        }
                    }
                    break;
                case ERR_NCX_SKIPPED:
                    res = NO_ERR;
                    break;
                case ERR_NCX_CANCELED:
                    break;
                default:
                    ;
                }
            }
            break;
        default: 
            res = SET_ERROR(instance, ERR_INTERNAL_VAL);
        }
    }

    server_cb->cli_fn = NULL;
    return res;

} /* fill_valset */


/********************************************************************
* FUNCTION create_session
* 
* Start a NETCONF session and change the program state
* Since this is called in sequence with readline, the STDIN IO
* handler may get called if the user enters keyboard text 
*
* The STDIN handler will not do anything with incoming chars
* while state == MGR_IO_ST_CONNECT
* 
* INPUTS:
*   server_cb == server control block to use
*
* OUTPUTS:
*   'server_cb->mysid' is set to the output session ID, if NO_ERR
*   'server_cb->state' is changed based on the success of 
*    the session setup
*
*********************************************************************/
static void
    create_session (ncx_instance_t *instance, server_cb_t *server_cb)
{
    const xmlChar          *server, *username, *password;
    const char             *publickey, *privatekey;
    modptr_t               *modptr;
    ncxmod_search_result_t *searchresult;
    val_value_t            *val;
    status_t                res;
    uint16                  port;
    boolean                 startedsession, tcp, portbydefault;
    boolean                 tcp_direct_enable;

    if (LOGDEBUG) {
        log_debug(instance, "\nConnect attempt with following parameters:");
        val_dump_value_max(instance,
                           server_cb->connect_valset,
                           0,
                           server_cb->defindent,
                           DUMP_VAL_LOG,
                           server_cb->display_mode,
                           FALSE,
                           FALSE);
        log_debug(instance, "\n");
    }
    
    /* make sure session not already running */
    if (server_cb->mysid) {
        if (mgr_ses_get_scb(server_cb->mysid)) {
            /* already connected; fn should not have been called */
            SET_ERROR(instance, ERR_INTERNAL_INIT_SEQ);
            return;
        } else {
            /* OK: reset session ID */
            server_cb->mysid = 0;
        }
    }

    /* make sure no stale search results in the control block */
    while (!dlq_empty(instance, &server_cb->searchresultQ)) {
        searchresult = (ncxmod_search_result_t *)
            dlq_deque(instance, &server_cb->searchresultQ);
        ncxmod_free_search_result(instance, searchresult);
    }

    /* make sure no stale modules in the control block */
    while (!dlq_empty(instance, &server_cb->modptrQ)) {
        modptr = (modptr_t *)dlq_deque(instance, &server_cb->modptrQ);
        free_modptr(instance, modptr);
    }

    /* retrieving the parameters should not fail */
    username = NULL;
    val =  val_find_child(instance,
                          server_cb->connect_valset,
                          YANGCLI_MOD, YANGCLI_USER);
    if (val && val->res == NO_ERR) {
        username = VAL_STR(val);
    } else {
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return;
    }

    server = NULL;
    val = val_find_child(instance,
                         server_cb->connect_valset,
                         YANGCLI_MOD, YANGCLI_SERVER);
    if (val && val->res == NO_ERR) {
        server = VAL_STR(val);
    } else {
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return;
    }

    password = NULL;
    val = val_find_child(instance,
                         server_cb->connect_valset,
                         YANGCLI_MOD, YANGCLI_PASSWORD);
    if (val && val->res == NO_ERR) {
        password = VAL_STR(val);
    }

    port = 0;
    portbydefault = FALSE;
    val = val_find_child(instance,
                         server_cb->connect_valset,
                         YANGCLI_MOD, YANGCLI_NCPORT);
    if (val && val->res == NO_ERR) {
        port = VAL_UINT16(val);
        portbydefault = val_set_by_default(instance, val);
    }

    publickey = NULL;
    val = val_find_child(instance,
                         server_cb->connect_valset,
                         YANGCLI_MOD, YANGCLI_PUBLIC_KEY);
    if (val && val->res == NO_ERR) {
        publickey = (const char *)VAL_STR(val);
    }

    privatekey = NULL;
    val = val_find_child(instance,
                         server_cb->connect_valset,
                         YANGCLI_MOD, YANGCLI_PRIVATE_KEY);
    if (val && val->res == NO_ERR) {
        privatekey = (const char *)VAL_STR(val);
    }

    tcp = FALSE;
    val = val_find_child(instance,
                         server_cb->connect_valset,
                         YANGCLI_MOD, YANGCLI_TRANSPORT);
    if (val != NULL && 
        val->res == NO_ERR && 
        !xml_strcmp(instance,
                    VAL_ENUM_NAME(val),
                    (const xmlChar *)"tcp")) {
        tcp = TRUE;
    }

     tcp_direct_enable = FALSE;
     val = val_find_child(instance,
                          server_cb->connect_valset,
                          YANGCLI_MOD, 
                          YANGCLI_TCP_DIRECT_ENABLE);
     if(val == NULL) printf("val is NULL.\n");
     if(val->res == NO_ERR) printf("val->res is NO_ERR.\n");

     if (val != NULL && 
         val->res == NO_ERR && 
        VAL_BOOL(val)) {
        tcp_direct_enable = TRUE;
    }
    if (tcp) {
        if (port == 0 || portbydefault) {
            port = SES_DEF_TCP_PORT;
        }
    }
        
    log_info(instance,
             "\nyangcli: Starting NETCONF session for %s on %s",
             username, 
             server);

    startedsession = FALSE;
    server_cb->state = MGR_IO_ST_CONNECT;

    /* this function call will cause us to block while the
     * protocol layer connect messages are processed
     */
    res = mgr_ses_new_session(instance, 
                              username, 
                              password, 
                              publickey,
                              privatekey,
                              server, 
                              port,
                              (tcp) ? ((tcp_direct_enable) ? SES_TRANSPORT_TCP_DIRECT : SES_TRANSPORT_TCP)
                              : SES_TRANSPORT_SSH,
                              server_cb->temp_progcb,
                              &server_cb->mysid,
                              (xpath_getvar_fn_t)xpath_getvar_fn,
                              server_cb->connect_valset);
    if (res == NO_ERR) {
        startedsession = TRUE;
        server_cb->state = MGR_IO_ST_CONN_START;
        log_debug(instance, 
                  "\nyangcli: Start session %d OK for server '%s'", 
                  server_cb->mysid, 
                  server_cb->name);

        res = mgr_set_getvar_fn(instance,
                                server_cb->mysid,
                                (xpath_getvar_fn_t)xpath_getvar_fn);
        if (res != NO_ERR) {
            log_error(instance, "\nError: Could not set XPath variable callback");
        }
    }

    if (res != NO_ERR) {
        if (startedsession) {
            mgr_ses_free_session(instance, server_cb->mysid);
            server_cb->mysid = 0;
        }
        log_info(instance, 
                 "\nyangcli: Start session failed for user %s on "
                 "%s (%s)\n", 
                 username, 
                 server, 
                 get_error_string(res));
        server_cb->state = MGR_IO_ST_IDLE;
    }
    
} /* create_session */


/********************************************************************
 * FUNCTION do_mgrload (local RPC)
 * 
 * mgrload module=mod-name
 *
 * Get the module parameter and load the specified NCX module
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the load command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   specified module is loaded into the definition registry, if NO_ERR
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_mgrload (ncx_instance_t *instance,
                server_cb_t *server_cb,
                obj_template_t *rpc,
                const xmlChar *line,
                uint32  len)
{
    val_value_t     *valset, *modval, *revval, *devval;
    ncx_module_t    *mod;
    modptr_t        *modptr;
    dlq_hdr_t       *mgrloadQ;
    dlq_hdr_t        savedevQ;
    logfn_t          logfn;
    status_t         res;

    modval = NULL;
    revval = NULL;
    res = NO_ERR;
    dlq_createSQue(instance, &savedevQ);

    if (interactive_mode()) {
        logfn = log_stdout;
    } else {
        logfn = log_write;
    }
        
    valset = get_valset(instance, server_cb, rpc, &line[len], &res);

    /* get the module name */
    if (res == NO_ERR) {
        modval = val_find_child(instance, valset, YANGCLI_MOD, NCX_EL_MODULE);
        if (!modval) {
            res = ERR_NCX_DEF_NOT_FOUND;
        } else if (modval->res != NO_ERR) {
            res = modval->res;
        }
    }

    /* get the module revision */
    if (res == NO_ERR) {
        revval = val_find_child(instance, valset, YANGCLI_MOD, NCX_EL_REVISION);
    }


    /* check if any version of the module is loaded already */
    if (res == NO_ERR) {
        mod = ncx_find_module(instance, VAL_STR(modval), NULL);
        if (mod) {
            if (mod->version) {
                (*logfn)(instance, "\nModule '%s' revision '%s' already loaded",
                         mod->name, 
                         mod->version);
            } else {
                (*logfn)(instance, "\nModule '%s' already loaded",
                         mod->name);
            }
            if (valset) {
                val_free_value(instance, valset);
            }
            return res;
        }
    }

    /* check if there are any deviation parameters to load first */
    if (res == NO_ERR) {
        for (devval = val_find_child(instance, valset, YANGCLI_MOD, NCX_EL_DEVIATION);
             devval != NULL && res == NO_ERR;
             devval = val_find_next_child(instance, valset, YANGCLI_MOD,
                                          NCX_EL_DEVIATION, devval)) {

            res = ncxmod_load_deviation(instance, VAL_STR(devval), &savedevQ);
        }
    }

    /* load the module */
    if (res == NO_ERR) {
        mod = NULL;
        res = ncxmod_load_module(instance, 
                                 VAL_STR(modval), 
                                 (revval) ? VAL_STR(revval) : NULL, 
                                 &savedevQ,
                                 &mod);
        if (res == NO_ERR) {
            /*** TBD: prompt user for features to enable/disable ***/
            modptr = new_modptr(instance, mod, NULL, NULL);
            if (!modptr) {
                res = ERR_INTERNAL_MEM;
            } else {
                mgrloadQ = get_mgrloadQ();
                dlq_enque(instance, modptr, mgrloadQ);
            }
        }
    }

    /* print the result to stdout */
    if (res == NO_ERR) {
        if (revval) {
            (*logfn)(instance, "\nLoad module '%s' revision '%s' OK", 
                     VAL_STR(modval),
                     VAL_STR(revval));
        } else {
            (*logfn)(instance, "\nLoad module '%s' OK", VAL_STR(modval));
        }
    } else {
        (*logfn)(instance, "\nError: Load module failed (%s)",
                 get_error_string(res));
    }
    (*logfn)(instance, "\n");

    if (valset) {
        val_free_value(instance, valset);
    }
    ncx_clean_save_deviationsQ(instance, &savedevQ);

    return res;

}  /* do_mgrload */


/********************************************************************
 * FUNCTION do_help_commands (sub-mode of local RPC)
 * 
 * help commands
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    mode == requested help mode
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_help_commands (ncx_instance_t *instance,
                      server_cb_t *server_cb,
                      help_mode_t mode)
{
    modptr_t            *modptr;
    obj_template_t      *obj;
    boolean              anyout, imode;

    imode = interactive_mode();
    anyout = FALSE;

    if (use_servercb(instance, server_cb)) {
        if (imode) {
            log_stdout(instance, "\nServer Commands:\n");
        } else {
            log_write(instance, "\nServer Commands:\n");
        }

        for (modptr = (modptr_t *)
                 dlq_firstEntry(instance, &server_cb->modptrQ);
             modptr != NULL;
             modptr = (modptr_t *)dlq_nextEntry(instance, modptr)) {

            obj = ncx_get_first_object(instance, modptr->mod);
            while (obj) {
                if (obj_is_rpc(instance, obj) && !obj_is_hidden(instance, obj)) {
                    if (mode == HELP_MODE_BRIEF) {
                        obj_dump_template(instance, obj, mode, 1, 0);
                    } else {
                        obj_dump_template(instance, obj, mode, 0, 0);
                    }
                    anyout = TRUE;
                }
                obj = ncx_get_next_object(instance, modptr->mod, obj);
            }
        }
    }

    if (imode) {
        log_stdout(instance, "\nLocal Commands:\n");
    } else {
        log_write(instance, "\nLocal Commands:\n");
    }

    obj = ncx_get_first_object(instance, get_yangcli_mod());
    while (obj) {
        if (obj_is_rpc(instance, obj) && !obj_is_hidden(instance, obj)) {
            if (mode == HELP_MODE_BRIEF) {
                obj_dump_template(instance, obj, mode, 1, 0);
            } else {
                obj_dump_template(instance, obj, mode, 0, 0);
            }
            anyout = TRUE;
        }
        obj = ncx_get_next_object(instance, get_yangcli_mod(), obj);
    }

    if (anyout) {
        if (imode) {
            log_stdout(instance, "\n");
        } else {
            log_write(instance, "\n");
        }
    }

    return NO_ERR;

} /* do_help_commands */


/********************************************************************
 * FUNCTION do_help
 * 
 * Print the general yangcli help text to STDOUT
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the load command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   log_stdout global help message
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_help (ncx_instance_t *instance,
             server_cb_t *server_cb,
             obj_template_t *rpc,
             const xmlChar *line,
             uint32  len)
{
    typ_template_t       *typ;
    obj_template_t       *obj;
    val_value_t          *valset, *parm;
    status_t              res;
    help_mode_t           mode;
    boolean               imode;
    ncx_node_t            dtyp;
    uint32                dlen;

    res = NO_ERR;
    imode = interactive_mode();
    valset = get_valset(instance, server_cb, rpc, &line[len], &res);
    if (res != NO_ERR) {
        if (valset) {
            val_free_value(instance, valset);
        }
        return res;
    }

    mode = HELP_MODE_NORMAL;

    /* look for the 'brief' parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_BRIEF);
    if (parm && parm->res == NO_ERR) {
        mode = HELP_MODE_BRIEF;
    } else {
        /* look for the 'full' parameter */
        parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_FULL);
        if (parm && parm->res == NO_ERR) {
            mode = HELP_MODE_FULL;
        }
    }

    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_COMMAND);
    if (parm && parm->res == NO_ERR) {
        dtyp = NCX_NT_OBJ;
        res = NO_ERR;
        obj = parse_def(instance, server_cb, &dtyp, VAL_STR(parm), &dlen, &res);
        if (obj != NULL) {
            if (obj->objtype == OBJ_TYP_RPC && 
                !obj_is_hidden(instance, obj)) {
                help_object(instance, obj, mode);
            } else {
                res = ERR_NCX_DEF_NOT_FOUND;
                if (imode) {
                    log_stdout(instance,
                               "\nError: command (%s) not found",
                               VAL_STR(parm));
                } else {
                    log_error(instance,
                              "\nError: command (%s) not found",
                              VAL_STR(parm));
                }
            }
        } else {
            if (res == ERR_NCX_DEF_NOT_FOUND) {
                if (imode) {
                    log_stdout(instance,
                               "\nError: command (%s) not found",
                               VAL_STR(parm));
                } else {
                    log_error(instance,
                              "\nError: command (%s) not found",
                              VAL_STR(parm));
                }
            } else if (res == ERR_NCX_AMBIGUOUS_CMD) {
                if (imode) {
                    log_stdout(instance, "\n");
                } else {
                    log_error(instance, "\n");
                }
            } else {
                if (imode) {
                    log_stdout(instance,
                               "\nError: command error (%s)",
                               get_error_string(res));
                } else {
                    log_error(instance,
                              "\nError: command error (%s)",
                              get_error_string(res));
                }
            }
        }
        val_free_value(instance, valset);
        return res;
    }

    /* look for the specific definition parameters */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_COMMANDS);
    if (parm && parm->res==NO_ERR) {
        res = do_help_commands(instance, server_cb, mode);
        val_free_value(instance, valset);
        return res;
    }

    parm = val_find_child(instance, valset, YANGCLI_MOD, NCX_EL_TYPE);
    if (parm && parm->res==NO_ERR) {
        res = NO_ERR;
        dtyp = NCX_NT_TYP;
        typ = parse_def(instance, server_cb, &dtyp, VAL_STR(parm), &dlen, &res);
        if (typ) {
            help_type(instance, typ, mode);
        } else {
            if (res == ERR_NCX_DEF_NOT_FOUND) {
                if (imode) {
                    log_stdout(instance,
                               "\nError: type definition (%s) not found",
                               VAL_STR(parm));
                } else {
                    log_error(instance,
                              "\nError: type definition (%s) not found",
                              VAL_STR(parm));
                }
            } else if (res == ERR_NCX_AMBIGUOUS_CMD) {
                if (imode) {
                    log_stdout(instance, "\n");
                } else {
                    log_error(instance, "\n");
                }
            } else {
                if (imode) {
                    log_stdout(instance,
                               "\nError: type error (%s)",
                               get_error_string(res));
                } else {
                    log_error(instance,
                              "\nError: type error (%s)",
                              get_error_string(res));
                }
            }
        }
        val_free_value(instance, valset);
        return res;
    }

    parm = val_find_child(instance, valset, YANGCLI_MOD, NCX_EL_OBJECT);
    if (parm && parm->res == NO_ERR) {
        xmlChar *valstr = VAL_STR(parm);

        obj = NULL;
        if (valstr && *valstr == '/') {
            /* union matched UrlPath type */
            xmlChar *urlpath;

            urlpath = xpath_convert_url_to_path(instance,
                                                valstr,
                                                server_cb->match_names,
                                                server_cb->alt_names,
                                                FALSE,  /* wildcards */
                                                FALSE,   /* withkeys */
                                                &res);
            if (urlpath != NULL && res == NO_ERR) {
                res = xpath_find_schema_target_int(instance, urlpath, &obj);
            }
            if (urlpath != NULL) {
                m__free(instance, urlpath);
            }
        } else {
            /* union match NcxIdentifier type */
            res = NO_ERR;
            dtyp = NCX_NT_OBJ;
            obj = parse_def(instance, server_cb, &dtyp, valstr, &dlen, &res);
        }

        if (obj) {
            if (obj_is_data(instance, obj) && !obj_is_hidden(instance, obj)) {
                help_object(instance, obj, mode);
                if (obj_is_leafy(instance, obj)) {
                    if (imode) {
                        log_stdout(instance, "\n");
                    } else {
                        log_write(instance, "\n");
                    }
                }
            } else {
                if (imode) {
                    log_stdout(instance,
                               "\nError: object definition (%s) not found",
                               VAL_STR(parm));
                } else {
                    log_error(instance,
                              "\nError: object definition (%s) not found",
                              VAL_STR(parm));
                }
            }
        } else {
            if (res == ERR_NCX_DEF_NOT_FOUND) {
                if (imode) {
                    log_stdout(instance,
                               "\nError: object definition (%s) not found",
                               VAL_STR(parm));
                } else {
                    log_error(instance,
                              "\nError: object definition (%s) not found",
                              VAL_STR(parm));
                }
            } else if (res == ERR_NCX_AMBIGUOUS_CMD) {
                if (imode) {
                    log_stdout(instance, "\n");
                } else {
                    log_error(instance, "\n");
                }
            } else {
                if (imode) {
                    log_stdout(instance,
                               "\nError: object error (%s)",
                               get_error_string(res));
                } else {
                    log_error(instance,
                              "\nError: object error (%s)",
                              get_error_string(res));
                }
            }
        }
        val_free_value(instance, valset);
        return res;
    }

    parm = val_find_child(instance, valset, YANGCLI_MOD, NCX_EL_NOTIFICATION);
    if (parm && parm->res == NO_ERR) {
        res = NO_ERR;
        dtyp = NCX_NT_OBJ;
        obj = parse_def(instance, server_cb, &dtyp, VAL_STR(parm), &dlen, &res);
        if (obj != NULL) {
            if (obj->objtype == OBJ_TYP_NOTIF &&
                !obj_is_hidden(instance, obj)) {
                help_object(instance, obj, mode);
            } else {
                res = ERR_NCX_DEF_NOT_FOUND;
                if (imode) {
                    log_stdout(instance,
                               "\nError: notification definition "
                               "(%s) not found",
                               VAL_STR(parm));
                } else {
                    log_error(instance,
                              "\nError: notification definition "
                              "(%s) not found",
                              VAL_STR(parm));
                }
            }
        } else {
            if (res == ERR_NCX_DEF_NOT_FOUND) {
                if (imode) {
                    log_stdout(instance,
                               "\nError: notification definition "
                               "(%s) not found",
                               VAL_STR(parm));
                } else {
                    log_error(instance,
                              "\nError: notification definition "
                              "(%s) not found",
                              VAL_STR(parm));
                }
            } else if (res == ERR_NCX_AMBIGUOUS_CMD) {
                if (imode) {
                    log_stdout(instance, "\n");
                } else {
                    log_error(instance, "\n");
                }
            } else {
                if (imode) {
                    log_stdout(instance,
                               "\nError: notification error (%s)",
                               get_error_string(res));
                } else {
                    log_error(instance,
                              "\nError: notification error (%s)",
                              get_error_string(res));
                }
            }
        }
        val_free_value(instance, valset);
        return res;
    }


    /* no parameters entered except maybe brief or full */
    switch (mode) {
    case HELP_MODE_BRIEF:
    case HELP_MODE_NORMAL:
        log_stdout(instance, "\n\nyangcli summary:");
        log_stdout(instance, "\n\n  Commands are defined with YANG rpc statements.");
        log_stdout(instance, "\n  Use 'help commands' to see current list of commands.");
        log_stdout(instance, "\n\n  Global variables are created with 2 dollar signs"
                   "\n  in assignment statements ($$foo = 7).");
        log_stdout(instance, "\n  Use 'show globals' to see current list "
                   "of global variables.");
        log_stdout(instance, "\n\n  Local variables (within a stack frame) are created"
                   "\n  with 1 dollar sign in assignment"
                   " statements ($foo = $bar).");
        log_stdout(instance, "\n  Use 'show locals' to see current list "
                   "of local variables.");
        log_stdout(instance, "\n\n  Use 'show vars' to see all program variables.\n");

        if (mode==HELP_MODE_BRIEF) {
            break;
        }

        obj = ncx_find_object(instance, get_yangcli_mod(), YANGCLI_HELP);
        if (obj && obj->objtype == OBJ_TYP_RPC) {
            help_object(instance, obj, HELP_MODE_FULL);
        } else {
            SET_ERROR(instance, ERR_INTERNAL_VAL);
        }
        break;
    case HELP_MODE_FULL:
        help_program_module(instance, 
                            YANGCLI_MOD, 
                            YANGCLI_BOOT, 
                            HELP_MODE_FULL);
        break;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    val_free_value(instance, valset);

    return res;

}  /* do_help */


/********************************************************************
 * FUNCTION do_start_script (sub-mode of run local RPC)
 * 
 * run script=script-filespec
 *
 * run the specified script
 *
 * INPUTS:
 *   server_cb == server control block to use
 *   source == file source
 *   valset == value set for the run script parameters
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_start_script (ncx_instance_t *instance,
                     server_cb_t *server_cb,
                     const xmlChar *source,
                     val_value_t *valset)
{
    xmlChar       *fspec;
    FILE          *fp;
    val_value_t   *parm;
    status_t       res;
    int            num;
    xmlChar        buff[4];

    res = NO_ERR;

    /* saerch for the script */
    fspec = ncxmod_find_script_file(instance, source, &res);
    if (!fspec) {
        return res;
    }

    /* open a new runstack frame if the file exists */
    fp = fopen((const char *)fspec, "r");
    if (!fp) {
        m__free(instance, fspec);
        return ERR_NCX_MISSING_FILE;
    } else {
        res = runstack_push(instance,
                            server_cb->runstack_context,
                            fspec, 
                            fp);
        m__free(instance, fspec);
        if (res != NO_ERR) {
            fclose(fp);
            return res;
        }
    }
    
    /* setup the numeric script parameters */
    memset(buff, 0x0, 4);
    buff[0] = 'P';

    /* add the P1 through P9 parameters that are present */
    for (num=1; num<=YANGCLI_MAX_RUNPARMS; num++) {
        buff[1] = '0' + num;
        parm = (valset) ? val_find_child(instance, 
                                         valset, 
                                         YANGCLI_MOD, 
                                         buff) : NULL;
        if (parm) {
            /* store P7 named as ASCII 7 */
            res = var_set_str(instance,
                              server_cb->runstack_context,
                              buff+1, 
                              1, 
                              parm, 
                              VAR_TYP_LOCAL);
            if (res != NO_ERR) {
                runstack_pop(instance, server_cb->runstack_context);
                return res;
            }
        }
    }

    return res;

}  /* do_start_script */


/********************************************************************
 * FUNCTION do_run (local RPC)
 * 
 * run script=script-filespec
 *
 *
 * Get the specified parameter and run the specified script
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the show command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *    status
 *********************************************************************/
static status_t
    do_run (ncx_instance_t *instance,
            server_cb_t *server_cb,
            obj_template_t *rpc,
            const xmlChar *line,
            uint32  len)
{
    val_value_t  *valset, *parm;
    status_t      res;

    res = NO_ERR;

    /* get the 'script' parameter */
    valset = get_valset(instance, server_cb, rpc, &line[len], &res);

    if (valset && res == NO_ERR) {
        /* there is 1 parm */
        parm = val_find_child(instance, 
                              valset, 
                              YANGCLI_MOD, 
                              NCX_EL_SCRIPT);
        if (!parm) {
            res = ERR_NCX_DEF_NOT_FOUND;
        } else if (parm->res != NO_ERR) {
            res = parm->res;
        } else {
            /* the parm val is the script filespec */
            res = do_start_script(instance, server_cb, VAL_STR(parm), valset);
            if (res != NO_ERR) {
                log_write(instance,
                          "\nError: start script %s failed (%s)",
                          obj_get_name(instance, rpc),
                          get_error_string(res));
            }
        }
    }

    if (valset) {
        val_free_value(instance, valset);
    }

    return res;

}  /* do_run */


/********************************************************************
 * FUNCTION do_log (local RPC)
 * 
 * log-error msg=string
 * log-warn msg=string
 * log-info msg=string
 * log-debug msg=string
 *
 * Print the log message if the log-level is high enough
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the show command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *    level == required log level
 *
 * RETURNS:
 *    status
 *********************************************************************/
static status_t
    do_log (ncx_instance_t *instance,
            server_cb_t *server_cb,
            obj_template_t *rpc,
            const xmlChar *line,
            uint32  len,
            log_debug_t level)
{
    val_value_t  *valset, *parm;
    status_t      res;

    res = NO_ERR;

    /* get the 'script' parameter */
    valset = get_valset(instance, server_cb, rpc, &line[len], &res);

    if (valset && res == NO_ERR) {
        /* there is 1 parm */
        parm = val_find_child(instance, 
                              valset, 
                              YANGCLI_MOD, 
                              YANGCLI_MSG);
        if (!parm) {
            res = ERR_NCX_DEF_NOT_FOUND;
        } else if (parm->res != NO_ERR) {
            res = parm->res;
        } else if (VAL_STR(parm)) {
            switch (level) {
            case LOG_DEBUG_ERROR:
                log_error(instance, "\nError: %s\n", VAL_STR(parm));
                break;
            case LOG_DEBUG_WARN:
                log_warn(instance, "\nWarning: %s\n", VAL_STR(parm));
                break;
            case LOG_DEBUG_INFO:
                log_info(instance, "\nInfo: %s\n", VAL_STR(parm));
                break;
            case LOG_DEBUG_DEBUG:
                log_debug(instance, "\nDebug: %s\n", VAL_STR(parm));
                break;
            default:
                SET_ERROR(instance, ERR_INTERNAL_VAL);
            }
        }
    }

    if (valset) {
        val_free_value(instance, valset);
    }

    return res;

}  /* do_log */


/********************************************************************
 * FUNCTION pwd
 * 
 * Print the current working directory
 *
 * INPUTS:
 *    rpc == RPC method for the load command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   log_stdout global help message
 *
 *********************************************************************/
static void
    pwd (ncx_instance_t *instance)
{
    char             *buff;
    boolean           imode;

    imode = interactive_mode();

    buff = m__getMem(instance, YANGCLI_BUFFLEN);
    if (!buff) {
        if (imode) {
            log_stdout(instance, "\nMalloc failure\n");
        } else {
            log_write(instance, "\nMalloc failure\n");
        }
        return;
    }

    if (!getcwd(buff, YANGCLI_BUFFLEN)) {
        if (imode) {
            log_stdout(instance, "\nGet CWD failure\n");
        } else {
            log_write(instance, "\nGet CWD failure\n");
        }
        m__free(instance, buff);
        return;
    }

    if (imode) {
        log_stdout(instance, "\nCurrent working directory is %s\n", buff);
    } else {
        log_write(instance, "\nCurrent working directory is %s\n", buff);
    }

    m__free(instance, buff);

}  /* pwd */


/********************************************************************
 * FUNCTION do_pwd
 * 
 * Print the current working directory
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the load command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   log_stdout global help message
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_pwd (ncx_instance_t *instance,
            server_cb_t *server_cb,
            obj_template_t *rpc,
            const xmlChar *line,
            uint32  len)
{
    val_value_t      *valset;
    status_t          res = NO_ERR;

    valset = get_valset(instance, server_cb, rpc, &line[len], &res);
    if (res == NO_ERR || res == ERR_NCX_SKIPPED) {
        pwd(instance);
    }
    if (valset) {
        val_free_value(instance, valset);
    }
    return res;

}  /* do_pwd */


/********************************************************************
 * FUNCTION do_cd
 * 
 * Change the current working directory
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the load command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   log_stdout global help message
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_cd (ncx_instance_t *instance,
           server_cb_t *server_cb,
           obj_template_t *rpc,
           const xmlChar *line,
           uint32  len)
{
    val_value_t      *valset, *parm;
    xmlChar          *pathstr;
    status_t          res;
    int               ret;
    boolean           imode;

    res = NO_ERR;
    imode = interactive_mode();
    valset = get_valset(instance, server_cb, rpc, &line[len], &res);
    if (res != NO_ERR) {
        if (valset) {
            val_free_value(instance, valset);
        }
        return res;
    }

    parm = val_find_child(instance, 
                          valset, 
                          YANGCLI_MOD, 
                          YANGCLI_DIR);
    if (parm == NULL || 
	parm->res != NO_ERR || 
	xml_strlen(instance, VAL_STR(parm)) == 0) {
        val_free_value(instance, valset);
        log_error(instance, "\nError: 'dir' parameter is missing");
        return ERR_NCX_MISSING_PARM;
    }

    res = NO_ERR;
    pathstr = ncx_get_source_ex(instance, VAL_STR(parm), FALSE, &res);
    if (!pathstr) {
        val_free_value(instance, valset);
        log_error(instance,
                  "\nError: get path string failed (%s)",
                  get_error_string(res));
        return res;
    }

    ret = chdir((const char *)pathstr);
    if (ret) {
        res = ERR_NCX_INVALID_VALUE;
        if (imode) {
            log_stdout(instance,
                       "\nChange CWD failure (%s)\n",
                       get_error_string(errno_to_status()));
        } else {
            log_write(instance,
                      "\nChange CWD failure (%s)\n",
                      get_error_string(errno_to_status()));
        }
    } else {
        pwd(instance);
    }

    val_free_value(instance, valset);
    m__free(instance, pathstr);

    return res;

}  /* do_cd */


/********************************************************************
 * FUNCTION do_fill
 * 
 * Fill an object for use in a PDU
 *
 * INPUTS:
 *    server_cb == server control block to use (NULL if none)
 *    rpc == RPC method for the load command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *    iswrite == TRUE if write op; FALSE if read op
 *
 * OUTPUTS:
 *   the completed data node is output and
 *   is usually part of an assignment statement
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_fill (ncx_instance_t *instance,
             server_cb_t *server_cb,
             obj_template_t *rpc,
             const xmlChar *line,
             uint32  len,
             boolean iswrite)
{
    val_value_t           *valset, *valroot, *parm, *newparm, *curparm;
    val_value_t           *targval;
    obj_template_t        *targobj;
    const xmlChar         *target;
    status_t               res;
    boolean                save_getopt;

    res = NO_ERR;
    valset = get_valset(instance, server_cb, rpc, &line[len], &res);
    if (res != NO_ERR) {
        if (valset) {
            val_free_value(instance, valset);
        }
        return res;
    }

    target = NULL;
    parm = val_find_child(instance, valset, YANGCLI_MOD, NCX_EL_TARGET);
    if (!parm || parm->res != NO_ERR) {
        val_free_value(instance, valset);
        return ERR_NCX_MISSING_PARM;
    } else {
        target = VAL_STR(parm);
    }

    save_getopt = server_cb->get_optional;

    newparm = NULL;
    curparm = NULL;
    targobj = NULL;
    targval = NULL;

    /* get the root object and the target node
     * if target == /foo/bar/baz then
     * valroot --> /foo and targobj --> /foo/bar/baz
     */
    valroot = get_instanceid_parm(instance, target, TRUE, TRUE, &targobj, &targval, &res);
    if (res != NO_ERR) {
        if (valroot) {
            val_free_value(instance, valroot);
        }
        val_free_value(instance, valset);
        clear_result(instance, server_cb);
        return res;
    } else if (valroot == NULL) {
        /* this means the docroot was selected */
        log_error(instance, "\nError: The document root is not allowed here as a target");
        res = ERR_NCX_OPERATION_NOT_SUPPORTED;
        val_free_value(instance, valset);
        clear_result(instance, server_cb);
        return res;
    } else if (targval != valroot) {
        /* keep targval, toss valroot */
        val_remove_child(instance, targval);
        val_free_value(instance, valroot);
        valroot = NULL;
    }

    /* check if targval is valid if it is an empty string
     * this corner-case is what the get_instanceid_parm will
     * return if the last node was a leaf
     */
    if (targval && 
        targval->btyp == NCX_BT_STRING &&
        VAL_STR(targval) == NULL) {

        /* the NULL string was not entered;
         * if a value was entered as '' it would be recorded
         * as a zero-length string, not a NULL string
         */
        val_free_value(instance, targval);
        targval = NULL;
    }

    /* find the value to use as content value or template, if any */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_VALUE);
    if (parm && parm->res == NO_ERR) {
        curparm = parm;
    }

    /* find the --optional flag */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_OPTIONAL);
    if (parm && parm->res == NO_ERR) {
        server_cb->get_optional = TRUE;
    }

    /* fill in the value based on all the parameters */
    switch (targobj->objtype) {
    case OBJ_TYP_ANYXML:
    case OBJ_TYP_LEAF:
    case OBJ_TYP_LEAF_LIST:
        /* make a new leaf, toss the targval if any */
        newparm = fill_value(instance, server_cb, rpc, targobj, curparm, &res);
        if (targval) {
            val_free_value(instance, targval);
            targval = NULL;
        }
        break;
    case OBJ_TYP_CHOICE:
        if (targval) {
            newparm = targval;
        } else {
            newparm = val_new_value(instance);
            if (!newparm) {
                log_error(instance, "\nError: malloc failure");
                res = ERR_INTERNAL_MEM;
            } else {
                val_init_from_template(instance, newparm, targobj);
            }
        }
        if (res == NO_ERR) {
            res = get_choice(instance, server_cb, rpc, targobj, newparm, 
                             curparm, iswrite, FALSE);
            if (res == ERR_NCX_SKIPPED) {
                res = NO_ERR;
            }
        }
        break;
    case OBJ_TYP_CASE:
        if (targval) {
            newparm = targval;
        } else {
            newparm = val_new_value(instance);
            if (!newparm) {
                log_error(instance, "\nError: malloc failure");
                res = ERR_INTERNAL_MEM;
            } else {
                val_init_from_template(instance, newparm, targobj);
            }
        }
        res = get_case(instance, server_cb, targobj, newparm, curparm, iswrite, FALSE);
        if (res == ERR_NCX_SKIPPED) {
            res = NO_ERR;
        }
        break;
    default:
        if (targval) {
            newparm = targval;
        } else {
            newparm = val_new_value(instance);
            if (!newparm) {
                log_error(instance, "\nError: malloc failure");
                res = ERR_INTERNAL_MEM;
            } else {
                val_init_from_template(instance, newparm, targobj);
            }
        }
        res = fill_valset(instance, server_cb, rpc, newparm, curparm, TRUE, FALSE);
        if (res == ERR_NCX_SKIPPED) {
            res = NO_ERR;
        }
    }

    /* check save result or clear it */
    if (res == NO_ERR) {
        if (server_cb->result_name || 
            server_cb->result_filename) {
            /* set the index chains because the value is being saved */
            res = val_build_index_chains(instance, newparm);

            /* save the filled in value */
            if (res == NO_ERR) {
                res = finish_result_assign(instance, server_cb, newparm, NULL);
                newparm = NULL;
            }
        }
    } else {
        clear_result(instance, server_cb);
    }

    /* cleanup */
    val_free_value(instance, valset);
    if (newparm) {
        val_free_value(instance, newparm);
    }
    server_cb->get_optional = save_getopt;

    return res;

}  /* do_fill */


/********************************************************************
* FUNCTION add_content
* 
* Add the config nodes to the parent
*
* INPUTS:
*   server_cb == server control block to use
*   rpc == RPC method in progress
*   config_content == the node associated with the target
*             to be used as content nested within the 
*             <config> element
*   curobj == the current object node for config_content, going
*                 up the chain to the root object.
*                 First call should pass config_content->obj
*   dofill == TRUE if interactive mode and mandatory parms
*             need to be specified (they can still be skipped)
*             FALSE if no checking for mandatory parms
*             Just fill out the minimum path to root from
*             the 'config_content' node
*   curtop == address of steady-storage node to add the
*             next new level
*
* OUTPUTS:
*    config node is filled in with child nodes
*    curtop is set to the latest top value
*       It is not needed after the last call and should be ignored
*
* RETURNS:
*    status; config_content is NOT freed if returning an error
*********************************************************************/
static status_t
    add_content (ncx_instance_t *instance,
                 server_cb_t *server_cb,
                 obj_template_t *rpc,
                 val_value_t *config_content,
                 obj_template_t *curobj,
                 boolean dofill,
                 val_value_t **curtop)
{

    obj_key_t             *curkey;
    val_value_t           *newnode, *keyval, *lastkey;
    status_t               res;
    boolean                done, content_used;

    res = NO_ERR;
    newnode = NULL;

    set_completion_state(instance,
                         &server_cb->completion_state,
                         rpc, curobj, CMD_STATE_GETVAL);

    /* add content based on the current node type */
    switch (curobj->objtype) {
    case OBJ_TYP_ANYXML:
    case OBJ_TYP_LEAF:
    case OBJ_TYP_LEAF_LIST:
        if (curobj != config_content->obj) {
            return SET_ERROR(instance, ERR_INTERNAL_VAL);
        }
        if (!obj_is_key(config_content->obj)) {
            val_add_child(instance, config_content, *curtop);
            *curtop = config_content;
        }
        break;
    case OBJ_TYP_LIST:
        /* get all the key nodes for the current object,
         * if they do not already exist
         */
        if (curobj == config_content->obj) {
            val_add_child(instance, config_content, *curtop);
            *curtop = config_content;
            return NO_ERR;
        }

        /* else need to fill in the keys for this content layer */
        newnode = val_new_value(instance);
        if (!newnode) {
            return ERR_INTERNAL_MEM;
        }
        val_init_from_template(instance, newnode, curobj);
        val_add_child(instance, newnode, *curtop);
        *curtop = newnode;
        content_used = FALSE;

        lastkey = NULL;
        for (curkey = obj_first_key(instance, curobj);
             curkey != NULL;
             curkey = obj_next_key(instance, curkey)) {

            keyval = val_find_child(instance,
                                    *curtop,
                                    obj_get_mod_name(instance, curkey->keyobj),
                                    obj_get_name(instance, curkey->keyobj));
            if (!keyval) {
                if (curkey->keyobj == config_content->obj) {
                    keyval = config_content;
                    val_insert_child(instance, keyval, lastkey, *curtop);
                    content_used = TRUE;
                    lastkey = keyval;
                    res = NO_ERR;
                } else if (dofill) {
                    res = get_parm(instance, 
                                   server_cb, 
                                   rpc, 
                                   curkey->keyobj, 
                                   *curtop, 
                                   NULL);
                    if (res == ERR_NCX_SKIPPED) {
                        res = NO_ERR;
                    } else if (res != NO_ERR) {
                        return res;
                    } else {
                        keyval = val_find_child(instance,
                                                *curtop,
                                                obj_get_mod_name
                                                (instance, curkey->keyobj),
                                                obj_get_name
                                                (instance, curkey->keyobj));
                        if (!keyval) {
                            return SET_ERROR(instance, ERR_INTERNAL_VAL);
                        }
                        val_remove_child(instance, keyval);
                        val_insert_child(instance, keyval, lastkey, *curtop);
                        lastkey = keyval;
                    } /* else skip this key (for debugging server) */
                }  /* else --nofill; skip this node */
            }
        }

        /* wait until all the key leafs are accounted for before
         * changing the *curtop pointer
         */
        if (content_used) {
            *curtop = config_content;
        }
        break;
    case OBJ_TYP_CONTAINER:
        if (curobj == config_content->obj) {
            val_add_child(instance, config_content, *curtop);
            *curtop = config_content;
        } else {
            newnode = val_new_value(instance);
            if (!newnode) {
                return ERR_INTERNAL_MEM;
            }
            val_init_from_template(instance, newnode, curobj);
            val_add_child(instance, newnode, *curtop);
            *curtop = newnode;
        }
        break;
    case OBJ_TYP_CHOICE:
    case OBJ_TYP_CASE:
        if (curobj != config_content->obj) {
            /* nothing to do for the choice level if the target is a case */
            res = NO_ERR;
            break;
        }
        done = FALSE;
        while (!done) {
            newnode = val_get_first_child(instance, config_content);
            if (newnode) {
                val_remove_child(instance, newnode);
                res = add_content(instance, server_cb, rpc, newnode, newnode->obj, 
                                  dofill, curtop);
                if (res != NO_ERR) {
                    val_free_value(instance, newnode);
                    newnode = NULL;
                    done = TRUE;
                }
            } else {
                done = TRUE;
            }
        }
        if (res == NO_ERR) {
            val_free_value(instance, config_content);
        }
        *curtop = newnode;
        break;
    default:
        /* any other object type is an error */
        res = SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    return res;

}  /* add_content */


/********************************************************************
* FUNCTION add_config_from_content_node
* 
* Add the config node content for the edit-config operation
* Build the <config> nodfe top-down, by recursing bottom-up
* from the node to be edited.
*
* INPUTS:
*   server_cb == server control block to use
*   rpc == RPC method in progress
*   config_content == the node associated with the target
*             to be used as content nested within the 
*             <config> element
*   curobj == the current object node for config_content, going
*                 up the chain to the root object.
*                 First call should pass config_content->obj
*   config == the starting <config> node to add the data into
*   curtop == address of stable storage for current add-to node
*            This pointer MUST be set to NULL upon first fn call
* OUTPUTS:
*    config node is filled in with child nodes
*
* RETURNS:
*    status; config_content is NOT freed if returning an error
*********************************************************************/
static status_t
    add_config_from_content_node (ncx_instance_t *instance,
                                  server_cb_t *server_cb,
                                  obj_template_t *rpc,
                                  val_value_t *config_content,
                                  obj_template_t *curobj,
                                  val_value_t *config,
                                  val_value_t **curtop)
{
    obj_template_t  *parent;
    status_t         res;

    /* get to the root of the object chain */
    parent = obj_get_parent(instance, curobj);
    if (parent && !obj_is_root(parent)) {
        res = add_config_from_content_node(instance, server_cb, rpc, config_content,
                                           parent, config, curtop);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* set the current target, working down the stack
     * on the way back from the initial dive
     */
    if (!*curtop) {
        /* first time through to this point */
        *curtop = config;
    }

    res = add_content(instance, server_cb, rpc, config_content, curobj, TRUE, curtop);
    return res;

}  /* add_config_from_content_node */


/********************************************************************
* FUNCTION complete_path_content
* 
* Use the valroot and content pointer to work up the already
* constructed value tree to find any missing mandatory
* nodes or key leafs
*
* INPUTS:
*   server_cb == server control block to use
*   rpc == RPC method in progress
*   valroot == value root of content
*           == NULL if not used (config_content used instead)
*   config_content == the node associated with the target
*                    within valroot (if valroot non-NULL)
*   dofill == TRUE if fill mode
*          == FALSE if --nofill is in effect
*   iswrite == TRUE if write op; FALSE if read op
*
* OUTPUTS:
*    valroot tree is filled out as needed and requested
*
* RETURNS:
*    status; config_content is NOT freed if returning an error
*********************************************************************/
static status_t
    complete_path_content (ncx_instance_t *instance,
                           server_cb_t *server_cb,
                           obj_template_t *rpc,
                           val_value_t *valroot,
                           val_value_t *config_content,
                           boolean dofill,
                           boolean iswrite)
{
    obj_key_t         *curkey;
    val_value_t       *curnode, *keyval, *lastkey;
    status_t           res;
    boolean            done;

    if (!server_cb->get_optional && !iswrite) {
        return NO_ERR;
    }

    res = NO_ERR;

    curnode = config_content;

    /* handle all the nodes from bottom to valroot */
    done = FALSE;
    while (!done) {
        if (curnode->btyp == NCX_BT_LIST) {
            lastkey = NULL;
            for (curkey = obj_first_key(instance, curnode->obj);
                 curkey != NULL;
                 curkey = obj_next_key(instance, curkey)) {

                keyval = val_find_child(instance,
                                        curnode,
                                        obj_get_mod_name(instance, curkey->keyobj),
                                        obj_get_name(instance, curkey->keyobj));
                if (keyval == NULL && dofill) {
                    res = get_parm(instance, server_cb, rpc, curkey->keyobj, 
                                   curnode, NULL);
                    if (res == ERR_NCX_SKIPPED) {
                        res = NO_ERR;
                    } else if (res != NO_ERR) {
                        return res;
                    } else {
                        keyval = val_find_child(instance, curnode, obj_get_mod_name
                                                (instance, curkey->keyobj),
                                                obj_get_name(instance, curkey->keyobj));
                        if (!keyval) {
                            return SET_ERROR(instance, ERR_INTERNAL_VAL);
                        }
                        val_remove_child(instance, keyval);
                        val_insert_child(instance, keyval, lastkey, curnode);
                        lastkey = keyval;
                    } /* else skip this key (for debugging server) */
                }  /* else --nofill; skip this node */
            } /* for all the keys in the list */
        } /* else not list so skip this node */

        /* move up the tree */
        if (curnode != valroot && curnode->parent != NULL) {
            curnode = curnode->parent;
        } else {
            done = TRUE;
        }
    }

    return res;

}  /* complete_path_content */


/********************************************************************
* FUNCTION add_filter_from_content_node
* 
* Add the filter node content for the get or get-config operation
* Build the <filter> node top-down, by recursing bottom-up
* from the node to be edited.
*
* INPUTS:
*   server_cb == server control block to use
*   rpc == RPC method in progress
*   get_content == the node associated with the target
*             to be used as content nested within the 
*             <filter> element
*   curobj == the current object node for get_content, going
*                 up the chain to the root object.
*                 First call should pass get_content->obj
*   filter == the starting <filter> node to add the data into
*   dofill == TRUE for fill mode
*             FALSE to skip filling any nodes
*   curtop == address of stable storage for current add-to node
*            This pointer MUST be set to NULL upon first fn call
* OUTPUTS:
*    filter node is filled in with child nodes
*
* RETURNS:
*    status; get_content is NOT freed if returning an error
*********************************************************************/
static status_t
    add_filter_from_content_node (ncx_instance_t *instance,
                                  server_cb_t *server_cb,
                                  obj_template_t *rpc,
                                  val_value_t *get_content,
                                  obj_template_t *curobj,
                                  val_value_t *filter,
                                  boolean dofill,
                                  val_value_t **curtop)
{
    obj_template_t  *parent;
    status_t         res;

    /* get to the root of the object chain */
    parent = obj_get_parent(instance, curobj);
    if (parent && !obj_is_root(parent)) {
        res = add_filter_from_content_node(instance, server_cb, rpc, get_content,
                                           parent, filter, dofill, curtop);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* set the current target, working down the stack
     * on the way back from the initial dive
     */
    if (!*curtop) {
        /* first time through to this point */
        *curtop = filter;
    }

    res = add_content(instance, server_cb, rpc, get_content, curobj, dofill, curtop);
    return res;

}  /* add_filter_from_content_node */


/********************************************************************
* FUNCTION send_edit_config_to_server
* 
* Send an <edit-config> operation to the server
*
* Fills out the <config> node based on the config_target node
* Any missing key nodes will be collected (via CLI prompt)
* along the way.
*
* INPUTS:
*   server_cb == server control block to use
*   valroot == value tree root if used
*              (via get_content_from_choice)
*   config_content == the node associated with the target
*             to be used as content nested within the 
*             <config> element
*   timeoutval == timeout value to use
*   def_editop == default-operation to use (or NOT_SET)
*
* OUTPUTS:
*    server_cb->state may be changed or other action taken
*
*    !!! valroot is consumed id non-NULL
*    !!! config_content is consumed -- freed or transfered to a PDU
*    !!! that will be freed later
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    send_edit_config_to_server (ncx_instance_t *instance,
                                server_cb_t *server_cb,
                                val_value_t *valroot,
                                val_value_t *config_content,
                                uint32 timeoutval,
                                op_defop_t def_editop)
{
    obj_template_t        *rpc, *input, *child;
    const xmlChar         *defopstr;
    mgr_rpc_req_t         *req;
    val_value_t           *reqdata, *parm, *target, *dummy_parm;
    ses_cb_t              *scb;
    status_t               res;
    boolean                freeroot, dofill;

    req = NULL;
    reqdata = NULL;
    res = NO_ERR;
    dofill = TRUE;

    if (LOGDEBUG) {
        log_debug(instance, "\nSending <edit-config> request");
    }

    /* either going to free valroot or config_content */
    if (valroot == NULL || valroot == config_content) {
        freeroot = FALSE;
    } else {
        freeroot = TRUE;
    }

    /* make sure there is an edit target on this server */
    if (!server_cb->default_target) {
        log_error(instance, "\nError: no <edit-config> target available on server");
        if (freeroot) {
            val_free_value(instance, valroot);
        } else {
            val_free_value(instance, config_content);
        }
        return ERR_NCX_OPERATION_FAILED;
    }

    /* get the <edit-config> template */
    rpc = ncx_find_object(instance, 
                          get_netconf_mod(instance, server_cb), 
                          NCX_EL_EDIT_CONFIG);
    if (!rpc) {
        if (freeroot) {
            val_free_value(instance, valroot);
        } else {
            val_free_value(instance, config_content);
        }
        return SET_ERROR(instance, ERR_NCX_DEF_NOT_FOUND);
    }

    /* get the 'input' section container */
    input = obj_find_child(instance, rpc, NULL, YANG_K_INPUT);
    if (!input) {
        if (freeroot) {
            val_free_value(instance, valroot);
        } else {
            val_free_value(instance, config_content);
        }
        return SET_ERROR(instance, ERR_NCX_DEF_NOT_FOUND);
    }

    /* construct a method + parameter tree */
    reqdata = xml_val_new_struct(instance, obj_get_name(instance, rpc), obj_get_nsid(instance, rpc));
    if (!reqdata) {
        if (freeroot) {
            val_free_value(instance, valroot);
        } else {
            val_free_value(instance, config_content);
        }
        log_error(instance, "\nError allocating a new RPC request");
        return ERR_INTERNAL_MEM;
    }

    /* set the edit-config/input/target node to the default_target */
    child = obj_find_child(instance, 
                           input, 
                           NC_MODULE,
                           NCX_EL_TARGET);
    parm = val_new_value(instance);
    if (!parm) {
        if (freeroot) {
            val_free_value(instance, valroot);
        } else {
            val_free_value(instance, config_content);
        }
        val_free_value(instance, reqdata);
        return ERR_INTERNAL_MEM;
    } else {
        val_init_from_template(instance, parm, child);
        val_add_child(instance, parm, reqdata);
    }

    target = xml_val_new_flag(instance, server_cb->default_target, obj_get_nsid(instance, child));
    if (!target) {
        if (freeroot) {
            val_free_value(instance, valroot);
        } else {
            val_free_value(instance, config_content);
        }
        val_free_value(instance, reqdata);
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, target, parm);
    }

    /* set the edit-config/input/default-operation node */
    if (!(def_editop == OP_DEFOP_NOT_USED ||
          def_editop == OP_DEFOP_NOT_SET)) {          
        child = obj_find_child(instance, input, NC_MODULE, NCX_EL_DEFAULT_OPERATION);
        res = NO_ERR;
        defopstr = op_defop_name(def_editop);
        parm = val_make_simval_obj(instance, child, defopstr, &res);
        if (!parm) {
            if (freeroot) {
                val_free_value(instance, valroot);
            } else {
                val_free_value(instance, config_content);
            }
            val_free_value(instance, reqdata);
            return res;
        } else {
            val_add_child(instance, parm, reqdata);
        }
    }

    /* set the test-option to the user-configured or default value */
    if (server_cb->testoption != OP_TESTOP_NONE) {
        /* set the edit-config/input/test-option node to
         * the user-specified value
         */
        child = obj_find_child(instance, input, NC_MODULE, NCX_EL_TEST_OPTION);
        res = NO_ERR;
        parm = val_make_simval_obj(instance,
                                   child,
                                   op_testop_name(server_cb->testoption),
                                   &res);
        if (!parm) {
            if (freeroot) {
                val_free_value(instance, valroot);
            } else {
                val_free_value(instance, config_content);
            }
            val_free_value(instance, reqdata);
            return res;
        } else {
            val_add_child(instance, parm, reqdata);
        }
    }

    /* set the error-option to the user-configured or default value */
    if (server_cb->erroption != OP_ERROP_NONE) {
        /* set the edit-config/input/error-option node to
         * the user-specified value
         */
        child = obj_find_child(instance, input, NC_MODULE, NCX_EL_ERROR_OPTION);
        res = NO_ERR;
        parm = val_make_simval_obj(instance,
                                   child,
                                   op_errop_name(server_cb->erroption),
                                   &res);
        if (!parm) {
            if (freeroot) {
                val_free_value(instance, valroot);
            } else {
                val_free_value(instance, config_content);
            }
            val_free_value(instance, reqdata);
            return ERR_INTERNAL_MEM;
        } else {
            val_add_child(instance, parm, reqdata);
        }
    }

    /* create the <config> node */
    child = obj_find_child(instance, input, NC_MODULE, NCX_EL_CONFIG);
    parm = val_new_value(instance);
    if (!parm) {
        if (freeroot) {
            val_free_value(instance, valroot);
        } else {
            val_free_value(instance, config_content);
        }
        val_free_value(instance, reqdata);
        return ERR_INTERNAL_MEM;
    } else {
        val_init_from_template(instance, parm, child);
        val_add_child(instance, parm, reqdata);
    }


    /* set the edit-config/input/config node to the
     * config_content, but after filling in any
     * missing nodes from the root to the target
     */
    if (valroot) {
        val_add_child(instance, valroot, parm);
        res = complete_path_content(instance, server_cb, rpc, valroot, config_content,
                                    dofill, TRUE);
        if (res != NO_ERR) {
            val_free_value(instance, valroot);
            val_free_value(instance, reqdata);
            return res;
        }
    } else {
        /* !!! use the parm as-is, even if a container !!! */
        dummy_parm = NULL;
        res = add_config_from_content_node(instance, server_cb, rpc, config_content,
                                           config_content->obj, parm, 
                                           &dummy_parm);
        if (res != NO_ERR) {
            val_free_value(instance, config_content);
            val_free_value(instance, reqdata);
            return res;
        }
    }

    /* now that all the content is gathered the OBJ_TYP_CHOICE
     * and OBJ_TYP_CASE place-holder nodes can be deleted
     * this can only happen if the edit target node requested
     * is a choice or a case;
     */
    if (config_content->btyp == NCX_BT_CHOICE ||
        config_content->btyp == NCX_BT_CASE) {

        /* insert all of the children of this choice
         * in place of itself in the parent node
         */
        val_move_children(instance, config_content, config_content->parent);

        /* now config_content should be empty with all its child
         * nodes now siblings; remove it and discard
         */
        val_remove_child(instance, config_content);
        val_free_value(instance, config_content);
        config_content = NULL;
    }

    /* rearrange the nodes to canonical order if requested */
    if (server_cb->fixorder) {
        /* must set the order of a root container seperately */
        val_set_canonical_order(instance, parm);
    }

    /* !!! config_content consumed at this point !!!
     * allocate an RPC request
     */
    scb = mgr_ses_get_scb(server_cb->mysid);
    if (!scb) {
        res = SET_ERROR(instance, ERR_INTERNAL_PTR);
    } else {
        req = mgr_rpc_new_request(instance, scb);
        if (!req) {
            res = ERR_INTERNAL_MEM;
            log_error(instance, "\nError allocating a new RPC request");
        } else {
            req->data = reqdata;
            req->rpc = rpc;
            req->timeout = timeoutval;
        }
    }
        
    /* if all OK, send the RPC request */
    if (res == NO_ERR) {
        if (LOGDEBUG2) {
            log_debug2(instance, "\nabout to send RPC request with reqdata:");
            val_dump_value_max(instance, reqdata, 0, server_cb->defindent,
                               DUMP_VAL_LOG, server_cb->display_mode,
                               FALSE, FALSE);
        }

        /* the request will be stored if this returns NO_ERR */
        res = mgr_rpc_send_request(instance, scb, req, (mgr_rpc_cbfn_t)yangcli_reply_handler);
    }

    /* cleanup and set next state */
    if (res != NO_ERR) {
        if (req) {
            mgr_rpc_free_request(instance, req);
        } else if (reqdata) {
            val_free_value(instance, reqdata);
        }
    } else {
        server_cb->state = MGR_IO_ST_CONN_RPYWAIT;
    }

    return res;

} /* send_edit_config_to_server */


/********************************************************************
 * FUNCTION add_filter_attrs
 * 
 * Add the type and possibly the select 
 * attribute to a value node
 *
 * INPUTS:
 *    val == value node to set
 *    selectval == select node to use, (type=xpath)
 *               == NULL for type = subtree
 *              !!! this memory is consumed here !!!
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    add_filter_attrs (ncx_instance_t *instance,
                      val_value_t *val,
                      val_value_t *selectval)
{
    val_value_t          *metaval;

    /* create a value node for the meta-value */
    metaval = val_make_string(instance, 0, NCX_EL_TYPE,
                              (selectval) 
                              ?  NCX_EL_XPATH : NCX_EL_SUBTREE);
    if (!metaval) {
        return ERR_INTERNAL_MEM;
    }
    dlq_enque(instance, metaval, &val->metaQ);

    if (selectval) {
        val_set_qname(instance, selectval, 0, NCX_EL_SELECT, xml_strlen(instance, NCX_EL_SELECT));
        dlq_enque(instance, selectval, &val->metaQ);
    }

    return NO_ERR;

} /* add_filter_attrs */


/********************************************************************
* FUNCTION send_get_to_server
* 
* Send an <get> operation to the specified server
*
* Fills out the <filter> node based on the config_target node
* Any missing key nodes will be collected (via CLI prompt)
* along the way.
*
* INPUTS:
*   server_cb == server control block to use
*   valroot == root of get_data if set via
*              the CLI Xpath code; if so, then the
*              get_content is the target within this
*              subtree (may == get_content)
*           == NULL if not used and get_content is
*              the only parameter gathered from input
*   get_content == the node associated with the target
*             to be used as content nested within the 
*             <filter> element
*             == NULL to use selectval instead
*   selectval == value with select string in it
*             == NULL to use get_content instead
*
*   (if both == NULL then no filter is added)
*
*   source == optional database source 
*             <candidate>, <running>
*   timeoutval == timeout value to use
*   dofill == TRUE if interactive mode and mandatory parms
*             need to be specified (they can still be skipped)
*             FALSE if no checking for mandatory parms
*             Just fill out the minimum path to root from
*             the 'get_content' node
*   withdef == the desired with-defaults parameter
*              It may be ignored or altered, depending on
*              whether the server supports the capability or not
*
* OUTPUTS:
*    server_cb->state may be changed or other action taken
*
*    !!! valroot is consumed -- freed or transfered to a PDU
*    !!! that will be freed later
*
*    !!! get_content is consumed -- freed or transfered to a PDU
*    !!! that will be freed later
*
*    !!! source is consumed -- freed or transfered to a PDU
*    !!! that will be freed later
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    send_get_to_server (ncx_instance_t *instance,
                        server_cb_t *server_cb,
                        val_value_t *valroot,
                        val_value_t *get_content,
                        val_value_t *selectval,
                        val_value_t *source,
                        uint32 timeoutval,
                        boolean dofill,
                        ncx_withdefaults_t withdef)
{
    obj_template_t        *rpc, *input, *withdefobj;
    val_value_t           *reqdata, *filter;
    val_value_t           *withdefval, *dummy_parm;
    mgr_rpc_req_t         *req;
    ses_cb_t              *scb;
    mgr_scb_t             *mscb;
    status_t               res;
    boolean                freeroot;

    req = NULL;
    reqdata = NULL;
    res = NO_ERR;
    input = NULL;
    filter = NULL;

    if (LOGDEBUG) {
        if (source != NULL) {
            log_debug(instance, "\nSending <get-config> request");
        } else {
            log_debug(instance, "\nSending <get> request");
        }
    }

    /* either going to free valroot or config_content */
    if (valroot == NULL || valroot == get_content) {
        freeroot = FALSE;
    } else {
        freeroot = TRUE;
    }

    /* get the <get> or <get-config> input template */
    rpc = ncx_find_object(instance,
                          get_netconf_mod(instance, server_cb),
                          (source) ? 
                          NCX_EL_GET_CONFIG : NCX_EL_GET);
    if (rpc) {
        input = obj_find_child(instance, rpc, NULL, YANG_K_INPUT);
    }

    if (!input) {
        res = SET_ERROR(instance, ERR_NCX_DEF_NOT_FOUND);
    } else {
        /* construct a method + parameter tree */
        reqdata = xml_val_new_struct(instance, 
                                     obj_get_name(instance, rpc), 
                                     obj_get_nsid(instance, rpc));
        if (!reqdata) {
            log_error(instance, "\nError allocating a new RPC request");
            res = ERR_INTERNAL_MEM;
        }
    }

    /* add /get-star/input/source */
    if (res == NO_ERR) {
        if (source) {
            val_add_child(instance, source, reqdata);
        }
    }

    /* add /get-star/input/filter */
    if (res == NO_ERR && (get_content || selectval)) {
        if (get_content) {
            /* set the get/input/filter node to the
             * get_content, but after filling in any
             * missing nodes from the root to the target
             */
            filter = xml_val_new_struct(instance, NCX_EL_FILTER, xmlns_nc_id(instance));
        } else {
            filter = xml_val_new_flag(instance, NCX_EL_FILTER, xmlns_nc_id(instance));
        }
        if (!filter) {
            log_error(instance, "\nError: malloc failure");
            res = ERR_INTERNAL_MEM;
        } else {
            val_add_child(instance, filter, reqdata);
        }

        /* add the type and maybe select attributes */
        if (res == NO_ERR) {
            res = add_filter_attrs(instance, filter, selectval);
        }
    }

    /* check any errors so far */
    if (res != NO_ERR) {
        if (freeroot) {
            val_free_value(instance, valroot);
        } else if (get_content) {
            val_free_value(instance, get_content);
        } else if (selectval) {
            val_free_value(instance, selectval);
        }
        val_free_value(instance, reqdata);
        return res;
    }

    /* add the content to the filter element
     * building the path from the content node
     * to the root; fill if dofill is true
     */
    if (valroot) {
        val_add_child(instance, valroot, filter);
        res = complete_path_content(instance, server_cb, rpc, valroot, get_content,
                                    dofill, FALSE);
        if (res != NO_ERR) {
            val_free_value(instance, valroot);
            val_free_value(instance, reqdata);
            return res;
        }
    } else if (get_content) {
        dummy_parm = NULL;
        res = add_filter_from_content_node(instance, server_cb, rpc, get_content,
                                           get_content->obj, filter, 
                                           dofill, &dummy_parm);
        if (res != NO_ERR && res != ERR_NCX_SKIPPED) {
            /*  val_free_value(get_content); already freed! */
            val_free_value(instance, reqdata);
            return res;
        }
        res = NO_ERR;
    }

    /* get the session control block */
    scb = mgr_ses_get_scb(server_cb->mysid);
    if (!scb) {
        res = SET_ERROR(instance, ERR_INTERNAL_PTR);
    }

    /* !!! get_content consumed at this point !!!
     * check if the with-defaults parmaeter should be added
     */
    if (res == NO_ERR) {
        mscb = mgr_ses_get_mscb(instance, scb);
        if (cap_std_set(&mscb->caplist, CAP_STDID_WITH_DEFAULTS)) {
            switch (withdef) {
            case NCX_WITHDEF_NONE:
                break;
            case NCX_WITHDEF_TRIM:
            case NCX_WITHDEF_EXPLICIT:
            case NCX_WITHDEF_REPORT_ALL_TAGGED:
                /*** !!! NEED TO CHECK IF TRIM / EXPLICT / TAGGED
                 *** !!! REALLY SUPPORTED IN THE caplist
                 ***/
                /* fall through */
            case NCX_WITHDEF_REPORT_ALL:
                /* it is OK to send a with-defaults to this server */
                withdefobj = obj_find_child(instance, 
                                            input, 
                                            NULL,
                                            NCX_EL_WITH_DEFAULTS);
                if (withdefobj == NULL) {
                    SET_ERROR(instance, ERR_NCX_DEF_NOT_FOUND);
                } else {
                    withdefval = val_make_simval_obj
                        (instance,
                         withdefobj,
                         ncx_get_withdefaults_string(instance, withdef),
                         &res);
                    if (withdefval != NULL) {
                        val_add_child(instance, withdefval, reqdata);
                    }
                }
                break;
            default:
                SET_ERROR(instance, ERR_INTERNAL_VAL);
            }
        } else if (withdef != NCX_WITHDEF_NONE) {
            log_warn(instance, "\nWarning: 'with-defaults' "
                     "capability not-supported so parameter ignored");
        }
    }

    if (res == NO_ERR) {
        /* allocate an RPC request and send it */
        req = mgr_rpc_new_request(instance, scb);
        if (!req) {
            res = ERR_INTERNAL_MEM;
            log_error(instance, "\nError allocating a new RPC request");
        } else {
            req->data = reqdata;
            req->rpc = rpc;
            req->timeout = timeoutval;
        }
    }
        
    if (res == NO_ERR) {
        if (LOGDEBUG2) {
            log_debug2(instance, "\nabout to send RPC request with reqdata:");
            val_dump_value_max(instance, 
                               reqdata, 
                               0,
                               server_cb->defindent,
                               DUMP_VAL_LOG,
                               server_cb->display_mode,
                               FALSE,
                               FALSE);
        }

        /* the request will be stored if this returns NO_ERR */
        res = mgr_rpc_send_request(instance, scb, req, (mgr_rpc_cbfn_t)yangcli_reply_handler);
    }

    if (res != NO_ERR) {
        if (req) {
            mgr_rpc_free_request(instance, req);
        } else if (reqdata) {
            val_free_value(instance, reqdata);
        }
    } else {
        server_cb->state = MGR_IO_ST_CONN_RPYWAIT;
    }

    return res;

} /* send_get_to_server */


/********************************************************************
 * FUNCTION get_content_from_choice
 * 
 * Get the content input for the EditOps from choice
 *
 * If the choice is 'from-cli' and this is interactive mode
 * then the fill_valset will be called to get input
 * based on the 'target' parameter also in the valset
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the load command
 *    valset == parsed CLI valset
 *    getoptional == TRUE if optional nodes are desired
 *    isdelete == TRUE if this is for an edit-config delete
 *                FALSE for some other operation
 *    dofill == TRUE to fill the content,
 *              FALSE to skip fill phase
 *    iswrite == TRUE if write operation
 *               FALSE if a read operation
 *    retres == address of return status
 *    valroot == address of return value root pointer
 *               used when the content is from the CLI XPath
 *
 * OUTPUTS:
 *    *valroot ==  pointer to malloced value tree content root. 
 *        This is the top of the tree that will be added
 *        as a child of the <config> node for the edit-config op
 *        If this is non-NULL, then the return value should
 *        not be freed. This pointer should be freed instead.
 *        == NULL if the from choice mode does not use
 *           an XPath schema-instance node as input
 *    *retres == return status
 *
 * RETURNS:
 *    result of the choice; this is the content
 *    that will be affected by the edit-config operation
 *    via create, merge, or replace
 *
 *   This is the specified target node in the
 *   content target, which may be the 
 *   same as the content root or will, if *valroot != NULL
 *   be a descendant of the content root 
 *
 *   If *valroot == NULL then this is a malloced pointer that
 *   must be freed by the caller
 *
 *   If the retun value is NULL and the *retres is NO_ERR
 *   then the document root was selected so there is no
 *   specific node to return as a subtree.
 *********************************************************************/
static val_value_t *
    get_content_from_choice (ncx_instance_t *instance,
                             server_cb_t *server_cb,
                             obj_template_t *rpc,
                             val_value_t *valset,
                             boolean getoptional,
                             boolean isdelete,
                             boolean dofill,
                             boolean iswrite,
                             status_t *retres,
                             val_value_t **valroot,
                             char* secondary_args)
{
    val_value_t           *parm, *curparm = NULL, *newparm = NULL;
    const val_value_t     *userval = NULL;
    val_value_t           *targval = NULL;
    obj_template_t        *targobj = NULL;
    const xmlChar         *fromstr = NULL, *target = NULL;
    xmlChar               *fromurl = NULL;
    var_type_t             vartype = VAR_TYP_NONE;
    boolean                isvarref = FALSE, iscli = FALSE;
    boolean                isselect = FALSE, saveopt = FALSE;
    boolean                isextern = FALSE, alt_names = FALSE;
    status_t               res = NO_ERR;
    ncx_name_match_t       match_names = FALSE;

    *valroot = NULL;
    *retres = NO_ERR;

    /* look for the 'match-names' parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_MATCH_NAMES);
    if (parm != NULL) {
        match_names = ncx_get_name_match_enum(instance, VAL_ENUM_NAME(parm));
        if (match_names == NCX_MATCH_NONE) {
            *retres = ERR_NCX_INVALID_VALUE;
            return NULL;
        }
    } else {
        match_names = server_cb->match_names;
    }

    /* look for the 'alt-names' parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_ALT_NAMES);
    if (parm != NULL) {
        alt_names = VAL_BOOL(parm);
    } else {
        alt_names = server_cb->alt_names;
    }

    /* look for the 'from' parameter variant */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_VARREF);
    if (parm != NULL) {
        /* content = uservar or $varref */
        isvarref = TRUE;
        fromstr = VAL_STR(parm);
    } else {
        parm = val_find_child(instance, valset, YANGCLI_MOD, NCX_EL_SELECT);
        if (parm != NULL) {
            /* content == select string for xget or xget-config */
            isselect = TRUE;
            fromstr = VAL_STR(parm);
        }
    }
    if (parm == NULL && 
        !iswrite && 
        !xml_strncmp(instance, obj_get_name(instance, rpc), (const xmlChar *)"xget", 4)) {

        /* try urltarget; convert to XPath first */
        parm = val_find_child(instance, valset, YANGCLI_MOD, NCX_EL_URLTARGET);
        if (parm != NULL) {
            fromurl = xpath_convert_url_to_path(instance,
                                                VAL_STR(parm),
                                                match_names,
                                                alt_names,
                                                !iswrite,  /* wildcards */
                                                TRUE,       /* withkeys */
                                                &res);
            if (fromurl == NULL || res != NO_ERR) {
                log_error(instance,
                          "\nError: urltarget '%s' has "
                          "invalid format (%s)",
                          VAL_STR(parm),
                          get_error_string(res));
                *retres = res;
                if (fromurl != NULL) {
                    m__free(instance, fromurl);
                }
                return NULL;
            } else {
                fromstr = fromurl;
                isselect = TRUE;
            }
        } else {
            /* last choice; content is from the CLI */
            iscli = TRUE;
        }
    } else if (!(isvarref || isselect)) {
        iscli = TRUE;
    }

    if (iscli) {
        saveopt = server_cb->get_optional;
        server_cb->get_optional = getoptional;

        /* from CLI -- look for the 'target' parameter */
        parm = val_find_child(instance, valset, YANGCLI_MOD, NCX_EL_TARGET);
        if (parm != NULL) {
            target = VAL_STR(parm);
        } else {
            parm = val_find_child(instance, valset, YANGCLI_MOD, NCX_EL_URLTARGET);
            if (parm != NULL) {
                res = NO_ERR;
                fromurl = xpath_convert_url_to_path(instance,
                                                    VAL_STR(parm),
                                                    match_names,
                                                    alt_names,
                                                    !iswrite,
                                                    TRUE, /* withkeys */
                                                    &res);
                if (fromurl == NULL || res != NO_ERR) {
                    log_error(instance,
                              "\nError: urltarget '%s' has "
                              "invalid format (%s)\n",
                              VAL_STR(parm),
                              get_error_string(res));
                    server_cb->get_optional = saveopt;
                    *retres = res;
                    if (fromurl != NULL) {
                        m__free(instance, fromurl);
                    }
                    return NULL;
                } else {
                    target = fromurl;
                }
            }
        }
        if (parm == NULL) {
            log_error(instance, "\nError: target or urltarget parameter is missing");
            server_cb->get_optional = saveopt;
            *retres = ERR_NCX_MISSING_PARM;
            return NULL;
        }

        *valroot = get_instanceid_parm(instance, target, TRUE, iswrite, &targobj,
                                       &targval, &res);
        if (res != NO_ERR) {
            log_error(instance,
                      "\nError: invalid target parameter (%s)",
                      get_error_string(res));
            if (*valroot) {
                val_free_value(instance, *valroot);
                *valroot = NULL;
            }
            if (fromurl != NULL) {
                m__free(instance, fromurl);
            }
            server_cb->get_optional = saveopt;
            *retres = res;
            return NULL;
        } else if (*valroot == NULL) {
            /* indicates the docroot was selected or path not found */
            if (fromurl != NULL) {
                m__free(instance, fromurl);
            }
            return NULL;
        }

        if (fromurl != NULL) {
            m__free(instance, fromurl);
            fromurl = NULL;
        }
        
        curparm = NULL;
        parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_VALUE);
        if (parm && parm->res == NO_ERR) {
            const xmlChar *parmval = NULL;
            if (parm->btyp == NCX_BT_EXTERN) {
                isextern = TRUE;
                curparm = mgr_load_extern_file(instance, VAL_EXTERN(parm), NULL,  &res);
                if (res != NO_ERR) {
                    *retres = res;
                    return NULL;
                }
            } else {
                if (typ_is_string(parm->btyp)) {
                    parmval = VAL_STR(parm);
                } else if (typ_is_enum(parm->btyp)) {
                    parmval = VAL_ENUM_NAME(parm);
                }
                curparm = var_get_script_val_ex(instance,
                                                server_cb->runstack_context,
                                                NULL, targobj, NULL, parmval, ISPARM, 
                                                (parmval) ? NULL : parm, &res);
            }
            if (curparm == NULL || res != NO_ERR) {
                log_error(instance, 
                          "\nError: Script value '%s' invalid (%s)", 
                          (parmval) ? parmval : 
                          (const xmlChar *)"<complex-val>",
                          get_error_string(res)); 
                server_cb->get_optional = saveopt;
                val_free_value(instance, *valroot);
                *valroot = NULL;
                if (curparm) {
                    val_free_value(instance, curparm);
                }
                *retres = res;
                return NULL;
            }
            if (curparm->obj != targobj && !isextern) {
                res = ERR_NCX_INVALID_VALUE;
                log_error(instance,
                          "\nError: current value '%s' "
                          "object type is incorrect.",
                          VAL_STR(parm));
                server_cb->get_optional = saveopt;
                val_free_value(instance, *valroot);
                *valroot = NULL;
                *retres = res;
                val_free_value(instance, curparm);
                return NULL;
            }
        }

        switch (targobj->objtype) {
        case OBJ_TYP_ANYXML:
        case OBJ_TYP_LEAF:
            if (isdelete) {
                dofill = FALSE;
            } else if (obj_get_basetype(instance, targobj) == NCX_BT_EMPTY) {
                /* already filled in the target value by creating it */
                dofill = FALSE;
            } else if (!iswrite && !server_cb->get_optional) {
                /* for get operations, need --optional
                 *  set to prompt for the target leaf
                 */
                dofill = FALSE;
            } /* else fall through */
        case OBJ_TYP_LEAF_LIST:
            if (dofill) {
                /* if targval is non-NULL then it is an empty
                 * value struct because it was the deepest node
                 * specified in the schema-instance string
                 */
                newparm = fill_value(instance, server_cb, rpc, targobj, curparm, &res);
                if (newparm && res == NO_ERR) {
                    if (targval) {
                        if (targval == *valroot) {
                            /* get a top-level leaf or leaf-list
                             * so get rid of the instance ID and
                             * just use the fill_value result
                             */
                            val_free_value(instance, *valroot);
                            *valroot = NULL;
                        } else {
                            /* got a leaf or leaf-list that is not
                             * a top-level node so just swap
                             * out the old leaf for the new leaf
                             */
                            val_swap_child(instance, newparm, targval);
                            val_free_value(instance, targval);
                            targval = NULL;
                        }
                    }
                }
            } else if (targval == NULL) {
                newparm = val_new_value(instance);
                if (!newparm) {
                    log_error(instance, "\nError: malloc failure");
                    res = ERR_INTERNAL_MEM;
                } else {
                    val_init_from_template(instance, newparm, targobj);
                }
                if (isdelete) {
                    val_reset_empty(instance, newparm);
                }
            } else if (isdelete) {
                val_reset_empty(instance, targval);
            }  /* else going to use targval, not newval */
            break;
        case OBJ_TYP_CHOICE:
            if (isdelete) {
                res = ERR_NCX_OPERATION_NOT_SUPPORTED;
                log_error(instance, "\nError: delete operation not supported "
                          "for choice");
            } else if (targval == NULL) {
                newparm = val_new_value(instance);
                if (!newparm) {
                    log_error(instance, "\nError: malloc failure");
                    res = ERR_INTERNAL_MEM;
                } else {
                    val_init_from_template(instance, newparm, targobj);
                }
            }
            if (res == NO_ERR && dofill) {
                res = get_choice(instance, server_cb, rpc, targobj,
                                 (newparm) ? newparm : targval,
                                 curparm, iswrite, isdelete);
            }
            break;
        case OBJ_TYP_CASE:
            if (isdelete) {
                res = ERR_NCX_OPERATION_NOT_SUPPORTED;
                log_error(instance, "\nError: delete operation not supported "
                          "for case");
            } else if (targval == NULL) {
                newparm = val_new_value(instance);
                if (!newparm) {
                    log_error(instance, "\nError: malloc failure");
                    res = ERR_INTERNAL_MEM;
                } else {
                    val_init_from_template(instance, newparm, targobj);
                }
            }
            if (res == NO_ERR && dofill) {
                res = get_case(instance, server_cb, targobj,
                               (newparm) ? newparm : targval, 
                               curparm, iswrite, isdelete);
            }
            break;
        default:
            if (targobj->objtype == OBJ_TYP_LIST) {
                ; // if delete, need to fill in just the keys
            } else if (isdelete) {
                dofill = FALSE;
            }
            if (targval == NULL) {
                newparm = val_new_value(instance);
                if (!newparm) {
                    log_error(instance, "\nError: malloc failure");
                    res = ERR_INTERNAL_MEM;
                } else {
                    val_init_from_template(instance, newparm, targobj);
                }
            }
            if (res == NO_ERR && dofill) {
                val_value_t *mytarg = (newparm) ? newparm : targval;

                if (isextern) {
                    if (curparm != NULL && 
                        mytarg != NULL &&
                        !typ_is_simple(instance, curparm->btyp) &&
                        !typ_is_simple(instance, mytarg->btyp)) {
                        val_change_nsid(instance, curparm, val_get_nsid(instance, mytarg));
                        val_move_children(instance, curparm, mytarg);
                    } else if (curparm != NULL && 
                               mytarg != NULL &&
                               typ_is_simple(instance, curparm->btyp) &&
                               !typ_is_simple(instance, mytarg->btyp)) {
                        val_change_nsid(instance, curparm, val_get_nsid(instance, mytarg));
                        val_add_child(instance, curparm, mytarg);
                        curparm = NULL;
                    } else if (curparm != NULL && mytarg != NULL) {
                        val_change_nsid(instance, curparm, val_get_nsid(instance, mytarg));
                        res = val_replace(instance, curparm, mytarg);
                    } /* else should not happen */
                } else if (!get_batchmode()){
                    /* secondary_args = "mtu=1500"; */
                    if(secondary_args) {
                        status_t status;
                        char* argv[2];
                        argv[0] = (char *)xml_strdup(instance, obj_get_name(instance, mytarg->obj));
                        argv[1] = secondary_args;
                        curparm = cli_parse(instance, server_cb->runstack_context, /*argc=*/2, argv, mytarg->obj, /*valonly=*/true, /*script=*/true, /*autocomp=*/true, /*mode=*/CLI_MODE_COMMAND, &status);
                        res = val_replace(instance, curparm, mytarg);
                    }
                    res = fill_valset(instance, server_cb, rpc, mytarg, curparm, 
                                      iswrite, isdelete);
                }
            }
        }

        /* done with the current value */
        if (curparm) {
            val_free_value(instance, curparm);
        }

        server_cb->get_optional = saveopt;
        if (res == ERR_NCX_SKIPPED) {
            res = NO_ERR;
            if (newparm) {
                val_free_value(instance, newparm);
            }
            newparm = 
                xml_val_new_flag(instance,
                                 obj_get_name(instance, targobj),
                                 obj_get_nsid(instance, targobj));
            if (!newparm) {
                res = ERR_INTERNAL_MEM;
                log_error(instance, "\nError: malloc failure");
            } else {
                /* need to set the real object so the
                 * path to root will be built correctly
                 */
                newparm->obj = targobj;
            }

            if (res != NO_ERR) {
                val_free_value(instance, *valroot);
                *valroot = NULL;
            } /* else *valroot is active and in use */

            *retres = res;
            return newparm;
        } else if (res != NO_ERR) {
            if (newparm) {
                val_free_value(instance, newparm);
            }
            val_free_value(instance, *valroot);
            *valroot = NULL;
            *retres = res;
            return NULL;
        } else {
            /* *valroot is active and in use */
            if (newparm != NULL) {
                *retres = res;
                return newparm;
            } else if (targval == *valroot) {
                /* need to make sure that a top-level
                 * leaf is not treated as match-node
                 * instead of a selection node
                 */
                if (obj_is_leafy(instance, targval->obj)) {
                    /* force the data type to be
                     * empty so it will be treated
                     * as a selection node
                     */
                    val_force_empty(instance, targval);
                }
            }
            return (newparm) ? newparm : targval;
        }
    } else if (isselect) {
        newparm = val_new_value(instance);
        if (!newparm) {
            log_error(instance, "\nError: malloc failed");
        } else {
            val_init_from_template(instance, newparm, parm->obj);
            res = val_set_simval_obj(instance, newparm, parm->obj, fromstr);
            if (res != NO_ERR) {
                log_error(instance,
                          "\nError: set 'select' failed (%s)",
                          get_error_string(res));
                val_free_value(instance, newparm);
                newparm = NULL;
            }
        }
        if (fromurl != NULL) {
            m__free(instance, fromurl);
        }

        /*  *valroot == NULL and not in use */
        *retres = res;
        return newparm;
    } else if (isvarref) {
        /* from global or local variable 
         * *valroot == NULL and not used
         */
        if (!fromstr) {
            ;  /* should not be NULL */
        } else if (*fromstr == '$' && fromstr[1] == '$') {
            /* $$foo */
            vartype = VAR_TYP_GLOBAL;
            fromstr += 2;
        } else if (*fromstr == '$') {
            /* $foo */
            vartype = VAR_TYP_LOCAL;
            fromstr++;
        } else {
            /* 'foo' : just assume local, not error */
            vartype = VAR_TYP_LOCAL;
        }
        if (fromstr) {
            userval = var_get(instance, server_cb->runstack_context, fromstr, vartype);
            if (!userval) {
                log_error(instance, 
                          "\nError: variable '%s' not found", 
                          fromstr);
                *retres = ERR_NCX_NOT_FOUND;
                return NULL;
            } else {
                newparm = val_clone(instance, userval);
                if (!newparm) {
                    log_error(instance, "\nError: malloc failed");
                }
                *retres = ERR_INTERNAL_MEM;
                return newparm;
            }
        }
    } else {
        res = SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    *retres = res;
    return NULL;

}  /* get_content_from_choice */


/********************************************************************
 * FUNCTION add_one_operation_attr
 * 
 * Add the nc:operation attribute to a value node
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    val == value node to set
 *    op == edit operation to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    add_one_operation_attr (ncx_instance_t *instance,
                            server_cb_t *server_cb,
                            val_value_t *val,
                            op_editop_t op)
{
    obj_template_t       *operobj;
    const xmlChar        *editopstr;
    val_value_t          *metaval;
    status_t              res;

    /* get the internal nc:operation object */
    operobj = ncx_find_object(instance, 
                              get_netconf_mod(instance, server_cb), 
                              NC_OPERATION_ATTR_NAME);
    if (operobj == NULL) {
        return SET_ERROR(instance, ERR_NCX_DEF_NOT_FOUND);
    }

    /* create a value node for the meta-value */
    metaval = val_new_value(instance);
    if (!metaval) {
        return ERR_INTERNAL_MEM;
    }
    val_init_from_template(instance, metaval, operobj);

    /* get the string value for the edit operation */
    editopstr = op_editop_name(op);
    if (editopstr  == NULL) {
        val_free_value(instance, metaval);
        return SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    /* set the meta variable value and other fields */
    res = val_set_simval(instance,
                         metaval,
                         obj_get_typdef(operobj),
                         obj_get_nsid(instance, operobj),
                         obj_get_name(instance, operobj),
                         editopstr);

    if (res != NO_ERR) {
        val_free_value(instance, metaval);
        return res;
    }

    dlq_enque(instance, metaval, &val->metaQ);
    return NO_ERR;

} /* add_one_operation_attr */


/********************************************************************
 * FUNCTION add_operation_attr
 * 
 * Add the nc:operation attribute to a value node
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    val == value node to set
 *    op == edit operation to use
 *    topcontainer == TRUE if a top-level container
 *                    is passed in 'val'
 *                 == FALSE otherwise
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    add_operation_attr (ncx_instance_t *instance,
                        server_cb_t *server_cb,
                        val_value_t *val,
                        op_editop_t op,
                        boolean topcontainer)
{
    val_value_t          *childval;
    status_t              res;

    res = NO_ERR;

    switch (val->obj->objtype) {
    case OBJ_TYP_CONTAINER:
        if (!topcontainer) {
            res = add_one_operation_attr(instance,
                                         server_cb,
                                         val, 
                                         op);
            break;
        } /* else fall through */
    case OBJ_TYP_CHOICE:
    case OBJ_TYP_CASE:
        for (childval = val_get_first_child(instance, val);
             childval != NULL;
             childval = val_get_next_child(instance, childval)) {

            res = add_one_operation_attr(instance, server_cb, childval, op);
            if (res != NO_ERR) {
                return res;
            }
        }
        break;
    default:
        res = add_one_operation_attr(instance, server_cb, val, op);
    }

    return res;

} /* add_operation_attr */


/********************************************************************
 * FUNCTION add_one_insert_attrs
 * 
 * Add the yang:insert attribute(s) to a value node
 *
 * INPUTS:
 *    val == value node to set
 *    insop == insert operation to use
 *    edit_target == edit target to use for key or value attr
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    add_one_insert_attrs (ncx_instance_t *instance,
                          val_value_t *val,
                          op_insertop_t insop,
                          const xmlChar *edit_target)
{
    obj_template_t       *operobj;
    const xmlChar        *insopstr;
    val_value_t          *metaval;
    status_t              res;
    xmlns_id_t            yangid;

    yangid = xmlns_yang_id(instance);

    /* get the internal nc:operation object */
    operobj = ncx_get_gen_string(instance);
    if (!operobj) {
        return SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    insopstr = op_insertop_name(insop);

    /* create a value node for the meta-value */
    metaval = val_new_value(instance);
    if (!metaval) {
        return ERR_INTERNAL_MEM;
    }
    val_init_from_template(instance, metaval, operobj);
    val_set_qname(instance, metaval, yangid, YANG_K_INSERT, xml_strlen(instance, YANG_K_INSERT));

    /* set the meta variable value and other fields */
    res = val_set_simval(instance, metaval, metaval->typdef, yangid, YANG_K_INSERT,
                         insopstr);
    if (res != NO_ERR) {
        val_free_value(instance, metaval);
        return res;
    } else {
        dlq_enque(instance, metaval, &val->metaQ);
    }

    if (insop == OP_INSOP_BEFORE || insop == OP_INSOP_AFTER) {
        /* create a value node for the meta-value */
        metaval = val_new_value(instance);
        if (!metaval) {
            return ERR_INTERNAL_MEM;
        }
        val_init_from_template(instance, metaval, operobj);

        /* set the attribute name */
        if (val->obj->objtype==OBJ_TYP_LEAF_LIST) {
            val_set_qname(instance, metaval, yangid, YANG_K_VALUE,
                          xml_strlen(instance, YANG_K_VALUE));
        } else {
            val_set_qname(instance, metaval, yangid, YANG_K_KEY,
                          xml_strlen(instance, YANG_K_KEY));
        }

        /* set the meta variable value and other fields */
        res = val_set_simval(instance,
                             metaval,
                             metaval->typdef,
                             yangid, NULL,
                             edit_target);
        if (res != NO_ERR) {
            val_free_value(instance, metaval);
            return res;
        } else {
            dlq_enque(instance, metaval, &val->metaQ);
        }
    }          

    return NO_ERR;

} /* add_one_insert_attrs */


/********************************************************************
 * FUNCTION add_insert_attrs
 * 
 * Add the yang:insert attribute(s) to a value node
 *
 * INPUTS:
 *    val == value node to set
 *    insop == insert operation to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    add_insert_attrs (ncx_instance_t *instance,
                      val_value_t *val,
                      op_insertop_t insop,
                      const xmlChar *edit_target)
{
    val_value_t          *childval;
    status_t              res;

    res = NO_ERR;

    switch (val->obj->objtype) {
    case OBJ_TYP_CHOICE:
    case OBJ_TYP_CASE:
        for (childval = val_get_first_child(instance, val);
             childval != NULL;
             childval = val_get_next_child(instance, childval)) {

            res = add_one_insert_attrs(instance, childval, insop, edit_target);
            if (res != NO_ERR) {
                return res;
            }
        }
        break;
    default:
        res = add_one_insert_attrs(instance, val, insop, edit_target);
    }

    return res;

} /* add_insert_attrs */


/********************************************************************
 * FUNCTION do_edit
 * 
 * Edit some database object on the server
 * operation attribute:
 *   create/delete/merge/replace
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the create command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   the completed data node is output and
 *   an edit-config operation is sent to the server
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_edit (ncx_instance_t *instance,
             server_cb_t *server_cb,
             obj_template_t *rpc,
             xmlChar *line,
             uint32  len,
             op_editop_t editop)
{
    val_value_t           *valset, *content, *parm, *valroot;
    status_t               res;
    uint32                 timeoutval;
    boolean                getoptional, dofill;
    boolean                isdelete, topcontainer, doattr;
    op_defop_t             def_editop;
    char                   *secondary_args;
    int                    secondary_args_flag;

    /* init locals */
    res = NO_ERR;
    content = NULL;
    valroot = NULL;
    dofill = TRUE;
    doattr = TRUE;
    topcontainer = FALSE;
    def_editop = server_cb->defop;

    if (editop == OP_EDITOP_DELETE || editop == OP_EDITOP_REMOVE) {
        isdelete = TRUE;
    } else {
        isdelete = FALSE;
    }

    /* get the command line parameters for this command */
    valset = get_valset(instance, server_cb, rpc, &line[len], &res);
    if (!valset || res != NO_ERR) {
        if (valset) {
            val_free_value(instance, valset);
        }
        return res;
    }

    /* get the timeout parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_TIMEOUT);
    if (parm && parm->res == NO_ERR) {
        timeoutval = VAL_UINT(parm);
    } else {
        timeoutval = server_cb->timeout;
    }

    /* get the 'fill-in optional parms' parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_OPTIONAL);
    if (parm && parm->res == NO_ERR) {
        getoptional = TRUE;
    } else {
        getoptional = server_cb->get_optional;
    }

    /* get the --nofill parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_NOFILL);
    if (parm && parm->res == NO_ERR) {
        dofill = FALSE;
    }

    secondary_args = (char *)line;
    secondary_args_flag = 0;
    while(strlen(secondary_args)>=strlen("--")) {
        if(memcmp(secondary_args, "--", strlen("--"))==0) {
            secondary_args+=strlen("--");
            secondary_args_flag = 1;
            break;
        }
        secondary_args++;
    }
    /* get the contents specified in the 'from' choice */
    content = get_content_from_choice(instance, server_cb, rpc, valset, getoptional,
                                      isdelete, dofill,
                                      TRUE, /* iswrite */
                                      &res, &valroot, secondary_args_flag?secondary_args:NULL);
    if (content == NULL) {
        if (res != NO_ERR) {
            if (LOGDEBUG2) {
                log_debug2(instance,
                           "get_content error %d = (%s)",
                           res,
                           get_error_string(res));
            }
            res = ERR_NCX_MISSING_PARM;
        } else {
            log_error(instance, "\nError: operations on root '/' not supported");
            res = ERR_NCX_OPERATION_NOT_SUPPORTED;
        }
        val_free_value(instance, valset);
        return res;
    }

    if (valroot == NULL && content->btyp == NCX_BT_CONTAINER) {
        switch (editop) {
        case OP_EDITOP_CREATE:
        case OP_EDITOP_DELETE:
        case OP_EDITOP_REMOVE:
            topcontainer = TRUE;
            break;
        case OP_EDITOP_MERGE:
            def_editop = OP_DEFOP_MERGE;
            break;
        case OP_EDITOP_REPLACE:
            def_editop = OP_DEFOP_REPLACE;
            break;
        default:
            SET_ERROR(instance, ERR_INTERNAL_VAL);
        }
    }

    if (doattr) {
        /* add nc:operation attribute to the value node */
        res = add_operation_attr(instance, server_cb, content, editop, topcontainer);
        if (res != NO_ERR) {
            log_error(instance, "\nError: Creation of nc:operation"
                      " attribute failed");
            val_free_value(instance, valset);
            if (valroot) {
                val_free_value(instance, valroot);
            } else {
                val_free_value(instance, content);
            }
            return res;
        }
    }

    /* construct an edit-config PDU with default parameters */
    res = send_edit_config_to_server(instance, server_cb, valroot, content, 
                                     timeoutval, def_editop);
    if (res != NO_ERR) {
        log_error(instance,
                  "\nError: send %s operation failed (%s)",
                  op_editop_name(editop),
                  get_error_string(res));
    }

    val_free_value(instance, valset);

    return res;

}  /* do_edit */


/********************************************************************
 * FUNCTION do_insert
 * 
 * Insert a database object on the server
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the create command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   the completed data node is output and
 *   an edit-config operation is sent to the server
 *
 * RETURNS:
 *  status
 *********************************************************************/
static status_t
    do_insert (ncx_instance_t *instance,
               server_cb_t *server_cb,
               obj_template_t *rpc,
               const xmlChar *line,
               uint32  len)
{
    val_value_t      *valset, *content, *tempval, *parm, *valroot;
    const xmlChar    *edit_target;
    op_editop_t       editop;
    op_insertop_t     insertop;
    status_t          res;
    uint32            timeoutval;
    boolean           getoptional, dofill;

    /* init locals */
    res = NO_ERR;
    content = NULL;
    valroot = NULL;
    dofill = TRUE;

    /* get the command line parameters for this command */
    valset = get_valset(instance, server_cb, rpc, &line[len], &res);
    if (!valset || res != NO_ERR) {
        if (valset) {
            val_free_value(instance, valset);
        }
        return res;
    }

    /* get the 'fill-in optional parms' parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_OPTIONAL);
    if (parm && parm->res == NO_ERR) {
        getoptional = TRUE;
    } else {
        getoptional = server_cb->get_optional;
    }

    /* get the --nofill parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_NOFILL);
    if (parm && parm->res == NO_ERR) {
        dofill = FALSE;
    }

    /* get the contents specified in the 'from' choice */
    content = get_content_from_choice(instance, server_cb, rpc, valset, getoptional,
                                      FALSE,  /* isdelete */
                                      dofill,
                                      TRUE,  /* iswrite */
                                      &res, &valroot, NULL);
    if (!content) {
        val_free_value(instance, valset);
        return (res == NO_ERR) ? ERR_NCX_MISSING_PARM : res;
    }

    /* get the timeout parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_TIMEOUT);
    if (parm && parm->res == NO_ERR) {
        timeoutval = VAL_UINT(parm);
    } else {
        timeoutval = server_cb->timeout;
    }

    /* get the insert order */
    tempval = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_ORDER);
    if (tempval && tempval->res == NO_ERR) {
        insertop = op_insertop_id(instance, VAL_ENUM_NAME(tempval));
    } else {
        insertop = OP_INSOP_LAST;
    }

    /* get the edit-config operation */
    tempval = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_OPERATION);
    if (tempval && tempval->res == NO_ERR) {
        editop = op_editop_id(instance, VAL_ENUM_NAME(tempval));
    } else {
        editop = OP_EDITOP_MERGE;
    }

    /* get the edit-target parameter only if the
     * order is 'before' or 'after'; ignore otherwise
     */
    tempval = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_EDIT_TARGET);
    if (tempval && tempval->res == NO_ERR) {
        edit_target = VAL_STR(tempval);
    } else {
        edit_target = NULL;
    }

    /* check if the edit-target is needed */
    switch (insertop) {
    case OP_INSOP_BEFORE:
    case OP_INSOP_AFTER:
        if (!edit_target) {
            log_error(instance, "\nError: edit-target parameter missing");
            if (valroot) {
                val_free_value(instance, valroot);
            } else {
                val_free_value(instance, content);
            }
            val_free_value(instance, valset);
            return ERR_NCX_MISSING_PARM;
        }
        break;
    default:
        ;
    }

    /* add nc:operation attribute to the value node */
    res = add_operation_attr(instance, server_cb, content, editop, FALSE);
    if (res != NO_ERR) {
        log_error(instance, "\nError: Creation of nc:operation"
                  " attribute failed");
    }

    /* add yang:insert attribute and possibly a key or value
     * attribute as well
     */
    if (res == NO_ERR) {
        res = add_insert_attrs(instance, content, insertop, edit_target);
        if (res != NO_ERR) {
            log_error(instance, "\nError: Creation of yang:insert"
                  " attribute(s) failed");
        }
    }

    /* send the PDU, hand off the content node */
    if (res == NO_ERR) {
        /* construct an edit-config PDU with default parameters */
        res = send_edit_config_to_server(instance, server_cb, valroot, content, 
                                         timeoutval, server_cb->defop);
        if (res != NO_ERR) {
            log_error(instance,
                      "\nError: send create operation failed (%s)",
                      get_error_string(res));
        }
        valroot = NULL;
        content = NULL;
    }

    /* cleanup and exit */
    if (valroot) {
        val_free_value(instance, valroot);
    } else if (content) {
        val_free_value(instance, content);
    }

    val_free_value(instance, valset);

    return res;

}  /* do_insert */


/********************************************************************
 * FUNCTION do_sget
 * 
 * Get some running config and/or state data with the <get> operation,
 * using an optional subtree
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the create command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   the completed data node is output and
 *   a <get> operation is sent to the server
 *
 * RETURNS:
 *  status
 *********************************************************************/
static status_t
    do_sget (ncx_instance_t *instance,
             server_cb_t *server_cb,
             obj_template_t *rpc,
             const xmlChar *line,
             uint32  len)
{
    val_value_t           *valset, *content, *parm, *valroot;
    status_t               res;
    uint32                 timeoutval;
    boolean                dofill, getoptional;
    ncx_withdefaults_t     withdef;

    /* init locals */
    res = NO_ERR;
    content = NULL;
    valroot = NULL;
    dofill = TRUE;

    /* get the command line parameters for this command */
    valset = get_valset(instance, server_cb, rpc, &line[len], &res);
    if (!valset || res != NO_ERR) {
        if (valset) {
            val_free_value(instance, valset);
        }
        return res;
    }

    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_TIMEOUT);
    if (parm && parm->res == NO_ERR) {
        timeoutval = VAL_UINT(parm);
    } else {
        timeoutval = server_cb->timeout;
    }

    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_NOFILL);
    if (parm && parm->res == NO_ERR) {
        dofill = FALSE;
    }

    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_OPTIONAL);
    if (parm && parm->res == NO_ERR) {
        getoptional = TRUE;
    } else {
        getoptional = server_cb->get_optional;
    }

    parm = val_find_child(instance, valset, YANGCLI_MOD, NCX_EL_WITH_DEFAULTS);
    if (parm && parm->res == NO_ERR) {
        withdef = ncx_get_withdefaults_enum(instance, VAL_STR(parm));
    } else {
        withdef = server_cb->withdefaults;
    }
    
    /* get the contents specified in the 'from' choice */
    content = get_content_from_choice(instance, server_cb, rpc, valset, getoptional,
                                      FALSE,  /* isdelete */
                                      dofill,
                                      FALSE,  /* iswrite */
                                      &res, &valroot, NULL);
    if (res != NO_ERR) {
        val_free_value(instance, valset);
        return (res==NO_ERR) ? ERR_NCX_MISSING_PARM : res;
    }

    /* construct a get PDU with the content as the filter */
    res = send_get_to_server(instance, server_cb, valroot, content, NULL, NULL, 
                             timeoutval, dofill, withdef);
    if (res != NO_ERR) {
        log_error(instance,
                  "\nError: send get operation failed (%s)",
                  get_error_string(res));
    }

    val_free_value(instance, valset);

    return res;

}  /* do_sget */


/********************************************************************
 * FUNCTION do_sget_config
 * 
 * Get some config data with the <get-config> operation,
 * using an optional subtree
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the create command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   the completed data node is output and
 *   a <get-config> operation is sent to the server
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_sget_config (ncx_instance_t *instance,
                    server_cb_t *server_cb,
                    obj_template_t *rpc,
                    const xmlChar *line,
                    uint32  len)
{
    val_value_t        *valset, *content, *source, *parm, *valroot;
    status_t            res;
    uint32              timeoutval;
    boolean             dofill, getoptional;
    ncx_withdefaults_t  withdef;

    dofill = TRUE;
    valroot = NULL;
    content = NULL;
    res = NO_ERR;

    /* get the command line parameters for this command */
    valset = get_valset(instance, server_cb, rpc, &line[len], &res);
    if (!valset || res != NO_ERR) {
        if (valset) {
            val_free_value(instance, valset);
        }
        return res;
    }

    /* get the timeout parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_TIMEOUT);
    if (parm && parm->res == NO_ERR) {
        timeoutval = VAL_UINT(parm);
    } else {
        timeoutval = server_cb->timeout;
    }

    /* get the'fill-in optional' parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_OPTIONAL);
    if (parm && parm->res == NO_ERR) {
        getoptional = TRUE;
    } else {
        getoptional = server_cb->get_optional;
    }

    /* get the --nofill parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_NOFILL);
    if (parm && parm->res == NO_ERR) {
        dofill = FALSE;
    }

    /* get the config source parameter */
    source = val_find_child(instance, valset, NULL, NCX_EL_SOURCE);
    if (!source) {
        log_error(instance, "\nError: mandatory source parameter missing");
        val_free_value(instance, valset);
        return ERR_NCX_MISSING_PARM;
    }

    /* get the with-defaults parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, NCX_EL_WITH_DEFAULTS);
    if (parm && parm->res == NO_ERR) {
        withdef = ncx_get_withdefaults_enum(instance, VAL_STR(parm));
    } else {
        withdef = server_cb->withdefaults;
    }

    /* get the contents specified in the 'from' choice */
    content = get_content_from_choice(instance, server_cb, rpc, valset, getoptional,
                                      FALSE,  /* isdelete */
                                      dofill,
                                      FALSE, /* iswrite */
                                      &res, &valroot, NULL);
    if (res != NO_ERR) {
        val_free_value(instance, valset);
        return res;
    }

    /* hand off this malloced node to send_get_to_server */
    val_remove_child(instance, source);
    val_change_nsid(instance, source, xmlns_nc_id(instance));

    /* construct a get PDU with the content as the filter */
    res = send_get_to_server(instance, server_cb, valroot, content, NULL, source, 
                             timeoutval, 
                             FALSE,  /* dofill; don't fill again */
                             withdef);
    if (res != NO_ERR) {
        log_error(instance,
                  "\nError: send get-config operation failed (%s)",
                  get_error_string(res));
    }

    val_free_value(instance, valset);

    return res;

}  /* do_sget_config */


/********************************************************************
 * FUNCTION do_xget
 * 
 * Get some running config and/or state data with the <get> operation,
 * using an optional XPath filter
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the create command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   the completed data node is output and
 *   a <get> operation is sent to the server
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_xget (ncx_instance_t *instance,
             server_cb_t *server_cb,
             obj_template_t *rpc,
             const xmlChar *line,
             uint32  len)
{
    const ses_cb_t      *scb;
    const mgr_scb_t     *mscb;
    val_value_t         *valset, *content, *parm, *valroot;
    const xmlChar       *str;
    status_t             res;
    uint32               retcode, timeoutval;
    ncx_withdefaults_t   withdef;

    /* init locals */
    res = NO_ERR;
    content = NULL;
    valroot = NULL;

    /* get the session info */
    scb = mgr_ses_get_scb(server_cb->mysid);
    if (!scb) {
        return SET_ERROR(instance, ERR_INTERNAL_VAL);
    }
    mscb = (const mgr_scb_t *)scb->mgrcb;

    /* get the command line parameters for this command */
    valset = get_valset(instance, server_cb, rpc, &line[len], &res);
    if (!valset || res != NO_ERR) {
        if (valset) {
            val_free_value(instance, valset);
        }
        return res;
    }

    /* get the timeout parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_TIMEOUT);
    if (parm && parm->res == NO_ERR) {
        timeoutval = VAL_UINT(parm);
    } else {
        timeoutval = server_cb->timeout;
    }

    /* get the with-defaults parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, NCX_EL_WITH_DEFAULTS);
    if (parm && parm->res == NO_ERR) {
        withdef = ncx_get_withdefaults_enum(instance, VAL_ENUM_NAME(parm));
    } else {
        withdef = server_cb->withdefaults;
    }

    /* check if the server supports :xpath */
    if (!cap_std_set(&mscb->caplist, CAP_STDID_XPATH)) {
        switch (server_cb->baddata) {
        case NCX_BAD_DATA_IGNORE:
            break;
        case NCX_BAD_DATA_WARN:
            log_warn(instance, "\nWarning: server does not have :xpath support");
            break;
        case NCX_BAD_DATA_CHECK:
            retcode = 0;
            log_warn(instance, "\nWarning: server does not have :xpath support");
            res = get_yesno(instance,
                            server_cb,
                            (const xmlChar *)"Send request anyway?",
                            YESNO_NO, &retcode);
            if (res == NO_ERR) {
                switch (retcode) {
                case YESNO_CANCEL:
                case YESNO_NO:
                    res = ERR_NCX_CANCELED;
                    break;
                case YESNO_YES:
                    break;
                default:
                    res = SET_ERROR(instance, ERR_INTERNAL_VAL);
                }
            }
            break;
        case NCX_BAD_DATA_ERROR:
            log_error(instance, "\nError: server does not have :xpath support");
            res = ERR_NCX_OPERATION_NOT_SUPPORTED;
            break;
        case NCX_BAD_DATA_NONE:
        default:
            res = SET_ERROR(instance, ERR_INTERNAL_VAL);
        }
    }

    /* check any error so far */
    if (res != NO_ERR) {
        if (valset) {
            val_free_value(instance, valset);
        }
        return res;
    }

    /* get the contents specified in the 'from' choice */
    content = get_content_from_choice(instance, server_cb, rpc, valset,
                                      FALSE,  /* getoptional */
                                      FALSE,  /* isdelete */
                                      FALSE,  /* dofill */
                                      FALSE,  /* iswrite */
                                      &res, &valroot, NULL);
    if (content) {
        if (content->btyp == NCX_BT_STRING && VAL_STR(content)) {
            str = VAL_STR(content);
            while (*str && *str != '"') {
                str++;
            }
            if (*str) {
                log_error(instance, "\nError: select string cannot "
                          "contain a double quote");
            } else {
                /* construct a get PDU with the content
                 * as the filter 
                 * !! passing off content memory even on error !!
                 */
                res = send_get_to_server(instance, server_cb, valroot, NULL, content, 
                                        NULL, timeoutval, FALSE, withdef);
                content = NULL;
                if (res != NO_ERR) {
                    log_error(instance,
                              "\nError: send get operation"
                              " failed (%s)",
                              get_error_string(res));
                }
            }
        } else {
            log_error(instance, "\nError: xget content wrong type");
        }
        if (valroot) {
            /* should not happen */
            val_free_value(instance, valroot);
        }
        if (content) {
            val_free_value(instance, content);
        }
    }

    val_free_value(instance, valset);

    return res;

}  /* do_xget */


/********************************************************************
 * FUNCTION do_xget_config
 * 
 * Get some config data with the <get-config> operation,
 * using an optional XPath filter
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the create command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   the completed data node is output and
 *   a <get-config> operation is sent to the server
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_xget_config (ncx_instance_t *instance,
                    server_cb_t *server_cb,
                    obj_template_t *rpc,
                    const xmlChar *line,
                    uint32  len)
{
    const ses_cb_t      *scb;
    const mgr_scb_t     *mscb;
    val_value_t         *valset, *content, *source, *parm, *valroot;
    const xmlChar       *str;
    status_t             res;
    uint32               retcode, timeoutval;
    ncx_withdefaults_t   withdef;

    /* init locals */
    res = NO_ERR;
    content = NULL;
    valroot = NULL;

    /* get the session info */
    scb = mgr_ses_get_scb(server_cb->mysid);
    if (!scb) {
        return SET_ERROR(instance, ERR_INTERNAL_VAL);
    }
    mscb = (const mgr_scb_t *)scb->mgrcb;
      
    /* get the command line parameters for this command */
    valset = get_valset(instance, server_cb, rpc, &line[len], &res);
    if (!valset || res != NO_ERR) {
        if (valset) {
            val_free_value(instance, valset);
        }
        return res;
    }

    /* get the source parameter */
    source = val_find_child(instance, valset, NULL, NCX_EL_SOURCE);
    if (!source) {
        log_error(instance, "\nError: mandatory source parameter missing");
        val_free_value(instance, valset);
        return ERR_NCX_MISSING_PARM;
    }

    /* get the with-defaults parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, NCX_EL_WITH_DEFAULTS);
    if (parm && parm->res == NO_ERR) {
        withdef = ncx_get_withdefaults_enum(instance, VAL_ENUM_NAME(parm));
    } else {
        withdef = server_cb->withdefaults;
    }

    /* get the timeout parameter */
    parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_TIMEOUT);
    if (parm && parm->res == NO_ERR) {
        timeoutval = VAL_UINT(parm);
    } else {
        timeoutval = server_cb->timeout;
    }

    /* check if the server supports :xpath */
    if (!cap_std_set(&mscb->caplist, CAP_STDID_XPATH)) {
        switch (server_cb->baddata) {
        case NCX_BAD_DATA_IGNORE:
            break;
        case NCX_BAD_DATA_WARN:
            log_warn(instance, "\nWarning: server does not have :xpath support");
            break;
        case NCX_BAD_DATA_CHECK:
            retcode = 0;
            log_warn(instance, "\nWarning: server does not have :xpath support");
            res = get_yesno(instance,
                            server_cb,
                            (const xmlChar *)"Send request anyway?",
                            YESNO_NO, &retcode);
            if (res == NO_ERR) {
                switch (retcode) {
                case YESNO_CANCEL:
                case YESNO_NO:
                    res = ERR_NCX_CANCELED;
                    break;
                case YESNO_YES:
                    break;
                default:
                    res = SET_ERROR(instance, ERR_INTERNAL_VAL);
                }
            }
            break;
        case NCX_BAD_DATA_ERROR:
            log_error(instance, "\nError: server does not have :xpath support");
            res = ERR_NCX_OPERATION_NOT_SUPPORTED;
            break;
        case NCX_BAD_DATA_NONE:
        default:
            res = SET_ERROR(instance, ERR_INTERNAL_VAL);
        }
    }

    /* check any error so far */
    if (res != NO_ERR) {
        if (valset) {
            val_free_value(instance, valset);
        }
        return res;
    }

    /* get the contents specified in the 'from' choice */
    content = get_content_from_choice(instance, server_cb, rpc, valset,
                                      FALSE,  /* getoptional */
                                      FALSE,  /* isdelete */
                                      FALSE,  /* dofill */
                                      FALSE,  /* iswrite */
                                      &res, &valroot, NULL);
    if (content) {
        if (content->btyp == NCX_BT_STRING && VAL_STR(content)) {
            str = VAL_STR(content);
            while (*str && *str != '"') {
                str++;
            }
            if (*str) {
                log_error(instance, "\nError: select string cannot "
                          "contain a double quote");
            } else {
                /* hand off this malloced node to send_get_to_server */
                val_remove_child(instance, source);
                val_change_nsid(instance, source, xmlns_nc_id(instance));

                /* construct a get PDU with the content as the filter
                 * !! hand off content memory here, even on error !! 
                 */
                res = send_get_to_server(instance, 
                                        server_cb, 
                                        valroot,
                                        NULL, 
                                        content, 
                                        source,
                                        timeoutval,
                                        FALSE,
                                        withdef);
                content = NULL;
                if (res != NO_ERR) {
                    log_error(instance,
                              "\nError: send get-config "
                              "operation failed (%s)",
                              get_error_string(res));
                }
            }
        } else {
            log_error(instance, "\nError: xget content wrong type");
        }
        if (valroot) {
            /* should not happen */
            val_free_value(instance, valroot);
        }
        if (content) {
            val_free_value(instance, content);
        }
    }

    val_free_value(instance, valset);

    return res;

}  /* do_xget_config */


/********************************************************************
 * FUNCTION do_history_show (sub-mode of history RPC)
 * 
 * history show [-1]
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    maxlines == max number of history entries to show
 *    mode == requested help mode
 * 
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_history_show (ncx_instance_t *instance,
                     server_cb_t *server_cb,
                     int maxlines,
                     help_mode_t mode)
{
    FILE          *outputfile;
    const char    *format;
    int            glstatus;
    status_t       res = NO_ERR;

    outputfile = log_get_logfile(instance);
    if (!outputfile) {
        /* use STDOUT instead */
        outputfile = stdout;
    }
    
    if (mode == HELP_MODE_FULL) {
        format = "\n  [%N]\t%D %T %H";
    } else {
        format = "\n  [%N]\t%H";
    }

    glstatus = gl_show_history(server_cb->cli_gl, outputfile, format, 1,
                               maxlines);
    if (glstatus) {
        log_error(instance, "\nError: show history failed");
        res = ERR_NCX_OPERATION_FAILED;
    }
    fputc('\n', outputfile);

    return res;

} /* do_history_show */


/********************************************************************
 * FUNCTION do_history_clear (sub-mode of history RPC)
 * 
 * history clear 
 *
 * INPUTS:
 *    server_cb == server control block to use
 * 
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_history_clear (server_cb_t *server_cb)
{

    gl_clear_history(server_cb->cli_gl, 1);
    return NO_ERR;

} /* do_history_clear */


/********************************************************************
 * FUNCTION do_history_load (sub-mode of history RPC)
 * 
 * history load
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    fname == filename parameter value
 *    if missing then try the previous history file name
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_history_load (ncx_instance_t *instance,
                     server_cb_t *server_cb,
                     const xmlChar *fname)
{
    status_t   res;
    int        glstatus;

    res = NO_ERR;

    if (fname && *fname) {
        if (server_cb->history_filename) {
            m__free(instance, server_cb->history_filename);
        }
        server_cb->history_filename = xml_strdup(instance, fname);
        if (!server_cb->history_filename) {
            res = ERR_INTERNAL_MEM;
        }
    } else if (server_cb->history_filename) {
        fname = server_cb->history_filename;
    } else {
        res = ERR_NCX_INVALID_VALUE;
    }

    if (res == NO_ERR) {
        glstatus = gl_load_history(server_cb->cli_gl,
                                   (const char *)fname,
                                   "#");   /* comment prefix */
        if (glstatus) {
            res = ERR_NCX_OPERATION_FAILED;
        }
    }

    return res;

} /* do_history_load */


/********************************************************************
 * FUNCTION do_history_save (sub-mode of history RPC)
 * 
 * history save
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    fname == filename parameter value
 *    if missing then try the previous history file name
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_history_save (ncx_instance_t *instance,
                     server_cb_t *server_cb,
                     const xmlChar *fname)
{
    status_t   res;
    int        glstatus;

    res = NO_ERR;

    if (fname && *fname) {
        if (server_cb->history_filename) {
            m__free(instance, server_cb->history_filename);
        }
        server_cb->history_filename = xml_strdup(instance, fname);
        if (!server_cb->history_filename) {
            res = ERR_INTERNAL_MEM;
        }
    } else if (server_cb->history_filename) {
        fname = server_cb->history_filename;
    } else {
        res = ERR_NCX_INVALID_VALUE;
    }

    if (res == NO_ERR) {
        glstatus = gl_save_history(server_cb->cli_gl,
                                   (const char *)fname,
                                   "#",   /* comment prefix */
                                   -1);    /* save all entries */
        if (glstatus) {
            res = ERR_NCX_OPERATION_FAILED;
        }
    }

    return res;

} /* do_history_save */


/********************************************************************
 * FUNCTION do_history (local RPC)
 * 
 * Do Command line history support operations
 *
 * history 
 *      show
 *      clear
 *      load
 *      save
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the history command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_history (ncx_instance_t *instance,
                server_cb_t *server_cb,
                obj_template_t *rpc,
                const xmlChar *line,
                uint32  len)
{
    val_value_t        *valset, *parm;
    status_t            res;
    boolean             done;
    help_mode_t         mode;

    done = FALSE;
    res = NO_ERR;

    valset = get_valset(instance, server_cb, rpc, &line[len], &res);
    if (valset && res == NO_ERR) {
        mode = HELP_MODE_NORMAL;

        /* check if the 'brief' flag is set first */
        parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_BRIEF);
        if (parm && parm->res == NO_ERR) {
            mode = HELP_MODE_BRIEF;
        } else {
            /* check if the 'full' flag is set first */
            parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_FULL);
            if (parm && parm->res == NO_ERR) {
                mode = HELP_MODE_FULL;
            }
        }

        /* find the 1 of N choice */
        parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_SHOW);
        if (parm) {
            /* do show history */
            res = do_history_show(instance, server_cb, VAL_INT(parm), mode);
            done = TRUE;
        }

        if (!done) {
            parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_CLEAR);
            if (parm) {
                /* do clear history */
                res = do_history_clear(server_cb);
                done = TRUE;
                if (res == NO_ERR) {
                    log_info(instance, "\nOK\n");
                } else {
                    log_error(instance, "\nError: clear history failed\n");
                }
            }
        }

        if (!done) {
            parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_LOAD);
            if (parm) {
                if (!VAL_STR(parm) || !*VAL_STR(parm)) {
                    /* do history load buffer: default filename */
                    res = do_history_load(instance, server_cb, YANGCLI_DEF_HISTORY_FILE);
                } else {
                    /* do history load buffer */
                    res = do_history_load(instance, server_cb, VAL_STR(parm));
                }
                done = TRUE;
                if (res == NO_ERR) {
                    log_info(instance, "\nOK\n");
                } else {
                    log_error(instance, "\nError: load history failed\n");
                }
            }
        }

        if (!done) {
            parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_SAVE);
            if (parm) {
                if (!VAL_STR(parm) || !*VAL_STR(parm)) {
                    /* do history save buffer: default filename */
                    res = do_history_save(instance, server_cb, YANGCLI_DEF_HISTORY_FILE);
                } else {
                    /* do history save buffer */
                    res = do_history_save(instance, server_cb, VAL_STR(parm));
                }
                done = TRUE;
                if (res == NO_ERR) {
                    log_info(instance, "\nOK\n");
                } else {
                    log_error(instance, "\nError: save history failed\n");
                }
            }
        }

        if (!done) {
            res = do_history_show(instance, server_cb, YANGCLI_DEF_HISTORY_LINES, mode);
        }
    }

    if (valset) {
        val_free_value(instance, valset);
    }

    return res;

}  /* do_history */


/********************************************************************
 * FUNCTION do_recall (local RPC)
 * 
 * Do Command line history support operations
 *
 * recall index=n
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the history command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_recall (ncx_instance_t *instance,
               server_cb_t *server_cb,
               obj_template_t *rpc,
               const xmlChar *line,
               uint32  len)
{
    val_value_t        *valset, *parm;
    status_t            res;

    res = NO_ERR;
    valset = get_valset(instance, server_cb, rpc, &line[len], &res);
    if (valset && res == NO_ERR) {
        /* find the mandatory index */
        parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_INDEX);
        if (parm) {
            /* do show history */
            res = do_line_recall(instance, server_cb, VAL_UINT(parm));
        } else {
            res = ERR_NCX_MISSING_PARM;
            log_error(instance, "\nError: missing index parameter");
        }
    }

    if (valset) {
        val_free_value(instance, valset);
    }

    return res;

}  /* do_recall */


/********************************************************************
 * FUNCTION do_eventlog_show (sub-mode of eventlog RPC)
 * 
 * eventlog show=-1 start=0 full
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    maxevents == max number of eventlog entries to show
 *    startindex == start event index number to show
 *    mode == requested help mode
 * 
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_eventlog_show (ncx_instance_t *instance,
                      server_cb_t *server_cb,
                      int32 maxevents,
                      uint32 startindex,
                      help_mode_t mode)
{
    mgr_not_msg_t  *notif;
    val_value_t    *sequence_id;
    logfn_t         logfn;
    boolean         done, imode;
    uint32          eventindex, maxindex, eventsdone;

    if (maxevents > 0) {
        maxindex = (uint32)maxevents;
    } else if (maxevents == -1) {
        maxindex = 0;
    } else {
        return ERR_NCX_INVALID_VALUE;
    }

    imode = interactive_mode();
    if (imode) {
        logfn = log_stdout;
    } else {
        logfn = log_write;
    }

    done = FALSE;
    eventindex = 0;
    eventsdone = 0;

    for (notif = (mgr_not_msg_t *)
             dlq_firstEntry(instance, &server_cb->notificationQ);
         notif != NULL && !done;
         notif = (mgr_not_msg_t *)dlq_nextEntry(instance, notif)) {

        if (eventindex >= startindex) {

            sequence_id = val_find_child(instance, notif->notification, NULL,
                                         NCX_EL_SEQUENCE_ID);


            (*logfn)(instance, "\n [%u]\t", eventindex);
            if (mode != HELP_MODE_BRIEF && notif->eventTime) {
                (*logfn)(instance, " [%s] ", VAL_STR(notif->eventTime));
            }

            if (mode != HELP_MODE_BRIEF) {
                if (sequence_id) {
                    (*logfn)(instance, "(%u)\t", VAL_UINT(sequence_id));
                } else {
                    (*logfn)(instance, "(--)\t");
                }
            }

            /* print the eventType in the desired format */
            if (notif->eventType) {
                switch (server_cb->display_mode) {
                case NCX_DISPLAY_MODE_PLAIN:
                    (*logfn)(instance, "<%s>", notif->eventType->name);
                    break;
                case NCX_DISPLAY_MODE_PREFIX:
                case NCX_DISPLAY_MODE_XML:
                    (*logfn)(instance, "<%s:%s>", 
                             val_get_mod_prefix(instance, notif->eventType),
                             notif->eventType->name);
                    break;
                case NCX_DISPLAY_MODE_MODULE:
                    (*logfn)(instance, "<%s:%s>", 
                             val_get_mod_name(instance, notif->eventType),
                             notif->eventType->name);
                    break;
                default:
                    SET_ERROR(instance, ERR_INTERNAL_VAL);
                }
            } else {
                (*logfn)(instance, "<>");
            }

            if (mode == HELP_MODE_FULL) {
                val_dump_value_max(instance, notif->notification, 0,
                                   server_cb->defindent,
                                   (imode) ? DUMP_VAL_STDOUT : DUMP_VAL_LOG,
                                   server_cb->display_mode, FALSE, FALSE);
                (*logfn)(instance, "\n");
            }
            eventsdone++;
        }

        if (maxindex && (eventsdone == maxindex)) {
            done = TRUE;
        }

        eventindex++;
    }

    if (eventsdone) {
        (*logfn)(instance, "\n");
    }

    return NO_ERR;

} /* do_eventlog_show */


/********************************************************************
 * FUNCTION do_eventlog_clear (sub-mode of eventlog RPC)
 * 
 * eventlog clear
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    maxevents == max number of events to clear
 *                 zero means clear all
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_eventlog_clear (ncx_instance_t *instance,
                       server_cb_t *server_cb,
                       int32 maxevents)
{
    mgr_not_msg_t  *notif;
    int32           eventcount;

    if (maxevents > 0) {
        eventcount = 0;
        while (!dlq_empty(instance, &server_cb->notificationQ) &&
               eventcount < maxevents) {
            notif = (mgr_not_msg_t *)
                dlq_deque(instance, &server_cb->notificationQ);
            mgr_not_free_msg(instance, notif);
            eventcount++;
        }
    } else if (maxevents == -1) {
        while (!dlq_empty(instance, &server_cb->notificationQ)) {
            notif = (mgr_not_msg_t *)
                dlq_deque(instance, &server_cb->notificationQ);
            mgr_not_free_msg(instance, notif);
        }
    } else {
        return ERR_NCX_INVALID_VALUE;
    }

    return NO_ERR;

} /* do_eventlog_clear */


/********************************************************************
 * FUNCTION do_eventlog (local RPC)
 * 
 * Do Notification Event Log support operations
 *
 * eventlog 
 *      show
 *      clear
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the history command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_eventlog (ncx_instance_t *instance,
                 server_cb_t *server_cb,
                 obj_template_t *rpc,
                 const xmlChar *line,
                 uint32  len)
{
    val_value_t        *valset, *parm, *start;
    status_t            res;
    boolean             done;
    help_mode_t         mode;

    done = FALSE;
    res = NO_ERR;

    valset = get_valset(instance, server_cb, rpc, &line[len], &res);
    if (valset && res == NO_ERR) {
        mode = HELP_MODE_NORMAL;

        /* check if the 'brief' flag is set first */
        parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_BRIEF);
        if (parm && parm->res == NO_ERR) {
            mode = HELP_MODE_BRIEF;
        } else {
            /* check if the 'full' flag is set first */
            parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_FULL);
            if (parm && parm->res == NO_ERR) {
                mode = HELP_MODE_FULL;
            }
        }

        /* optional start position for show */
        start = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_START);

        /* find the 1 of N choice */
        parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_SHOW);
        if (parm) {
            /* do show eventlog  */
            res = do_eventlog_show(instance, server_cb, VAL_INT(parm),
                                   (start) ? VAL_UINT(start) : 0, mode);
            done = TRUE;
        }

        if (!done) {
            parm = val_find_child(instance, valset, YANGCLI_MOD, YANGCLI_CLEAR);
            if (parm) {
                /* do clear event log */
                res = do_eventlog_clear(instance, server_cb, VAL_INT(parm));
                done = TRUE;
                if (res == NO_ERR) {
                    log_info(instance, "\nOK\n");
                } else {
                    log_error(instance, "\nError: clear event log failed\n");
                }
            }
        }

        if (!done) {
            res = do_eventlog_show(instance, server_cb, -1, 
                                   (start) ? VAL_UINT(start) : 0, mode);
        }
    }

    if (valset) {
        val_free_value(instance, valset);
    }

    return res;

}  /* do_eventlog */


/********************************************************************
* FUNCTION do_local_conn_command
* 
* Handle local connection mode RPC operations from yangcli.yang
*
* INPUTS:
*   server_cb == server control block to use
*   rpc == template for the local RPC
*   line == input command line from user
*   len == line length
*
* OUTPUTS:
*    server_cb->state may be changed or other action taken
*    the line buffer is NOT consumed or freed by this function
*
* RETURNS:
*    NO_ERR if a RPC was executed OK
*    ERR_NCX_SKIPPED if no command was invoked
*    some other error if command execution failed
*********************************************************************/
static status_t
    do_local_conn_command (ncx_instance_t *instance,
                           server_cb_t *server_cb,
                           obj_template_t *rpc,
                           xmlChar *line,
                           uint32  len)
{
    const xmlChar *rpcname;
    status_t       res;
    boolean        cond;

    res = NO_ERR;
    rpcname = obj_get_name(instance, rpc);
    cond = runstack_get_cond_state(instance, server_cb->runstack_context);

    if (!xml_strcmp(instance, rpcname, YANGCLI_CREATE)) {
        if (cond) {
            res = do_edit(instance, server_cb, rpc, line, len, OP_EDITOP_CREATE);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_DELETE)) {
        if (cond) {
            res = do_edit(instance, server_cb, rpc, line, len, OP_EDITOP_DELETE);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_REMOVE)) {
        if (cond) {
            res = do_edit(instance, server_cb, rpc, line, len, OP_EDITOP_REMOVE);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_GET_LOCKS)) {
        if (cond) {
            res = do_get_locks(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_INSERT)) {
        if (cond) {
            res = do_insert(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_MERGE)) {
        if (cond) {
            res = do_edit(instance, server_cb, rpc, line, len, OP_EDITOP_MERGE);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_RELEASE_LOCKS)) {
        if (cond) {
            res = do_release_locks(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_REPLACE)) {
        if (cond) {
            res = do_edit(instance, server_cb, rpc, line, len, OP_EDITOP_REPLACE);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_SAVE)) {
        if (cond) {
            if (len < xml_strlen(instance, line)) {
                res = ERR_NCX_INVALID_VALUE;
                log_error(instance,
                          "\nError: Extra characters found (%s)",
                          &line[len]);
            } else {
                res = do_save(instance, server_cb);
            }
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_SGET)) {
        if (cond) {
            res = do_sget(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_SGET_CONFIG)) {
        if (cond) {
            res = do_sget_config(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_XGET)) {
        if (cond) {
            res = do_xget(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_XGET_CONFIG)) {
        if (cond) {
            res = do_xget_config(instance, server_cb, rpc, line, len);
        }
    } else {
        res = ERR_NCX_SKIPPED;
    }

    if (res == NO_ERR && !cond && LOGDEBUG) {
        log_debug(instance,
                  "\nrSkipping false conditional command '%s'",
                  rpcname);
    }

    return res;

} /* do_local_conn_command */


/********************************************************************
* FUNCTION do_local_command
* 
* Handle local RPC operations from yangcli.yang
*
* INPUTS:
*   server_cb == server control block to use
*   rpc == template for the local RPC
*   line == input command line from user
*   len == length of line in bytes
*
* OUTPUTS:
*    state may be changed or other action taken
*    the line buffer is NOT consumed or freed by this function
*
* RETURNS:
*  status
*********************************************************************/
static status_t
    do_local_command (ncx_instance_t *instance,
                      server_cb_t *server_cb,
                      obj_template_t *rpc,
                      xmlChar *line,
                      uint32  len)
{
    const xmlChar *rpcname;
    status_t       res;
    boolean        cond, didcmd;

    res = NO_ERR;
    didcmd = FALSE;
    rpcname = obj_get_name(instance, rpc);
    cond = runstack_get_cond_state(instance, server_cb->runstack_context);

    if (!xml_strcmp(instance, rpcname, YANGCLI_ALIAS)) {
        if (cond) {
            res = do_alias(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_ALIASES)) {
        if (cond) {
            res = do_aliases(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_CD)) {
        if (cond) {
            res = do_cd(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_CONNECT)) {
        if (cond) {
            res = do_connect(instance, server_cb, rpc, line, len, FALSE);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_ELIF)) {
        res = do_elif(instance, server_cb, rpc, line, len);
        didcmd = TRUE;
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_ELSE)) {
        res = do_else(instance, server_cb, rpc, line, len);
        didcmd = TRUE;
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_END)) {
        res = do_end(instance, server_cb, rpc, line, len);
        didcmd = TRUE;
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_EVAL)) {
        if (cond) {
            res = do_eval(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_EVENTLOG)) {
        if (cond) {
            res = do_eventlog(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_FILL)) {
        if (cond) {
            res = do_fill(instance, server_cb, rpc, line, len, TRUE);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_HELP)) {
        if (cond) {
            res = do_help(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_HISTORY)) {
        if (cond) {
            res = do_history(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_IF)) {
        res = do_if(instance, server_cb, rpc, line, len);
        didcmd = TRUE;
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_LIST)) {
        if (cond) {
            res = do_list(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_LOG_ERROR)) {
        if (cond) {
            res = do_log(instance, server_cb, rpc, line, len, LOG_DEBUG_ERROR);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_LOG_WARN)) {
        if (cond) {
            res = do_log(instance, server_cb, rpc, line, len, LOG_DEBUG_WARN);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_LOG_INFO)) {
        if (cond) {
            res = do_log(instance, server_cb, rpc, line, len, LOG_DEBUG_INFO);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_LOG_DEBUG)) {
        if (cond) {
            res = do_log(instance, server_cb, rpc, line, len, LOG_DEBUG_DEBUG);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_MGRLOAD)) {
        if (cond) {
            res = do_mgrload(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_PWD)) {
        if (cond) {
            res = do_pwd(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_QUIT)) {
        if (cond) {
            server_cb->state = MGR_IO_ST_SHUT;
            mgr_request_shutdown(instance);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_RECALL)) {
        if (cond) {
            res = do_recall(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_RUN)) {
        if (cond) {
            res = do_run(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_SHOW)) {
        if (cond) {
            res = do_show(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_START_TIMER)) {
        if (cond) {
            res = yangcli_timer_start(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_STOP_TIMER)) {
        if (cond) {
            res = yangcli_timer_stop(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_WHILE)) {
        res = do_while(instance, server_cb, rpc, line, len);
        didcmd = TRUE;
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_UNSET)) {
        if (cond) {
            res = do_unset(instance, server_cb, rpc, line, len);
        }
    } else if (!xml_strcmp(instance, rpcname, YANGCLI_USERVARS)) {
        if (cond) {
            res = do_uservars(instance, server_cb, rpc, line, len);
        }
    } else {
        res = ERR_NCX_INVALID_VALUE;
        log_error(instance,
                   "\nError: The %s command is not allowed in this mode",
                   rpcname);
    }

    if (res == NO_ERR && !cond && !didcmd && LOGDEBUG) {
        log_debug(instance,
                  "\nrSkipping false conditional command '%s'",
                  rpcname);
    }
    if (res != NO_ERR) {
        log_error(instance, "\n");
    }

    return res;

} /* do_local_command */


/**************    E X T E R N A L   F U N C T I O N S **********/


/********************************************************************
* FUNCTION top_command
* 
* Top-level command handler
*
* INPUTS:
*   server_cb == server control block to use
*   line == input command line from user
*
* OUTPUTS:
*    state may be changed or other action taken
*    the line buffer is NOT consumed or freed by this function
*
* RETURNS:
*   status
*********************************************************************/
status_t
    top_command (ncx_instance_t *instance,
                 server_cb_t *server_cb,
                 xmlChar *line)
{
    obj_template_t         *rpc = NULL;
    xmlChar                *newline = NULL;
    xmlChar                *useline = NULL;
    uint32                 len = 0;
    ncx_node_t             dtyp = NCX_NT_OBJ;
    status_t               res = NO_ERR;

#ifdef DEBUG
    if (!server_cb || !line) {
        return SET_ERROR(instance, ERR_INTERNAL_PTR);
    }
#endif

    if (xml_strlen(instance, line) == 0) {
        return res;
    }

    /* first check the command keyword to see if it is an alias */
    newline = expand_alias(instance, line, &res);
    if (res == ERR_NCX_SKIPPED) {
        res = NO_ERR;
        useline = line;
    } else if (res == NO_ERR) {
        if (newline == NULL) {
            return SET_ERROR(instance, ERR_INTERNAL_VAL);
        }
        useline = newline;
    } else {
        log_error(instance, "\nError: %s\n", get_error_string(res));
        if (newline) {
            m__free(instance, newline);
        }
        return res;
    }

    /* look for an RPC command match for the command keyword */
    rpc = (obj_template_t *)parse_def(instance, server_cb, &dtyp, useline, &len, &res);
    if (rpc==NULL || !obj_is_rpc(instance, rpc)) {
        if (server_cb->result_name || server_cb->result_filename) {
            res = finish_result_assign(instance, server_cb, NULL, useline);
        } else {
            if (res == ERR_NCX_DEF_NOT_FOUND) {
                /* this is an unknown command */
                log_error(instance, "\nError: Unrecognized command\n");
            } else if (res == ERR_NCX_AMBIGUOUS_CMD) {
                log_error(instance, "\n");
            } else {
                log_error(instance, "\nError: %s\n", get_error_string(res));
            }
        }
    } else if (is_yangcli_ns(instance, obj_get_nsid(instance, rpc))) {
        /* check handful of yangcli commands */
        res = do_local_command(instance, server_cb, rpc, useline, len);
    } else {
        res = ERR_NCX_OPERATION_FAILED;
        log_error(instance, "\nError: Not connected to server."
                  "\nLocal commands only in this mode.\n");
    }

    if (newline) {
        m__free(instance, newline);
    }

    return res;

} /* top_command */


/********************************************************************
* FUNCTION conn_command
* 
* Connection level command handler
*
* INPUTS:
*   server_cb == server control block to use
*   line == input command line from user
*
* OUTPUTS:
*    state may be changed or other action taken
*    the line buffer is NOT consumed or freed by this function
*
* RETURNS:
*   status
*********************************************************************/
status_t
    conn_command (ncx_instance_t *instance,
                  server_cb_t *server_cb,
                  xmlChar *line)
{
    obj_template_t        *rpc, *input;
    mgr_rpc_req_t         *req = NULL;
    val_value_t           *reqdata = NULL, *valset = NULL, *parm;
    ses_cb_t              *scb;
    xmlChar               *newline, *useline = NULL;
    uint32                 len, linelen;
    status_t               res = NO_ERR;
    boolean                shut = FALSE;
    ncx_node_t             dtyp;

#ifdef DEBUG
    if (!server_cb || !line) {
        return SET_ERROR(instance, ERR_INTERNAL_PTR);
    }
#endif

    /* make sure there is something to parse */
    linelen = xml_strlen(instance, line);
    if (!linelen) {
        return res;
    }

    /* first check the command keyword to see if it is an alias */
    newline = expand_alias(instance, line, &res);
    if (res == ERR_NCX_SKIPPED) {
        res = NO_ERR;
        useline = line;
    } else if (res == NO_ERR) {
        if (newline == NULL) {
            return SET_ERROR(instance, ERR_INTERNAL_VAL);
        }
        useline = newline;
        linelen = xml_strlen(instance, newline);
    } else {
        log_error(instance, "\nError: %s\n", get_error_string(res));
        if (newline) {
            m__free(instance, newline);
        }
        return res;
    }

    /* get the RPC method template */
    dtyp = NCX_NT_OBJ;
    rpc = (obj_template_t *)parse_def(instance, server_cb, &dtyp, useline, &len, &res);
    if (rpc == NULL || !obj_is_rpc(instance, rpc)) {
        if (server_cb->result_name || server_cb->result_filename) {
            res = finish_result_assign(instance, server_cb, NULL, useline);
        } else {
            if (res == ERR_NCX_DEF_NOT_FOUND) {
                /* this is an unknown command */
                log_error(instance, "\nError: Unrecognized command");
            } else if (res == ERR_NCX_AMBIGUOUS_CMD) {
                log_error(instance, "\n");
            } else {
                log_error(instance, "\nError: %s", get_error_string(res));
            }
        }
        if (newline) {
            m__free(instance, newline);
        }
        return res;
    }

    /* check local commands */
    if (is_yangcli_ns(instance, obj_get_nsid(instance, rpc))) {
        if (!xml_strcmp(instance, obj_get_name(instance, rpc), YANGCLI_CONNECT)) {
            res = ERR_NCX_OPERATION_FAILED;
            log_stdout(instance, "\nError: Already connected");
        } else {
            res = do_local_conn_command(instance, server_cb, rpc, useline, len);
            if (res == ERR_NCX_SKIPPED) {
                res = do_local_command(instance, server_cb, rpc, useline, len);
            }
        }
        if (newline) {
            m__free(instance, newline);
        }
        return res;
    }

    /* else treat this as an RPC request going to the server
     * make sure this is a TRUE conditional command
     */

    /* construct a method + parameter tree */
    reqdata = xml_val_new_struct(instance, obj_get_name(instance, rpc), obj_get_nsid(instance, rpc));
    if (!reqdata) {
        log_error(instance, "\nError allocating a new RPC request");
        res = ERR_INTERNAL_MEM;
        input = NULL;
    } else {
        /* should find an input node */
        input = obj_find_child(instance, rpc, NULL, YANG_K_INPUT);
    }

    /* check if any params are expected */
    if (res == NO_ERR && input) {
        while (useline[len] && xml_isspace(useline[len])) {
            len++;
        }

        if (len < linelen) {
            valset = parse_rpc_cli(instance, server_cb, rpc, &useline[len], &res);
            if (res != NO_ERR) {
                log_error(instance,
                          "\nError in the parameters for '%s' command (%s)",
                          obj_get_name(instance, rpc), get_error_string(res));
            }
        }

        /* check no input from user, so start a parmset */
        if (res == NO_ERR && !valset) {
            valset = val_new_value(instance);
            if (!valset) {
                res = ERR_INTERNAL_MEM;
            } else {
                val_init_from_template(instance, valset, input);
            }
        }

        /* fill in any missing parameters from the CLI */
        if (res == NO_ERR) {
            if (interactive_mode()) {
                res = fill_valset(instance, server_cb, rpc, valset, NULL, TRUE, FALSE);
                if (res == ERR_NCX_SKIPPED) {
                    res = NO_ERR;
                }
            }
        }

        /* make sure the values are in canonical order
         * so compliant some servers will not complain
         */
        val_set_canonical_order(instance, valset);

        /* go through the parm list and move the values 
         * to the reqdata struct. 
         */
        if (res == NO_ERR) {
            parm = val_get_first_child(instance, valset);
            while (parm) {
                val_remove_child(instance, parm);
                val_add_child(instance, parm, reqdata);
                parm = val_get_first_child(instance, valset);
            }
        }
    }

    /* check the close-session corner case */
    if (res == NO_ERR && 
        (obj_get_nsid(instance, rpc) == xmlns_nc_id(instance)) &&
        !xml_strcmp(instance, obj_get_name(instance, rpc), NCX_EL_CLOSE_SESSION)) {
        shut = TRUE;
    }
            
    /* allocate an RPC request and send it */
    if (res == NO_ERR) {
        scb = mgr_ses_get_scb(server_cb->mysid);
        if (!scb) {
            res = SET_ERROR(instance, ERR_INTERNAL_PTR);
        } else {
            req = mgr_rpc_new_request(instance, scb);
            if (!req) {
                res = ERR_INTERNAL_MEM;
                log_error(instance, "\nError allocating a new RPC request");
            } else {
                req->data = reqdata;
                req->rpc = rpc;
                req->timeout = server_cb->timeout;
            }
        }
        
        if (res == NO_ERR) {
            if (LOGDEBUG2) {
                log_debug2(instance,
                           "\nabout to send <%s> RPC request with reqdata:",
                           obj_get_name(instance, rpc));
                val_dump_value_max(instance, reqdata, 0, server_cb->defindent,
                                   DUMP_VAL_LOG, server_cb->display_mode,
                                   FALSE, FALSE);
            }

            /* the request will be stored if this returns NO_ERR */
            res = mgr_rpc_send_request(instance, scb, req, (mgr_rpc_cbfn_t)yangcli_reply_handler);
            if (res != NO_ERR) {
                log_error(instance,
                          "\nError: send <%s> message failed (%s)",
                          obj_get_name(instance, rpc),
                          get_error_string(res));
            }
        }
    }

    if (res != NO_ERR) {
        log_error(instance, "\n");
    }

    if (valset) {
        val_free_value(instance, valset);
    }

    if (newline) {
        m__free(instance, newline);
    }

    if (res != NO_ERR) {
        if (req) {
            mgr_rpc_free_request(instance, req);
        } else if (reqdata) {
            val_free_value(instance, reqdata);
        }
    } else if (shut) {
        server_cb->state = MGR_IO_ST_CONN_CLOSEWAIT;
    } else {
        server_cb->state = MGR_IO_ST_CONN_RPYWAIT;
    }

    return res;

} /* conn_command */


/********************************************************************
 * FUNCTION do_startup_script
 * 
 * Process run-script CLI parameter
 *
 * INPUTS:
 *   server_cb == server control block to use
 *   runscript == name of the script to run (could have path)
 *
 * SIDE EFFECTS:
 *   runstack start with the runscript script if no errors
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_startup_script (ncx_instance_t *instance,
                       server_cb_t *server_cb,
                       const xmlChar *runscript)
{
    obj_template_t       *rpc;
    xmlChar              *line, *p;
    status_t              res;
    uint32                linelen;

#ifdef DEBUG
    if (!server_cb || !runscript) {
        return SET_ERROR(instance, ERR_INTERNAL_PTR);
    }
#endif

    if (!*runscript) {
        return ERR_NCX_INVALID_VALUE;
    }

    /* get the 'run' RPC method template */
    rpc = ncx_find_object(instance, get_yangcli_mod(), YANGCLI_RUN);
    if (!rpc) {
        return ERR_NCX_DEF_NOT_FOUND;
    }

    /* create a dummy command line 'script <runscipt-text>' */
    linelen = xml_strlen(instance, runscript) + xml_strlen(instance, NCX_EL_SCRIPT) + 1;
    line = m__getMem(instance, linelen+1);
    if (!line) {
        return ERR_INTERNAL_MEM;
    }
    p = line;
    p += xml_strcpy(instance, p, NCX_EL_SCRIPT);
    *p++ = ' ';
    xml_strcpy(instance, p, runscript);

    if (LOGDEBUG) {
        log_debug(instance,  "\nBegin startup script '%s'",  runscript);
    }

    /* fill in the value set for the input parameters */
    res = do_run(instance, server_cb, rpc, line, 0);

    m__free(instance, line);

    return res;

}  /* do_startup_script */


/********************************************************************
 * FUNCTION do_startup_command
 * 
 * Process run-command CLI parameter
 *
 * INPUTS:
 *   server_cb == server control block to use
 *   runcommand == command string to run
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_startup_command (ncx_instance_t *instance,
                        server_cb_t *server_cb,
                        const xmlChar *runcommand)
{
    xmlChar        *copystring;
    status_t        res;

#ifdef DEBUG
    if (!server_cb || !runcommand) {
        return SET_ERROR(instance, ERR_INTERNAL_PTR);
    }
#endif

    if (!*runcommand) {
        return ERR_NCX_INVALID_VALUE;
    }

    if (xml_strlen(instance, runcommand) > YANGCLI_LINELEN) {
        return ERR_NCX_RESOURCE_DENIED;
    }

    /* the top_command and conn_command functions
     * expect this buffer to be wriable
     */
    copystring = xml_strdup(instance, runcommand);
    if (!copystring) {
        return ERR_INTERNAL_MEM;
    }

    if (LOGDEBUG) {
        log_debug(instance,  "\nBegin startup command '%s'",  copystring);
    }

    /* only invoke the command in idle or connection idle states */
    switch (server_cb->state) {
    case MGR_IO_ST_IDLE:
        res = top_command(instance, server_cb, copystring);
        break;
    case MGR_IO_ST_CONN_IDLE:
        res = conn_command(instance, server_cb, copystring);
        break;
    default:
        res = ERR_NCX_OPERATION_FAILED;
    }

    m__free(instance, copystring);
    return res;

}  /* do_startup_command */


/********************************************************************
* FUNCTION get_cmd_line
* 
*  Read the current runstack context and construct
*  a command string for processing by do_run.
*    - Extended lines will be concatenated in the
*      buffer.  If a buffer overflow occurs due to this
*      concatenation, an error will be returned
* 
* INPUTS:
*   server_cb == server control block to use
*   res == address of status result
*
* OUTPUTS:
*   *res == function result status
*
* RETURNS:
*   pointer to the command line to process (should treat as CONST !!!)
*   NULL if some error
*********************************************************************/
xmlChar *
    get_cmd_line (ncx_instance_t *instance,
                  server_cb_t *server_cb,
                  status_t *res)
{
    xmlChar        *start, *str, *clibuff;
    boolean         done;
    int             len, total, maxlen;

#ifdef DEBUG
    if (!server_cb || !res) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return NULL;
    }
#endif

    /* init locals */
    clibuff = server_cb->clibuff;
    total = 0;
    str = NULL;
    maxlen = YANGCLI_BUFFLEN;
    done = FALSE;
    start = clibuff;

    /* get a command line, handling comment and continuation lines */
    while (!done) {

        /* read the next line from the user */
        str = get_line(instance, server_cb);
        if (!str) {
            *res = ERR_NCX_READ_FAILED;
            done = TRUE;
            continue;
        }

        /* find end of string */
        len = xml_strlen(instance, str);
        
        /* get rid of EOLN if present */
        if (len && str[len-1]=='\n') {
            str[--len] = 0;
        }

        /* check line continuation */
        if (len && str[len-1]=='\\') {
            /* get rid of the final backslash */
            str[--len] = 0;
            server_cb->climore = TRUE;
        } else {
            /* done getting lines */
            *res = NO_ERR;
            done = TRUE;
        }

        /* copy the string to the clibuff */
        if (total + len < maxlen) {
            xml_strcpy(instance, start, str);
            start += len;
            total += len;
        } else {
            *res = ERR_BUFF_OVFL;
            done = TRUE;
        }
            
        str = NULL;
    }

    server_cb->climore = FALSE;
    if (*res == NO_ERR) {
        /* trim all the trailing whitespace
         * the user or the tab completion might
         * add an extra space at the end of
         * value, and this will cause an invalid-value
         * error to be incorrectly generated
         */
        len = xml_strlen(instance, clibuff);
        if (len > 0) {
            while (len > 0 && isspace(clibuff[len-1])) {
                len--;
            }
            clibuff[len] = 0;
        }
        return clibuff;
    } else {
        return NULL;
    }

}  /* get_cmd_line */


/********************************************************************
 * FUNCTION do_connect
 * 
 * INPUTS:
 *   server_cb == server control block to use
 *   rpc == rpc header for 'connect' command
 *   line == input text from readline call, not modified or freed here
 *   start == byte offset from 'line' where the parse RPC method
 *            left off.  This is eiother empty or contains some 
 *            parameters from the user
 *   startupmode == TRUE if starting from init and should try
 *              to connect right away if the mandatory parameters
 *              are present.
 *               == FALSE to check --optional and add parameters
 *                  if set or any missing mandatory parms
 *
 * OUTPUTS:
 *   connect_valset parms may be set 
 *   create_session may be called
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_connect (ncx_instance_t *instance,
                server_cb_t *server_cb,
                obj_template_t *rpc,
                const xmlChar *line,
                uint32 start,
                boolean startupmode)
{
    obj_template_t        *obj;
    val_value_t           *connect_valset;
    val_value_t           *valset, *testval;
    status_t               res;
    boolean                s1, s2, s3, tcp;

#ifdef DEBUG
    if (server_cb == NULL) {
        return SET_ERROR(instance, ERR_INTERNAL_PTR);
    }
#endif

    /* retrieve the 'connect' RPC template, if not done already */
    if (rpc == NULL) {
        rpc = ncx_find_object(instance, get_yangcli_mod(), YANGCLI_CONNECT);
        if (rpc == NULL) {
            server_cb->state = MGR_IO_ST_IDLE;
            log_write(instance, "\nError finding the 'connect' RPC method");
            return ERR_NCX_DEF_NOT_FOUND;
        }
    }            

    obj = obj_find_child(instance, rpc, NULL, YANG_K_INPUT);
    if (obj == NULL) {
        server_cb->state = MGR_IO_ST_IDLE;
        log_write(instance, "\nError finding the connect RPC 'input' node");        
        return SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    res = NO_ERR;
    tcp = FALSE;

    connect_valset = get_connect_valset();

    /* process any parameters entered on the command line */
    valset = NULL;
    if (line != NULL) {
        while (line[start] && xml_isspace(line[start])) {
            start++;
        }
        if (line[start]) {
            valset = parse_rpc_cli(instance, server_cb, rpc, &line[start], &res);
            if (valset == NULL || res != NO_ERR) {
                if (valset != NULL) {
                    val_free_value(instance, valset);
                }
                log_write(instance,
                          "\nError in the parameters for '%s' command (%s)",
                          obj_get_name(instance, rpc), 
                          get_error_string(res));
                server_cb->state = MGR_IO_ST_IDLE;
                return res;
            }
        }
    }

    if (valset == NULL) {
        if (startupmode) {
            /* just clone the connect valset to start with */
            valset = val_clone(instance, connect_valset);
            if (valset == NULL) {
                server_cb->state = MGR_IO_ST_IDLE;
                log_write(instance, "\nError: malloc failed");
                return ERR_INTERNAL_MEM;
            }
        } else {
            valset = val_new_value(instance);
            if (valset == NULL) {
                log_write(instance, "\nError: malloc failed");
                server_cb->state = MGR_IO_ST_IDLE;
                return ERR_INTERNAL_MEM;
            } else {
                val_init_from_template(instance, valset, obj);
            }
        }
    }

    /* make sure the 3 required parms are set */
    s1 = val_find_child(instance, valset, YANGCLI_MOD, 
                        YANGCLI_SERVER) ? TRUE : FALSE;
    s2 = val_find_child(instance, valset, YANGCLI_MOD,
                        YANGCLI_USER) ? TRUE : FALSE;
    s3 = val_find_child(instance, valset, YANGCLI_MOD,
                        YANGCLI_PASSWORD) ? TRUE : FALSE;

    /* check the transport parameter */
    testval = val_find_child(instance, 
                             valset, 
                             YANGCLI_MOD,
                             YANGCLI_TRANSPORT);
    if (testval != NULL && 
        testval->res == NO_ERR && 
        !xml_strcmp(instance,
                    VAL_ENUM_NAME(testval),
                    (const xmlChar *)"tcp")) {
        tcp = TRUE;
    }
    
    /* complete the connect valset if needed
     * and transfer it to the server_cb version
     *
     * try to get any missing params in valset 
     */
    if (interactive_mode()) {
        if (startupmode && s1 && s2 && (s3 || tcp)) {
            if (LOGDEBUG3) {
                log_debug3(instance, "\nyangcli: CLI direct connect mode");
            }
        } else {
            res = fill_valset(instance, server_cb, rpc, valset, connect_valset, 
                              TRUE, FALSE);
            if (res == ERR_NCX_SKIPPED) {
                res = NO_ERR;
            }
        }
    }

    /* check error or operation canceled */
    if (res != NO_ERR) {
        if (res != ERR_NCX_CANCELED) {
            log_write(instance, 
                      "\nError: Connect failed (%s)", 
                      get_error_string(res));
        }
        server_cb->state = MGR_IO_ST_IDLE;
        val_free_value(instance, valset);
        return res;
    }

    /* passing off valset memory here */
    s1 = s2 = s3 = FALSE;
    if (valset != NULL) {
        /* save the malloced valset */
        if (server_cb->connect_valset != NULL) {
            val_free_value(instance, server_cb->connect_valset);
        }
        server_cb->connect_valset = valset;

        /* make sure the 3 required parms are set */
        s1 = val_find_child(instance,
                            server_cb->connect_valset,
                            YANGCLI_MOD, 
                            YANGCLI_SERVER) ? TRUE : FALSE;
        s2 = val_find_child(instance, 
                            server_cb->connect_valset, 
                            YANGCLI_MOD,
                            YANGCLI_USER) ? TRUE : FALSE;
        s3 = val_find_child(instance, 
                            server_cb->connect_valset, 
                            YANGCLI_MOD,
                            YANGCLI_PASSWORD) ? TRUE : FALSE;
    }

    /* check if all params present yet */
    if (s1 && s2 && (s3 || tcp)) {
        res = replace_connect_valset(instance, server_cb->connect_valset);
        if (res != NO_ERR) {
            log_warn(instance, "\nWarning: connection parameters could not be saved");
            res = NO_ERR;
        }
        create_session(instance, server_cb);
    } else {
        res = ERR_NCX_MISSING_PARM;
        log_write(instance, "\nError: Connect failed due to missing parameter(s)");
        server_cb->state = MGR_IO_ST_IDLE;
    }

    return res;

}  /* do_connect */


/********************************************************************
* FUNCTION parse_def
* 
* Definitions have two forms:
*   def       (all available modules searched)
*   module:def (explicit module name used)
*   prefix:def (if prefix-to-module found, explicit module name used)
*
* Parse the possibly module-qualified definition (module:def)
* and find the template for the requested definition
*
* INPUTS:
*   server_cb == server control block to use
*   dtyp == definition type 
*       (NCX_NT_OBJ or  NCX_NT_TYP)
*   line == input command line from user
*   len == address of output var for number of bytes parsed
*   retres == address of return status
*
* OUTPUTS:
*    *dtyp is set if it started as NONE
*    *len == number of bytes parsed
*    *retres == return status
*
* RETURNS:
*   pointer to the found definition template or NULL if not found
*********************************************************************/
void *
    parse_def (ncx_instance_t *instance,
               server_cb_t *server_cb,
               ncx_node_t *dtyp,
               xmlChar *line,
               uint32 *len,
               status_t *retres)
{
    void           *def, *lastmatch;
    xmlChar        *start, *p, *q, oldp, oldq;
    const xmlChar  *prefix, *defname, *modname, *defmod;
    ncx_module_t   *mod;
    modptr_t       *modptr;
    uint32          tempcount, matchcount;
    xmlns_id_t      nsid;
    status_t        res;
    boolean         first;

    def = NULL;
    q = NULL;
    oldq = 0;
    *len = 0;
    start = line;
    modname = NULL;
    res = NO_ERR;

    /* skip any leading whitespace */
    while (*start && xml_isspace(*start)) {
        start++;
    }

    p = start;

    /* look for a colon or EOS or whitespace to end command name */
    while (*p && (*p != ':') && !xml_isspace(*p)) {
        p++;
    }

    /* make sure got something */
    if (p==start) {
        return NULL;
    }

    /* search for a module prefix if a separator was found */
    if (*p == ':') {

        /* use an explicit module prefix in YANG
         * only this exact module will be tried
         */
        q = p+1;
        while (*q && !xml_isspace(*q)) {
            q++;
        }
        *len = q - line;

        /* save the old char and zero-terminate the definition
         * that will be parsed as a command name
         */
        oldq = *q;
        *q = 0;
        oldp = *p;
        *p = 0;

        prefix = start;
        defname = p+1;
    } else {
        /* no prefix given so search all the available modules */
        *len = p - line;

        oldp = *p;
        *p = 0;

        prefix = NULL;
        defname = start;
    }

    /* look in the registry for the definition name 
     * first check if only the user supplied a module name
     */
    if (prefix) {
        nsid = xmlns_find_ns_by_prefix(instance, prefix);
        if (nsid) {
            modname = xmlns_get_module(instance, nsid);
        }
        if (modname) {
            /* try exact match in specified module */
            def = try_parse_def(instance, server_cb, NULL, modname, defname, dtyp);
        } else {
            log_error(instance, 
                      "\nError: no module found for prefix '%s'", 
                      prefix);
        }
    } else {
        /* search for the command; try YANGCLI module first
         * do not care about duplicate matches in other
         * modules. An unprefixed keyword will match
         * a yangcli command first
         */
        def = try_parse_def(instance, server_cb, NULL, YANGCLI_MOD, defname, dtyp);

        if (def == NULL) {
            /* 2) try an exact match in the default module
             * do not care about duplicates because the purpose
             * of the default-module parm is to resolve any name
             * collisions by picking the default module
             */
            defmod = get_default_module();
            if (defmod != NULL) {
                /* do not re-check the yangcli and netconf modules */
                if (xml_strcmp(instance, defmod, NC_MODULE) &&
                    xml_strcmp(instance, defmod, NCXMOD_IETF_NETCONF)) {
                    def = try_parse_def(instance, server_cb, NULL, defmod, defname, dtyp);
                }
            }
        }

        /* 3) if not found, try any server advertised module */
        if (def == NULL && use_servercb(instance, server_cb)) {
            /* try any of the server modules first
             * make sure there is only 1 exact match 
             */
            matchcount = 0;
            lastmatch = NULL;
            for (modptr = (modptr_t *)
                     dlq_firstEntry(instance, &server_cb->modptrQ);
                 modptr != NULL;
                 modptr = (modptr_t *)dlq_nextEntry(instance, modptr)) {

                def = try_parse_def(instance, server_cb, modptr->mod, modptr->mod->name, 
                                    defname, dtyp);
                if (def != NULL) {
                    matchcount++;
                    lastmatch = def;
                }
            }

            if (matchcount > 1) {
                /* generate ambiguous command error */
                res = ERR_NCX_AMBIGUOUS_CMD;
                first = TRUE;
                for (modptr = (modptr_t *)
                         dlq_firstEntry(instance, &server_cb->modptrQ);
                     modptr != NULL;
                     modptr = (modptr_t *)dlq_nextEntry(instance, modptr)) {

                    ncx_match_rpc_error(instance, modptr->mod, modptr->mod->name,
                                        defname, FALSE, first);
                    if (first) {
                        first = FALSE;
                    }
                }
                /* check any matches for local commands too */
                ncx_match_rpc_error(instance, get_yangcli_mod(), NULL, defname,
                                    FALSE, FALSE);
            } else {
                /*  0 or 1 matches found */
                def = lastmatch;
            }
        }

        /* 4) try any of the manager-loaded modules */
        if (res == NO_ERR && def == NULL) {
            /* make sure there is only 1 exact match */
            matchcount = 0;
            lastmatch = NULL;
            for (mod = ncx_get_first_module(instance);
                 mod != NULL;
                 mod = ncx_get_next_module(instance, mod)) {

                def = try_parse_def(instance, server_cb, mod, mod->name, defname, dtyp);
                if (def != NULL) {
                    lastmatch = def;
                    matchcount++;
                }
            }

            if (matchcount > 1) {
                /* generate ambiguous command error */
                res = ERR_NCX_AMBIGUOUS_CMD;
                ncx_match_rpc_error(instance, NULL, NULL, defname, FALSE, TRUE);
            } else {
                /* 0 or 1 matches found */
                def = lastmatch;
            }
        }
    }

    /* if not found and no error, try a partial command name */
    if (res == NO_ERR && def == NULL && get_autocomp()) {
        matchcount = 0;
        lastmatch = NULL;

        if (prefix != NULL) {
            if (modname != NULL) {
                /* try to match a command from this module */
                def = ncx_match_any_rpc(instance, modname, defname, &matchcount);
                if (matchcount > 1) {
                    res = ERR_NCX_AMBIGUOUS_CMD;
                    ncx_match_rpc_error(instance, NULL, modname, defname, TRUE, TRUE);
                } /* else 0 or 1 matches found */
            } /* else module not found error already done */
        } else {
            switch (*dtyp) {
            case NCX_NT_NONE:
            case NCX_NT_OBJ:
                if (use_servercb(instance, server_cb)) {
                    /* check for 1 or more matches */
                    for (modptr = (modptr_t *)
                             dlq_firstEntry(instance, &server_cb->modptrQ);
                         modptr != NULL;
                         modptr = (modptr_t *)dlq_nextEntry(instance, modptr)) {

                        tempcount = 0;
                        def = ncx_match_any_rpc_mod(instance, modptr->mod, defname,
                                                    &tempcount);
                        if (def) {
                            lastmatch = def;
                            matchcount += tempcount;
                        }
                    }
                    if (matchcount > 1) {
                        /* server_cb had multiple matches */
                        res = ERR_NCX_AMBIGUOUS_CMD;
                        first = TRUE;
                        for (modptr = (modptr_t *)
                                 dlq_firstEntry(instance, &server_cb->modptrQ);
                             modptr != NULL;
                             modptr = (modptr_t *)dlq_nextEntry(instance, modptr)) {
                            ncx_match_rpc_error(instance, modptr->mod, modptr->mod->name,
                                                defname, TRUE, first);
                            if (first) {
                                first = FALSE;
                            }
                        }
                        /* list any partial local command matches */
                        ncx_match_rpc_error(instance, get_yangcli_mod(), NULL,  defname,
                                            TRUE, FALSE);
                    }
                }

                if (res == NO_ERR && lastmatch == NULL) {
                    /* did not match any of the server modules
                     * or maybe no session active right now
                     * check the modules that the client has loaded
                     * with mgrload or at boot-time with the CLI
                     */
                    matchcount = 0;
                    def = ncx_match_any_rpc(instance, NULL, defname, &matchcount);
                    if (matchcount > 1) {
                        res = ERR_NCX_AMBIGUOUS_CMD;
                        ncx_match_rpc_error(instance, NULL, NULL, defname, TRUE, TRUE);
                    }
                }
                break;
            default:
                /* return NULL because only object types or
                 * match anything mode will cause a partial match
                 */
                ;
            }
        }
    }

    /* restore string as needed */
    *p = oldp;
    if (q) {
        *q = oldq;
    }

    /* return status if requested */
    if (retres != NULL) {
        if (res != NO_ERR) {
            *retres = res;
        } else if (def == NULL) {
            *retres = ERR_NCX_DEF_NOT_FOUND;
        } else {
            *retres = NO_ERR;
        }
    }

    if (res == NO_ERR) {
        return def;
    } else {
        return NULL;
    }
    
} /* parse_def */


/********************************************************************
* FUNCTION send_keepalive_get
* 
* Send a <get> operation to the server to keep the session
* from getting timed out; server sent a keepalive request
* and SSH will drop the session unless data is sent
* within a configured time
*
* !!! NOT IMPLEMENTED !!!
*
* INPUTS:
*    server_cb == server control block to use
*
* OUTPUTS:
*    state may be changed or other action taken
*
* RETURNS:
*    status
*********************************************************************/
status_t
    send_keepalive_get (server_cb_t *server_cb)
{
    (void)server_cb;
    return NO_ERR;
} /* send_keepalive_get */


/********************************************************************
 * FUNCTION get_valset
 * 
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the command being processed
 *    line == CLI input in progress
 *    res == address of status result
 *
 * OUTPUTS:
 *    *res is set to the status
 *
 * RETURNS:
 *   malloced valset filled in with the parameters for 
 *   the specified RPC
 *
 *********************************************************************/
val_value_t *
    get_valset (ncx_instance_t *instance,
                server_cb_t *server_cb,
                obj_template_t *rpc,
                const xmlChar *line,
                status_t  *res)
{
    obj_template_t        *obj;
    val_value_t           *valset;
    uint32                 len;

    *res = NO_ERR;
    valset = NULL;
    len = 0;

    set_completion_state(instance,
                         &server_cb->completion_state,
                         rpc, NULL, CMD_STATE_GETVAL);

    /* skip leading whitespace */
    while (line[len] && xml_isspace(line[len])) {
        len++;
    }

    /* check any non-whitespace entered after RPC method name */
    if (line[len]) {
        valset = parse_rpc_cli(instance, server_cb, rpc, &line[len], res);
        if (*res == ERR_NCX_SKIPPED) {
            log_stdout(instance,
                       "\nError: no parameters defined for '%s' command",
                       obj_get_name(instance, rpc));
        } else if (*res != NO_ERR) {
            log_stdout(instance,
                       "\nError in the parameters for '%s' command (%s)",
                       obj_get_name(instance, rpc), 
                       get_error_string(*res));
        }
    }

    obj = obj_find_child(instance, rpc, NULL, YANG_K_INPUT);
    if (!obj || !obj_get_child_count(instance, obj)) {
        *res = ERR_NCX_SKIPPED;
        if (valset) {
            val_free_value(instance, valset);
        }
        return NULL;
    }

    /* check no input from user, so start a parmset */
    if (*res == NO_ERR && !valset) {
        valset = val_new_value(instance);
        if (!valset) {
            *res = ERR_INTERNAL_MEM;
        } else {
            val_init_from_template(instance, valset, obj);
            *res = val_add_defaults(instance, valset, NULL, NULL, SCRIPTMODE);
        }
    }

    /* fill in any missing parameters from the CLI */
    if (*res==NO_ERR && interactive_mode()) {
        *res = fill_valset(instance, server_cb, rpc, valset, NULL, TRUE, FALSE);
    }

    if (*res==NO_ERR) {
        *res = val_instance_check(instance, valset, valset->obj);
    }
    return valset;

}  /* get_valset */


/********************************************************************
 * FUNCTION do_line_recall (execute the recall local RPC)
 * 
 * recall n 
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    num == entry number of history entry entry to recall
 * 
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_line_recall (ncx_instance_t *instance,
                    server_cb_t *server_cb,
                    unsigned long num)
{
    GlHistoryLine   history_line;
    int             glstatus;

    server_cb->history_line_active = FALSE;
    memset(&history_line, 0x0, sizeof(GlHistoryLine));
    glstatus = gl_lookup_history(server_cb->cli_gl, num, &history_line);

    if (glstatus == 0) {
        log_error(instance, "\nError: lookup command line history failed");
        return ERR_NCX_OPERATION_FAILED; 
    }

    if (server_cb->history_line) {
        m__free(instance, server_cb->history_line);
    }

    /* save the line in the server_cb for next call
     * to get_line
     */

    server_cb->history_line = xml_strdup(instance, (const xmlChar *)history_line.line);
    if (!server_cb->history_line) {
        return ERR_INTERNAL_MEM;
    }
    server_cb->history_line_active = TRUE;

    return NO_ERR;

} /* do_line_recall */


/********************************************************************
 * FUNCTION do_line_recall_string
 * 
 * bang recall support
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    line  == command line to recall
 * 
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_line_recall_string (ncx_instance_t *instance,
                           server_cb_t *server_cb,
                           const xmlChar *line)
{
    GlHistoryLine   history_line;
    GlHistoryRange  history_range;
    int             glstatus;
    uint32          len;
    unsigned long   curindex;
    boolean         done;

    len = xml_strlen(instance, line);
    if (len == 0) {
        log_error(instance, "\nError: missing recall string\n");
        return ERR_NCX_MISSING_PARM;
    }

    server_cb->history_line_active = FALSE;
    memset(&history_line, 0x0, sizeof(GlHistoryLine));
    memset(&history_range, 0x0, sizeof(GlHistoryRange));

    gl_range_of_history(server_cb->cli_gl, &history_range);

    if (history_range.nlines == 0) {
        log_error(instance, "\nError: no command line history found\n");
        return ERR_NCX_OPERATION_FAILED; 
    }

    /* look backwards through history buffer */
    done = FALSE;
    for (curindex = history_range.newest;
         curindex >= history_range.oldest && !done;
         curindex--) {

        glstatus = gl_lookup_history(server_cb->cli_gl, curindex,
                                     &history_line);
        if (glstatus == 0) {
            continue;
        }
        if (!xml_strnicmp(instance, (const xmlChar *)history_line.line, line, len)) {
            done = TRUE;
            continue;
        }

        if (curindex == history_range.oldest) {
            log_error(instance, "\nError: command line '%s' not found\n", line);
            return ERR_NCX_OPERATION_FAILED; 
        }
    }

    if (server_cb->history_line != NULL) {
        m__free(instance, server_cb->history_line);
    }

    /* save the line in the server_cb for next call
     * to get_line
     */

    server_cb->history_line = 
        xml_strdup(instance, (const xmlChar *)history_line.line);
    if (server_cb->history_line == NULL) {
        return ERR_INTERNAL_MEM;
    }
    server_cb->history_line_active = TRUE;

    return NO_ERR;

} /* do_line_recall_string */


/* END yangcli_cmd.c */
