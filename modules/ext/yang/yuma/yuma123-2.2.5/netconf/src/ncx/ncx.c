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
/*  FILE: ncx.c

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
30oct05      abb      begun
30oct07      abb      change identifier separator from '.' to '/'
                      and change valid identifier chars to match YANG
02Sep14      rajv     Support for unique tag ids

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#ifdef HAS_FLOAT
#include <math.h>
#include <tgmath.h>
#endif

#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include <xmlstring.h>
#include <xmlreader.h>

#include "procdefs.h"

#ifndef RELEASE
#include "curversion.h"
#endif

#include "bobhash.h"
#include "cfg.h"
#include "cli.h"
#include "def_reg.h"
#include "dlq.h"
#include "ext.h"
#include "grp.h"
#include "log.h"
#include "ncx.h"
#include "ncx_appinfo.h"
#include "ncx_feature.h"
#include "ncx_list.h"
#include "ncx_num.h"
#include "ncxconst.h"
#include "ncxmod.h"
#include "obj.h"
#include "rpc.h"
#include "rpc_err.h"
#include "runstack.h"
#include "status.h"
#include "ses_msg.h"
#include "typ.h"
#include "top.h"
#include "val.h"
#include "version.h"
#include "xml_util.h"
#include "xmlns.h"
#include "yang.h"
#include "yangconst.h"

/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

#define INV_PREFIX  ((const xmlChar *)"inv")
#define WILDCARD_PREFIX  ((const xmlChar *)"___")


#ifdef DEBUG
/* this flag will cause debug4 trace statements to be printed
 * to the log during operation for module alloc and free operations
 */
/* #define NCX_DEBUG_MOD_MEMORY  1 */
/* #define WITH_TRACEFILE 1 */
#endif

/********************************************************************
*                                                                   *
*                            T Y P E S                              *
*                                                                   *
*********************************************************************/

/* warning suppression entry */
typedef struct warnoff_t_ {
    dlq_hdr_t    qhdr;
    status_t     res;
} warnoff_t;


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/

/* Gloval instance, set by the application, if needed. */
ncx_instance_t *ncx_g_instance;




/**
 * \fn check_moddef
 * \brief Check if a specified module is loaded; if not, load it.
 * Search the module for the data struct for the specified definition
 * name.
 * \param pcb parser control block to use
 * \param imp import struct to use, imp->mod may be set if not already
 * \param defname name of the app-specific definition to find
 * \param dtyp address of return definition type (for verification)
 * \param *dtyp NCX_NT_NONE for any match, or a specific type to find
 * \param dptr addres of return definition pointer
 * \param (output) pcb parser control block to use
 * \param (output) *dtyp node type found NCX_NT_OBJ or NCX_NT_TYPE, etc. 
 * \param (output) *dptr pointer to data struct or NULL if not found
 * \return status
 */   
static status_t
    check_moddef (ncx_instance_t *instance,
                  yang_pcb_t *pcb,
                  ncx_import_t *imp,
                  const xmlChar *defname,
                  ncx_node_t *dtyp,
                  void **dptr)
{
    status_t       res;

    /* First find or load the module if it has not already been tried */
    if (imp->res != NO_ERR) {
        return imp->res;
    }

    if (!imp->mod) {
        imp->mod = ncx_find_module(instance, imp->module, imp->revision);
    }

    if (!imp->mod) {
        res = ncxmod_load_module(instance, 
                                 imp->module, 
                                 imp->revision, 
                                 pcb->savedevQ,
                                 &imp->mod);
        if (!imp->mod || res != NO_ERR) {
            return ERR_NCX_MOD_NOT_FOUND;
        }
    }

    /* have a module loaded that might contain this def 
     * look for the defname
     * the module may be loaded with non-fatal errors
     */
    switch (*dtyp) {
    case NCX_NT_TYP:
        *dptr = ncx_find_type(instance, imp->mod, defname, FALSE);
        break;
    case NCX_NT_GRP:
        *dptr = ncx_find_grouping(instance, imp->mod, defname, FALSE);
        break;
    case NCX_NT_OBJ:
        *dptr = obj_find_template(instance, 
                                  &imp->mod->datadefQ, 
                                  imp->mod->name, 
                                  defname);
        break;
    case NCX_NT_NONE:
        *dptr = ncx_find_type(instance, imp->mod, defname, FALSE);
        if (*dptr) {
            *dtyp = NCX_NT_TYP;
        }
        if (!*dptr) {
            *dptr = ncx_find_grouping(instance, imp->mod, defname, FALSE);
            if (*dptr) {
                *dtyp = NCX_NT_GRP;
            }
        }
        if (!*dptr) {
            *dptr = obj_find_template(instance, 
                                      &imp->mod->datadefQ, 
                                      imp->mod->name, 
                                      defname);
            if (*dptr) {
                *dtyp = NCX_NT_OBJ;
            }
        }
        break;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        *dptr = NULL;
    }

    return (*dptr) ? NO_ERR : ERR_NCX_DEF_NOT_FOUND;

}  /* check_moddef */

// ----------------------------------------------------------------------------!

/**
 * \fn set_toplevel_defs 
 * \brief 
 * \param mod module to check
 * \param nsid namespace ID of the main module being added
 * \return status 
 */
static status_t 
    set_toplevel_defs (ncx_instance_t *instance,
                       ncx_module_t *mod,
                       xmlns_id_t    nsid)
{
    typ_template_t *typ;
    grp_template_t *grp;
    obj_template_t *obj;
    ext_template_t *ext;

    for (typ = (typ_template_t *)dlq_firstEntry(instance, &mod->typeQ);
         typ != NULL;
         typ = (typ_template_t *)dlq_nextEntry(instance, typ)) {

        typ->nsid = nsid;
    }

    for (grp = (grp_template_t *)dlq_firstEntry(instance, &mod->groupingQ);
         grp != NULL;
         grp = (grp_template_t *)dlq_nextEntry(instance, grp)) {

        grp->nsid = nsid;
    }

    /* the first time this is called (for yuma-ncx) the
     * gen_root object has not been set and is still NULL
     * The 'root' object is defined in yuma-ncx.
     * There are no real data objects in this module.
     * All other modules that follow will set the parent of
     * top-level objects to 'gen_root' for XPath usage */
    for (obj = (obj_template_t *)dlq_firstEntry(instance, &mod->datadefQ);
         obj != NULL;
         obj = (obj_template_t *)dlq_nextEntry(instance, obj)) {

        if (obj_is_data_db(instance, obj) || obj_is_rpc(instance, obj) || obj_is_notif(instance, obj)) {
            /* set the parent to the gen_root object so XPath expressions
             * that reference the root via ancestor (../../foo) will work; */
            obj->parent = instance->gen_root;
        }
    }

    for (ext = (ext_template_t *)dlq_firstEntry(instance, &mod->extensionQ);
         ext != NULL;
         ext = (ext_template_t *)dlq_nextEntry(instance, ext)) {
        ext->nsid = nsid;
    }

    return NO_ERR;

}  /* set_toplevel_defs */

// ----------------------------------------------------------------------------!

/**
 * \fn free_module
 * \brief Scrub the memory in a ncx_module_t by freeing all the
 * sub-fields and then freeing the entire struct itself 
 * \details MUST remove this struct from the ncx_modQ before calling
 * Do not need to remove module definitions from the registry
 * Use the ncx_remove_module function if the module was 
 * already successfully added to the modQ and definition registry
 * \param mod ncx_module_t data structure to free
 * \return none
 */
static void 
    free_module (ncx_instance_t *instance, ncx_module_t *mod)
{
    ncx_revhist_t  *revhist;
    ncx_import_t   *import;
    ncx_include_t  *incl;
    ncx_feature_t  *feature;
    ncx_identity_t *identity;
    yang_stmt_t    *stmt;

    (void)instance;
#ifdef NCX_DEBUG_MOD_MEMORY
    if (LOGDEBUG3) {
        log_debug3(instance, 
                   "\n%p:modtrace:freemod:%s:%s", 
                   mod, 
                   (mod->ismod) ? NCX_EL_MODULE : NCX_EL_SUBMODULE,
                   mod->name);
        if (instance->usedeadmodQ) {
            log_debug3(instance, " save in deadmodQ");
        }
    }
#endif

    if (instance->usedeadmodQ) {
        dlq_enque(instance, mod, &instance->deadmodQ);
        return;
    }

    /* clear the revision Q */
    while (!dlq_empty(instance, &mod->revhistQ)) {
        revhist = (ncx_revhist_t *)dlq_deque(instance, &mod->revhistQ);
        ncx_free_revhist(instance, revhist);
    }

    /* clear the import Que */
    while (!dlq_empty(instance, &mod->importQ)) {
        import = (ncx_import_t *)dlq_deque(instance, &mod->importQ);
        ncx_free_import(instance, import);
    }

    /* clear the include Que */
    while (!dlq_empty(instance, &mod->includeQ)) {
        incl = (ncx_include_t *)dlq_deque(instance, &mod->includeQ);
        ncx_free_include(instance, incl);
    }

    /* clear the allinc Q , empty unless this is a mod w/ submods */
    yang_clean_nodeQ(instance, &mod->allincQ);

    /* clear the incchain Q , empty unless this is a mod w/ submods */
    yang_clean_nodeQ(instance, &mod->incchainQ);

    /* clear the type Que */
    typ_clean_typeQ(instance, &mod->typeQ);

    /* clear the grouping Que */
    grp_clean_groupingQ(instance, &mod->groupingQ);

    /* clear the datadefQ */
    obj_clean_datadefQ(instance, &mod->datadefQ);

    /* clear the extension Que */
    ext_clean_extensionQ(instance, &mod->extensionQ);

    obj_clean_deviationQ(instance, &mod->deviationQ);

    ncx_clean_appinfoQ(instance, &mod->appinfoQ);

    ncx_clean_typnameQ(instance, &mod->typnameQ);

    yang_clean_import_ptrQ(instance, &mod->saveimpQ);

    /* clear the YANG stmtQ, used for docmode only */
    while (!dlq_empty(instance, &mod->stmtQ)) {
        stmt = (yang_stmt_t *)dlq_deque(instance, &mod->stmtQ);
        yang_free_stmt(instance, stmt);
    }

    /* clear the YANG featureQ */
    while (!dlq_empty(instance, &mod->featureQ)) {
        feature = (ncx_feature_t *)dlq_deque(instance, &mod->featureQ);
        ncx_free_feature(instance, feature);
    }

    /* clear the YANG identityQ */
    while (!dlq_empty(instance, &mod->identityQ)) {
        identity = (ncx_identity_t *)dlq_deque(instance, &mod->identityQ);
        ncx_free_identity(instance, identity);
    }

    /* clear the name and other fields last for easier debugging */
    if (mod->name) {
        m__free(instance, mod->name);
    }
    if (mod->version) {
        m__free(instance, mod->version);
    }
    if (mod->organization) {
        m__free(instance, mod->organization);
    }
    if (mod->contact_info) {
        m__free(instance, mod->contact_info);
    }
    if (mod->descr) {
        m__free(instance, mod->descr);
    }
    if (mod->ref) {
        m__free(instance, mod->ref);
    }
    if (mod->ismod && mod->ns) {
        m__free(instance, mod->ns);
    } 
    if (mod->prefix) {
        m__free(instance, mod->prefix);
    }
    if (mod->xmlprefix) {
        m__free(instance, mod->xmlprefix);
    }
    if (mod->source) {
        m__free(instance, mod->source);
    }
    if (mod->belongs) {
        m__free(instance, mod->belongs);
    }

    ncx_clean_list(instance, &mod->devmodlist);

    m__free(instance, mod);

}  /* free_module */

// ----------------------------------------------------------------------------!

/**
 * \fn bootstrap_cli
 * \brief Handle the CLI parms that need to be set even before
 * any other initialization has been done, or any YANG modules
 * have been loaded.
 * \details Hardwired list of bootstrap parameters
 * DO NOT RESET THEM FROM THE OUTPUT OF THE cli_parse function
 * THESE CLI PARAMS WILL STILL BE IN THE argv array
 *
 * Common CLI parameters handled as bootstrap parameters
 *
 *   log-level
 *   log-append
 *   log
 *   modpath
 * \param argc CLI argument count
 * \param argv array of CLI parms
 * \param dlevel default debug level
 * \param logtstamps flag for log_open, if 'log' parameter is present
 * \return status
 */
static status_t 
    bootstrap_cli (ncx_instance_t *instance,
                   int argc,
                   char *argv[],
                   log_debug_t dlevel,
                   boolean logtstamps)
{
    dlq_hdr_t        parmQ;
    cli_rawparm_t   *parm;
    char            *logfilename;
    log_debug_t      loglevel;
    boolean          logappend;
    status_t         res;

    dlq_createSQue(instance, &parmQ);

    res = NO_ERR;
    logfilename = NULL;
    logappend = FALSE;
    loglevel = LOG_DEBUG_NONE;

    /* create bootstrap parm: home */
    parm = cli_new_rawparm(instance, NCX_EL_HOME);
    if (parm) {
        dlq_enque(instance, parm, &parmQ);
    } else {
        log_error(instance, "\nError: malloc failed");
        res = ERR_INTERNAL_MEM;
    }

    /* create bootstrap parm: log-level */
    if (res == NO_ERR) {
        parm = cli_new_rawparm(instance, NCX_EL_LOGLEVEL);
        if (parm) {
            dlq_enque(instance, parm, &parmQ);
        } else {
            log_error(instance, "\nError: malloc failed");
            res = ERR_INTERNAL_MEM;
        }
    }

    /* create bootstrap parm: log */
    if (res == NO_ERR) {
        parm = cli_new_rawparm(instance, NCX_EL_LOG);
        if (parm) {
            dlq_enque(instance, parm, &parmQ);
        } else {
            log_error(instance, "\nError: malloc failed");
            res = ERR_INTERNAL_MEM;
        }
    }

    /* create bootstrap parm: log-append */
    if (res == NO_ERR) {
        parm = cli_new_empty_rawparm(instance, NCX_EL_LOGAPPEND);
        if (parm) {
            dlq_enque(instance, parm, &parmQ);
        } else {
            log_error(instance, "\nError: malloc failed");
            res = ERR_INTERNAL_MEM;
        }
    }

    /* create bootstrap parm: modpath */
    if (res == NO_ERR) {
        parm = cli_new_rawparm(instance, NCX_EL_MODPATH);
        if (parm) {
            dlq_enque(instance, parm, &parmQ);
        } else {
            log_error(instance, "\nError: malloc failed");
            res = ERR_INTERNAL_MEM;
        }
    }

    /* create bootstrap parm: yuma-home */
    if (res == NO_ERR) {
        parm = cli_new_rawparm(instance, NCX_EL_YUMA_HOME);
        if (parm) {
            dlq_enque(instance, parm, &parmQ);
        } else {
            log_error(instance, "\nError: malloc failed");
            res = ERR_INTERNAL_MEM;
        }
    }

    /* check if any of these bootstrap parms are present
     * and process them right away; this is different than
     * normal CLI where all the parameters are gathered,
     * validated, and then invoked
     */
    if (res == NO_ERR) {
        res = cli_parse_raw(instance, argc, argv, &parmQ);
        if (res != NO_ERR) {
            log_error(instance,
                      "\nError: bootstrap CLI failed (%s)",
                      get_error_string(res));
        }
    }

    if (res != NO_ERR) {
        cli_clean_rawparmQ(instance, &parmQ);
        return res;
    }

    /* --home=<dirspec> */
    parm = cli_find_rawparm(instance, NCX_EL_HOME, &parmQ);
    if (parm && parm->count) {
        if (parm->count > 1) {
            log_error(instance, "\nError: Only one home parameter allowed");
            res = ERR_NCX_DUP_ENTRY;
        } else if (parm->value) {
            ncxmod_set_home(instance, (const xmlChar *)parm->value);
        } else {
            log_error(instance, "\nError: no value entered for 'home' parameter");
            res = ERR_NCX_INVALID_VALUE;
        }
    }

    /* --log-level=<debug_level> */
    parm = cli_find_rawparm(instance, NCX_EL_LOGLEVEL, &parmQ);
    if (parm && parm->count) {
        if (parm->count > 1) {
            log_error(instance, "\nError: Only one log-level parameter allowed");
            res = ERR_NCX_DUP_ENTRY;
        } else if (parm->value) {
            loglevel = log_get_debug_level_enum(instance, parm->value);
            if (loglevel == LOG_DEBUG_NONE) {
                log_error(instance,
                          "\nError: '%s' not valid log-level",
                          parm->value);
                res = ERR_NCX_INVALID_VALUE;
            } else {
                log_set_debug_level(instance, loglevel);
            }
        } else {
            log_error(instance, "\nError: no value entered for "
                      "'log-level' parameter");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else {
        log_set_debug_level(instance, dlevel);
    }

    /* --log-append */
    if (res == NO_ERR) {
        parm = cli_find_rawparm(instance, NCX_EL_LOGAPPEND, &parmQ);
        logappend = (parm && parm->count) ? TRUE : FALSE;
        if (parm && parm->value) {
            log_error(instance, "\nError: log-append is empty parameter");
            res = ERR_NCX_INVALID_VALUE;
        }
    }

    /* --log=<logfilespec> */
    if (res == NO_ERR) {
        parm = cli_find_rawparm(instance, NCX_EL_LOG, &parmQ);
        if (parm && parm->count) {
            if (parm->count > 1) {
                log_error(instance, "\nError: Only one 'log' filename allowed");
                res = ERR_NCX_DUP_ENTRY;
            } else if (parm->value) {
                res = NO_ERR;
                logfilename = (char *)
                    ncx_get_source(instance, (const xmlChar *)parm->value, &res);
                if (logfilename) {
                    res = log_open(instance, logfilename, logappend, logtstamps);
                    if (res != NO_ERR) {
                        log_error(instance,
                                  "\nError: open logfile '%s' failed (%s)",
                                  logfilename,
                                  get_error_string(res));
                    }
                    m__free(instance, logfilename);
                }
            } else {
                log_error(instance, "\nError: no value entered for "
                          "'log' parameter");
                res = ERR_NCX_INVALID_VALUE;
            }
        } /* else use default log (stdout) */
    }

    /* --modpath=<pathspeclist> */
    if (res == NO_ERR) {
        parm = cli_find_rawparm(instance, NCX_EL_MODPATH, &parmQ);
        if (parm && parm->count) {
            if (parm->count > 1) {
                log_error(instance, "\nError: Only one 'modpath' parameter allowed");
                res = ERR_NCX_DUP_ENTRY;
            } else if (parm->value) {
                /*** VALIDATE MODPATH FIRST ***/
                ncxmod_set_modpath(instance, (const xmlChar *)parm->value);
            } else {
                log_error(instance, "\nError: no value entered for "
                          "'modpath' parameter");
                res = ERR_NCX_INVALID_VALUE;
            }
        } /* else use default modpath */
    }

    /* --yuma-home=<$YUMA_HOME> */
    if (res == NO_ERR) {
        parm = cli_find_rawparm(instance, NCX_EL_YUMA_HOME, &parmQ);
        if (parm && parm->count) {
            if (parm->count > 1) {
                log_error(instance, "\nError: Only one 'yuma-home' parameter allowed");
                res = ERR_NCX_DUP_ENTRY;
            } else {
                /*** VALIDATE YUMA_HOME ***/
                ncxmod_set_yuma_home(instance, (const xmlChar *)parm->value);
            }
        } /* else use default modpath */
    }

    cli_clean_rawparmQ(instance, &parmQ);

    return res;

} /* bootstrap_cli */


// ----------------------------------------------------------------------------!

/**
 * \fn add_to_modQ
 * \brief 
 * \param mod == module to add to modQ
 * \param modQ == Q of ncx_module_t to use
 * \return none
 */
static void
    add_to_modQ (ncx_instance_t *instance,
                 ncx_module_t *mod,
                 dlq_hdr_t *modQ)
{
    ncx_module_t   *testmod;
    boolean         done;
    int32           retval;

    done = FALSE;

    for (testmod = (ncx_module_t *)dlq_firstEntry(instance, modQ);
         testmod != NULL && !done;
         testmod = (ncx_module_t *)dlq_nextEntry(instance, testmod)) {

        retval = xml_strcmp(instance, mod->name, testmod->name);
        if (retval == 0) {
            retval = yang_compare_revision_dates(instance,
                                                 mod->version,
                                                 testmod->version);
            if (retval == 0) {
                if ((!mod->version && !testmod->version) ||
                    (mod->version && testmod->version)) {
                    /* !!! adding duplicate version !!! */
                    log_info(instance,
                             "\nInfo: Adding duplicate revision '%s' of "
                             "%s module (%s)",
                             (mod->version) ? mod->version : EMPTY_STRING,
                             mod->name,
                             mod->source);
                }
                testmod->defaultrev = FALSE;
                mod->defaultrev = TRUE;
                dlq_insertAhead(instance, mod, testmod);
                done = TRUE;
            } else if (retval > 0) {
                testmod->defaultrev = FALSE;
                mod->defaultrev = TRUE;
                dlq_insertAhead(instance, mod, testmod);
                done = TRUE;
            } else {
                mod->defaultrev = FALSE;
                dlq_insertAfter(instance, mod, testmod);
                done = TRUE;
            }
        } else if (retval < 0) {
            mod->defaultrev = TRUE;
            dlq_insertAhead(instance, mod, testmod);
            done = TRUE;
        } /* else keep going */
    }

    if (!done) {
        mod->defaultrev = TRUE;
        dlq_enque(instance, mod, modQ);
    }
            
}  /* add_to_modQ */

// ----------------------------------------------------------------------------!

/**
 * \fn do_match_rpc_error
 * \brief Generate an error for multiple matches for 1 module
 * \param mod module struct to use
 * \param rpcname partial name string to match
 * \param match TRUE if match allowed, FALSE for exact match only
 * \return none
 */
static void
    do_match_rpc_error (ncx_instance_t *instance,
                        ncx_module_t *mod,
                        const xmlChar *rpcname,
                        boolean match)
{
    obj_template_t *rpc;
    uint32          len;

    len = 0;
    if (match) {
        len = xml_strlen(instance, rpcname);
    }

    for (rpc = (obj_template_t *)dlq_firstEntry(instance, &mod->datadefQ);
         rpc != NULL;
         rpc = (obj_template_t *)dlq_nextEntry(instance, rpc)) {
        if (rpc->objtype != OBJ_TYP_RPC) {
            continue;
        }
        if (match) {
            if (xml_strncmp(instance, obj_get_name(instance, rpc), rpcname, len)) {
                continue;
            }
        } else {
            if (xml_strcmp(instance, obj_get_name(instance, rpc), rpcname)) {
                continue;
            }
        }

        if (mod->version) {
            log_error(instance,
                      "\n    '%s:%s' from module '%s@%s'",
                      ncx_get_mod_xmlprefix(mod),
                      obj_get_name(instance, rpc),
                      obj_get_mod_name(instance, rpc),
                      mod->version);
        } else {
            log_error(instance,
                      "\n    '%s:%s' from module '%s'",
                      ncx_get_mod_xmlprefix(mod),
                      obj_get_name(instance, rpc),
                      obj_get_mod_name(instance, rpc));
        }
    }

}   /* do_match_rpc_error */



/**************    E X T E R N A L   F U N C T I O N S **********/


/**
 * \fn ncx_alloc
 * \brief Allocate NCX instance structure
 * \return The allocated and initialized structure
 */
ncx_instance_t *
    ncx_alloc (void)
{
    ncx_instance_t *instance = calloc(1, sizeof(ncx_instance_t));

    if (instance) {
        instance->basetypes = calloc(NCX_NUM_BASETYPES+1, sizeof(instance->basetypes[0]));
        instance->cfg_arr = calloc(CFG_NUM_STATIC, sizeof(instance->cfg_arr[0]));
        instance->topht = calloc(DR_TOP_HASH_SIZE, sizeof(instance->topht[0]));
        instance->defcxt = calloc(1, sizeof(instance->defcxt[0]));
        instance->error_stack = calloc(MAX_ERR_LEVEL, sizeof(instance->error_stack[0]));
        instance->staterr = calloc(1, sizeof(instance->staterr[0]));
        instance->totals = calloc(1, sizeof(instance->totals[0]));
        instance->xmlns = calloc(XMLNS_MAX_NS, sizeof(instance->xmlns[0]));
        instance->custom = calloc(1, sizeof(instance->custom[0]));
        instance->custom->instance = instance;

        if (   !instance->basetypes
            || !instance->cfg_arr
            || !instance->topht
            || !instance->defcxt
            || !instance->error_stack
            || !instance->staterr
            || !instance->totals
            || !instance->xmlns )
        {
            free(instance->basetypes);
            free(instance->cfg_arr);
            free(instance->topht);
            free(instance->defcxt);
            free(instance->error_stack);
            free(instance->staterr);
            free(instance->totals);
            free(instance->xmlns);
            free(instance->custom);
            free(instance);
            return NULL;
        }
    }

    return instance;
}


/**
 * \fn ncx_init
 * \brief Initialize the NCX module
 * \param savestr TRUE if parsed description strings that are
 *                not needed by the agent at runtime should
 *                be saved anyway.  Converters should use this value.
 *                 
 *                FALSE if uneeded strings should not be saved.
 *                Embedded agents should use this value
 * \param dlevel desired debug output level
 * \param logtstamps TRUE if log should use timestamps
 *                   FALSE if not; not used unless 'log' is present
 * \param startmsg log_debug2 message to print before starting;
 *                 NULL if not used;
 * \param argc CLI argument count for bootstrap CLI
 * \param argv array of CLI parms for bootstrap CLI 
 *             (may be NULL to skip bootstrap CLI parameter parsing)
 * \return status 
 */
status_t 
    ncx_init (ncx_instance_t *instance,
              boolean savestr,
              log_debug_t dlevel,
              boolean logtstamps,
              const char *startmsg,
              int argc,
              char *argv[])
{
    ncx_module_t  *mod;
    status_t       res;
    xmlns_id_t     nsid;
    
    if (instance->ncx_init_done) {
        return NO_ERR;
    }

    instance->malloc_cnt = 0;
    instance->free_cnt = 0;

    status_init(instance);

    instance->save_descr = savestr;
    instance->warn_idlen = NCX_DEF_WARN_IDLEN;
    instance->warn_linelen = NCX_DEF_WARN_LINELEN;
    ncx_feature_init(instance);

    instance->mod_load_callback = NULL;
    log_set_debug_level(instance, dlevel);

    /* create the module and appnode queues */
    dlq_createSQue(instance, &instance->ncx_modQ);
    dlq_createSQue(instance, &instance->deadmodQ);
    instance->ncx_curQ = &instance->ncx_modQ;
    instance->ncx_sesmodQ = NULL;
    dlq_createSQue(instance, &instance->ncx_filptrQ);
    dlq_createSQue(instance, &instance->warnoffQ);

    instance->temp_modQ = NULL;

    instance->display_mode = NCX_DISPLAY_MODE_PREFIX;

    instance->ncx_max_filptrs = NCX_DEF_FILPTR_CACHESIZE;
    instance->ncx_cur_filptrs = 0;

    /* no CLI parameters for these 2 parms yet */
    instance->use_prefix = FALSE;
    instance->cwd_subdirs = FALSE;
    instance->system_sorted = FALSE;

    instance->tracefile = NULL;

#ifdef WITH_TRACEFILE
    instance->tracefile = fopen("tracefile.xml", "w");
#endif

    instance->allow_top_mandatory = TRUE;

    /* check that the correct version of libxml2 is installed */
    LIBXML_TEST_VERSION;

    /* init module library handler */
    res = ncxmod_init(instance);
    if (res != NO_ERR) {
        return res;
    }

    /* deal with bootstrap CLI parms */
    if (argv != NULL) {
        res = bootstrap_cli(instance, argc, argv, dlevel, logtstamps);
        if (res != NO_ERR) {
            return res;
        }
    }

    if (startmsg) {
        log_write(instance, startmsg);
    }

    /* init runstack script support */
    runstack_init(instance);

    /* init top level msg dispatcher */
    top_init(instance);

    /* initialize the definition resistry */
    def_reg_init(instance);

    /* initialize the namespace registry */
    xmlns_init(instance);

    instance->ncx_init_done = TRUE;

    /* Initialize the INVALID namespace to help filter handling */
    res = xmlns_register_ns(instance, 
                            INVALID_URN, 
                            INV_PREFIX, 
                            NCX_MODULE, 
                            NULL, 
                            &nsid);
    if (res != NO_ERR) {
        return res;
    }

    /* Initialize the Wildcard namespace for base:1.1 filter handling */
    res = xmlns_register_ns(instance, 
                            WILDCARD_URN, 
                            WILDCARD_PREFIX, 
                            NCX_MODULE, 
                            NULL, 
                            &nsid);
    if (res != NO_ERR) {
        return res;
    }

    /* Initialize the XML namespace for NETCONF */
    res = xmlns_register_ns(instance, 
                            NC_URN, 
                            NC_PREFIX, 
                            NC_MODULE, 
                            NULL, 
                            &nsid);
    if (res != NO_ERR) {
        return res;
    }

    /* Initialize the XML namespace for YANG */
    res = xmlns_register_ns(instance, 
                            YANG_URN, 
                            YANG_PREFIX, 
                            YANG_MODULE, 
                            NULL, 
                            &nsid);
    if (res != NO_ERR) {
        return res;
    }

    /* Initialize the XML namespace for YIN */
    res = xmlns_register_ns(instance, 
                            YIN_URN, 
                            YIN_PREFIX, 
                            YIN_MODULE, 
                            NULL, 
                            &nsid);
    if (res != NO_ERR) {
        return res;
    }

    /* Initialize the XMLNS namespace for xmlns attributes */
    res = xmlns_register_ns(instance, 
                            NS_URN, 
                            NS_PREFIX, 
                            (const xmlChar *)"W3C XML Namespaces", 
                            NULL, 
                            &nsid);
    if (res != NO_ERR) {
        return res;
    }

    /* Initialize the XSD namespace for ncxdump program */
    res = xmlns_register_ns(instance, 
                            XSD_URN, 
                            XSD_PREFIX, 
                            (const xmlChar *)"W3C XML Schema Definition",
                            NULL, 
                            &nsid);
    if (res != NO_ERR) {
        return res;
    }

    /* Initialize the XSI namespace for ncxdump program */
    res = xmlns_register_ns(instance, 
                            XSI_URN, 
                            XSI_PREFIX, 
                            (const xmlChar *)"W3C XML Schema Instance",
                            NULL, 
                            &nsid);
    if (res != NO_ERR) {
        return res;
    }

    /* Initialize the XML namespace for xml:lang attribute support */
    res = xmlns_register_ns(instance, 
                            XML_URN, 
                            XML_PREFIX,
                            (const xmlChar *)"W3C XML Lang Attribute",
                            NULL, 
                            &nsid);
    if (res != NO_ERR) {
        return res;
    }

    /* Initialize the XML namespace for wd:default attribute support */
    res = xmlns_register_ns(instance, 
                            NC_WD_ATTR_URN, 
                            NC_WD_ATTR_PREFIX,
                            (const xmlChar *)"wd:default Attribute",
                            NULL, 
                            &nsid);
    if (res != NO_ERR) {
        return res;
    }

    /* load the basetypes into the definition registry */
    res = typ_load_basetypes(instance);
    if (res != NO_ERR) {
        return res;
    }

    /* initialize the configuration manager */
    cfg_init(instance);

    /* initialize the session message manager */
    ses_msg_init(instance);

    mod = NULL;
    res = ncxmod_load_module(instance, 
                             NCXMOD_NCX, 
                             NULL, 
                             NULL,
                             &mod);
    if (mod == NULL) {
        return res;
    }

    instance->gen_anyxml = ncx_find_object(instance, mod, NCX_EL_ANY);
    if (!instance->gen_anyxml) {
        return SET_ERROR(instance, ERR_NCX_DEF_NOT_FOUND);
    }

    instance->gen_container = ncx_find_object(instance, mod, NCX_EL_STRUCT);
    if (!instance->gen_container) {
        return SET_ERROR(instance, ERR_NCX_DEF_NOT_FOUND);
    }

    instance->gen_string = ncx_find_object(instance, mod, NCX_EL_STRING);
    if (!instance->gen_string) {
        return SET_ERROR(instance, ERR_NCX_DEF_NOT_FOUND);
    }

    instance->gen_empty = ncx_find_object(instance, mod, NCX_EL_EMPTY);
    if (!instance->gen_empty) {
        return SET_ERROR(instance, ERR_NCX_DEF_NOT_FOUND);
    }

    instance->gen_root = ncx_find_object(instance, mod, NCX_EL_ROOT);
    if (!instance->gen_root) {
        return SET_ERROR(instance, ERR_NCX_DEF_NOT_FOUND);
    }

    instance->gen_binary = ncx_find_object(instance, mod, NCX_EL_BINARY);
    if (!instance->gen_binary) {
        return SET_ERROR(instance, ERR_NCX_DEF_NOT_FOUND);
    }

    return NO_ERR;

}  /* ncx_init */

/**
 * \fn ncx_custom_init
 * \brief Initialize the NCX module custom structure
 * \param yang model  - associated yang model
 * \param logging routine - call back routine for log events
 * \return status 
 */
status_t 
    ncx_custom_init (ncx_instance_t *instance,
              void *yang_model,
              void *logging_context,
              Logging_Routine logging_routine)
{
    if (!instance->ncx_init_done) {
        return ERR_INTERNAL_VAL;
    }
    assert(instance->custom != NULL);
    instance->custom->yang_model = yang_model;
    instance->custom->logging_context = logging_context;
    instance->custom->logging_routine = logging_routine;

    return NO_ERR;

}  /* ncx_init */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_cleanup
 * \brief cleanup NCX module
 * \return none
 */
void
    ncx_cleanup (ncx_instance_t *instance)
{
    ncx_module_t     *mod;
    ncx_filptr_t     *filptr;
    warnoff_t        *warnoff;

    if (!instance->ncx_init_done) {
        return;
    }

    while (!dlq_empty(instance, &instance->ncx_modQ)) {
        mod = (ncx_module_t *)dlq_deque(instance, &instance->ncx_modQ);
        free_module(instance, mod);
    }
    instance->usedeadmodQ = FALSE;
    while (!dlq_empty(instance, &instance->deadmodQ)) {
        mod = (ncx_module_t *)dlq_deque(instance, &instance->deadmodQ);
        free_module(instance, mod);
    }

    while (!dlq_empty(instance, &instance->ncx_filptrQ)) {
        filptr = (ncx_filptr_t *)dlq_deque(instance, &instance->ncx_filptrQ);
        m__free(instance, filptr);
    }

    while (!dlq_empty(instance, &instance->warnoffQ)) {
        warnoff = (warnoff_t *)dlq_deque(instance, &instance->warnoffQ);
        m__free(instance, warnoff);
    }

    instance->gen_anyxml = NULL;
    instance->gen_container = NULL;
    instance->gen_string = NULL;
    instance->gen_empty = NULL;
    instance->gen_root = NULL;
    instance->gen_binary = NULL;

    ncx_feature_cleanup(instance);
    typ_unload_basetypes(instance);
    xmlns_cleanup(instance);
    def_reg_cleanup(instance);
    cfg_cleanup(instance);
    ses_msg_cleanup(instance);
    top_cleanup(instance);
    runstack_cleanup(instance);
    ncxmod_cleanup(instance);
    xmlCleanupParser();
    status_cleanup(instance);

    if (instance->malloc_cnt > instance->free_cnt) {
        log_error(instance, 
                  "\n+++ Warning: memory leak (m:%u f:%u)\n", 
                  instance->malloc_cnt, 
                  instance->free_cnt);
    } else if (instance->malloc_cnt < instance->free_cnt) {
        log_error(instance, 
                  "\n+++ Error: memory corruption (m:%u f:%u)\n", 
                  instance->malloc_cnt, 
                  instance->free_cnt);
    }

    log_close(instance);

    if (instance->tracefile != NULL) {
        fclose(instance->tracefile);
    }

    instance->ncx_init_done = FALSE;


    free(instance->basetypes);
    free(instance->cfg_arr);
    free(instance->topht);
    free(instance->defcxt);
    free(instance->error_stack);
    free(instance->staterr);
    free(instance->totals);
    free(instance->xmlns);
    if (instance->custom) 
    {
      free(instance->custom);
    }
    free(instance);

}   /* ncx_cleanup */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_new_module
 * \brief Malloc and initialize the fields in a ncx_module_t
 * \return pointer to the malloced and initialized struct or NULL if an error
 */
ncx_module_t *
    ncx_new_module (ncx_instance_t *instance)
{
    ncx_module_t  *mod;

    mod = m__getObj(instance, ncx_module_t);
    if (!mod) {
        return NULL;
    }

#ifdef NCX_DEBUG_MOD_MEMORY
    if (LOGDEBUG3) {
        log_debug3(instance, "\n%p:modtrace:newmod", mod);
    }
#endif

    mod->modtag = 1;
    (void)memset(mod, 0x0, sizeof(ncx_module_t));
    mod->langver = 1;
    mod->defaultrev = TRUE;
    dlq_createSQue(instance, &mod->revhistQ);
    dlq_createSQue(instance, &mod->importQ);
    dlq_createSQue(instance, &mod->includeQ);
    dlq_createSQue(instance, &mod->allincQ);
    dlq_createSQue(instance, &mod->incchainQ);
    dlq_createSQue(instance, &mod->typeQ);
    dlq_createSQue(instance, &mod->groupingQ);
    dlq_createSQue(instance, &mod->datadefQ);
    dlq_createSQue(instance, &mod->extensionQ);
    dlq_createSQue(instance, &mod->deviationQ);
    dlq_createSQue(instance, &mod->appinfoQ);
    dlq_createSQue(instance, &mod->typnameQ);
    dlq_createSQue(instance, &mod->saveimpQ);
    dlq_createSQue(instance, &mod->stmtQ);
    dlq_createSQue(instance, &mod->featureQ);
    dlq_createSQue(instance, &mod->identityQ);
    ncx_init_list(instance, &mod->devmodlist, NCX_BT_STRING);
    return mod;

}  /* ncx_new_module */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_module
 * \brief Find an ncx_module_t in the ncx_modQ; These are the modules that are
 * already loaded*
 * \param modname module name
 * \param revision module revision date
 * \return module pointer if found or NULL if not
 */
ncx_module_t *
    ncx_find_module (ncx_instance_t *instance,
                     const xmlChar *modname,
                     const xmlChar *revision)
{
    ncx_module_t *mod;

    assert ( modname && " param modname is NULL" );

    /* check the yangcli session module Q
     * and then the current module Q
     */
    mod = NULL;
    if (instance->ncx_sesmodQ) {
        mod = ncx_find_module_que(instance, instance->ncx_sesmodQ, modname, revision);
    }
    if (mod == NULL) {
        mod = ncx_find_module_que(instance, instance->ncx_curQ, modname, revision);
    }
    return mod;

}   /* ncx_find_module */


// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_module_que
 * \brief Find a ncx_module_t in the specified Q and check the namespace ID
 * \param modQ module Q to search
 * \param modname module name
 * \param revision module revision date 
 * \return module pointer if found or NULL if not
 */
ncx_module_t *
    ncx_find_module_que (ncx_instance_t *instance,
                         dlq_hdr_t *modQ,
                         const xmlChar *modname,
                         const xmlChar *revision)
{
    ncx_module_t  *mod;
    int32          retval;

    assert ( modQ && " param modQ is NULL" );
    assert ( modname && " param modname is NULL" );

    for (mod = (ncx_module_t *)dlq_firstEntry(instance, modQ);
         mod != NULL;
         mod = (ncx_module_t *)dlq_nextEntry(instance, mod)) {

        retval = xml_strcmp(instance, modname, mod->name);
        if (retval == 0) {
            if (!revision || !mod->version) {
                if (mod->defaultrev) {
                    return mod;
                }
            } else {
                retval = yang_compare_revision_dates(instance,
                                                     revision,
                                                     mod->version);
                if (retval == 0) {
                    return mod;
                } else if (retval > 0) {
                    return NULL;
                }
            }
        } else if (retval < 0) {
            return NULL;
        }
    }
    return NULL;

}   /* ncx_find_module_que */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_module_que_nsid
 * \brief Find a ncx_module_t in the specified Q and check the namespace ID
 * \param modQ module Q to search
 * \param nsid xmlns ID to find
 * \return module pointer if found or NULL if not
 */
ncx_module_t *
    ncx_find_module_que_nsid (ncx_instance_t *instance,
                              dlq_hdr_t *modQ,
                              xmlns_id_t nsid)
{
    ncx_module_t  *mod;
    (void)instance;

    assert ( modQ && " param modQ is NULL" );
    assert ( nsid && " param nsid is NULL" );

    for (mod = (ncx_module_t *)dlq_firstEntry(instance, modQ);
         mod != NULL;
         mod = (ncx_module_t *)dlq_nextEntry(instance, mod)) {

        if (mod->nsid == nsid) {
            return mod;
        }
    }
    return NULL;

}   /* ncx_find_module_que_nsid */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_free_module
 * \brief Scrub the memory in a ncx_module_t by freeing all the sub-fields and
 * then freeing the entire struct itself use if module was not added to registry
 * \details MUST remove this struct from the ncx_modQ before calling Does not
 * remove module definitions from the registry
 * Use the ncx_remove_module function if the module was 
 * already successfully added to the modQ and definition registry
 * \param  mod == ncx_module_t data structure to free
 * \return none 
 */
void 
    ncx_free_module (ncx_instance_t *instance, ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL" );

    free_module(instance, mod);

}  /* ncx_free_module */


// ----------------------------------------------------------------------------!

/**
 * \fn ncx_any_mod_errors
 * \brief Check if any of the loaded modules are loaded with non-fatal errors
 * \return TRUE if any modules are loaded with non-fatal errors; FALSE if all
 * modules present have a status of NO_ERR
 */
boolean
    ncx_any_mod_errors (ncx_instance_t *instance)
{
    ncx_module_t  *mod;

    for (mod = (ncx_module_t *)dlq_firstEntry(instance, instance->ncx_curQ);
         mod != NULL;
         mod = (ncx_module_t *)dlq_nextEntry(instance, mod)) {
        if (mod->status != NO_ERR) {
            return TRUE;
        }
    }

    return FALSE;

}  /* ncx_any_mod_errors */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_any_dependency_errors
 * \brief Check if any of the imports that this module relies on were loadeds
 * are loaded with non-fatal errors
 * \param mod module to check 
 * \return TRUE if any modules are loaded with non-fatal errors; FALSE if all
 * modules present have a status of NO_ERR
 */
boolean
    ncx_any_dependency_errors (ncx_instance_t *instance, const ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL" );

    const yang_import_ptr_t *impptr = 
        (const yang_import_ptr_t *)dlq_firstEntry(instance, &mod->saveimpQ);
    for (; impptr != NULL;  impptr = (const yang_import_ptr_t *)
             dlq_nextEntry(instance, impptr)) {

        /*** hack: skip ietf-netconf because it is not stored
         *** remove when ietf-netconf is supported
         ***/
        if (!xml_strcmp(instance, impptr->modname, NCXMOD_IETF_NETCONF)) {
            continue;
        }

        ncx_module_t *testmod = ncx_find_module(instance,
                                                impptr->modname,
                                                impptr->revision);
        if (!testmod) {
            /* missing import */
            return TRUE;
        }
            
        if (testmod->errors) {
            return TRUE;
        }
    }

    return FALSE;

}  /* ncx_any_dependency_errors */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_type
 * \brief Check if a typ_template_t in the mod->typeQ
 * \param mod == ncx_module to check 
 * \param typname type name
 * \param useall TRUE to use all submodules
 *               FALSE to only use the ones in the mod->includeQ
 * \return pointer to struct if present, NULL otherwise
 */
typ_template_t *
    ncx_find_type (ncx_instance_t *instance,
                   ncx_module_t *mod,
                   const xmlChar *typname,
                   boolean useall)
{
    assert ( mod && " param mod is NULL" );
    assert ( typname && " param typname is NULL" );

    typ_template_t *typ;
    yang_node_t    *node;
    ncx_include_t  *inc;
    dlq_hdr_t      *que;

    typ = ncx_find_type_que(instance, &mod->typeQ, typname);
    if (typ) {
        return typ;
    }

    que = ncx_get_allincQ(mod);

    if (useall) {
        for (node = (yang_node_t *)dlq_firstEntry(instance, que);
             node != NULL;
             node = (yang_node_t *)dlq_nextEntry(instance, node)) {

            if (node->submod) {
                typ = ncx_find_type_que(instance, 
                                        &node->submod->typeQ, 
                                        typname);
                if (typ) {
                    return typ;
                }
            }
        }
    } else {
        /* check all the submodules, but only the ones visible
         * to this module or submodule, YANG only
         */
        for (inc = (ncx_include_t *)dlq_firstEntry(instance, &mod->includeQ);
             inc != NULL;
             inc = (ncx_include_t *)dlq_nextEntry(instance, inc)) {

            /* get the real submodule struct */
            if (!inc->submod) {
                node = yang_find_node(instance, 
                                      que, 
                                      inc->submodule,
                                      inc->revision);
                if (node) {
                    inc->submod = node->submod;
                }
                if (!inc->submod) {
                    /* include not found or errors in it */
                    continue;
                }
            }

            /* check the type Q in this submodule */
            typ = ncx_find_type_que(instance, &inc->submod->typeQ, typname);
            if (typ) {
                return typ;
            }
        }
    }

    return NULL;

}   /* ncx_find_type */


/********************************************************************
* FUNCTION ncx_find_type_que
*
* Check if a typ_template_t in the mod->typeQ
*
* INPUTS:
*   que == type Q to check
*   typname == type name
*
* RETURNS:
*  pointer to struct if present, NULL otherwise
*********************************************************************/
typ_template_t *
    ncx_find_type_que (ncx_instance_t *instance,
                       const dlq_hdr_t *typeQ,
                       const xmlChar *typname)
{
    typ_template_t *typ;

    assert ( typeQ && " param typeQ is NULL");
    assert ( typname && " param typname is NULL");

    for (typ = (typ_template_t *)dlq_firstEntry(instance, typeQ);
         typ != NULL;
         typ = (typ_template_t *)dlq_nextEntry(instance, typ)) {
        if (typ->name && !xml_strcmp(instance, typ->name, typname)) {
            return typ;
        }
    }
    return NULL;

}   /* ncx_find_type_que */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_grouping
 * \brief Check if a grp_template_t in the mod->groupingQ
 * \param mod ncx_module to check 
 * \param grpname group name
 * \param useall TRUE to check all existing nodes
 *               FALSE to only use includes visible to this [sub]mod
 * \return pointer to struct if present, NULL otherwise
 */
grp_template_t *
    ncx_find_grouping (ncx_instance_t *instance,
                       ncx_module_t *mod,
                       const xmlChar *grpname,
                       boolean useall)
{
    grp_template_t *grp;
    yang_node_t    *node;
    ncx_include_t  *inc;
    dlq_hdr_t      *que;

    assert ( mod && " param mod is NULL" );
    assert ( grpname && " param grpname is NULL" );

    /* check the main module */
    grp = ncx_find_grouping_que(instance, &mod->groupingQ, grpname);
    if (grp) {
        return grp;
    }

    que = ncx_get_allincQ(mod);

    if (useall) {
        for (node = (yang_node_t *)dlq_firstEntry(instance, que);
             node != NULL;
             node = (yang_node_t *)dlq_nextEntry(instance, node)) {

            if (node->submod) {
                grp = ncx_find_grouping_que(instance, 
                                            &node->submod->groupingQ, 
                                            grpname);
                if (grp) {
                    return grp;
                }
            }
        }
    } else {
        /* check all the submodules, but only the ones visible
         * to this module or submodule, YANG only
         */
        for (inc = (ncx_include_t *)dlq_firstEntry(instance, &mod->includeQ);
             inc != NULL;
             inc = (ncx_include_t *)dlq_nextEntry(instance, inc)) {

            /* get the real submodule struct */
            if (!inc->submod) {
                node = yang_find_node(instance, 
                                      que, 
                                      inc->submodule,
                                      inc->revision);
                if (node) {
                    inc->submod = node->submod;
                }
                if (!inc->submod) {
                    /* include not found or errors in it */
                    continue;
                }
            }

            /* check the grouping Q in this submodule */
            grp = ncx_find_grouping_que(instance, &inc->submod->groupingQ, grpname);
            if (grp) {
                return grp;
            }
        }
    }
    return NULL;

}   /* ncx_find_grouping */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_grouping_que
 * \brief Check if a grp_template_t in the specified Q
 * \param groupingQ Queue of grp_template_t to check
 * \param grpname group name
 * \return pointer to struct if present, NULL otherwise
 */
grp_template_t *
    ncx_find_grouping_que (ncx_instance_t *instance,
                           const dlq_hdr_t *groupingQ,
                           const xmlChar *grpname)
{
    grp_template_t *grp;

    assert ( groupingQ && " param groupingQ is NULL" );
    assert ( grpname && " param grpname is NULL" );

    for (grp = (grp_template_t *)dlq_firstEntry(instance, groupingQ);
         grp != NULL;
         grp = (grp_template_t *)dlq_nextEntry(instance, grp)) {
        if (grp->name && !xml_strcmp(instance, grp->name, grpname)) {
            return grp;
        }
    }
    return NULL;

}   /* ncx_find_grouping_que */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_rpc
 * \brief Check if a rpc_template_t in the mod->rpcQ
 * \param mod ncx_module to check
 * \param rpcname RPC name
 * \return pointer to struct if present, NULL otherwise
 */
obj_template_t *
    ncx_find_rpc (ncx_instance_t *instance,
                  const ncx_module_t *mod,
                  const xmlChar *rpcname)
{
    obj_template_t *rpc;

    assert ( mod && " param mod is NULL" );
    assert ( rpcname && " param rpcname is NULL" );

    for (rpc = (obj_template_t *)dlq_firstEntry(instance, &mod->datadefQ);
         rpc != NULL;
         rpc = (obj_template_t *)dlq_nextEntry(instance, rpc)) {
        if (rpc->objtype == OBJ_TYP_RPC) {
            if (!xml_strcmp(instance, obj_get_name(instance, rpc), rpcname)) {
                return rpc;
            }
        }
    }
    return NULL;

}   /* ncx_find_rpc */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_match_rpc
 * \brief Check if a rpc_template_t is in the mod->rpcQ; Partial match the
 * commmand name
 * \param mod ncx_module to check
 * \param rpcname RPC name to match
 * \param retcount address of return match count; updated with
 * number of matches found
 * \return pointer to struct if present, NULL otherwise
 */
obj_template_t *
    ncx_match_rpc (ncx_instance_t *instance,
                   const ncx_module_t *mod,
                   const xmlChar *rpcname,
                   uint32 *retcount)
{
    obj_template_t *rpc, *firstfound;
    uint32          len, cnt;

    assert ( mod && " param mod is NULL" );
    assert ( rpcname && " param rpcname is NULL" );
    assert ( retcount && " param retcount is NULL" );

    *retcount = 0;
    cnt = 0;
    firstfound = NULL;
    len = xml_strlen(instance, rpcname);

    for (rpc = (obj_template_t *)dlq_firstEntry(instance, &mod->datadefQ);
         rpc != NULL;
         rpc = (obj_template_t *)dlq_nextEntry(instance, rpc)) {
        if (rpc->objtype == OBJ_TYP_RPC) {
            if (!xml_strncmp(instance, obj_get_name(instance, rpc), rpcname, len)) {
                if (firstfound == NULL) {
                    firstfound = rpc;
                }
                cnt++;
            }
        }
    }

    *retcount = cnt;
    return firstfound;

}   /* ncx_match_rpc */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_match_any_rpc
 * \brief Check if a rpc_template_t is in any module that matches the rpc name
 * string
 * \param module module name to check (NULL == check all)
 * \param rpcname RPC name to match
 * \param retcount address of return count of matches; set to
 * number of matches
 * \return pointer to struct if present, NULL otherwise
 */
obj_template_t *
    ncx_match_any_rpc (ncx_instance_t *instance,
                       const xmlChar *module,
                       const xmlChar *rpcname,
                       uint32 *retcount)
{
    obj_template_t *rpc, *firstfound;
    ncx_module_t   *mod;
    uint32          cnt, tempcnt;

    assert ( rpcname && " param rpcname is NULL" );
    assert ( retcount && " param retcount is NULL" );

    firstfound = NULL;
    *retcount = 0;

    if (module) {
        mod = ncx_find_module(instance, module, NULL);
        if (mod) {
            firstfound = ncx_match_rpc(instance, mod, rpcname, retcount);
        }
    } else {
        cnt = 0;
        for (mod = ncx_get_first_module(instance);
             mod != NULL;
             mod =  ncx_get_next_module(instance, mod)) {

            tempcnt = 0;
            rpc = ncx_match_rpc(instance, mod, rpcname, &tempcnt);
            if (rpc) {
                if (firstfound == NULL) {
                    firstfound = rpc;
                }
                cnt += tempcnt;
            }
        }
        *retcount = cnt;
    }

    return firstfound;

}   /* ncx_match_any_rpc */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_match_any_rpc_mod
 * \brief Check if a rpc_template_t is in the specified module
 * \param mod module struct to check
 * \param rpcname RPC name to match
 * \param retcount address of return count of matches
 * \return pointer to struct if present, NULL otherwise
 */
obj_template_t *
    ncx_match_any_rpc_mod (ncx_instance_t *instance,
                           ncx_module_t *mod,
                           const xmlChar *rpcname,
                           uint32 *retcount)
{
    assert ( mod && " param mod is NULL" );
    assert ( rpcname && " param rpcname is NULL" );
    assert ( retcount && " param retcount is NULL" );

    *retcount = 0;

    obj_template_t *firstfound = ncx_match_rpc(instance, mod, rpcname, retcount);

    return firstfound;

}   /* ncx_match_any_rpc_mod */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_match_rpc_error
 * \brief Generate an error for multiple matches
 * \param mod module struct to check (may be NULL)
 * \param modname module name if mod not set
 *                NULL to match all modules
 * \param rpcname RPC name to match
 * \param match TRUE to match partial command names
 *              FALSE for exact match only
 * \param firstmsg TRUE to do the first log_error banner msg
 *                 FALSE to skip this step
 * \return none 
 */
void
    ncx_match_rpc_error (ncx_instance_t *instance,
                         ncx_module_t *mod,
                         const xmlChar *modname,
                         const xmlChar *rpcname,
                         boolean match,
                         boolean firstmsg)
{

    assert ( rpcname && " param rpcname is NULL" );

    if (firstmsg) {
        if (match) {
            log_error(instance,
                      "\nError: Ambiguous partial command name: '%s'",
                      rpcname);
        } else {
            log_error(instance,
                      "\nError: Ambiguous command name: '%s'",
                      rpcname);
        }
    }

    if (mod != NULL) {
        do_match_rpc_error(instance, mod, rpcname, match);
    } else if (modname != NULL) {
        mod = ncx_find_module(instance, modname, NULL);
        if (mod != NULL) {
            do_match_rpc_error(instance, mod, rpcname, match);
        }
    } else {
        for (mod = ncx_get_first_module(instance);
             mod != NULL;
             mod =  ncx_get_next_module(instance, mod)) {
             do_match_rpc_error(instance, mod, rpcname, match);
        }
    }

}   /* ncx_match_rpc_error */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_any_object
 * \brief Check if an obj_template_t in in any module that
 * matches the object name string
 * \param objname object name to match
 * \return pointer to struct if present, NULL otherwise
 */
obj_template_t *
    ncx_find_any_object (ncx_instance_t *instance, const xmlChar *objname)
{
    assert ( objname && " param objname is NULL" );

    obj_template_t *obj = NULL;
    ncx_module_t   *mod = NULL;
    boolean        useses = FALSE;

    if (instance->ncx_sesmodQ != NULL) {
        mod = (ncx_module_t *)dlq_firstEntry(instance, instance->ncx_sesmodQ);
        if (mod != NULL) {
            useses = TRUE;
        }
    }
    if (mod == NULL) {
        mod = ncx_get_first_module(instance);
    }

    for (;
         mod != NULL;
         mod =  ncx_get_next_module(instance, mod)) {

        obj = obj_find_template_top(instance, 
                                    mod, 
                                    ncx_get_modname(mod), 
                                    objname);
        if (obj) {
            return obj;
        }
    }

    if (useses) {
        /* make 1 more loop trying the main moduleQ */
        for (mod = ncx_get_first_module(instance);
             mod != NULL;
             mod = ncx_get_next_module(instance, mod)) {

            obj = obj_find_template_top(instance, 
                                        mod, 
                                        ncx_get_modname(mod), 
                                        objname);
            if (obj) {
                return obj;
            }
        }
    }

    return NULL;

}   /* ncx_find_any_object */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_match_any_object
 * \brief Check if an obj_template_t in in any module that
 * matches the object name string
 * \param objname object name to match
 * \param name_match name match mode enumeration
 * \param alt_names TRUE if alternate names should be checked
 *                  after regular names; FALSE if not
 * \param retres address of return status
 * \return pointer to struct if present, NULL otherwise
 */
obj_template_t *
    ncx_match_any_object (ncx_instance_t *instance,
                          const xmlChar *objname,
                          ncx_name_match_t name_match,
                          boolean alt_names,
                          status_t *retres)
{
    assert ( objname && " param objname is NULL" );

    obj_template_t *obj = NULL;
    ncx_module_t   *mod = NULL;
    boolean        useses = FALSE;

    /* find a queue of modules to use; get first entry */
    if (instance->ncx_sesmodQ != NULL) {
        mod = (ncx_module_t *)dlq_firstEntry(instance, instance->ncx_sesmodQ);
        if (mod != NULL) {
            useses = TRUE;
        }
    }
    if (mod == NULL) {
        mod = ncx_get_first_module(instance);
    }

    /* always check exact match first */
    for (;
         mod != NULL;
         mod =  ncx_get_next_module(instance, mod)) {

        obj = obj_find_template_top_ex(instance, 
                                       mod, 
                                       ncx_get_modname(mod), 
                                       objname,
                                       name_match,
                                       alt_names,
                                       TRUE,     /* dataonly */
                                       retres);
        if (obj) {
            return obj;
        }
        if (*retres == ERR_NCX_MULTIPLE_MATCHES) {
            return NULL;
        }
    }

    if (useses) {
        /* make 1 more loop trying the main moduleQ */
        for (mod = ncx_get_first_module(instance);
             mod != NULL;
             mod = ncx_get_next_module(instance, mod)) {

            obj = obj_find_template_top_ex(instance, 
                                           mod, 
                                           ncx_get_modname(mod), 
                                           objname,
                                           name_match,
                                           alt_names,
                                           TRUE,    /* dataonly */
                                           retres);
            if (obj) {
                return obj;
            }
            if (*retres == ERR_NCX_MULTIPLE_MATCHES) {
                return NULL;
            }
        }
    }

    return NULL;

}   /* ncx_match_any_object */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_any_object_que 
 * \brief Check if an obj_template_t in in any module that
 * matches the object name string
 * \param modQ Q of modules to check
 * \param objname object name to match
 * \return pointer to struct if present, NULL otherwise
 */
obj_template_t *
    ncx_find_any_object_que (ncx_instance_t *instance,
                             dlq_hdr_t *modQ,
                             const xmlChar *objname)
{
    assert ( modQ && " param modQ is NULL" );
    assert ( objname && " param objname is NULL" );

    obj_template_t *obj = NULL;
    ncx_module_t   *mod;

    for (mod = (ncx_module_t *)dlq_firstEntry(instance, modQ);
         mod != NULL;
         mod = (ncx_module_t *)dlq_nextEntry(instance, mod)) {

        obj = obj_find_template_top(instance, 
                                    mod, 
                                    ncx_get_modname(mod), 
                                    objname);
        if (obj) {
            return obj;
        }
    }
    return NULL;

}   /* ncx_find_any_object_que */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_object
 * \brief Find a top level module object
 * \param mod ncx_module to check
 * \param typname type name
 * \return pointer to struct if present, NULL otherwise
 */
obj_template_t *
    ncx_find_object (ncx_instance_t *instance,
                     ncx_module_t *mod,
                     const xmlChar *objname)
{
    assert ( mod && " param mod is NULL" );
    assert ( objname && " param objname is NULL" );

    return obj_find_template_top(instance, mod, mod->name, objname);

}  /* ncx_find_object */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_add_namespace_to_registry
 * \brief Add the namespace and prefix to the registry
 * or retrieve it if already set
 * \param mod module to add to registry
 * \param tempmod TRUE if this is a temporary add mode
 *                FALSE if this is a real registry add
 * \return status of the operation
 */
status_t 
    ncx_add_namespace_to_registry (ncx_instance_t *instance,
                                   ncx_module_t *mod,
                                   boolean tempmod)
{
    assert ( mod && " param mod is NULL" );

    xmlns_t        *ns;
    const xmlChar  *modname;
    status_t        res;
    xmlns_id_t      nsid;
    boolean         isnetconf;

    if (!mod->ismod) {
        return NO_ERR;
    }

    if (mod->ns == NULL) {
        return SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    res = NO_ERR;
    isnetconf = FALSE;

    /* if this is the XSD module, then use the NS ID already registered */
    if (!xml_strcmp(instance, mod->name, NCX_EL_XSD)) {
        mod->nsid = xmlns_xs_id(instance);
    } else if (!xml_strcmp(instance,
                           mod->name,
                           (const xmlChar *)"ietf-netconf")) {
        mod->nsid = xmlns_nc_id(instance);
        isnetconf = TRUE;
    } else {
        mod->nsid = xmlns_find_ns_by_module(instance, mod->name);
    }

    if (!tempmod) {
        /* check module prefix collision */
        nsid = xmlns_find_ns_by_prefix(instance, mod->prefix);
        if (nsid) {
            modname = xmlns_get_module(instance, nsid);
            if (xml_strcmp(instance, mod->name, modname) && !isnetconf) {
                if (ncx_warning_enabled(instance, ERR_NCX_DUP_PREFIX)) {
                    log_warn(instance,
                             "\nWarning: prefix '%s' already in use "
                             "by module '%s'",
                             mod->prefix, 
                             modname);
                    ncx_print_errormsg(instance, NULL, mod, ERR_NCX_DUP_PREFIX);
                } else {
                    ncx_inc_warnings(mod);
                }
                
                /* redo the module xmlprefix, the length is the length of the 
                 * prefix + 6 characters for the suffix value and a NULL */
                uint32 buflen = xml_strlen(instance, mod->prefix) + 6;
                xmlChar *buffer = m__getMem(instance, buflen + 6);
                if (!buffer) {
                    return ERR_INTERNAL_MEM;
                }
                xmlChar* p = buffer;
                p += xml_strcpy(instance, p, mod->prefix);

                /* keep adding numbers to end of prefix until
                 * 1 is unused or run out of numbers
                 */
                uint32 i = 1;
                for ( ; i<10000 && nsid; ++i) {
                    snprintf((char *)p, 6, "%u", i);
                    nsid = xmlns_find_ns_by_prefix(instance, buffer);
                }
                if (nsid) {
                    log_error(instance, "\nError: could not assign module prefix");
                    res = ERR_NCX_OPERATION_FAILED;
                    ncx_print_errormsg(instance, NULL, mod, res);
                    m__free(instance, buffer);
                    return res;
                }

                /* else the current buffer contains an unused prefix */
                mod->xmlprefix = buffer;
            }
        }
    }

    ns = def_reg_find_ns(instance, mod->ns);
    if (ns) {
        if (tempmod) {
            mod->nsid = ns->ns_id;
        } else if (isnetconf) {
            /* ignore the hack corner-case for yuma-netconf */
            ;
        } else if (ns->ns_id == xmlns_xs_id(instance) ||
                   ns->ns_id == xmlns_xsi_id(instance) ||
                   ns->ns_id == xmlns_xml_id(instance)) {
            /* ignore these special XML duplicates used by yangdump */
            ;
        } else if (xml_strcmp(instance, mod->name, ns->ns_module) &&
                   xml_strcmp(instance, ns->ns_module, NCX_MODULE)) {
            /* this NS string already registered to another module */
            log_error(instance,
                      "\nError: Module '%s' registering "
                      "duplicate namespace '%s'\n    "
                      "registered by module '%s'",
                      mod->name, 
                      mod->ns, 
                      ns->ns_module);
            res = ERR_DUP_NS;
        } else {
            /* same owner so okay */
            mod->nsid = ns->ns_id;
        }
    } else if (!isnetconf) {
        res = xmlns_register_ns(instance, 
                                mod->ns, 
                                (mod->xmlprefix) 
                                ? mod->xmlprefix : mod->prefix, 
                                mod->name, 
                                (tempmod) ? NULL : mod, 
                                &mod->nsid);
        if (res != NO_ERR) {
            /* this NS registration failed */
            log_error(instance,
                      "\nncx reg: Module '%s' registering "
                      "namespace '%s' failed (%s)",
                      mod->name, 
                      mod->ns,
                      get_error_string(res));
            return res;
        }
    }

    return res;

}  /* ncx_add_namespace_to_registry */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_add_to_registry
 * \brief Add all the definitions stored in an ncx_module_t to the registry
 * registry This step is deferred to keep the registry stable as possible
 * and only add modules in an all-or-none fashion.
 * \param mod module to add to registry
 * \return status of the operation
 */
status_t 
    ncx_add_to_registry (ncx_instance_t *instance, ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL" );

    if (!mod->ismod) {
        return NO_ERR;
    }

    status_t res = NO_ERR;

    /* check module parse code */
    if (mod->status != NO_ERR) {
        res = mod->status;
        if (NEED_EXIT(res)) {
            /* should not happen */
            log_error(instance, 
                      "\nError: cannot add module '%s' to registry"
                      " with fatal errors", 
                      mod->name);
            ncx_print_errormsg(instance, NULL, mod, res);
            return SET_ERROR(instance, ERR_INTERNAL_VAL);
        } else {
            log_warn(instance, 
                     "\nWarning: Adding module '%s' to registry"
                     " with errors", 
                     mod->name);
            res = NO_ERR;
        }
    }

    res = set_toplevel_defs(instance, mod, mod->nsid);
    if (res != NO_ERR) {
        return res;
    }

    /* add all the submodules included in this module */
    yang_node_t    *node;

    for (node = (yang_node_t *)dlq_firstEntry(instance, &mod->allincQ);
         node != NULL;
         node = (yang_node_t *)dlq_nextEntry(instance, node)) {

        node->submod->nsid = mod->nsid;
        res = set_toplevel_defs(instance, node->submod, mod->nsid);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* add the module itself for fast lookup in imports
     * of other modules
     */
    if (mod->ismod) {
        /* save the module in the module Q */
        add_to_modQ(instance, mod, &instance->ncx_modQ);
        mod->added = TRUE;

        /* !!! hack to cleanup after xmlns init cycle !!!
         * check for netconf.yang or ncx.yang and back-fill
         * all the xmlns entries for those modules with the
         * real module pointer
         */
        if (!xml_strcmp(instance, mod->name, NC_MODULE)) {
            xmlns_set_modptrs(instance, NC_MODULE, mod);
        } else if (!xml_strcmp(instance, mod->name, NCX_MODULE)) {
            xmlns_set_modptrs(instance, NCX_MODULE, mod);
        }

        if (instance->mod_load_callback) {
            (*instance->mod_load_callback)(instance, mod);
        }

    }
    
    return res;

}  /* ncx_add_to_registry */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_add_to_modQ
 * \brief Add module to the current module Q
 * Used by yangdiff to bypass add_to_registry to support
 * N different module trees
 * \param mod module to add to current module Q
 * \return status of the operation
 */
status_t 
    ncx_add_to_modQ (ncx_instance_t *instance, ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL" );

    if (mod->ismod) {
        add_to_modQ(instance, mod, instance->ncx_curQ);
        mod->added = TRUE;
    }
    return NO_ERR;

} /* ncx_add_to_modQ */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_is_duplicate
 * \brief Search the specific module for the specified definition name.
 * This function is for modules in progress which have not been
 * added to the registry yet.
 * \param mod ncx_module_t to check
 * \param defname name of definition to find
 * \return TRUE if found, FALSE otherwise
 */
boolean
    ncx_is_duplicate (ncx_instance_t *instance,
                      ncx_module_t *mod,
                      const xmlChar *defname)
{
    assert ( mod && " param mod is NULL" );
    assert ( defname && " param defname is NULL" );

    if (ncx_find_type(instance, mod, defname, TRUE)) {
        return TRUE;
    }
    if (ncx_find_rpc(instance, mod, defname)) {
        return TRUE;
    }
    return FALSE;

}  /* ncx_is_duplicate */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_first_module
 * \brief Get the first module in the ncx_modQ
 * \return pointer to the first entry or NULL if empty Q
 */
ncx_module_t *
    ncx_get_first_module (ncx_instance_t *instance)
{
    ncx_module_t *mod = (ncx_module_t *)dlq_firstEntry(instance, instance->ncx_curQ);
    while (mod) {
        if (mod->defaultrev) {
            return mod;
        }
        mod = (ncx_module_t *)dlq_nextEntry(instance, mod);
    }
    return mod;

}  /* ncx_get_first_module */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_next_module
 * \brief Get the next module in the ncx_modQ
 * \param mod current module to find next 
 * \return pointer to the first entry or NULL if empty Q
 */
ncx_module_t *
    ncx_get_next_module (ncx_instance_t *instance, const ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL" );
    (void)instance;

    ncx_module_t *nextmod = (ncx_module_t *)dlq_nextEntry(instance, mod);
    while (nextmod) {
        if (nextmod->defaultrev) {
            return nextmod;
        }
        nextmod = (ncx_module_t *)dlq_nextEntry(instance, nextmod);
    }
    return nextmod;

}  /* ncx_get_next_module */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_first_session_module
 * \brief Get the first module in the ncx_sesmodQ
 * \return pointer to the first entry or NULL if empty Q
 */
ncx_module_t *
    ncx_get_first_session_module (ncx_instance_t *instance)
{
    if (instance->ncx_sesmodQ == NULL) {
        return NULL;
    }

    ncx_module_t *mod = (ncx_module_t *)dlq_firstEntry(instance, instance->ncx_sesmodQ);
    return mod;

}  /* ncx_get_first_session_module */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_next_session_module
 * \brief Get the next module in the ncx_sesmodQ
 * \param mod module to get next session 
 * \return pointer to the first entry or NULL if empty Q
 */
ncx_module_t *
    ncx_get_next_session_module (ncx_instance_t *instance, const ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL" );
    (void)instance;

    ncx_module_t *nextmod = (ncx_module_t *)dlq_nextEntry(instance, mod);
    return nextmod;

}  /* ncx_get_next_session_module */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_modname
 * \brief Get the main module name
 * \param mod module or submodule to get main module name
 * \return main module name or NULL if error
 */
const xmlChar *
    ncx_get_modname (const ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL" );
    return (mod->ismod) ? mod->name : mod->belongs;

}  /* ncx_get_modname */

// ----------------------------------------------------------------------------!

/**
 * \fn  ncx_get_mod_nsid
 * \brief Get the main module namespace ID
 * \param mod module or submodule to get main module namespace ID
 * \return namespace id number
 */
xmlns_id_t
    ncx_get_mod_nsid (const ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL" );

    while (mod->parent != NULL) {
        mod = mod->parent;
    }
    return mod->nsid;

}  /* ncx_get_mod_nsid */


// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_modversion
 * \brief Get the [sub]module version
 * \param mod module or submodule to get module version
 * \return module version or NULL if error
 */
const xmlChar *
    ncx_get_modversion (const ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL");
    return mod->version;

}  /* ncx_get_modversion */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_modnamespace
 * \brief Get the module namespace URI
 * \param mod module or submodule to get module namespace
 * \return module namespace or NULL if error
 */
const xmlChar *
    ncx_get_modnamespace (const ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL");
    return mod->ns;

}  /* ncx_get_modnamespace */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_modsource
 * \brief Get the module filespec source string
 * \param mod module or submodule to use
 * \return module filespec source string
 */
const xmlChar *
    ncx_get_modsource (const ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL");
    return mod->source;

}  /* ncx_get_modsource */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_mainmod
 * \brief Get the main module
 * \param mod submodule to get main module
 * \return main module NULL if error
 */
ncx_module_t *
    ncx_get_mainmod (ncx_instance_t *instance, ncx_module_t *mod)
{

    assert ( mod && " param mod is NULL");

    if (mod->ismod) {
        return mod;
    }

    /**** DO NOT KNOW THE REAL MAIN MODULE REVISION ****/
    return ncx_find_module(instance, mod->belongs, NULL);

}  /* ncx_get_mainmod */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_first_object
 * \brief Get the first object in the datadefQs for the specified module
 * Get any object with a name
 * \param mod module to search for the first object
 * \return pointer to the first object or NULL if empty Q
 */
obj_template_t *
    ncx_get_first_object (ncx_instance_t *instance, ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL");

    obj_template_t *obj;
    for (obj = (obj_template_t *)dlq_firstEntry(instance, &mod->datadefQ);
         obj != NULL;
         obj = (obj_template_t *)dlq_nextEntry(instance, obj)) {
        if (!obj_has_name(instance, obj) ||
            !obj_is_enabled(instance, obj) ||
            obj_is_cli(instance, obj) || 
            obj_is_abstract(instance, obj)) {
            continue;
        }
        return obj;
    }

    if (!mod->ismod) {
        return NULL;
    }

    yang_node_t    *node;
    for (node = (yang_node_t *)dlq_firstEntry(instance, &mod->allincQ);
         node != NULL;
         node = (yang_node_t *)dlq_nextEntry(instance, node)) {

        if (!node->submod) {
            SET_ERROR(instance, ERR_INTERNAL_PTR);
            continue;
        }

        for (obj = (obj_template_t *)
                 dlq_firstEntry(instance, &node->submod->datadefQ);
             obj != NULL;
             obj = (obj_template_t *)dlq_nextEntry(instance, obj)) {

            if (!obj_has_name(instance, obj)  || 
                !obj_is_enabled(instance, obj) ||
                obj_is_cli(instance, obj) ||
                obj_is_abstract(instance, obj)) {
                continue;
            }

            return obj;
        }
    }

    return NULL;

}  /* ncx_get_first_object */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_next_object
 * \brief Get the next object in the specified module
 * Get any object with a name
 * \param mod module struct to get the next object from
 * \param curobj pointer to the current object to get the next for
 * \return pointer to the next object or NULL if none
 */
obj_template_t *
    ncx_get_next_object (ncx_instance_t *instance,
                         ncx_module_t *mod,
                         obj_template_t *curobj)
{
    assert ( mod && " param mod is NULL" );
    assert ( curobj && " param curobj is NULL" );

    obj_template_t *obj;
    for (obj = (obj_template_t *)dlq_nextEntry(instance, curobj);
         obj != NULL;
         obj = (obj_template_t *)dlq_nextEntry(instance, obj)) {

        if (!obj_has_name(instance, obj) || 
            !obj_is_enabled(instance, obj) ||
            obj_is_cli(instance, obj) ||
            obj_is_abstract(instance, obj)) {
            continue;
        }

        return obj;
    }

    boolean start = (curobj->tkerr.mod == mod) ? TRUE : FALSE;
    if (!mod->ismod) {
        return NULL;
    }

    yang_node_t    *node;
    for (node = (yang_node_t *)dlq_firstEntry(instance, &mod->allincQ);
         node != NULL;
         node = (yang_node_t *)dlq_nextEntry(instance, node)) {

        if (!node->submod) {
            SET_ERROR(instance, ERR_INTERNAL_PTR);
            continue;
        }

        if (!start) {
            if (node->submod == curobj->tkerr.mod) {
                start = TRUE;
            }
            continue;
        }

        for (obj = (obj_template_t *)
                 dlq_firstEntry(instance, &node->submod->datadefQ);
             obj != NULL;
             obj = (obj_template_t *)dlq_nextEntry(instance, obj)) {

            if (!obj_has_name(instance, obj) || 
                !obj_is_enabled(instance, obj) ||
                obj_is_cli(instance, obj) ||
                obj_is_abstract(instance, obj)) {
                continue;
            }

            return obj;
        }
    }

    return NULL;

}  /* ncx_get_next_object */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_first_data_object
 * \brief Get the first database object in the datadefQs 
 * for the specified module
 * \param mod module to search for the first object 
 * \return pointer to the first object or NULL if empty Q
 */
obj_template_t *
    ncx_get_first_data_object (ncx_instance_t *instance, ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL");

    obj_template_t *obj;
    for (obj = (obj_template_t *)dlq_firstEntry(instance, &mod->datadefQ);
         obj != NULL;
         obj = (obj_template_t *)dlq_nextEntry(instance, obj)) {
        if (!obj_has_name(instance, obj) || 
            !obj_is_enabled(instance, obj) ||
            obj_is_cli(instance, obj) ||
            obj_is_abstract(instance, obj)) {
            continue;
        }
        if (obj_is_data_db(instance, obj)) {
            return obj;
        }
    }

    if (!mod->ismod) {
        return NULL;
    }

    yang_node_t    *node;
    for (node = (yang_node_t *)dlq_firstEntry(instance, &mod->allincQ);
         node != NULL;
         node = (yang_node_t *)dlq_nextEntry(instance, node)) {

        if (!node->submod) {
            SET_ERROR(instance, ERR_INTERNAL_PTR);
            continue;
        }

        for (obj = (obj_template_t *)
                 dlq_firstEntry(instance, &node->submod->datadefQ);
             obj != NULL;
             obj = (obj_template_t *)dlq_nextEntry(instance, obj)) {

            if (!obj_has_name(instance, obj) || 
                !obj_is_enabled(instance, obj) ||
                obj_is_cli(instance, obj) ||
                obj_is_abstract(instance, obj)) {
                continue;
            }

            if (obj_is_data_db(instance, obj)) {
                return obj;
            }
        }
    }

    return NULL;

}  /* ncx_get_first_data_object */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_next_data_object
 * \brief Get the next database object in the specified module
 * \param mod pointer to module to get object from
 * \param curobj pointer to current object to get next from
 * \return pointer to the next object or NULL if none
 */
obj_template_t *
    ncx_get_next_data_object (ncx_instance_t *instance,
                              ncx_module_t *mod,
                              obj_template_t *curobj)
{
    assert ( mod && " param mod is NULL");

    obj_template_t *obj;
    for (obj = (obj_template_t *)dlq_nextEntry(instance, curobj);
         obj != NULL;
         obj = (obj_template_t *)dlq_nextEntry(instance, obj)) {

        if (!obj_has_name(instance, obj) || 
            !obj_is_enabled(instance, obj) ||
            obj_is_cli(instance, obj) ||
            obj_is_abstract(instance, obj)) {
            continue;
        }

        if (obj_is_data_db(instance, obj)) {
            return obj;
        }
    }

    if (!mod->ismod) {
        return NULL;
    }

    boolean start = (curobj->tkerr.mod == mod) ? TRUE : FALSE;
    yang_node_t    *node;
    for (node = (yang_node_t *)dlq_firstEntry(instance, &mod->allincQ);
         node != NULL;
         node = (yang_node_t *)dlq_nextEntry(instance, node)) {

        if (!node->submod) {
            SET_ERROR(instance, ERR_INTERNAL_PTR);
            continue;
        }

        if (!start) {
            if (node->submod == curobj->tkerr.mod) {
                start = TRUE;
            }
            continue;
        }

        for (obj = (obj_template_t *)
                 dlq_firstEntry(instance, &node->submod->datadefQ);
             obj != NULL;
             obj = (obj_template_t *)dlq_nextEntry(instance, obj)) {

            if (!obj_has_name(instance, obj) || 
                !obj_is_enabled(instance, obj) ||
                obj_is_cli(instance, obj) ||
                obj_is_abstract(instance, obj)) {
                continue;
            }

            if (obj_is_data_db(instance, obj)) {
                return obj;
            }
        }
    }

    return NULL;

}  /* ncx_get_next_data_object */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_new_import
 * \brief Malloc and initialize the fields in a ncx_import_t
 * \return pointer to the malloced and initialized struct or NULL if an error
 */
ncx_import_t * 
    ncx_new_import (ncx_instance_t *instance)
{
    ncx_import_t  *import = m__getObj(instance, ncx_import_t);
    if (!import) {
        return NULL;
    }
    (void)memset(import, 0x0, sizeof(ncx_import_t));
    dlq_createSQue(instance, &import->appinfoQ);
    return import;

}  /* ncx_new_import */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_free_import
 * \brief Scrub the memory in a ncx_import_t by freeing all
 * the sub-fields and then freeing the entire struct itself 
 * The struct must be removed from any queue it is in before
 * this function is called.
 * \param import ncx_import_t data structure to free
 * \return none 
 */
void 
    ncx_free_import (ncx_instance_t *instance, ncx_import_t *import)
{
    assert ( import && " param import is NULL");

    if (import->module) {
        m__free(instance, import->module);
    }

    if (import->prefix) {
        m__free(instance, import->prefix);
    }

    if (import->revision) {
        m__free(instance, import->revision);
    }

    /* YANG only */
    ncx_clean_appinfoQ(instance, &import->appinfoQ);

    m__free(instance, import);

}  /* ncx_free_import */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_import
 * \brief Search the importQ for a specified module name
 * \param mod module to search (mod->importQ)
 * \param module module name to find
 * \return pointer to the node if found, NULL if not found
 */
ncx_import_t * 
    ncx_find_import (ncx_instance_t *instance,
                     const ncx_module_t *mod,
                     const xmlChar *module)
{
    assert ( mod && " param mod is NULL");
    assert ( module && " param module is NULL");
    return ncx_find_import_que(instance, &mod->importQ, module);

} /* ncx_find_import */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_import_que
 * \brief Search the specified importQ for a specified module name
 * \param importQ Q of ncx_import_t to search
 * \param module module name to find
 * \return pointer to the node if found, NULL if not found
 */
ncx_import_t * 
    ncx_find_import_que (ncx_instance_t *instance,
                         const dlq_hdr_t *importQ,
                         const xmlChar *module)
{
    assert ( importQ && " param importQ is NULL");
    assert ( module && " param module is NULL");

    ncx_import_t  *import;
    for (import = (ncx_import_t *)dlq_firstEntry(instance, importQ);
         import != NULL;
         import = (ncx_import_t *)dlq_nextEntry(instance, import)) {
        if (!xml_strcmp(instance, import->module, module)) {
            import->used = TRUE;
            return import;
        }
    }
    return NULL;

} /* ncx_find_import_que */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_import_test
 * \brief Search the importQ for a specified module name
* Do not set used flag
 * \param mod module to search (mod->importQ)
 * \param module module name to find
 * \return pointer to the node if found, NULL if not found
 */
ncx_import_t * 
    ncx_find_import_test (ncx_instance_t *instance,
                          const ncx_module_t *mod,
                          const xmlChar *module)
{
    assert ( mod && " param mod is NULL");
    assert ( module && " param module is NULL");

    ncx_import_t  *import;
    for (import = (ncx_import_t *)dlq_firstEntry(instance, &mod->importQ);
         import != NULL;
         import = (ncx_import_t *)dlq_nextEntry(instance, import)) {
        if (!xml_strcmp(instance, import->module, module)) {
            return import;
        }
    }
    return NULL;

} /* ncx_find_import_test */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_pre_import
 * \brief Search the importQ for a specified prefix value
 * \param mod module to search (mod->importQ)
 * \param prefix prefix string to find
 * \return pointer to the node if found, NULL if not found
 */
ncx_import_t * 
    ncx_find_pre_import (ncx_instance_t *instance,
                         const ncx_module_t *mod,
                         const xmlChar *prefix)
{
    assert ( mod && " param mod is NULL");
    assert ( prefix && " param prefix is NULL");
    return ncx_find_pre_import_que(instance, &mod->importQ, prefix);

} /* ncx_find_pre_import */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_pre_import_que
 * \brief Search the specified importQ for a specified prefix value
 * \param importQ Q of ncx_import_t to search
 * \param prefix prefix string to find
 * \return pointer to the node if found, NULL if not found
 */
ncx_import_t * 
    ncx_find_pre_import_que (ncx_instance_t *instance,
                             const dlq_hdr_t *importQ,
                             const xmlChar *prefix)
{
    assert ( importQ && " param importQ is NULL");
    assert ( prefix && " param prefix is NULL");

    ncx_import_t  *import;
    for (import = (ncx_import_t *)dlq_firstEntry(instance, importQ);
         import != NULL;
         import = (ncx_import_t *)dlq_nextEntry(instance, import)) {
        if (import->prefix && !xml_strcmp(instance, import->prefix, prefix)) {
            import->used = TRUE;
            return import;
        }
    }
    return NULL;

} /* ncx_find_pre_import_que */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_pre_import_test
 * \brief Search the importQ for a specified prefix value
* Test only, do not set used flag
 * \param mod module to search (mod->importQ)
 * \param prefix prefix string to find
 * \return pointer to the node if found, NULL if not found
 */
ncx_import_t * 
    ncx_find_pre_import_test (ncx_instance_t *instance,
                              const ncx_module_t *mod,
                              const xmlChar *prefix)
{
    assert ( mod && " param mod is NULL");
    assert ( prefix && " param prefix is NULL");

    ncx_import_t  *import;
    for (import = (ncx_import_t *)dlq_firstEntry(instance, &mod->importQ);
         import != NULL;
         import = (ncx_import_t *)dlq_nextEntry(instance, import)) {
        if (import->prefix && !xml_strcmp(instance, import->prefix, prefix)) {
            return import;
        }
    }
    return NULL;

}  /* ncx_find_pre_import_test */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_locate_modqual_import
 * \brief Search the specific module for the specified definition name.
 * Okay for YANG or NCX
 *  - typ_template_t (NCX_NT_TYP)
 *  - grp_template_t (NCX_NT_GRP)
 *  - obj_template_t (NCX_NT_OBJ)
 *  - rpc_template_t  (NCX_NT_RPC)
 *  - not_template_t (NCX_NT_NOTIF)
 * \param pcb parser control block to use
 * \param imp NCX import struct to use; imp->mod may get set if not already
 * \param defname name of definition to find
 * \param *deftyp specified type or NCX_NT_NONE if any will do; set to type
 * retrieved if NO_ERR
 * \return pointer to the located definition or NULL if not found
 */
void *
    ncx_locate_modqual_import (ncx_instance_t *instance,
                               yang_pcb_t *pcb,
                               ncx_import_t *imp,
                               const xmlChar *defname,
                               ncx_node_t *deftyp)
{
    assert ( imp && " param imp is NULL");
    assert ( defname && " param defname is NULL");
    assert ( deftyp && " param deftyp is NULL");

    void *dptr;
    status_t  res = check_moddef(instance, pcb, imp, defname, deftyp, &dptr);
    return (res==NO_ERR) ? dptr : NULL;
    /*** error res is lost !!! ***/

}  /* ncx_locate_modqual_import */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_new_include
 * \brief Malloc and initialize the fields in a ncx_include_t
 * \return pointer to the malloced and initialized struct or NULL if an error
 */
ncx_include_t * 
    ncx_new_include (ncx_instance_t *instance)
{
    ncx_include_t  *inc = m__getObj(instance, ncx_include_t);
    if (!inc) {
        return NULL;
    }
    (void)memset(inc, 0x0, sizeof(ncx_include_t));
    dlq_createSQue(instance, &inc->appinfoQ);
    return inc;

}  /* ncx_new_include */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_free_include
 * \brief Scrub the memory in a ncx_include_t by freeing all
 * the sub-fields and then freeing the entire struct itself 
 * The struct must be removed from any queue it is in before
 * this function is called.
 * \param inc ncx_include_t data structure to free
 * \return none 
 */
void 
    ncx_free_include (ncx_instance_t *instance, ncx_include_t *inc)
{
    assert ( inc && " param inc is NULL");

    if (inc->submodule) {
        m__free(instance, inc->submodule);
    }

    if (inc->revision) {
        m__free(instance, inc->revision);
    }

    ncx_clean_appinfoQ(instance, &inc->appinfoQ);
    m__free(instance, inc);

}  /* ncx_free_include */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_include
 * \brief Search the includeQ for a specified submodule name
 * \param mod module to search (mod->includeQ)
 * \param submodule submodule name to find
 * \return pointer to the node if found, NULL if not found
 */
ncx_include_t * 
    ncx_find_include (ncx_instance_t *instance,
                      const ncx_module_t *mod,
                      const xmlChar *submodule)
{
    assert ( mod && " param mod is NULL");
    assert ( submodule && " param submodule is NULL");

    ncx_include_t  *inc;
    for (inc = (ncx_include_t *)dlq_firstEntry(instance, &mod->includeQ);
         inc != NULL;
         inc = (ncx_include_t *)dlq_nextEntry(instance, inc)) {
        if (!xml_strcmp(instance, inc->submodule, submodule)) {
            return inc;
        }
    }
    return NULL;

} /* ncx_find_include */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_new_binary
 * \brief Malloc and fill in a new ncx_binary_t struct
 * \return pointer to malloced and initialized ncx_binary_t struct
 *  NULL if malloc error
 */
ncx_binary_t *
    ncx_new_binary (ncx_instance_t *instance)
{
    ncx_binary_t  *binary = m__getObj(instance, ncx_binary_t);
    if (!binary) {
        return NULL;
    }

    ncx_init_binary(binary);
    return binary;

}  /* ncx_new_binary */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_init_binary
 * \brief Init the memory of a ncx_binary_t struct
 * \param binary ncx_binary_t struct to init
 * \return none 
 */
void
    ncx_init_binary (ncx_binary_t *binary)
{
    assert ( binary && " param binary is NULL");
    memset(binary, 0x0, sizeof(ncx_binary_t));

} /* ncx_init_binary */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_clean_binary
 * \brief Scrub the memory of a ncx_binary_t but do not delete it
 * \param binary ncx_binary_t struct to clean
 * \return none
 */
void
    ncx_clean_binary (ncx_instance_t *instance, ncx_binary_t *binary)
{
    assert ( binary && " param binary is NULL");

    if (binary->ustr) {
        m__free(instance, binary->ustr);
    }
    memset(binary, 0x0, sizeof(ncx_binary_t));

} /* ncx_clean_binary */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_free_binary
 * \brief Free all the memory in a  ncx_binary_t struct
 * \param binary struct to clean and free
 * \return none
 */
void
    ncx_free_binary (ncx_instance_t *instance, ncx_binary_t *binary)
{
    assert ( binary && " param binary is NULL");
    ncx_clean_binary(instance, binary);
    m__free(instance, binary);

}  /* ncx_free_binary */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_new_identity
 * \brief Get a new ncx_identity_t struct
 * \return pointer to a malloced ncx_identity_t struct,
 * or NULL if malloc error
 */
ncx_identity_t *
    ncx_new_identity (ncx_instance_t *instance)
{
    ncx_identity_t *identity = m__getObj(instance, ncx_identity_t);
    if (!identity) {
        return NULL;
    }
    memset(identity, 0x0, sizeof(ncx_identity_t));

    dlq_createSQue(instance, &identity->childQ);
    dlq_createSQue(instance, &identity->appinfoQ);
    identity->idlink.identity = identity;
    return identity;

} /* ncx_new_identity */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_free_identity
 * \brief Free a malloced ncx_identity_t struct
 * \param identity struct to free
 * \return none
 */
void 
    ncx_free_identity (ncx_instance_t *instance, ncx_identity_t *identity)
{
    assert ( identity && " param identity is NULL");

    /*** !!! ignoring the back-ptr Q threading the
     *** !!! idlink headers; do not delete from system
     *** !!! until some way to clear all or issue an error
     *** !!! is done.  Assume this free is part of the 
     *** !!! system cleanup for now
     ***
     *** !!! Clearing out the back-ptrs in case the heap 'free'
     *** !!! function does not set these fields to garbage
     ***/
    identity->idlink.inq = FALSE;
    identity->idlink.identity = NULL;

    if (identity->name) {
        m__free(instance, identity->name);
    }

    if (identity->baseprefix) {
        m__free(instance, identity->baseprefix);
    }

    if (identity->basename) {
        m__free(instance, identity->basename);
    }

    if (identity->descr) {
        m__free(instance, identity->descr);
    }

    if (identity->ref) {
        m__free(instance, identity->ref);
    }

    ncx_clean_appinfoQ(instance, &identity->appinfoQ);

    m__free(instance, identity);
    
} /* ncx_free_identity */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_identity
 * \brief Find a ncx_identity_t struct in the module and perhaps
 * any of its submodules
 * \param mod module to search
 * \param name identity name to find
 * \param useall TRUE if all submodules should be checked
 *               FALSE if only visible included submodules
 *               should be checked
 * \return pointer to found feature or NULL if not found
 */
ncx_identity_t *
    ncx_find_identity (ncx_instance_t *instance,
                       ncx_module_t *mod,
                       const xmlChar *name,
                       boolean useall)
{
    assert ( mod && " param mod is NULL");
    assert ( name && " param name NULL");

    ncx_identity_t *identity = ncx_find_identity_que(instance, &mod->identityQ, name);
    if (identity) {
        return identity;
    }

    dlq_hdr_t *que = ncx_get_allincQ(mod);

    yang_node_t     *node;
    ncx_include_t   *inc;
    if (useall) {
        for (node = (yang_node_t *)dlq_firstEntry(instance, que);
             node != NULL;
             node = (yang_node_t *)dlq_nextEntry(instance, node)) {

            if (node->submod) {
                /* check the identity Q in this submodule */
                identity = ncx_find_identity_que(instance, 
                                                 &node->submod->identityQ, 
                                                 name);
                if (identity) {
                    return identity;
                }
            }
        }
    } else {
        /* check all the submodules, but only the ones visible
         * to this module or submodule
         */
        for (inc = (ncx_include_t *)dlq_firstEntry(instance, &mod->includeQ);
             inc != NULL;
             inc = (ncx_include_t *)dlq_nextEntry(instance, inc)) {

            /* get the real submodule struct */
            if (!inc->submod) {
                node = yang_find_node(instance, 
                                      que, 
                                      inc->submodule,
                                      inc->revision);
                if (node) {
                    inc->submod = node->submod;
                }
                if (!inc->submod) {
                    /* include not found or errors in it */
                    continue;
                }
            }

            /* check the identity Q in this submodule */
            identity = ncx_find_identity_que(instance, &inc->submod->identityQ, name);
            if (identity) {
                return identity;
            }
        }
    }
    return NULL;

} /* ncx_find_identity */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_identity_que
 * \brief Find a ncx_identity_t struct in the specified Q
 * \param identityQ Q of ncx_identity_t to search
 * \param name identity name to find
 * \return pointer to found identity or NULL if not found
 */
ncx_identity_t *
    ncx_find_identity_que (ncx_instance_t *instance,
                           dlq_hdr_t *identityQ,
                           const xmlChar *name)
{
    assert ( identityQ && " param identityQ is NULL");
    assert ( name && " param name is NULL");

    ncx_identity_t *identity;
    for (identity = (ncx_identity_t *)dlq_firstEntry(instance, identityQ);
         identity != NULL;
         identity = (ncx_identity_t *)dlq_nextEntry(instance, identity)) {

        if (!xml_strcmp(instance, identity->name, name)) {
            return identity;
        }
    }
    return NULL;
         
} /* ncx_find_identity_que */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_new_filptr
 * \brief Get a new ncx_filptr_t struct
 * \return pointer to a malloced or cached ncx_filptr_t struct,
 * or NULL if none available
 */
ncx_filptr_t *
    ncx_new_filptr (ncx_instance_t *instance)
{
    ncx_filptr_t *filptr;

    /* check the cache first */
    if (instance->ncx_cur_filptrs) {
        filptr = (ncx_filptr_t *)dlq_deque(instance, &instance->ncx_filptrQ);
        instance->ncx_cur_filptrs--;
        return filptr;
    }
    
    /* create a new one */
    filptr = m__getObj(instance, ncx_filptr_t);
    if (!filptr) {
        return NULL;
    }
    memset (filptr, 0x0, sizeof(ncx_filptr_t));
    dlq_createSQue(instance, &filptr->childQ);
    return filptr;

} /* ncx_new_filptr */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_free_filptr
 * \brief Free a new ncx_filptr_t struct or add to the cache if room
 * \param filptr struct to free
 * \return none
 */
void 
    ncx_free_filptr (ncx_instance_t *instance, ncx_filptr_t *filptr)
{
    assert ( filptr && " param filptr is NULL");
 
    /* recursively clean out the child Queues */
    ncx_filptr_t *fp;
    while (!dlq_empty(instance, &filptr->childQ)) {
        fp = (ncx_filptr_t *)dlq_deque(instance, &filptr->childQ);
        ncx_free_filptr(instance, fp);
    }

    /* check if this entry should be put in the cache */
    if (instance->ncx_cur_filptrs < instance->ncx_max_filptrs) {
        memset(filptr, 0x0, sizeof(ncx_filptr_t));
        dlq_createSQue(instance, &filptr->childQ);
        dlq_enque(instance, filptr, &instance->ncx_filptrQ);
        instance->ncx_cur_filptrs++;
    } else {
        /* cache full, so just delete this entry */
        m__free(instance, filptr);
    }
    
} /* ncx_free_filptr */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_new_revhist
 * \brief Create a revision history entry
 * \return malloced revision history entry or NULL if malloc error
 */
ncx_revhist_t *
    ncx_new_revhist (ncx_instance_t *instance)
{
    ncx_revhist_t *revhist = m__getObj(instance, ncx_revhist_t);
    if (!revhist) {
        return NULL;
    }
    memset(revhist, 0x0, sizeof(ncx_revhist_t));
    return revhist;

}  /* ncx_new_revhist */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_free_revhist
 * \brief Free a revision history entry
 * \param revhist ncx_revhist_t data structure to free
 * \return none
 */
void 
    ncx_free_revhist (ncx_instance_t *instance, ncx_revhist_t *revhist)
{
    assert ( revhist && " param revhist is NULL");

    if (revhist->version) {
        m__free(instance, revhist->version);
    }
    if (revhist->descr) {
        m__free(instance, revhist->descr);
    }
    if (revhist->ref) {
        m__free(instance, revhist->ref);
    }
    m__free(instance, revhist);

}  /* ncx_free_revhist */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_revhist
 * \brief Search the revhistQ for a specified revision
 * \param mod module to search (mod->importQ)
 * \param ver version string to find
 * \return pointer to the node if found, NULL if not found
 */
ncx_revhist_t * 
    ncx_find_revhist (ncx_instance_t *instance,
                      const ncx_module_t *mod,
                      const xmlChar *ver)
{
    assert ( mod && " param mod is NULL");
    assert ( ver && " param ver is NULL");

    ncx_revhist_t  *revhist;
    for (revhist = (ncx_revhist_t *)dlq_firstEntry(instance, &mod->revhistQ);
         revhist != NULL;
         revhist = (ncx_revhist_t *)dlq_nextEntry(instance, revhist)) {
        if (!xml_strcmp(instance, revhist->version, ver)) {
            return revhist;
        }
    }
    return NULL;

} /* ncx_find_revhist */


/********************** ncx_enum_t *********************/

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_init_enum
 * \brief Init the memory of a ncx_enum_t
 * \param enu ncx_enum_t struct to init
 * \return none
 */
void
    ncx_init_enum (ncx_enum_t *enu)
{
    assert ( enu && " param enu is NULL" );
    enu->name = NULL;
    enu->dname = NULL;
    enu->val = 0;

} /* ncx_init_enum */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_clean_enum
 * \brief Scrub the memory of a ncx_enum_t but do not delete it
 * \param enu ncx_enum_t struct to clean
 * \return none
 */
void
    ncx_clean_enum (ncx_instance_t *instance, ncx_enum_t *enu)
{
    assert ( enu && " param enu is NULL" );

    enu->name = NULL;
    if (enu->dname) {
        m__free(instance, enu->dname);
        enu->dname = NULL;
    }
    enu->val = 0;

} /* ncx_clean_enum */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_compare_enums
 * \brief Compare 2 enum values
 * \param enu1 first  ncx_enum_t check
 * \param enu2 second ncx_enum_t check
 * \return-1 if enu1 is < enu2 
 *         0 if enu1 == enu2
 *         1 if enu1 is > enu2
 */
int32
    ncx_compare_enums (ncx_instance_t *instance,
                       const ncx_enum_t *enu1,
                       const ncx_enum_t *enu2)
{
    assert ( enu1 && " param enu1 is NULL" );
    assert ( enu2 && " param enu2 is NULL" );
    /* just check strings exact match */
    return xml_strcmp(instance, enu1->name, enu2->name);

} /* ncx_compare_enums */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_set_enum
 * \brief Parse an enumerated integer string into an ncx_enum_t
 * without matching it against any typdef
 * Mallocs a copy of the enum name, using the enu->dname field
 * \param enumval enum string value to parse
 * \param retenu pointer to return enum variable to fill in 
 * \return status 
 */
status_t
    ncx_set_enum (ncx_instance_t *instance,
                  const xmlChar *enumval,
                  ncx_enum_t *retenu)
{
    assert ( enumval && " param enumval is NULL" );
    assert ( retenu && " param retenu is NULL" );

    xmlChar       *str = xml_strdup(instance, enumval);
    if (!str) {
        return ERR_INTERNAL_MEM;
    }

    if (retenu->dname != NULL) {
        m__free(instance, retenu->dname);
    }

    retenu->dname = str;
    retenu->name = str;
    retenu->val = 0;   /***  NOT USED ***/

    return NO_ERR;

} /* ncx_set_enum */


/********************** ncx_bit_t *********************/

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_init_bit
 * \brief Init the memory of a ncx_bit_t
 * \param bit ncx_bit_t struct to init
 * \return none
 */
void
    ncx_init_bit (ncx_bit_t *bit)
{
    assert ( bit && " param bit is NULL" );
    bit->name = NULL;
    bit->dname = NULL;
    bit->pos = 0;

} /* ncx_init_bit */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_clean_bit
 * \brief Scrub the memory of a ncx_bit_t but do not delete it
 * \param bit ncx_bit_t struct to clean
 * \return none
 */
void
    ncx_clean_bit (ncx_instance_t *instance, ncx_bit_t *bit)
{
    assert ( bit && " param bit is NULL" );

    if (bit->dname) {
        m__free(instance, bit->dname);
        bit->dname = NULL;
    }
    bit->pos = 0;
    bit->name = NULL;

} /* ncx_clean_bit */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_compare_bits
 * \brief Compare 2 bit values by their schema order position
 * \param bitone first ncx_bit_t check
 * \param bittwo second ncx_bit_t check
 * \return -1 if bitone is < bittwo
 *          0 if bitone == bittwo
 *          1 if bitone is > bittwo
 */
int32
    ncx_compare_bits (const ncx_bit_t *bitone,
                      const ncx_bit_t *bittwo)
{
    assert ( bitone && " param bitone is NULL" );
    assert ( bittwo && " param bittwo is NULL" );

    if (bitone->pos < bittwo->pos) {
        return -1;
    } else if (bitone->pos > bittwo->pos) {
        return 1;
    } else {
        return 0;
    }
    /*NOTREACHED*/

} /* ncx_compare_bits */


/********************** ncx_typname_t *********************/

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_new_typname
 * \brief Malloc and init a typname struct
 * \return malloced struct or NULL if memory error
 */
ncx_typname_t *
    ncx_new_typname (ncx_instance_t *instance)
{
    ncx_typname_t  *tn = m__getObj(instance, ncx_typname_t);
    if (!tn) {
        return NULL;
    }
    memset(tn, 0x0, sizeof(ncx_typname_t));
    return tn;

} /* ncx_new_typname */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_free_typname
 * \brief Free a typname struct
 * \param typnam ncx_typname_t struct to free
 * \return none
 */
void
    ncx_free_typname (ncx_instance_t *instance, ncx_typname_t *typnam)
{
    assert ( typnam && " param typnam is NULL" );

    if (typnam->typname_malloc) {
        m__free(instance, typnam->typname_malloc);
    }
    m__free(instance, typnam);

} /* ncx_free_typname */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_typname
 * \brief Find a typname struct in the specified Q for a typ pointer
 * \param que Q of ncx_typname_t struct to check
 * \param typ matching type template to find
 * \return name assigned to this type template
 */
const xmlChar *
    ncx_find_typname (ncx_instance_t *instance,
                      const typ_template_t *typ,
                      const dlq_hdr_t *que)
{
    assert ( typ && " param typ is NULL" );
    assert ( que && " param que is NULL" );
    (void)instance;

    const ncx_typname_t  *tn;

    for (tn = (const ncx_typname_t *)dlq_firstEntry(instance, que);
         tn != NULL;
         tn = (const ncx_typname_t *)dlq_nextEntry(instance, tn)) {
        if (tn->typ == typ) {
            return tn->typname;
        }
    }
    return NULL;

} /* ncx_find_typname */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_find_typname_type
 * \brief Find a typ_template_t pointer in a typename mapping, 
 * in the specified Q
 * \param que Q of ncx_typname_t struct to check
 * \param typname matching type name to find
 * \return pointer to the stored typstatus
 */
const typ_template_t *
    ncx_find_typname_type (ncx_instance_t *instance,
                           const dlq_hdr_t *que,
                           const xmlChar *typname)
{
    assert ( que && " param que is NULL" );
    assert ( typname && " param typname is NULL" );

    const ncx_typname_t  *tn;

    for (tn = (const ncx_typname_t *)dlq_firstEntry(instance, que);
         tn != NULL;
         tn = (const ncx_typname_t *)dlq_nextEntry(instance, tn)) {
        if (!xml_strcmp(instance, tn->typname, typname)) {
            return tn->typ;
        }
    }
    return NULL;

}  /* ncx_find_typname_type */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_clean_typnameQ
 * \brief Delete all the Q entries, of typname mapping structs
 * \param que Q of ncx_typname_t struct to delete
 * \return none
 */
void
    ncx_clean_typnameQ (ncx_instance_t *instance, dlq_hdr_t *que)
{
    assert ( que && " param que is NULL" );

    ncx_typname_t  *tn;

    while (!dlq_empty(instance, que)) {
        tn = (ncx_typname_t *)dlq_deque(instance, que);
        ncx_free_typname(instance, tn);
    }

}  /* ncx_clean_typnameQ */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_gen_anyxml
 * \brief Get the object template for the NCX generic anyxml container
 * \return pointer to generic anyxml object template
 */
obj_template_t *
    ncx_get_gen_anyxml (ncx_instance_t *instance)
{
    return instance->gen_anyxml;

} /* ncx_get_gen_anyxml */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_gen_container
 * \brief Get the object template for the NCX generic container
 * \return pointer to generic container object template
 */
obj_template_t *
    ncx_get_gen_container (ncx_instance_t *instance)
{
    return instance->gen_container;

} /* ncx_get_gen_container */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_gen_string
 * \brief Get the object template for the NCX generic string leaf
 * \return pointer to generic string object template
 */
obj_template_t *
    ncx_get_gen_string (ncx_instance_t *instance)
{
    return instance->gen_string;

} /* ncx_get_gen_string */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_gen_empty
 * \brief Get the object template for the NCX generic empty leaf
 * \return pointer to generic empty object template
 */
obj_template_t *
    ncx_get_gen_empty (ncx_instance_t *instance)
{
    return instance->gen_empty;

} /* ncx_get_gen_empty */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_gen_root
 * \brief Get the object template for the NCX generic root container
 * \return pointer to generic root container object template
 */
obj_template_t *
    ncx_get_gen_root (ncx_instance_t *instance)
{
    return instance->gen_root;

} /* ncx_get_gen_root */


// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_gen_binary
 * \brief Get the object template for the NCX generic binary leaf
 * \return pointer to generic binary object template
 */
obj_template_t *
    ncx_get_gen_binary (ncx_instance_t *instance)
{
    return instance->gen_binary;

} /* ncx_get_gen_binary */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_layer
 * \brief Translate ncx_layer_t enum to a string
 * Get the ncx_layer_t string
 * \param layer ncx_layer_t to convert to a string
 * \return const pointer to the string value
 */
const xmlChar *
    ncx_get_layer (ncx_instance_t *instance, ncx_layer_t  layer)
{
    switch (layer) {
    case NCX_LAYER_NONE:
        return (const xmlChar *)"none";
    case NCX_LAYER_TRANSPORT:
        return (const xmlChar *)"transport";
    case NCX_LAYER_RPC:
        return (const xmlChar *)"rpc";
    case NCX_LAYER_OPERATION:
        return (const xmlChar *)"protocol";
    case NCX_LAYER_CONTENT:
        return (const xmlChar *)"application";
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return (const xmlChar *)"--";
    }
}  /* ncx_get_layer */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_name_segment
 * \brief Get the name string between the dots
 * \param str scoped string
 * \param buff address of return buffer
 * \param buffsize buffer size
 * \return current string pointer after operation
 */
const xmlChar *
    ncx_get_name_segment (ncx_instance_t *instance,
                          const xmlChar *str,
                          xmlChar  *buff,
                          uint32 buffsize)
{

    assert ( str && " param str is NULL" );
    assert ( buff && " param buff is NULL" );

    const xmlChar *teststr = str;
    while (*teststr && *teststr != NCX_SCOPE_CH) {
        teststr++;
    }

    if ((uint32)(teststr - str) >= buffsize) {
        SET_ERROR(instance, ERR_BUFF_OVFL);
        return NULL;
    }

    while (*str && *str != NCX_SCOPE_CH) {
        *buff++ = *str++;
    }
    *buff = 0;
    return str;

} /* ncx_get_name_segment */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_cvttyp_enum
 * \brief Get the enum for the string name of a ncx_cvttyp_t enum
 * \param str string name of the enum value 
 * \return enum value
 */
ncx_cvttyp_t
    ncx_get_cvttyp_enum (ncx_instance_t *instance, const char *str)
{
    assert ( str && " param str is NULL" );

    if (!xml_strcmp(instance, NCX_EL_XSD, (const xmlChar *)str)) {
        return NCX_CVTTYP_XSD;
    } else if (!xml_strcmp(instance, NCX_EL_SQL, (const xmlChar *)str)) {
        return NCX_CVTTYP_SQL;
    } else if (!xml_strcmp(instance, NCX_EL_SQLDB, (const xmlChar *)str)) {
        return NCX_CVTTYP_SQLDB;
    } else if (!xml_strcmp(instance, NCX_EL_HTML, (const xmlChar *)str)) {
        return NCX_CVTTYP_HTML;
    } else if (!xml_strcmp(instance, NCX_EL_H, (const xmlChar *)str)) {
        return NCX_CVTTYP_H;
    } else if (!xml_strcmp(instance, NCX_EL_C, (const xmlChar *)str)) {
        return NCX_CVTTYP_C;
    } else if (!xml_strcmp(instance, NCX_EL_CPP_TEST, (const xmlChar *)str)) {
        return NCX_CVTTYP_CPP_TEST;
    } else if (!xml_strcmp(instance, NCX_EL_YANG, (const xmlChar *)str)) {
        return NCX_CVTTYP_YANG;
    } else if (!xml_strcmp(instance, NCX_EL_COPY, (const xmlChar *)str)) {
        return NCX_CVTTYP_COPY;
    } else if (!xml_strcmp(instance, NCX_EL_YIN, (const xmlChar *)str)) {
        return NCX_CVTTYP_YIN;
    } else if (!xml_strcmp(instance, NCX_EL_TG2, (const xmlChar *)str)) {
        return NCX_CVTTYP_TG2;
    } else if (!xml_strcmp(instance, NCX_EL_UC, (const xmlChar *)str)) {
        return NCX_CVTTYP_UC;
    } else if (!xml_strcmp(instance, NCX_EL_UH, (const xmlChar *)str)) {
        return NCX_CVTTYP_UH;
    } else if (!xml_strcmp(instance, NCX_EL_YC, (const xmlChar *)str)) {
        return NCX_CVTTYP_YC;
    } else if (!xml_strcmp(instance, NCX_EL_YH, (const xmlChar *)str)) {
        return NCX_CVTTYP_YH;
    } else {
        return NCX_CVTTYP_NONE;
    }
    /*NOTREACHED*/

}  /* ncx_get_cvttype_enum */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_status_enum
 * \brief Get the enum for the string name of a ncx_status_t enum
 * \param str string name of the enum value 
 * \return enum value
 */
ncx_status_t
    ncx_get_status_enum (ncx_instance_t *instance, const xmlChar *str)
{
    assert ( str && " param str is NULL" );

    if (!xml_strcmp(instance, NCX_EL_CURRENT, str)) {
        return NCX_STATUS_CURRENT;
    } else if (!xml_strcmp(instance, NCX_EL_DEPRECATED, str)) {
        return NCX_STATUS_DEPRECATED;
    } else if (!xml_strcmp(instance, NCX_EL_OBSOLETE, str)) {
        return NCX_STATUS_OBSOLETE;
    } else {
        return NCX_STATUS_NONE;
    }
    /*NOTREACHED*/

}  /* ncx_get_status_enum */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_status_string
 * \brief Get the string for the enum value of a ncx_status_t enum
 * \param status enum value
 * \return string name of the enum value 
 */
const xmlChar *
    ncx_get_status_string (ncx_instance_t *instance, ncx_status_t status)
{
    switch (status) {
    case NCX_STATUS_CURRENT:
    case NCX_STATUS_NONE:
        return NCX_EL_CURRENT;
    case NCX_STATUS_DEPRECATED:
        return NCX_EL_DEPRECATED;
    case NCX_STATUS_OBSOLETE:
        return NCX_EL_OBSOLETE;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return (const xmlChar *)"none";
    }
    /*NOTREACHED*/

}  /* ncx_get_status_string */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_check_yang_status
 * \brief Check the backward compatibility of the 2 YANG status fields
 * \param mystatus enum value for the node to be tested
 * \param depstatus status value of the dependency
 * \return status of the operation
 */
status_t
    ncx_check_yang_status (ncx_instance_t *instance,
                           ncx_status_t mystatus,
                           ncx_status_t depstatus)
{
    switch (mystatus) {
    case NCX_STATUS_CURRENT:
        /* current definition can use another
         * current definition 
         */
        switch (depstatus) {
        case NCX_STATUS_CURRENT:
            return NO_ERR;
        case NCX_STATUS_DEPRECATED:
            return ERR_NCX_USING_DEPRECATED;
        case NCX_STATUS_OBSOLETE:
            return ERR_NCX_USING_OBSOLETE;
        default:
            return SET_ERROR(instance, ERR_INTERNAL_VAL);
        }
        /*NOTRECHED*/
    case NCX_STATUS_DEPRECATED:
        /* deprecated definition can use anything but an 
         * an obsolete definition
         */
        switch (depstatus) {
        case NCX_STATUS_CURRENT:
        case NCX_STATUS_DEPRECATED:
            return NO_ERR;
        case NCX_STATUS_OBSOLETE:
            return ERR_NCX_USING_OBSOLETE;
        default:
            return SET_ERROR(instance, ERR_INTERNAL_VAL);
        }
        /*NOTREACHED*/
    case NCX_STATUS_OBSOLETE:
        /* obsolete definition can use any definition */
        return NO_ERR;
    case NCX_STATUS_NONE:
    default:
        return SET_ERROR(instance, ERR_INTERNAL_VAL);
    }
    /*NOTREACHED*/

}  /* ncx_check_yang_status */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_save_descr
 * \brief Get the value of the save description strings variable
 * \return TRUE descriptive strings should be saved
 *         FALSE descriptive strings should not be saved
 */
boolean 
    ncx_save_descr (ncx_instance_t *instance)
{
    return instance->save_descr;
}  /* ncx_save_descr */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_print_errormsg
 * \brief Print an parse error message to STDOUT
 * \param tkc token chain   (may be NULL)
 * \param mod module in progress  (may be NULL)
 * \param res error status
 * \return none
 */
void
    ncx_print_errormsg (ncx_instance_t *instance,
                        tk_chain_t *tkc,
                        ncx_module_t  *mod,
                        status_t     res)
{
    ncx_print_errormsg_ex(instance, tkc, mod, res, NULL, 0, TRUE);

} /* ncx_print_errormsg */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_print_errormsg_ex
 * \brief Print an parse error message to STDOUT (Extended)
 * \param tkc token chain (may be NULL)
 * \param mod module in progress (may be NULL)
 * \param res error status
 * \param filename script finespec
 * \param linenum script file number
 * \param fineoln TRUE if finish with a newline, FALSE if not
 * \return none
 */
void
    ncx_print_errormsg_ex (ncx_instance_t *instance,
                           tk_chain_t *tkc,
                           ncx_module_t  *mod,
                           status_t     res,
                           const char *filename,
                           uint32 linenum,
                           boolean fineoln)
{
    if (res == NO_ERR) {
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return;
    }

    boolean iserr = (res <= ERR_LAST_USR_ERR) ? TRUE : FALSE;

    if (!iserr && !ncx_warning_enabled(instance, res)) {
         log_debug3(instance, "\nSuppressed warning %d (%s.%u)", res, 
               get_error_string(res),
               ( mod ? (const xmlChar*)mod->name : (const xmlChar*)"UNKNOWN" ),
               linenum);
        return;
    }

    if (mod) {
        if (iserr) {
            mod->errors++;
        } else {
            mod->warnings++;
        }
    }

    if (iserr) {
        if (!LOGERROR) {
            /* errors turned off by the user! */
            return;
        }
    } else if (!LOGWARN) {
        /* warnings turned off by the user */
        return;
    }

    if (tkc && tkc->curerr && tkc->curerr->mod) {
        log_write(instance, "\n%s:", (tkc->curerr->mod->sourcefn) ? 
                  (const char *)tkc->curerr->mod->sourcefn : "--");
    } else if (mod && mod->sourcefn) {
        log_write(instance, "\n%s:", (mod->sourcefn) ? 
                  (const char *)mod->sourcefn : "--");
    } else if (tkc && tkc->filename) {
        log_write(instance, "\n%s:", tkc->filename);
    } else if (filename) {
        log_write(instance, "\n%s:", filename);
        if (linenum) {
            log_write(instance, "line %u:", linenum);
        }
    } else {
        log_write(instance, "\n");
    }

    if (tkc) {
        if (tkc->curerr && tkc->curerr->mod) {
            log_write(instance, 
                      "%u.%u:", 
                      tkc->curerr->linenum, 
                      tkc->curerr->linepos);
        } else if (tkc->cur && 
                   (tkc->cur != (tk_token_t *)&tkc->tkQ) &&
                   TK_CUR_VAL(tkc)) {
            log_write(instance, 
                      "%u.%u:", 
                      TK_CUR_LNUM(tkc), 
                      TK_CUR_LPOS(tkc));
        } else {
            log_write(instance, 
                      "%u.%u:", 
                      tkc->linenum, 
                      tkc->linepos);

        }
        tkc->curerr = NULL;
    }

    if (iserr) {
        log_write(instance, " error(%u): %s", res, get_error_string(res));
    } else {
        log_write(instance, " warning(%u): %s", res, get_error_string(res));
    }

    if (fineoln) {
        log_write(instance, "\n");
    }

} /* ncx_print_errormsg_ex */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_conf_exp_err
 * \brief Print an error for wrong token, expected a different token
 * \param tkc token chain
 * \param result error code
 * \param expstr expected token description
 * \return none
 */
void
    ncx_conf_exp_err (ncx_instance_t *instance,
                      tk_chain_t  *tkc,
                      status_t result,
                      const char *expstr)
{
    ncx_print_errormsg_ex(instance, 
                          tkc, 
                          NULL, 
                          result, 
                          NULL, 
                          0,
                          (expstr) ? FALSE : TRUE);
    if (expstr) {
        log_write(instance, "  Expected: %s\n", expstr);
    }

}  /* ncx_conf_exp_err */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_mod_exp_err
 * \brief Print an error for wrong token, expected a different token
 * \param tkc token chain
 * \param mod module in progress
 * \param result error code
 * \param expstr expected token description
 * \return none
 */
void
    ncx_mod_exp_err (ncx_instance_t *instance,
                     tk_chain_t  *tkc,
                     ncx_module_t *mod,
                     status_t result,
                     const char *expstr)
{
    const char *gotval;
    tk_type_t   tktyp;
    boolean skip = FALSE;

    if (TK_CUR(tkc)) {
        tktyp = TK_CUR_TYP(tkc);
    } else {
        tktyp = TK_TT_NONE;
    }

    if (tktyp == TK_TT_NONE) {
        gotval = NULL;
    } else if (TK_TYP_STR(tktyp)) {
        gotval = (const char *)TK_CUR_VAL(tkc);
    } else if (TK_CUR_TYP(tkc) == TK_TT_LBRACE) {
        gotval = "left brace, skipping to closing right brace";
        skip = TRUE;
    } else {
        gotval = tk_get_token_name(tktyp);
    }

    if (LOGERROR) {
        if (gotval && expstr) {
            log_error(instance, "\nError:  Got '%s', Expected: %s", gotval, expstr);
        } else if (expstr) {
            log_error(instance, "\nError:  Expected: %s", expstr);
        }
        ncx_print_errormsg_ex(instance, tkc, mod, result, NULL, 0,
                              (expstr) ? FALSE : TRUE);
        log_error(instance, "\n");
    }

    if (skip) {
        /* got an unexpected left brace, so skip to the
         * end of this unknown section to resynch;
         * otherwise the first unknown closing right brace
         * will end the parent section, which causes
         * a false 'unexpected EOF' error
         */
        uint32 skipcount = 1;
        boolean done = FALSE;
        status_t res = NO_ERR;
        while (!done && res == NO_ERR) {
            res = TK_ADV(instance, tkc);
            if (res == NO_ERR) {
                tktyp = TK_CUR_TYP(tkc);
                if (tktyp == TK_TT_LBRACE) {
                    skipcount++;
                } else if (tktyp == TK_TT_RBRACE) {
                    skipcount--;
                }
                if (!skipcount) {
                    done = TRUE;
                }
            }
        }
    }

}  /* ncx_mod_exp_err */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_mod_missing_err
 * \brief Print an error for wrong token, mandatory
 * sub-statement is missing
 * \param tkc token chain
 * \param mod module in progress
 * \param stmtstr parent statement
 * \param expstr expected sub-statement
 * \return none
 */
void
    ncx_mod_missing_err (ncx_instance_t *instance,
                         tk_chain_t  *tkc,
                         ncx_module_t *mod,
                         const char *stmtstr,
                         const char *expstr)
{
    if (LOGERROR) {
        if (stmtstr && expstr) {
            log_error(instance, 
                      "\nError: '%s' statement missing "
                      "mandatory '%s' sub-statement", 
                      stmtstr, 
                      expstr);
        } else {
            SET_ERROR(instance, ERR_INTERNAL_VAL);
        }

        ncx_print_errormsg_ex(instance, 
                              tkc, 
                              mod, 
                              ERR_NCX_DATA_MISSING,
                              NULL, 
                              0,
                              (expstr) ? FALSE : TRUE);
        log_error(instance, "\n");
    }

}  /* ncx_mod_missing_err */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_free_node
 * \brief Delete a node based on its type
 * \param nodetyp NCX node type
 * \param node node top free
 * \return none
 */
void
    ncx_free_node (ncx_instance_t *instance,
                   ncx_node_t nodetyp,
                   void *node)
{
    assert ( node && " param node is NULL" );

    switch (nodetyp) {
    case NCX_NT_NONE:                          /* uninitialized */
        m__free(instance, node);
        break;
    case NCX_NT_TYP:                          /* typ_template_t */
        typ_free_template(instance, node);
        break;
    case NCX_NT_GRP:                          /* grp_template_t */
        grp_free_template(instance, node);
        break;
    case NCX_NT_VAL:                             /* val_value_t */
        val_free_value(instance, node);
        break;
    case NCX_NT_OBJ:                          /* obj_template_t */
        obj_free_template(instance, node);
        break;
    case NCX_NT_STRING:                       /* xmlChar string */
        m__free(instance, node);
        break;
    case NCX_NT_CFG:                          /* cfg_template_t */
        cfg_free_template(instance, node);
        break;
    case NCX_NT_QNAME:                         /* xmlns_qname_t */
        xmlns_free_qname(instance, node);
        break;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        m__free(instance, node);
    }

} /* ncx_free_node */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_data_class_enum
 * \brief Get the enum for the string name of a ncx_data_class_t enum
 * \param str string name of the enum value 
 * \return enum value
 */
ncx_data_class_t
    ncx_get_data_class_enum (ncx_instance_t *instance, const xmlChar *str)
{
    if (!str) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return NCX_DC_NONE;
    } else if (!xml_strcmp(instance, NCX_EL_CONFIG, str)) {
        return NCX_DC_CONFIG;
    } else if (!xml_strcmp(instance, NCX_EL_STATE, str)) {
        return NCX_DC_STATE;
    } else {
        /* SET_ERROR(ERR_INTERNAL_VAL); */
        return NCX_DC_NONE;
    }
    /*NOTREACHED*/

}  /* ncx_get_data_class_enum */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_data_class_str
 * \brief Get the string value for the ncx_data_class_t enum
 * \param dataclass enum value to convert
 * \return string value for the enum
 */
const xmlChar *
    ncx_get_data_class_str (ncx_instance_t *instance, ncx_data_class_t dataclass)
{
    switch (dataclass) {
    case NCX_DC_NONE:
        return NULL;
    case NCX_DC_CONFIG:
        return NCX_EL_CONFIG;
    case NCX_DC_STATE:
        return NCX_EL_STATE;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return NULL;
    }
    /*NOTREACHED*/

}  /* ncx_get_data_class_str */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_access_str
 * \brief Get the string name of a ncx_access_t enum
 * \param access == enum value
 * \return string value
 */
const xmlChar * 
    ncx_get_access_str (ncx_access_t max_access)
{
    switch (max_access) {
    case NCX_ACCESS_NONE:    return (const xmlChar *) "not set";
    case NCX_ACCESS_RO:      return NCX_EL_ACCESS_RO;
    case NCX_ACCESS_RW:      return NCX_EL_ACCESS_RW;
    case NCX_ACCESS_RC:      return NCX_EL_ACCESS_RC;
    default:                 return (const xmlChar *) "illegal";
    }
    /*NOTREACHED*/

}  /* ncx_get_access_str */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_access_enum
 * \brief Get the enum for the string name of a ncx_access_t enum
 * \param str string name of the enum value 
 * \return enum value
 */
ncx_access_t
    ncx_get_access_enum (ncx_instance_t *instance, const xmlChar *str)
{
    assert ( str && " param str is NULL" );

    if (!xml_strcmp(instance, NCX_EL_ACCESS_RO, str)) {
        return NCX_ACCESS_RO;
    } else if (!xml_strcmp(instance, NCX_EL_ACCESS_RW, str)) {
        return NCX_ACCESS_RW;
    } else if (!xml_strcmp(instance, NCX_EL_ACCESS_RC, str)) {
        return NCX_ACCESS_RC;
    } else {
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return NCX_ACCESS_NONE;
    }
    /*NOTREACHED*/

}  /* ncx_get_access_enum */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_tclass
 * \brief Get the token class
 * \param btyp base type enum
 * \return tclass enum 
 */
ncx_tclass_t
    ncx_get_tclass (ncx_instance_t *instance, ncx_btype_t btyp)
{
    switch (btyp) {
    case NCX_BT_NONE:
        return NCX_CL_NONE;
    case NCX_BT_ANY:
    case NCX_BT_BOOLEAN:
    case NCX_BT_BITS:
    case NCX_BT_ENUM:
    case NCX_BT_EMPTY:
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
    case NCX_BT_STRING:
    case NCX_BT_BINARY:
    case NCX_BT_LEAFREF:
    case NCX_BT_SLIST:
    case NCX_BT_UNION:
        return NCX_CL_SIMPLE;
    case NCX_BT_CONTAINER:
    case NCX_BT_LIST:
    case NCX_BT_CHOICE:
    case NCX_BT_CASE:
        return NCX_CL_COMPLEX;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return NCX_CL_NONE;
    }
}  /* ncx_get_tclass */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_valid_name_ch
 * \brief Check if an xmlChar is a valid NCX name string char
 * \param ch xmlChar to check
 * \return TRUE if a valid name char, FALSE otherwise
 */
boolean
    ncx_valid_name_ch (uint32 ch)
{
    char c;

    if (ch & bit7) {
        return FALSE;       /* TEMP -- handling ASCII only */
    } else {
        c = (char)ch;
        return (isalpha((int)c) || isdigit((int)c) || 
		c=='_' || c=='-' || c=='.')
            ? TRUE : FALSE;
    }
    /*NOTREACHED*/
} /* ncx_valid_name_ch */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_valid_fname_ch
 * \brief Check if an xmlChar is a valid NCX name string first char
 * \param ch xmlChar to check
 * \return TRUE if a valid first name char, FALSE otherwise
 */
boolean
    ncx_valid_fname_ch (uint32 ch)
{
    char c;

    if (ch & bit7) {
        return FALSE;       /* TEMP -- handling ASCII only */
    } else {
        c = (char)ch;
        return (isalpha((int)c) || (c=='_')) ? TRUE : FALSE;
    }
    /*NOTREACHED*/
} /* ncx_valid_fname_ch */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_valid_name
 * \brief Check if an xmlChar string is a valid YANG identifier value
 * \param str xmlChar string to check
 * \param len length of the string to check (in case of substr)
 * \return TRUE if a valid name string, FALSE otherwise 
 */
boolean
    ncx_valid_name (const xmlChar *str, 
                    uint32 len)
{
    assert ( str && " param str is NULL" );

    if (len == 0 || len > NCX_MAX_NLEN) {
        return FALSE;
    }
    if (!ncx_valid_fname_ch(*str)) {
        return FALSE;
    }

    uint32  i;
    for (i=1; i<len; i++) {
        if (!ncx_valid_name_ch(str[i])) {
            return FALSE;
        }
    }

    if (len >= 3 &&
        ((*str == 'X' || *str == 'x') &&
         (str[1] == 'M' || str[1] == 'm') &&
         (str[2] == 'L' || str[2] == 'l'))) {
        return FALSE;
    }

    return TRUE;

} /* ncx_valid_name */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_valid_name2
 * \brief Check if an xmlChar string is a valid NCX name
 * \param str xmlChar string to check (zero-terminated)
 * \return TRUE if a valid name string, FALSE otherwise
 */
boolean
    ncx_valid_name2 (ncx_instance_t *instance, const xmlChar *str)
{
    assert ( str && " param str is NULL" );
    return ncx_valid_name(str, xml_strlen(instance, str));

} /* ncx_valid_name2 */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_parse_name
 * \brief Check if the next N chars represent a valid NcxName
 * Will end on the first non-name char
 * \param str xmlChar string to check 
 * \param len address of name length
 *       set to 0 if no valid name parsed
 *       set to > 0 for the numbers of chars in the NcxName
 * \return status_t i(error if name too long)
 */
status_t
    ncx_parse_name (const xmlChar *str,
                    uint32 *len)
{
    assert ( str && " param str is NULL" );

    if (!ncx_valid_fname_ch(*str)) {
        *len = 0;
        return ERR_NCX_INVALID_NAME;
    }

    const xmlChar *s = str+1;

    while (ncx_valid_name_ch(*s)) {
        s++;
    }
    *len = (uint32)(s - str);
    if (*len > NCX_MAX_NLEN) {
        return ERR_NCX_TOO_BIG;
    } else {
        return NO_ERR;
    }

} /* ncx_parse_name */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_is_true
 * \brief Check if an xmlChar string is a string OK for XSD boolean
 * \param str xmlChar string to check
 * \return TRUE if a valid boolean value indicating true
 * FALSE otherwise
 */
boolean
    ncx_is_true (ncx_instance_t *instance, const xmlChar *str)
{
    assert ( str && " param str is NULL" );

    if (!xml_strcmp(instance, str, (const xmlChar *)"true") ||
        !xml_strcmp(instance, str, (const xmlChar *)"1")) {
        return TRUE;
    } else {
        return FALSE;
    }

} /* ncx_is_true */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_is_false
 * \brief Check if an xmlChar string is a string OK for XSD boolean
 * \param str xmlChar string to check
 * \return TRUE if a valid boolean value indicating false
 * FALSE otherwise
 */
boolean
    ncx_is_false (ncx_instance_t *instance, const xmlChar *str)
{
    assert ( str && " param str is NULL" );

    if (!xml_strcmp(instance, str, (const xmlChar *)"false") ||
        !xml_strcmp(instance, str, (const xmlChar *)"0")) {
        return TRUE;
    } else {
        return FALSE;
    }

} /* ncx_is_false */


/*********   P A R S E R   H E L P E R   F U N C T I O N S   *******/

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_consume_tstring
 * \brief Consume a TK_TT_TSTRING with the specified value
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
 * \param tkc token chain 
 * \param mod module in progress (NULL if none)
 * \param name token name
 * \param opt TRUE for optional param
 *        FALSE for mandatory param
 * \return status of the operation
 */
status_t 
    ncx_consume_tstring (ncx_instance_t *instance,
                         tk_chain_t *tkc,
                         ncx_module_t *mod,
                         const xmlChar *name,
                         ncx_opt_t opt)
{
    assert ( tkc && " param tkc is NULL" );

    status_t res = TK_ADV(instance, tkc);
    if (res == NO_ERR) {
        if (TK_CUR_TYP(tkc) != TK_TT_TSTRING) {
            if (opt==NCX_OPT) {
                TK_BKUP(instance, tkc);
                return ERR_NCX_SKIPPED;
            } else {
                res = ERR_NCX_WRONG_TKTYPE;
            }
        } else {
            if (xml_strcmp(instance, TK_CUR_VAL(tkc), name)) {
                if (opt==NCX_OPT) {
                    TK_BKUP(instance, tkc);
                    return ERR_NCX_SKIPPED;
                } else {
                    res = ERR_NCX_WRONG_TKVAL;
                }
            }
        }
    }

    if (res != NO_ERR) {
        ncx_print_errormsg(instance, tkc, mod, res);
    }

    return res;

} /* ncx_consume_tstring */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_consume_name
 * \brief Consume a TK_TSTRING that matches the 'name', then
 * retrieve the next TK_TSTRING token into the namebuff
 * If ctk specified, then consume the specified close token
 * Store the results in a malloced buffer
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 * \param tkc token chain 
 * \param mod module in progress (NULL if none)
 * \param name first token name
 * \param namebuff ptr to OUTPUT name string
 * \param opt NCX_OPT for optional param
 *            NCX_REQ for mandatory param
 * \param ctyp close token (use TK_TT_NONE to skip this part)
 * \return status of the operation
 */
status_t 
    ncx_consume_name (ncx_instance_t *instance,
                      tk_chain_t *tkc,
                      ncx_module_t *mod,
                      const xmlChar *name,
                      xmlChar **namebuff,
                      ncx_opt_t opt,
                      tk_type_t  ctyp)
{
    assert ( tkc && " param tkc is NULL" );
    assert ( name && " param name is NULL" );
    assert ( namebuff && " param namebuff is NULL" );

    status_t retres = NO_ERR;
    const char *expstr = "name string";

    /* check 'name' token */
    status_t res = TK_ADV(instance, tkc);
    if (res != NO_ERR) {
        ncx_mod_exp_err(instance, tkc, mod, res, expstr);
        return res;
    }
    if (TK_CUR_TYP(tkc) != TK_TT_TSTRING) {
        if (opt==NCX_OPT) {
            TK_BKUP(instance, tkc);
            return ERR_NCX_SKIPPED;
        } else {
            res = ERR_NCX_WRONG_TKTYPE;
            ncx_mod_exp_err(instance, tkc, mod, res, expstr);
        }
    }
    
    if (res==NO_ERR && xml_strcmp(instance, TK_CUR_VAL(tkc), name)) {
        if (opt==NCX_OPT) {
            TK_BKUP(instance, tkc);
            return ERR_NCX_SKIPPED;
        } else {
            res = ERR_NCX_WRONG_TKVAL;
            ncx_mod_exp_err(instance, tkc, mod, res, expstr);
        }
    }

    retres = res;
    expstr = "name string";

    /* check string value token */
    res = TK_ADV(instance, tkc);
    if (res != NO_ERR) {
        ncx_mod_exp_err(instance, tkc, mod, res, expstr);
        return res;
    } else {
        if (TK_CUR_TYP(tkc) != TK_TT_TSTRING) {
            res = ERR_NCX_WRONG_TKTYPE;
            ncx_mod_exp_err(instance, tkc, mod, res, expstr);
        } else {
            *namebuff = xml_strdup(instance, TK_CUR_VAL(tkc));
            if (!*namebuff) {
                res = ERR_INTERNAL_MEM;
                ncx_print_errormsg(instance, tkc, mod, res);
                return res;
            }
        }
    }

    retres = res;
    expstr = "closing token";

    /* check for a closing token */
    if (ctyp != TK_TT_NONE) {
        res = TK_ADV(instance, tkc);
        if (res != NO_ERR) {
            ncx_mod_exp_err(instance, tkc, mod, res, expstr);
        } else {
            if (TK_CUR_TYP(tkc) != ctyp) {
                res = ERR_NCX_WRONG_TKTYPE;
                ncx_mod_exp_err(instance, tkc, mod, res, expstr);
            }
        }
        CHK_EXIT(res, retres);
    }

    return retres;

} /* ncx_consume_name */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_consume_token
 * \brief Consume the next token which should be a 1 or 2 char token
 * without any value. However this function does not check the value,
 * just the token type.
 * Error messages are printed by this function!!
 * Do not duplicate error messages upon error return
 * \param tkc token chain 
 * \param mod module in progress (NULL if none)
 * \param ttyp token type
 * \return status of the operation
 */
status_t 
    ncx_consume_token (ncx_instance_t *instance,
                       tk_chain_t *tkc,
                       ncx_module_t *mod,
                       tk_type_t  ttyp)
{
    assert ( tkc && " param tkc is NULL" );

    const char  *tkname;

    status_t res = TK_ADV(instance, tkc);
    if (res != NO_ERR) {
        ncx_print_errormsg(instance, tkc, mod, res);
        return res;
    }

    res = (TK_CUR_TYP(tkc) == ttyp) ? 
        NO_ERR : ERR_NCX_WRONG_TKTYPE;

    if (res != NO_ERR) {
        tkname = tk_get_token_name(ttyp);
        switch (tkc->source) {
        case TK_SOURCE_YANG:
            ncx_mod_exp_err(instance, tkc, mod, res, tkname);

            /* if a token is missing and the token
             * parsed instead looks like the continuation
             * of a statement, or the end of a section,
             * then backup and let parsing continue
             */
            switch (ttyp) {
            case TK_TT_SEMICOL:
            case TK_TT_LBRACE:
                switch (TK_CUR_TYP(tkc)) {
                case TK_TT_TSTRING:
                case TK_TT_MSTRING:
                case TK_TT_RBRACE:
                    TK_BKUP(instance, tkc);
                    break;
                default:
                    ;
                }
                break;
            case TK_TT_RBRACE:
                switch (TK_CUR_TYP(tkc)) {
                case TK_TT_TSTRING:
                case TK_TT_MSTRING:
                    TK_BKUP(instance, tkc);
                    break;
                default:
                    ;
                }
                break;

            default:
                ;
            }
            break;
        default:
            ;
        }
    }

    return res;

} /* ncx_consume_token */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_new_errinfo
 * \brief Malloc and init a new ncx_errinfo_t
 * \return pointer to malloced ncx_errinfo_t, or NULL if memory error
 */
ncx_errinfo_t *
    ncx_new_errinfo (ncx_instance_t *instance)
{
    ncx_errinfo_t *err = m__getObj(instance, ncx_errinfo_t);
    if (!err) {
        return NULL;
    }
    ncx_init_errinfo(err);
    return err;

}  /* ncx_new_errinfo */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_init_errinfo
 * \brief Init the fields in an ncx_errinfo_t struct
 * \param err ncx_errinfo_t data structure to init
 * \return none
 */
void 
    ncx_init_errinfo (ncx_errinfo_t *err)
{
    assert ( err && " param err is NULL" );
    memset(err, 0x0, sizeof(ncx_errinfo_t));

}  /* ncx_init_errinfo */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_clean_errinfo
 * \brief Scrub the memory in a ncx_errinfo_t by freeing all
 * the sub-fields
 * \param err ncx_errinfo_t data structure to clean
 * \return none
 */
void 
    ncx_clean_errinfo (ncx_instance_t *instance, ncx_errinfo_t *err)
{
    assert ( err && " param err is NULL" );

    if (err->descr) {
        m__free(instance, err->descr);
        err->descr = NULL;
    }
    if (err->ref) {
        m__free(instance, err->ref);
        err->ref = NULL;
    }
    if (err->error_app_tag) {
        m__free(instance, err->error_app_tag);
        err->error_app_tag = NULL;
    }
    if (err->error_message) {
        m__free(instance, err->error_message);
        err->error_message = NULL;
    }

}  /* ncx_clean_errinfo */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_free_errinfo
 * \brief Scrub the memory in a ncx_errinfo_t by freeing all
 * the sub-fields, then free the errinfo struct
 * \param err ncx_errinfo_t data structure to free
 * \return none
 */
void 
    ncx_free_errinfo (ncx_instance_t *instance, ncx_errinfo_t *err)
{
    assert ( err && " param err is NULL" );
    ncx_clean_errinfo(instance, err);
    m__free(instance, err);

}  /* ncx_free_errinfo */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_errinfo_set
 * \brief Check if error-app-tag or error-message set
 * Check if the errinfo struct is set or empty
 * Checks only the error_app_tag and error_message fields
 * \param errinfo ncx_errinfo_t struct to check
 * \return TRUE if at least one field set
 *         FALSE if the errinfo struct is empty
 */
boolean
    ncx_errinfo_set (const ncx_errinfo_t *errinfo)
{
    assert( errinfo && "errinfo is NULL" );

    if (errinfo->error_app_tag || errinfo->error_message) {
        return TRUE;
    } else {
        return FALSE;
    }

}  /* ncx_errinfo_set */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_copy_errinfo
 * \brief Copy the fields from one errinfo to a blank errinfo
 * \param src struct with starting contents
 * \param src struct with starting contents
 * \return status
 */
status_t
    ncx_copy_errinfo (ncx_instance_t *instance,
                      const ncx_errinfo_t *src,
                      ncx_errinfo_t *dest)
{
    assert ( src && " param src is NULL" );
    assert ( dest && " param dest is NULL" );

    if (src->descr) {
        if (dest->descr) {
            m__free(instance, dest->descr);
        }
        dest->descr = xml_strdup(instance, src->descr);
        if (!dest->descr) {
            return ERR_INTERNAL_MEM;
        }
    }

    if (src->ref) {
        if (dest->ref) {
            m__free(instance, dest->ref);
        }
        dest->ref = xml_strdup(instance, src->ref);
        if (!dest->ref) {
            return ERR_INTERNAL_MEM;
        }
    }

    if (src->error_app_tag) {
        if (dest->error_app_tag) {
            m__free(instance, dest->error_app_tag);
        }
        dest->error_app_tag = xml_strdup(instance, src->error_app_tag);
        if (!dest->error_app_tag) {
            return ERR_INTERNAL_MEM;
        }
    }

    if (src->error_message) {
        if (dest->error_message) {
            m__free(instance, dest->error_message);
        }
        dest->error_message = xml_strdup(instance, src->error_message);
        if (!dest->error_message) {
            return ERR_INTERNAL_MEM;
        }
    }

    return NO_ERR;

}  /* ncx_copy_errinfo */

// ----------------------------------------------------------------------------!

/**
 * \fn ncx_get_source_ex
 * \brief Get a malloced buffer containing the complete filespec
 * for the given input string.  If this is a complete dirspec,
 * this this will just strdup the value.
 * This is just a best effort to get the full spec.
 * If the full spec is greater than 1500 bytes,
 * then a NULL value (error) will be returned
 *   - Change ./ --> cwd/
 *   - Remove ~/  --> $HOME
 *   - add trailing '/' if not present
 * \param fspec input filespec
 * \param expand_cwd TRUE if foo should be expanded to /cur/dir/foo
 *                   FALSE if not
 * \param res address of return status OUTPUT
 * \return malloced buffer containing possibly expanded full filespec
 */
xmlChar *
    ncx_get_source_ex (ncx_instance_t *instance,
                       const xmlChar *fspec,
                       boolean expand_cwd,
                       status_t *res)
{
#define DIRBUFF_SIZE 1500

    assert ( fspec && " param fspec is NULL" );
    assert ( res && " param res is NULL" );

    if (*fspec == 0) {
        *res = ERR_NCX_INVALID_VALUE;
        return NULL;
    }

    const xmlChar  *start;
    xmlChar        *bp;
    uint32          bufflen, userlen;
    boolean         with_dot;

    *res = NO_ERR;
    xmlChar *buff = NULL;
    const xmlChar *user = NULL;
    uint32 len = 0;
    const xmlChar *p = fspec;

    if (*p == NCXMOD_PSCHAR) {
        /* absolute path */
        buff = xml_strdup(instance, fspec);
        if (!buff) {
            *res = ERR_INTERNAL_MEM;
        }
    } else if (*p == NCXMOD_HMCHAR) {
        /* starts with ~[username]/some/path */
        if (p[1] && p[1] != NCXMOD_PSCHAR) {
            /* explicit user name */
            start = &p[1];   
            p = &p[2];
            while (*p && *p != NCXMOD_PSCHAR) {
                p++;
            }
            userlen = (uint32)(p-start);
            user = ncxmod_get_userhome(instance, start, userlen);
        } else {
            /* implied current user */
            p++;   /* skip ~ char */
            /* get current user home dir */
            user = ncxmod_get_userhome(instance, NULL, 0);
        }

        if (user == NULL) {
            log_error(instance,
                      "\nError: invalid user name in path string (%s)",
                      fspec);
            *res = ERR_NCX_INVALID_VALUE;
            return NULL;
        }

        /* string pointer 'p' stopped on the PSCHAR to start the
         * rest of the path string
         */
        len = xml_strlen(instance, user) + xml_strlen(instance, p);
        buff = m__getMem(instance, len+1);
        if (buff == NULL) {
            *res = ERR_INTERNAL_MEM;
            return NULL;
        }

        bp = buff;
        bp += xml_strcpy(instance, bp, user);
        xml_strcpy(instance, bp, p);
    } else if (*p == NCXMOD_ENVCHAR) {
        /* should start with $ENVVAR/some/path */
        start = ++p;  /* skip dollar sign */
        while (*p && *p != NCXMOD_PSCHAR) {
                p++;
        }
        userlen = (uint32)(p-start);
        if (userlen) {
            user = ncxmod_get_envvar(instance, start, userlen);
        }

        len = 0;
        if (!user) {
            /* ignoring this environment variable
             * could be something like $DESTDIR/usr/share
             */
            if (LOGDEBUG) {
                log_debug(instance,
                          "\nEnvironment variable not "
                          "found in path string (%s)",
                          fspec);
            }
        } else {
            len = xml_strlen(instance, user);
        }

        /* string pointer 'p' stopped on the PSCHAR to start the
         * rest of the path string
         */
        len += xml_strlen(instance, p);

        buff = m__getMem(instance, len+1);
        if (buff == NULL) {
            *res = ERR_INTERNAL_MEM;
            return NULL;
        }

        bp = buff;
        if (user) {
            bp += xml_strcpy(instance, bp, user);
        }
        xml_strcpy(instance, bp, p);
    } else if ((*p == NCXMOD_DOTCHAR && p[1] == NCXMOD_PSCHAR) ||
               (*p != NCXMOD_DOTCHAR && expand_cwd)) {

        /* check for ./some/path or some/path */
        with_dot = (*p == NCXMOD_DOTCHAR);

	if (with_dot) {
	    p++;
	}

        /* prepend string with current directory */
        buff = m__getMem(instance, DIRBUFF_SIZE);
        if (buff == NULL) {
            *res = ERR_INTERNAL_MEM;
            return NULL;
        }

        if (!getcwd((char *)buff, DIRBUFF_SIZE)) {
            SET_ERROR(instance, ERR_BUFF_OVFL);
            m__free(instance, buff);
            return NULL;
        }
            
        bufflen = xml_strlen(instance, buff);

        if ((bufflen + xml_strlen(instance, p) + 2) >= DIRBUFF_SIZE) {
            *res = ERR_BUFF_OVFL;
            m__free(instance, buff);
            return NULL;
        }

        if (!with_dot) {
            buff[bufflen++] = NCXMOD_PSCHAR;
        }
        xml_strcpy(instance, &buff[bufflen], p);
    } else {
        /* probably contains ../ and that is not handled yet */
        buff = xml_strdup(instance, fspec);
        if (buff == NULL) {
            *res = ERR_INTERNAL_MEM;
        }
    }

    return buff;

}  /* ncx_get_source_ex */


/********************************************************************
* FUNCTION ncx_get_source
* 
* Get a malloced buffer containing the complete filespec
* for the given input string.  If this is a complete dirspec,
* this this will just strdup the value.
*
* This is just a best effort to get the full spec.
* If the full spec is greater than 1500 bytes,
* then a NULL value (error) will be returned
*
* This will expand the cwd!
*
*   - Change ./ --> cwd/
*   - Remove ~/  --> $HOME
*   - add trailing '/' if not present
*
* INPUTS:
*    fspec == input filespec
*    res == address of return status
*
* OUTPUTS:
*   *res == return status, NO_ERR if return is non-NULL
*
* RETURNS:
*   malloced buffer containing possibly expanded full filespec
*********************************************************************/
xmlChar *
    ncx_get_source (ncx_instance_t *instance,
                    const xmlChar *fspec,
                    status_t *res)
{
    return ncx_get_source_ex(instance, fspec, TRUE, res);
}  /* ncx_get_source */


/********************************************************************
* FUNCTION ncx_set_cur_modQ
* 
* Set the current module Q to an alternate (for yangdiff)
* This will be used for module searches usually in ncx_modQ
*
* INPUTS:
*    que == Q of ncx_module_t to use
*********************************************************************/
void
    ncx_set_cur_modQ (ncx_instance_t *instance, dlq_hdr_t *que)
{
    assert ( que && " param que is NULL");
    instance->ncx_curQ = que;

}  /* ncx_set_cur_modQ */


/********************************************************************
* FUNCTION ncx_reset_modQ
* 
* Set the current module Q to the original ncx_modQ
*
*********************************************************************/
void
    ncx_reset_modQ (ncx_instance_t *instance)
{
    instance->ncx_curQ = &instance->ncx_modQ;

}  /* ncx_reset_modQ */


/********************************************************************
* FUNCTION ncx_set_session_modQ
* 
* !!! THIS HACK IS NEEDED BECAUSE val.c
* !!! USES ncx_find_module sometimes, and
* !!! yangcli sessions are not loaded into the
* !!! main database of modules.
* !!! THIS DOES NOT WORK FOR MULTIPLE CONCURRENT PROCESSES
*
* Set the current session module Q to an alternate (for yangdiff)
* This will be used for module searches usually in ncx_modQ
*
* INPUTS:
*    que == Q of ncx_module_t to use
*********************************************************************/
void
    ncx_set_session_modQ (ncx_instance_t *instance, dlq_hdr_t *que)
{
    assert ( que && " param que is NULL");
    instance->ncx_sesmodQ = que;

}  /* ncx_set_sesion_modQ */


/********************************************************************
* FUNCTION ncx_clear_session_modQ
* 
* !!! THIS HACK IS NEEDED BECAUSE val.c
* !!! USES ncx_find_module sometimes, and
* !!! yangcli sessions are not loaded into the
* !!! main database of modules.
* !!! THIS DOES NOT WORK FOR MULTIPLE CONCURRENT PROCESSES
*
* Clear the current session module Q
*
*********************************************************************/
void
    ncx_clear_session_modQ (ncx_instance_t *instance)
{

    instance->ncx_sesmodQ = NULL;

}  /* ncx_clear_sesion_modQ */


/********************************************************************
* FUNCTION ncx_set_load_callback
* 
* Set the callback function for a load-module event
*
* INPUT:
*   cbfn == callback function to use
*
*********************************************************************/
void
    ncx_set_load_callback (ncx_instance_t *instance, ncx_load_cbfn_t cbfn)
{

    instance->mod_load_callback = cbfn;

}  /* ncx_set_load_callback */


/********************************************************************
* FUNCTION ncx_prefix_different
* 
* Check if the specified prefix pair reference different modules
* 
* INPUT:
*   prefix1 == 1st prefix to check (may be NULL)
*   prefix2 == 2nd prefix to check (may be NULL)
*   modprefix == module prefix to check (may be NULL)
*
* RETURNS:
*   TRUE if prefix1 and prefix2 reference different modules
*   FALSE if prefix1 and prefix2 reference the same modules
*********************************************************************/
boolean
    ncx_prefix_different (ncx_instance_t *instance,
                          const xmlChar *prefix1,
                          const xmlChar *prefix2,
                          const xmlChar *modprefix)
{
    if (!prefix1) {
        prefix1 = modprefix;
    }
    if (!prefix2) {
        prefix2 = modprefix;
    }
    if (prefix1 == prefix2) {
        return FALSE;
    }
    if (prefix1 == NULL || prefix2 == NULL) {
        return TRUE;
    }
    return (xml_strcmp(instance, prefix1, prefix2)) ? TRUE : FALSE;

}  /* ncx_prefix_different */


/********************************************************************
* FUNCTION ncx_get_baddata_enum
* 
* Check if the specified string matches an ncx_baddata_t enum
* 
* INPUT:
*   valstr == value string to check
*
* RETURNS:
*   enum value if OK
*   NCX_BAD_DATA_NONE if an error
*********************************************************************/
ncx_bad_data_t
    ncx_get_baddata_enum (ncx_instance_t *instance, const xmlChar *valstr)
{
    assert ( valstr && " param valstr is NULL");

    if (!xml_strcmp(instance, valstr, E_BAD_DATA_IGNORE)) {
        return NCX_BAD_DATA_IGNORE;
    } else if (!xml_strcmp(instance, valstr, E_BAD_DATA_WARN)) {
        return NCX_BAD_DATA_WARN;
    } else if (!xml_strcmp(instance, valstr, E_BAD_DATA_CHECK)) {
        return NCX_BAD_DATA_CHECK;
    } else if (!xml_strcmp(instance, valstr, E_BAD_DATA_ERROR)) {
        return NCX_BAD_DATA_ERROR;
    } else {
        return NCX_BAD_DATA_NONE;
    }
    /*NOTREACHED*/

}  /* ncx_get_baddata_enum */


/********************************************************************
* FUNCTION ncx_get_baddata_string
* 
* Get the string for the specified enum value
* 
* INPUT:
*   baddatar == enum value to check
*
* RETURNS:
*   string pointer if OK
*   NULL if an error
*********************************************************************/
const xmlChar *
    ncx_get_baddata_string (ncx_instance_t *instance, ncx_bad_data_t baddata)
{
    switch (baddata) {
    case NCX_BAD_DATA_NONE:
        return NCX_EL_NONE;
    case NCX_BAD_DATA_IGNORE:
        return E_BAD_DATA_IGNORE;
    case NCX_BAD_DATA_WARN:
        return E_BAD_DATA_WARN;
    case NCX_BAD_DATA_CHECK:
        return E_BAD_DATA_CHECK;
    case NCX_BAD_DATA_ERROR:
        return E_BAD_DATA_ERROR;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return NULL;
    }

}  /* ncx_get_baddata_string */


/********************************************************************
* FUNCTION ncx_get_withdefaults_string
* 
* Get the string for the specified enum value
* 
* INPUT:
*   withdef == enum value to check
*
* RETURNS:
*   string pointer if OK
*   NULL if an error
*********************************************************************/
const xmlChar *
    ncx_get_withdefaults_string (ncx_instance_t *instance, ncx_withdefaults_t withdef)
{
    switch (withdef) {
    case NCX_WITHDEF_NONE:
        return NCX_EL_NONE;
    case NCX_WITHDEF_REPORT_ALL:
        return NCX_EL_REPORT_ALL;
    case NCX_WITHDEF_REPORT_ALL_TAGGED:
        return NCX_EL_REPORT_ALL_TAGGED;
    case NCX_WITHDEF_TRIM:
        return NCX_EL_TRIM;
    case NCX_WITHDEF_EXPLICIT:
        return NCX_EL_EXPLICIT;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return NULL;
    }

}  /* ncx_get_withdefaults_string */


/********************************************************************
* FUNCTION ncx_get_withdefaults_enum
* 
* Get the enum for the specified string value
* 
* INPUT:
*   withdefstr == string value to check
*
* RETURNS:
*   enum value for the string
*   NCX_WITHDEF_NONE if invalid value
*********************************************************************/
ncx_withdefaults_t
    ncx_get_withdefaults_enum (ncx_instance_t *instance, const xmlChar *withdefstr)
{
    assert ( withdefstr && " param withdefstr is NULL");

    if (!xml_strcmp(instance, withdefstr, NCX_EL_REPORT_ALL)) {
        return NCX_WITHDEF_REPORT_ALL;
    } else if (!xml_strcmp(instance, withdefstr, NCX_EL_REPORT_ALL_TAGGED)) {
        return NCX_WITHDEF_REPORT_ALL_TAGGED;
    } else if (!xml_strcmp(instance, withdefstr, NCX_EL_TRIM)) {
        return NCX_WITHDEF_TRIM;
    } else if (!xml_strcmp(instance, withdefstr, NCX_EL_EXPLICIT)) {
        return NCX_WITHDEF_EXPLICIT;
    } else {
        return NCX_WITHDEF_NONE;
    }

}  /* ncx_get_withdefaults_enum */


/********************************************************************
* FUNCTION ncx_get_mod_prefix
* 
* Get the module prefix for the specified module
* 
* INPUT:
*   mod == module to check
*
* RETURNS:
*   pointer to module YANG prefix
*********************************************************************/
const xmlChar *
    ncx_get_mod_prefix (const ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL");
    return mod->prefix;

}  /* ncx_get_mod_prefix */


/********************************************************************
* FUNCTION ncx_get_mod_xmlprefix
* 
* Get the module XML prefix for the specified module
* 
* INPUT:
*   mod == module to check
*
* RETURNS:
*   pointer to module XML prefix
*********************************************************************/
const xmlChar *
    ncx_get_mod_xmlprefix (const ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL");

    if (mod->xmlprefix) {
        return mod->xmlprefix;
    } else {
        return mod->prefix;
    }

}  /* ncx_get_mod_xmlprefix */


/********************************************************************
* FUNCTION ncx_get_display_mode_enum
* 
* Get the enum for the specified string value
* 
* INPUT:
*   dmstr == string value to check
*
* RETURNS:
*   enum value for the string
*   NCX_DISPLAY_MODE_NONE if invalid value
*********************************************************************/
ncx_display_mode_t
    ncx_get_display_mode_enum (ncx_instance_t *instance, const xmlChar *dmstr)
{
    assert ( dmstr && " param dmstr is NULL" );

    if (!xml_strcmp(instance, dmstr, NCX_EL_PLAIN)) {
        return NCX_DISPLAY_MODE_PLAIN;
    } else if (!xml_strcmp(instance, dmstr, NCX_EL_PREFIX)) {
        return NCX_DISPLAY_MODE_PREFIX;
    } else if (!xml_strcmp(instance, dmstr, NCX_EL_MODULE)) {
        return NCX_DISPLAY_MODE_MODULE;
    } else if (!xml_strcmp(instance, dmstr, NCX_EL_XML)) {
        return NCX_DISPLAY_MODE_XML;
    } else if (!xml_strcmp(instance, dmstr, NCX_EL_XML_NONS)) {
        return NCX_DISPLAY_MODE_XML_NONS;
    } else if (!xml_strcmp(instance, dmstr, NCX_EL_JSON)) {
        return NCX_DISPLAY_MODE_JSON;
    } else {
        return NCX_DISPLAY_MODE_NONE;
    }

}  /* ncx_get_display_mode_enum */


/********************************************************************
* FUNCTION ncx_get_display_mode_str
* 
* Get the string for the specified enum value
* 
* INPUT:
*   dmode == enum display mode value to check
*
* RETURNS:
*   string value for the enum
*   NULL if none found
*********************************************************************/
const xmlChar *
    ncx_get_display_mode_str (ncx_display_mode_t dmode)
{
    switch (dmode) {
    case NCX_DISPLAY_MODE_NONE:
        return NULL;
    case NCX_DISPLAY_MODE_PLAIN:
        return NCX_EL_PLAIN;
    case NCX_DISPLAY_MODE_PREFIX:
        return NCX_EL_PREFIX;
    case NCX_DISPLAY_MODE_MODULE:
        return NCX_EL_MODULE;
    case NCX_DISPLAY_MODE_XML:
        return NCX_EL_XML;
    case NCX_DISPLAY_MODE_XML_NONS:
        return NCX_EL_XML_NONS;
    case NCX_DISPLAY_MODE_JSON:
        return NCX_EL_JSON;
    default:
        return NULL;
    }

}  /* ncx_get_display_mode_str */


/********************************************************************
* FUNCTION ncx_set_warn_idlen
* 
* Set the warning length for identifiers
* 
* INPUT:
*   warnlen == warning length to use
*
*********************************************************************/
void
    ncx_set_warn_idlen (ncx_instance_t *instance, uint32 warnlen)
{
    instance->warn_idlen = warnlen;

}  /* ncx_set_warn_idlen */


/********************************************************************
* FUNCTION ncx_get_warn_idlen
* 
* Get the warning length for identifiers
* 
* RETURNS:
*   warning length to use
*********************************************************************/
uint32
    ncx_get_warn_idlen (ncx_instance_t *instance)
{
    return instance->warn_idlen;

}  /* ncx_get_warn_idlen */


/********************************************************************
* FUNCTION ncx_set_warn_linelen
* 
* Set the warning length for YANG file lines
* 
* INPUT:
*   warnlen == warning length to use
*
*********************************************************************/
void
    ncx_set_warn_linelen (ncx_instance_t *instance, uint32 warnlen)
{
    instance->warn_linelen = warnlen;

}  /* ncx_set_warn_linelen */


/********************************************************************
* FUNCTION ncx_get_warn_linelen
* 
* Get the warning length for YANG file lines
* 
* RETURNS:
*   warning length to use
*
*********************************************************************/
uint32
    ncx_get_warn_linelen (ncx_instance_t *instance)
{
    return instance->warn_linelen;

}  /* ncx_get_warn_linelen */


/********************************************************************
* FUNCTION ncx_check_warn_idlen
* 
* Check if the identifier length is greater than
* the specified amount.
*
* INPUTS:
*   tkc == token chain to use (for warning message only)
*   mod == module (for warning message only)
*   id == identifier string to check; must be Z terminated
*
* OUTPUTS:
*    may generate log_warn ouput
*********************************************************************/
void
    ncx_check_warn_idlen (ncx_instance_t *instance,
                          tk_chain_t *tkc,
                          ncx_module_t *mod,
                          const xmlChar *id)
{
    assert ( id && " param id is NULL" );

    uint32   idlen;

    if (!instance->warn_idlen) {
        return;
    }

    idlen = xml_strlen(instance, id);
    if (idlen > instance->warn_idlen) {
        log_warn(instance,
                 "\nWarning: identifier '%s' length is %u chars, "
                 "limit is %u chars",
                 id,
                 idlen,
                 instance->warn_idlen);
        ncx_print_errormsg(instance, tkc, mod, ERR_NCX_IDLEN_EXCEEDED);
    }

} /* ncx_check_warn_idlen */


/********************************************************************
* FUNCTION ncx_check_warn_linelen
* 
* Check if the line display length is greater than
* the specified amount.
*
* INPUTS:
*   tkc == token chain to use
*   mod == module (used in warning message only
*   linelen == line length to check
*
* OUTPUTS:
*    may generate log_warn ouput
*********************************************************************/
void
    ncx_check_warn_linelen (ncx_instance_t *instance,
                            tk_chain_t *tkc,
                            ncx_module_t *mod,
                            const xmlChar *line)
{
    assert ( line && " param line is NULL" );
    (void)tkc;
    (void)mod;

    const xmlChar   *str;
    uint32           len;

    if (!instance->warn_linelen) {
        return;
    }

    str = line;
    len = 0;

    /* check for line starting with newline */
    if (*str == '\n') {
        str++;
    }

    /* get the display length */
    while (*str && *str != '\n') {
        if (*str == '\t') {
            len += 8;
        } else {
            len++;
        }
        str++;
    }

    /*
    Shut-up this warning.  RIFT 2014/03/17 - Tom Sidenberg
    if (len > warn_linelen) {
        log_warn("\nWarning: line is %u chars, limit is %u chars",
                 len,
                 warn_linelen);
        ncx_print_errormsg(tkc, mod, ERR_NCX_LINELEN_EXCEEDED);
    }
    */

} /* ncx_check_warn_linelen */


/********************************************************************
* FUNCTION ncx_turn_off_warning
* 
* Add a warning suppression entry
*
* INPUTS:
*   res == internal status code to suppress
*
* RETURNS:
*   status (duplicates are silently dropped)
*********************************************************************/
status_t
    ncx_turn_off_warning (ncx_instance_t *instance, status_t res)
{
    warnoff_t         *warnoff;

    if (res == NO_ERR) {
        return SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    if (res < ERR_WARN_BASE) {
        return ERR_NCX_INVALID_VALUE;
    }

    /* check if 'res' already entered */
    for (warnoff = (warnoff_t *)dlq_firstEntry(instance, &instance->warnoffQ);
         warnoff != NULL;
         warnoff = (warnoff_t *)dlq_nextEntry(instance, warnoff)) {
        if (warnoff->res == res) {
            return NO_ERR;
        }
    }

    warnoff = m__getObj(instance, warnoff_t);
    if (!warnoff) {
        return ERR_INTERNAL_MEM;
    }
    memset(warnoff, 0x0, sizeof(warnoff_t));
    warnoff->res = res;
    dlq_enque(instance, warnoff, &instance->warnoffQ);
    return NO_ERR;

} /* ncx_turn_off_warning */


/********************************************************************
* FUNCTION ncx_turn_on_warning
* 
* Remove a warning suppression entry if it exists
*
* INPUTS:
*   res == internal status code to enable
*
* RETURNS:
*   status (duplicates are silently dropped)
*********************************************************************/
status_t
    ncx_turn_on_warning (ncx_instance_t *instance, status_t res)
{
    warnoff_t         *warnoff;

    if (res == NO_ERR) {
        return SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    if (res < ERR_WARN_BASE) {
        return ERR_NCX_INVALID_VALUE;
    }

    /* check if 'res' exists and remove it if found */
    for (warnoff = (warnoff_t *)dlq_firstEntry(instance, &instance->warnoffQ);
         warnoff != NULL;
         warnoff = (warnoff_t *)dlq_nextEntry(instance, warnoff)) {
        if (warnoff->res == res) {
            dlq_remove(instance, warnoff);
            m__free(instance, warnoff);
            return NO_ERR;
        }
    }

    return NO_ERR;

} /* ncx_turn_on_warning */


/********************************************************************
* FUNCTION ncx_warning_enabled
* 
* Check if a specific status_t code is enabled
*
* INPUTS:
*   res == internal status code to check
*
* RETURNS:
*   TRUE if warning is enabled
*   FALSE if warning is suppressed
*********************************************************************/
boolean
    ncx_warning_enabled (ncx_instance_t *instance, status_t res)
{
    const warnoff_t   *warnoff;

    if (res < ERR_WARN_BASE) {
        return TRUE;
    }

    if (!LOGWARN) {
        return FALSE;
    }

    /* check if 'res' already entered */
    for (warnoff = (const warnoff_t *)dlq_firstEntry(instance, &instance->warnoffQ);
         warnoff != NULL;
         warnoff = (const warnoff_t *)dlq_nextEntry(instance, warnoff)) {
        if (warnoff->res == res) {
            return FALSE;
        }
    }
    return TRUE;

} /* ncx_warning_enabled */


/********************************************************************
* FUNCTION ncx_get_version
* 
* Get the the Yuma version ID string
*
* INPUT:
*    buffer == buffer to hold the version string
*    buffsize == number of bytes in buffer
*
* RETURNS:
*   status
*********************************************************************/
status_t
    ncx_get_version (ncx_instance_t *instance,
                     xmlChar *buffer,
                     uint32 buffsize)
{
    xmlChar    *str;
    uint32      versionlen;
#ifdef RELEASE
    xmlChar     numbuff[NCX_MAX_NUMLEN];
#endif
    
    assert ( buffer && " param buffer is NULL" );

#ifdef RELEASE
    snprintf((char *)numbuff, sizeof(numbuff), "%d", RELEASE);

    versionlen = xml_strlen(instance, YUMA_VERSION) +
        xml_strlen(instance, numbuff) + 1;

    if (versionlen >= buffsize) {
        return ERR_BUFF_OVFL;
    }

    str = buffer;
    str += xml_strcpy(instance, str, YUMA_VERSION);
    *str++ = '-';
    xml_strcpy(instance, str, numbuff);
#else
    versionlen = xml_strlen(instance, YUMA_VERSION) +
        xml_strlen(instance, (const xmlChar *)SVNVERSION) + 1;

    if (versionlen >= buffsize) {
        return ERR_BUFF_OVFL;
    }

    str = buffer;
    str += xml_strcpy(instance, str, YUMA_VERSION);
    *str++ = '.';
    xml_strcpy(instance, str, (const xmlChar *)SVNVERSION);
#endif

    return NO_ERR;

}  /* ncx_get_version */


/********************************************************************
* FUNCTION ncx_new_save_deviations
* 
* create a deviation save structure
*
* INPUTS:
*   devmodule == deviations module name
*   devrevision == deviation module revision (optional)
*   devnamespace == deviation module namespace URI value
*   devprefix == local module prefix (optional)
*
* RETURNS:
*   malloced and initialized save_deviations struct, 
*   or NULL if malloc error
*********************************************************************/
ncx_save_deviations_t *
    ncx_new_save_deviations (ncx_instance_t *instance,
                             const xmlChar *devmodule,
                             const xmlChar *devrevision,
                             const xmlChar *devnamespace,
                             const xmlChar *devprefix)
{
    assert ( devmodule && " param devmodule is NULL" );
    assert ( devnamespace && " param devnamespace is NULL" );

    ncx_save_deviations_t  *savedev;

    savedev = m__getObj(instance, ncx_save_deviations_t);
    if (savedev == NULL) {
        return NULL;
    }

    memset(savedev, 0x0, sizeof(ncx_save_deviations_t));
    dlq_createSQue(instance, &savedev->importQ);
    dlq_createSQue(instance, &savedev->deviationQ);

    savedev->devmodule = xml_strdup(instance, devmodule);
    if (savedev->devmodule == NULL) {
        ncx_free_save_deviations(instance, savedev);
        return NULL;
    }

    if (devprefix) {
        savedev->devprefix = xml_strdup(instance, devprefix);
        if (savedev->devprefix == NULL) {
            ncx_free_save_deviations(instance, savedev);
            return NULL;
        }
    }

    if (devrevision) {
        savedev->devrevision = xml_strdup(instance, devrevision);
        if (savedev->devrevision == NULL) {
            ncx_free_save_deviations(instance, savedev);
            return NULL;
        }
    }

    savedev->devnamespace = xml_strdup(instance, devnamespace);
    if (savedev->devnamespace == NULL) {
        ncx_free_save_deviations(instance, savedev);
        return NULL;
    }
    
    return savedev;

}  /* ncx_new_save_deviations */


/********************************************************************
* FUNCTION ncx_free_save_deviations
* 
* free a deviation save struct
*
* INPUT:
*    savedev == struct to clean and delete
*********************************************************************/
void
    ncx_free_save_deviations (ncx_instance_t *instance, ncx_save_deviations_t *savedev)
{
    assert ( savedev && " param savedev is NULL" );

    ncx_import_t      *import;
    obj_deviation_t   *deviation;

    while (!dlq_empty(instance, &savedev->importQ)) {
        import = (ncx_import_t *)
            dlq_deque(instance, &savedev->importQ);
        ncx_free_import(instance, import);
    }

    while (!dlq_empty(instance, &savedev->deviationQ)) {
        deviation = (obj_deviation_t *)
            dlq_deque(instance, &savedev->deviationQ);
        obj_free_deviation(instance, deviation);
    }

    if (savedev->devmodule) {
        m__free(instance, savedev->devmodule);
    }

    if (savedev->devrevision) {
        m__free(instance, savedev->devrevision);
    }

    if (savedev->devnamespace) {
        m__free(instance, savedev->devnamespace);
    }

    if (savedev->devprefix) {
        m__free(instance, savedev->devprefix);
    }

    m__free(instance, savedev);

}  /* ncx_free_save_deviations */


/********************************************************************
* FUNCTION ncx_clean_save_deviationsQ
* 
* clean a Q of deviation save structs
*
* INPUT:
*    savedevQ == Q of ncx_save_deviations_t to clean
*********************************************************************/
void
    ncx_clean_save_deviationsQ (ncx_instance_t *instance, dlq_hdr_t *savedevQ)
{
    ncx_save_deviations_t *savedev;

    while (!dlq_empty(instance, savedevQ)) {
        savedev = (ncx_save_deviations_t *)dlq_deque(instance, savedevQ);
        ncx_free_save_deviations(instance, savedev);
    }
} /* ncx_clean_save_deviationsQ */


/********************************************************************
* FUNCTION ncx_set_error
* 
* Set the fields in an ncx_error_t struct
* When called from NACM or <get> internally, there is no
* module or line number info
*
* INPUTS:
*   tkerr== address of ncx_error_t struct to set
*   mod == [sub]module containing tkerr
*   linenum == current linenum
*   linepos == current column position on the current line
*
* OUTPUTS:
*   *tkerr is filled in
*********************************************************************/
void ncx_set_error( ncx_error_t *tkerr,
                    ncx_module_t *mod,
                    uint32 linenum,
                    uint32 linepos )
{
    assert ( tkerr && " param tkerr is NULL" );
    tkerr->mod = mod;
    tkerr->linenum = linenum;
    tkerr->linepos = linepos;

}  /* ncx_set_error */


/********************************************************************
* FUNCTION ncx_set_temp_modQ
* 
* Set the temp_modQ for yangcli session-specific module list
*
* INPUTS:
*   modQ == new Q pointer to use
*
*********************************************************************/
void
    ncx_set_temp_modQ (ncx_instance_t *instance, dlq_hdr_t *modQ)
{
    assert ( modQ && " param modQ is NULL" );
    instance->temp_modQ = modQ;

}  /* ncx_set_temp_modQ */


/********************************************************************
* FUNCTION ncx_get_temp_modQ
* 
* Get the temp_modQ for yangcli session-specific module list
*
* RETURNS:
*   pointer to the temp modQ, if set
*********************************************************************/
dlq_hdr_t *
    ncx_get_temp_modQ (ncx_instance_t *instance)
{
    return instance->temp_modQ;

}  /* ncx_get_temp_modQ */


/********************************************************************
* FUNCTION ncx_clear_temp_modQ
* 
* Clear the temp_modQ for yangcli session-specific module list
*
*********************************************************************/
void
    ncx_clear_temp_modQ (ncx_instance_t *instance)
{
    instance->temp_modQ = NULL;

}  /* ncx_clear_temp_modQ */


/********************************************************************
* FUNCTION ncx_get_display_mode
* 
* Get the current default display mode
*
* RETURNS:
*  the current dispay mode enumeration
*********************************************************************/
ncx_display_mode_t
    ncx_get_display_mode (ncx_instance_t *instance)
{
    return instance->display_mode;

}  /* ncx_get_display_mode */


/********************************************************************
* FUNCTION ncx_get_confirm_event_str
* 
* Get the string for the specified enum value
* 
* INPUT:
*   event == enum confirm event value to convert
*
* RETURNS:
*   string value for the enum
*   NULL if none found
*********************************************************************/
const xmlChar *
    ncx_get_confirm_event_str (ncx_confirm_event_t event)
{
    switch (event) {
    case NCX_CC_EVENT_NONE:
        return NULL;
    case NCX_CC_EVENT_START:
        return NCX_EL_START;
    case NCX_CC_EVENT_CANCEL:
        return NCX_EL_CANCEL;
    case NCX_CC_EVENT_TIMEOUT:
        return NCX_EL_TIMEOUT;
    case NCX_CC_EVENT_EXTEND:
        return NCX_EL_EXTEND;
    case NCX_CC_EVENT_COMPLETE:
        return NCX_EL_COMPLETE;
    default:
        return NULL;
    }

}  /* ncx_get_confirm_event_str */


/********************************************************************
* FUNCTION ncx_mod_revision_count
*
* Find all the ncx_module_t structs in the ncx_modQ
* that have the same module name
*
* INPUTS:
*   modname == module name
*
* RETURNS:
*   count of modules that have this name (exact match)
*********************************************************************/
uint32
    ncx_mod_revision_count (ncx_instance_t *instance, const xmlChar *modname)
{
    assert ( modname && " param modname is NULL" );

    uint32        count;

    if (instance->ncx_sesmodQ != NULL) {
        count = ncx_mod_revision_count_que(instance, instance->ncx_sesmodQ, modname);
    } else {
        count = ncx_mod_revision_count_que(instance, instance->ncx_curQ, modname);
    }

    return count;

}   /* ncx_mod_revision_count */


/********************************************************************
* FUNCTION ncx_mod_revision_count_que
*
* Find all the ncx_module_t structs in the specified queue
* that have the same module name
*
* INPUTS:
*   modQ == queue of ncx_module_t structs to check
*   modname == module name
*
* RETURNS:
*   count of modules that have this name (exact match)
*********************************************************************/
uint32
    ncx_mod_revision_count_que (ncx_instance_t *instance,
                                dlq_hdr_t *modQ,
                                const xmlChar *modname)
{
    assert ( modQ && " param modQ is NULL" );
    assert ( modname && " param modname is NULL" );

    ncx_module_t *mod;
    uint32        count;
    int32         retval;

    count = 0;
    for (mod = (ncx_module_t *)dlq_firstEntry(instance, modQ);
         mod != NULL;
         mod = (ncx_module_t *)dlq_nextEntry(instance, mod)) {

        retval = xml_strcmp(instance, modname, mod->name);
        if (retval == 0) {
            count++;
        }
    }
    return count;

}   /* ncx_mod_revision_count_que */


/********************************************************************
* FUNCTION ncx_get_allincQ
*
* Find the correct Q of yang_node_t for all include files
* that have the same 'belongs-to' value
*
* INPUTS:
*   mod == module to check
*
* RETURNS:
*   pointer to Q  of all include nodes
*********************************************************************/
dlq_hdr_t *
    ncx_get_allincQ (ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL" );

    while (mod->parent != NULL) {
        mod = mod->parent;
    }
    return &mod->allincQ;

}   /* ncx_get_allincQ */


/********************************************************************
* FUNCTION ncx_get_const_allincQ
*
* Find the correct Q of yang_node_t for all include files
* that have the same 'belongs-to' value (const version)
*
* INPUTS:
*   mod == module to check
*
* RETURNS:
*   pointer to Q  of all include nodes
*********************************************************************/
const dlq_hdr_t *
    ncx_get_const_allincQ (const ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL" );

    while (mod->parent != NULL) {
        mod = mod->parent;
    }
    return &mod->allincQ;

}   /* ncx_get_allincQ */


/********************************************************************
* FUNCTION ncx_get_parent_mod
*
* Find the correct module by checking mod->parent nodes
*
* INPUTS:
*   mod == module to check
*
* RETURNS:
*   pointer to parent module (!! not submodule !!)
*   NULL if none found
*********************************************************************/
ncx_module_t *
    ncx_get_parent_mod (ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL" );

    while (mod->parent != NULL) {
        mod = mod->parent;
        if (mod != NULL && mod->ismod) {
            return mod;
        }
    }
    return NULL;

}   /* ncx_get_parent_mod */


/********************************************************************
* FUNCTION ncx_get_vtimeout_value
*
* Get the virtual node cache timeout value
*
* RETURNS:
*   number of seconds for the cache timeout; 0 == disabled
*********************************************************************/
uint32
    ncx_get_vtimeout_value (void)
{
    /* TBD: make configurable */
    return NCX_DEF_VTIMEOUT;

}   /* ncx_get_vtimeout_value */


/********************************************************************
* FUNCTION ncx_compare_base_uris
*
* Compare the base part of 2 URI strings
*
* INPUTS:
*    str1 == URI string 1
*    str2 == URI string 2
*
* RETURNS:
*   compare of base parts (up to '?')
*   -1, 0 or 1
*********************************************************************/
int32
    ncx_compare_base_uris (ncx_instance_t *instance,
                           const xmlChar *str1,
                           const xmlChar *str2)
{ 
    assert ( str1 && " param str1 is NULL" );
    assert ( str2 && " param str2 is NULL" );

    const xmlChar *s;
    uint32  len1, len2; 

    s = str1;
    while (*s && *s != '?') {
        s++;
    }
    len1 = (uint32)(s - str1);

    s = str2;
    while (*s && *s != '?') {
        s++;
    }
    len2 = (uint32)(s - str2);

    if (len1 != len2) {
        return (len1 > len2) ? 1 : -1;
    }

    if (len1 == 0) {
        return 0;
    }

    return xml_strncmp(instance, str1, str2, len1);

}   /* ncx_compare_base_uris */


/********************************************************************
* FUNCTION ncx_get_useprefix
*
* Get the use_prefix value
*
* RETURNS:
*   TRUE if XML prefixes should be used
*   FALSE if XML messages should use default namespace (no prefix)
*********************************************************************/
boolean
    ncx_get_useprefix (ncx_instance_t *instance)
{
    return instance->use_prefix;

}   /* ncx_get_useprefix */


/********************************************************************
* FUNCTION ncx_set_useprefix
*
* Set the use_prefix value
*
* INPUTS:
*   val == 
*      TRUE if XML prefixes should be used
*      FALSE if XML messages should use default namespace (no prefix)
*********************************************************************/
void
    ncx_set_useprefix (ncx_instance_t *instance, boolean val)
{
    instance->use_prefix = val;

}   /* ncx_set_useprefix */


/********************************************************************
* FUNCTION ncx_get_system_sorted
*
* Get the system_sorted value
*
* RETURNS:
*   TRUE if system ordered objects should be sorted
*   FALSE if system ordered objects should not be sorted
*********************************************************************/
boolean
    ncx_get_system_sorted (ncx_instance_t *instance)
{
    return instance->system_sorted;

}   /* ncx_get_system_sorted */


/********************************************************************
* FUNCTION ncx_set_system_sorted
*
* Set the system_sorted value
*
* INPUTS:
*   val == 
*     TRUE if system ordered objects should be sorted
*     FALSE if system ordered objects should not be sorted
*********************************************************************/
void
    ncx_set_system_sorted (ncx_instance_t *instance, boolean val)
{
    instance->system_sorted = val;

}   /* ncx_set_system_sorted */


/********************************************************************
* FUNCTION ncx_inc_warnings
*
* Increment the module warning count
*
* INPUTS:
*   mod == module being parsed
*********************************************************************/
void
    ncx_inc_warnings (ncx_module_t *mod)
{
    assert ( mod && " param mod is NULL" );
    mod->warnings++;

}   /* ncx_inc_warnings */


/********************************************************************
* FUNCTION ncx_get_cwd_subdirs
*
* Get the CLI parameter value whether to search for modules
* in subdirs of the CWD by default.  Does not affect YUMA_MODPATH
* or other hard-wired searches
*
* RETURNS:
*   TRUE if ncxmod should search for modules in subdirs of the CWD
*   FALSE if ncxmod should not search for modules in subdirs of the CWD
*********************************************************************/
boolean
    ncx_get_cwd_subdirs (ncx_instance_t *instance)
{
    return instance->cwd_subdirs;
}   /* ncx_get_cwd_subdirs */


/********************************************************************
* FUNCTION ncx_protocol_enabled
*
* Check if the specified protocol version is enabled
*
* RETURNS:
*   TRUE if protocol enabled
*   FALSE if protocol not enabled or error
*********************************************************************/
boolean
    ncx_protocol_enabled (ncx_instance_t *instance, ncx_protocol_t proto)
{
    boolean ret = FALSE;
    switch (proto) {
    case NCX_PROTO_NETCONF10:
        ret = (instance->protocols_enabled & NCX_FL_PROTO_NETCONF10) ? TRUE : FALSE;
        break;
    case NCX_PROTO_NETCONF11:
        ret = (instance->protocols_enabled & NCX_FL_PROTO_NETCONF11) ? TRUE : FALSE;
        break;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
    }
    return ret;

}   /* ncx_protocol_enabled */


/********************************************************************
* FUNCTION ncx_set_protocol_enabled
*
* Set the specified protocol version to be enabled
*
* INPUTS:
*   proto == protocol version to enable
*********************************************************************/
void
    ncx_set_protocol_enabled (ncx_instance_t *instance, ncx_protocol_t proto)
{
    switch (proto) {
    case NCX_PROTO_NETCONF10:
        instance->protocols_enabled |= NCX_FL_PROTO_NETCONF10;
        break;
    case NCX_PROTO_NETCONF11:
        instance->protocols_enabled |= NCX_FL_PROTO_NETCONF11;
        break;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

}   /* ncx_set_protocol_enabled */


/********************************************************************
* FUNCTION ncx_set_use_deadmodQ
*
* Set the usedeadmodQ flag
*
*********************************************************************/
void
    ncx_set_use_deadmodQ (ncx_instance_t *instance)
{
    instance->usedeadmodQ = TRUE;

}   /* ncx_set_use_deadmodQ */


/********************************************************************
* FUNCTION ncx_delete_all_obsolete_objects
* 
* Go through all the modules and delete the obsolete nodes
* 
*********************************************************************/
void
    ncx_delete_all_obsolete_objects (ncx_instance_t *instance)
{
    ncx_module_t   *mod;

    for (mod = ncx_get_first_module(instance);
         mod != NULL;
         mod = ncx_get_next_module(instance, mod)) {

        ncx_delete_mod_obsolete_objects(instance, mod);
    }

}  /* ncx_delete_all_obsolete_objects */


/********************************************************************
* FUNCTION ncx_delete_mod_obsolete_objects
* 
* Go through one module and delete the obsolete nodes
* 
* INPUTS:
*    mod == module to check
*********************************************************************/
void
    ncx_delete_mod_obsolete_objects (ncx_instance_t *instance, ncx_module_t *mod)
{
    yang_node_t    *node;

    obj_delete_obsolete(instance, &mod->datadefQ);

    if (!mod->ismod) {
        return;
    }

    for (node = (yang_node_t *)dlq_firstEntry(instance, &mod->allincQ);
         node != NULL;
         node = (yang_node_t *)dlq_nextEntry(instance, node)) {

        if (node->submod == NULL) {
            SET_ERROR(instance, ERR_INTERNAL_PTR);
            continue;
        }

        obj_delete_obsolete(instance, &node->submod->datadefQ);
    }

}  /* ncx_delete_mod_obsolete_objects */


/********************************************************************
* FUNCTION ncx_get_name_match_enum
* 
* Get the enum for the string name of a ncx_name_match_t enum
* 
* INPUTS:
*   str == string name of the enum value 
*
* RETURNS:
*   enum value
*********************************************************************/
ncx_name_match_t
    ncx_get_name_match_enum (ncx_instance_t *instance, const xmlChar *str)
{
    assert ( str && " param str is NULL" );

    if (!xml_strcmp(instance, NCX_EL_EXACT, str)) {
        return NCX_MATCH_EXACT;
    } else if (!xml_strcmp(instance, NCX_EL_EXACT_NOCASE, str)) {
        return NCX_MATCH_EXACT_NOCASE;
    } else if (!xml_strcmp(instance, NCX_EL_ONE, str)) {
        return NCX_MATCH_ONE;
    } else if (!xml_strcmp(instance, NCX_EL_ONE_NOCASE, str)) {
        return NCX_MATCH_ONE_NOCASE;
    } else if (!xml_strcmp(instance, NCX_EL_FIRST, str)) {
        return NCX_MATCH_FIRST;
    } else if (!xml_strcmp(instance, NCX_EL_FIRST_NOCASE, str)) {
        return NCX_MATCH_FIRST_NOCASE;
    } else {
        return NCX_MATCH_NONE;
    }
    /*NOTREACHED*/

}  /* ncx_get_name_match_enum */


/********************************************************************
* FUNCTION ncx_get_name_match_string
* 
* Get the string for the ncx_name_match_t enum
* 
* INPUTS:
*   match == enum value 
*
* RETURNS:
*   string value
*********************************************************************/
const xmlChar *
    ncx_get_name_match_string (ncx_instance_t *instance, ncx_name_match_t match)
{
    switch (match) {
    case NCX_MATCH_NONE:
        return NCX_EL_NONE;
    case NCX_MATCH_EXACT:
        return NCX_EL_EXACT;
    case NCX_MATCH_EXACT_NOCASE:
        return NCX_EL_EXACT_NOCASE;
    case NCX_MATCH_ONE:
        return NCX_EL_ONE;
    case NCX_MATCH_ONE_NOCASE:
        return NCX_EL_ONE_NOCASE;
    case NCX_MATCH_FIRST:
        return NCX_EL_FIRST;
    case NCX_MATCH_FIRST_NOCASE:
        return NCX_EL_FIRST_NOCASE;
    default:
        SET_ERROR(instance, ERR_INTERNAL_VAL);
        return NCX_EL_NONE;
    }
    /*NOTREACHED*/

}  /* ncx_get_name_match_string */


/********************************************************************
* FUNCTION ncx_write_tracefile
* 
* Write a byte to the tracefile
* 
* INPUTS:
*   buff == buffer to write
*   count == number of chars to write
*********************************************************************/
void
    ncx_write_tracefile (ncx_instance_t *instance, const char *buff, uint32 count)
{
    uint32 i;

    if (instance->tracefile != NULL) {
        for (i = 0; i < count; i++) {
            fputc((int)buff[i], instance->tracefile);
        }
    }
}  /* ncx_write_tracefile */


/********************************************************************
* FUNCTION ncx_set_top_mandatory_allowed
* 
* Allow or disallow modules with top-level mandatory object
* to be loaded; used by the server when agt_running_error is TRUE
*
* INPUTS:
*   allowed == value to set T: to allow; F: to disallow
*********************************************************************/
void
    ncx_set_top_mandatory_allowed (ncx_instance_t *instance, boolean allowed)
{
    instance->allow_top_mandatory = allowed;
}

/********************************************************************
* FUNCTION ncx_get_top_mandatory_allowed
* 
* Check if top-level mandatory objects are allowed or not
*
* RETURNS:
*   T: allowed; F: disallowed
*********************************************************************/
boolean
    ncx_get_top_mandatory_allowed (ncx_instance_t *instance)
{
    return instance->allow_top_mandatory;
}


/* END file ncx.c */
