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
/*  FILE: netconfd.c

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
04-jun-06    abb      begun; cloned from ncxmain.c
250aug-06    abb      renamed from ncxagtd.c to netconfd.c

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef MEMORY_DEBUG
#include <mcheck.h>
#endif

#include <pthread.h>

#define _C_main 1

#include  "procdefs.h"
#include  "agt.h"
#include  "agt_ncxserver.h"
#include  "agt_util.h"
#include  "help.h"
#include  "log.h"
#include  "ncx.h"
#include  "ncxconst.h"
#include  "ncxmod.h"
#include  "status.h"
#include  "xmlns.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/
#define NETCONFD_MOD       (const xmlChar *)"netconfd"
#define NETCONFD_CLI       (const xmlChar *)"netconfd"

#define START_MSG          "Starting netconfd...\n"

/********************************************************************
 * FUNCTION load_base_schema 
 * 
 * RETURNS:
 *     status
 *********************************************************************/
static status_t load_base_schema (ncx_instance_t *instance)
{
    status_t res;

    /* load in the NETCONF data types and RPC methods */
    res = ncxmod_load_module(instance,  NCXMOD_YUMA_NETCONF, NULL, NULL, NULL );
    if (res != NO_ERR) {
        return res;
    }

    /* load in the server boot parameter definition file */
    res = ncxmod_load_module(instance,  NCXMOD_NETCONFD, NULL, NULL, NULL );
    if (res != NO_ERR) {
        return res;
    }

    return res;
}   /* load_base_schema */

#ifdef NETCONFD_DEBUG_LOAD_TEST
/********************************************************************
 * FUNCTION load_debug_test_module 
 * 
 * RETURNS:
 *     status
 *********************************************************************/
static status_t load_debug_test_module(ncx_instance_t *instance,  agt_profile_t* profile )
{
#define TESTMOD             (const xmlChar *)"test"
#define TESTFEATURE1        (const xmlChar *)"feature1"
#define TESTFEATURE2        (const xmlChar *)"feature2"
#define TESTFEATURE3        (const xmlChar *)"feature3"
#define TESTFEATURE4        (const xmlChar *)"feature4"

    log_debug2(instance, "\nnetconfd: Loading Debug Test Module");

    /* Load test module */
    status_t res = ncxmod_load_module(instance,  TESTMOD, NULL, 
                                       &profile->agt_savedevQ, NULL );

    if (res != NO_ERR) {
        return res;
    } else {
        agt_enable_feature(instance, TESTMOD, TESTFEATURE1);
        agt_disable_feature(instance, TESTMOD, TESTFEATURE2);
        agt_enable_feature(instance, TESTMOD, TESTFEATURE3);
        agt_disable_feature(instance, TESTMOD, TESTFEATURE4);
    }
}
#endif

/********************************************************************
 * FUNCTION load_core_schema 
 * 
 * RETURNS:
 *     status
 *********************************************************************/
static status_t load_core_schema (ncx_instance_t *instance,  agt_profile_t *profile )
{
    log_debug2(instance, "\nnetconfd: Loading NCX Module");

    /* load in the with-defaults extension module */
    status_t res = ncxmod_load_module(instance,  NCXMOD_WITH_DEFAULTS, NULL, 
                                      &profile->agt_savedevQ, NULL );
    if (res != NO_ERR) {
        return res;
    }


#ifdef NETCONFD_DEBUG_LOAD_TEST
    res = load_debug_test_module(instance,  profile );
#endif

    return res;

}  /* load_core_schema */


/********************************************************************
 * FUNCTION cmn_init
 * 
 * 
 * 
 *********************************************************************/
static status_t cmn_init (ncx_instance_t *instance,  int argc, char *argv[], boolean *showver,
                           help_mode_t *showhelpmode)
{
#define BUFFLEN 256

    status_t     res;
    log_debug_t  dlevel;
    int          len;
    char         buff[BUFFLEN];

    /* set the default debug output level */
    dlevel = LOG_DEBUG_INFO;

    /* initialize the NCX Library first to allow NCX modules to be processed.  
     * No module can get its internal config until the NCX module parser and 
     * definition registry is up */
    len = strlen(START_MSG) + strlen(COPYRIGHT_STRING) + 2;

    if (len < BUFFLEN) {
        strcpy(buff, START_MSG);
        strcat(buff, COPYRIGHT_STRING);
    } else {
        return ERR_BUFF_OVFL;
    }

    res = ncx_init(instance,  FALSE, dlevel, TRUE, buff, argc, argv);

    if (res != NO_ERR) {
        return res;
    }

    log_debug2(instance, "\nnetconfd: Loading Netconf Server Library");

    /* at this point, modules that need to read config params can be 
     * initialized */

    /* Load the core modules (netconfd and netconf) */
    res = load_base_schema(instance);
    if (res != NO_ERR) {
        return res;
    }

    /* Initialize the Netconf Server Library with command line and conf file 
     * parameters */
    res = agt_init1(instance, argc, argv, showver, showhelpmode);
    if (res != NO_ERR) {
        return res;
    }

    /* check quick-exit mode */
    if (*showver || *showhelpmode != HELP_MODE_NONE) {
        return NO_ERR;
    }

    /* Load the core modules (netconfd and netconf) */
    res = load_core_schema(instance, agt_get_profile());
    if (res != NO_ERR) {
        return res;
    }

    /* finidh initializing server data structures */
    res = agt_init2(instance);
    if (res != NO_ERR) {
        return res;
    }

    log_debug(instance, "\nnetconfd init OK, ready for sessions\n");

    return NO_ERR;

}  /* cmn_init */


/********************************************************************
 * FUNCTION show_server_banner
 *  
 * Show startup server string
 * 
 *********************************************************************/
static void show_server_banner (ncx_instance_t *instance)
{
#define BANNER_BUFFLEN 32

    xmlChar buff[BANNER_BUFFLEN];
    status_t  res;

    if (LOGINFO) {
        res = ncx_get_version(instance, buff, BANNER_BUFFLEN);
        if (res == NO_ERR) {
            log_info(instance, "\nRunning netconfd server (%s)\n", buff);
        } else {
            log_info(instance, "\nRunning netconfd server\n");
        }
    }
    
}  /* show_server_banner */


/********************************************************************
 * FUNCTION netconfd_run
 *  
 * Startup and run the NCX server loop
 * 
 * RETURNS:
 *    status:  NO_ERR if startup OK and then run OK this will be a delayed 
 *             return code some error if server startup failed
 *             e.g., socket already in use
 *********************************************************************/
static status_t
    netconfd_run (ncx_instance_t *instance)
{
    status_t  res;

    show_server_banner(instance);
    res = agt_ncxserver_run(instance);
    if (res != NO_ERR) {
        log_error(instance, "\nncxserver failed (%s)", get_error_string(res));
    }
    return res;
    
}  /* netconfd_run */


/********************************************************************
 * FUNCTION netconfd_cleanup
 *********************************************************************/
static void netconfd_cleanup (ncx_instance_t *instance)
{

    if (LOGINFO) {
        log_info(instance, "\nShutting down the netconfd server\n");
    }

    /* Cleanup the Netconf Server Library */
    agt_cleanup(instance);

    /* cleanup the NCX engine and registries */
    ncx_cleanup(instance);

}  /* netconfd_cleanup */

/********************************************************************
 * FUNCTION show_version
 *********************************************************************/
static void show_version(ncx_instance_t *instance)
{
    xmlChar versionbuffer[NCX_VERSION_BUFFSIZE];

    status_t res = ncx_get_version(instance, versionbuffer, NCX_VERSION_BUFFSIZE);
    if (res == NO_ERR) {
        log_write(instance,  "\nnetconfd version %s\n", versionbuffer );
    } else {
        SET_ERROR(instance,  res );
    }
    agt_request_shutdown(instance, NCX_SHUT_EXIT);
}

/********************************************************************
*                                                                   *
*                       FUNCTION main                               *
*                                                                   *
*********************************************************************/
int main (int argc, char *argv[])
{
    status_t           res;
    boolean            showver = FALSE;
    boolean            done = FALSE;
    help_mode_t        showhelpmode;

#ifdef MEMORY_DEBUG
    mtrace();
#endif

    /* this loop is used to implement the restart command the sw image is not 
     * reloaded; instead everything is cleaned up and re-initialized from 
     * scratch. If the shutdown operation (or Ctl-C exit) is used instead of 
     * restart, then the loop will only be executed once */
    while (!done) {
        ncx_instance_t* instance = ncx_alloc();
        res = cmn_init( instance, argc, argv, &showver, &showhelpmode );
    
        if (res != NO_ERR) {
            log_error( instance,
                       "\nnetconfd: init returned (%s)", 
                       get_error_string(res) );
            agt_request_shutdown(instance, NCX_SHUT_EXIT);
        } else {
            if (showver) {
                show_version(instance);
            } else if (showhelpmode != HELP_MODE_NONE) {
                help_program_module(instance, NETCONFD_MOD, NETCONFD_CLI, showhelpmode);
                agt_request_shutdown(instance, NCX_SHUT_EXIT);
            } else {
                res = netconfd_run(instance);
                if (res != NO_ERR) {
                    agt_request_shutdown(instance, NCX_SHUT_EXIT);
                }
            }
        }

        netconfd_cleanup(instance);
        print_error_count(instance);

        if ( NCX_SHUT_EXIT == agt_shutdown_mode_requested() ) {
            done = TRUE;
        }
    }

    // ATTN: print_errors(instance);
    // ATTN: print_error_count(instance);

    // ATTN: if ( !log_is_open(instance) ) {
    // ATTN:     printf("\n");
    // ATTN: }

#ifdef MEMORY_DEBUG
    muntrace();
#endif

    return 0;
} /* main */

/* END netconfd.c */



