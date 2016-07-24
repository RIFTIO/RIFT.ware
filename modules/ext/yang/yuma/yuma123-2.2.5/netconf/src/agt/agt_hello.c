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
/*  FILE: agt_hello.c

    Handle the NETCONF <hello> (top-level) element.

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
15jan07      abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "procdefs.h"
#include "agt.h"
#include "agt_cap.h"
#include "agt_hello.h"
#include "agt_ses.h"
#include "agt_util.h"
#include "agt_val_parse.h"
#include "cap.h"
#include "log.h"
#include "ncx.h"
#include "obj.h"
#include "op.h"
#include "ses.h"
#include "status.h"
#include "top.h"
#include "val.h"
#include "xml_wr.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

#define CLIENT_HELLO_CON ((const xmlChar *)"client-hello")

/********************************************************************
*                                                                   *
*                       V A R I A B L E S                            *
*                                                                   *
*********************************************************************/
static boolean agt_hello_init_done = FALSE;

static ses_total_stats_t   *mytotals;

/********************************************************************
* FUNCTION check_manager_hello
*
* Verify that the same NETCONF protocol verion is supported
* by the manager and this server
* 
* INPUTS:
*    scb == session control block
*    val == value struct for the hello message to check
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    check_manager_hello (ncx_instance_t *instance,
                         ses_cb_t *scb,
                         val_value_t *val)
{
    val_value_t  *caps, *cap;

    /* look for the NETCONF base capability string */
    caps = val_find_child(instance, val, NC_MODULE, NCX_EL_CAPABILITIES);
    if (caps && caps->res == NO_ERR) {

        if (ncx_protocol_enabled(instance, NCX_PROTO_NETCONF11)) {
            for (cap = val_find_child(instance, caps, NC_MODULE, NCX_EL_CAPABILITY);
                 cap != NULL;
                 cap = val_find_next_child(instance, 
                                           caps, 
                                           NC_MODULE, 
                                           NCX_EL_CAPABILITY, 
                                           cap)) {
                if (cap->res == NO_ERR) {
                    if (!xml_strcmp(instance, VAL_STR(cap), CAP_BASE_URN11)) {
                        if (LOGDEBUG3) {
                            log_debug3(instance, "\nagt_hello: set "
                                       "protocol to base:1.1");
                        }
                        ses_set_protocol(instance, scb, NCX_PROTO_NETCONF11);
                        return NO_ERR;
                    }
                }
            }
        }

        if (ncx_protocol_enabled(instance, NCX_PROTO_NETCONF10)) {
            for (cap = val_find_child(instance, caps, NC_MODULE, NCX_EL_CAPABILITY);
                 cap != NULL;
                 cap = val_find_next_child(instance, 
                                           caps, 
                                           NC_MODULE, 
                                           NCX_EL_CAPABILITY, 
                                           cap)) {
                if (cap->res == NO_ERR) {
                    if (!xml_strcmp(instance, VAL_STR(cap), CAP_BASE_URN)) {
                        if (LOGDEBUG3) {
                            log_debug3(instance, "\nagt_hello: set "
                                       "protocol to base:1.0");
                        }
                        ses_set_protocol(instance, scb, NCX_PROTO_NETCONF10);
                        return NO_ERR;
                    }
                }
            }
        }
    }

    log_info(instance, "\nagt_hello: no NETCONF base:1.0 or base:1.1 URI found");
    return ERR_NCX_MISSING_VAL_INST;

} /* check_manager_hello */


/********************************************************************
* FUNCTION agt_hello_init
*
* Initialize the agt_hello module
* Adds the agt_hello_dispatch function as the handler
* for the NETCONF <hello> top-level element.
*
* INPUTS:
*   none
* RETURNS:
*   NO_ERR if all okay, the minimum spare requests will be malloced
*********************************************************************/
status_t 
    agt_hello_init (ncx_instance_t *instance)
{
    status_t  res;

    if (!agt_hello_init_done) {
        res = top_register_node(instance, 
                                NC_MODULE, 
                                NCX_EL_HELLO, 
                                agt_hello_dispatch);
        if (res != NO_ERR) {
            return res;
        }
        mytotals = ses_get_total_stats(instance);
        agt_hello_init_done = TRUE;
    }
    return NO_ERR;


} /* agt_hello_init */


/********************************************************************
* FUNCTION agt_hello_cleanup
*
* Cleanup the agt_hello module.
* Unregister the top-level NETCONF <hello> element
*
*********************************************************************/
void 
    agt_hello_cleanup (ncx_instance_t *instance)
{
    if (agt_hello_init_done) {
        top_unregister_node(instance, NC_MODULE, NCX_EL_HELLO);
        agt_hello_init_done = FALSE;
    }

} /* agt_hello_cleanup */


/********************************************************************
* FUNCTION agt_hello_dispatch
*
* Handle an incoming <hello> message from the client
*
* INPUTS:
*   scb == session control block
*   top == top element descriptor
*********************************************************************/
void 
    agt_hello_dispatch (ncx_instance_t *instance,
                        ses_cb_t *scb,
                        xml_node_t *top)
{
    val_value_t           *val;
    ncx_module_t          *mod;
    obj_template_t        *obj;
    xml_msg_hdr_t          msg;
    status_t               res;

#ifdef DEBUG
    if (!scb || !top) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    if (LOGDEBUG2) {
        log_debug2(instance, "\nagt_hello got node");
        if (LOGDEBUG3) {
            xml_dump_node(instance, top);
        }
    }

    /* only process this message in hello wait state */
    if (scb->state != SES_ST_HELLO_WAIT) {
        if (LOGINFO) {
            log_info(instance,
                     "\nagt_hello dropped, wrong state "
                     "(%d) for session %d",
                     scb->state, 
                     scb->sid);
        }
        mytotals->inBadHellos++;
        mytotals->droppedSessions++;
        agt_ses_request_close(instance,
                              scb,
                              scb->sid,
                              SES_TR_BAD_HELLO);
        return;
    }

    /* init local vars */
    res = NO_ERR;
    obj = NULL;
    xml_msg_init_hdr(instance, &msg);

    /* get a value struct to hold the client hello msg */
    val = val_new_value(instance);
    if (!val) {
        res = ERR_INTERNAL_MEM;
    }

    /* get the type definition from the registry */
    if (res == NO_ERR) {
        obj = NULL;
        mod = ncx_find_module(instance, NC_MODULE, NULL);
        if (mod) {
            obj = ncx_find_object(instance, mod, CLIENT_HELLO_CON);
        }
        if (!obj) {
            /* netconf module should have loaded this definition */
            res = SET_ERROR(instance, ERR_INTERNAL_PTR);
        }
    }

    /* parse a manager hello message */
    if (res == NO_ERR) {
        res = agt_val_parse_nc(instance, 
                               scb, 
                               &msg, 
                               obj, 
                               top, 
                               NCX_DC_STATE, 
                               val);
    }
    
    /* check that the NETCONF base capability is included
     * and it matches the server protocol version
     */
    if (res == NO_ERR) {
        res = check_manager_hello(instance, scb, val);
    }

    /* report first error and close session */
    if (res != NO_ERR) {
        if (LOGINFO) {
            log_info(instance,
                     "\nagt_connect error "
                     "(%s), dropping session %d",
                     get_error_string(res), 
                     scb->sid);
        }
        mytotals->inBadHellos++;
        mytotals->droppedSessions++;
        agt_ses_request_close(instance,
                              scb,
                              scb->sid,
                              SES_TR_BAD_HELLO);

    } else {
        scb->state = SES_ST_IDLE;
        scb->active = TRUE;
        /* start the timer for the first rpc request */
        (void)time(&scb->last_rpc_time);

        if (LOGINFO) {
            log_info(instance, 
                     "\nSession %d for %s@%s now active", 
                     scb->sid, 
                     scb->username, 
                     scb->peeraddr);
            if (ses_get_protocol(instance, scb) == NCX_PROTO_NETCONF11) {
                log_info(instance, " (base:1.1)");
            } else {
                log_info(instance, " (base:1.0)");
            }
        }

    }

    if (val) {
        val_free_value(instance, val);
    }

} /* agt_hello_dispatch */


/********************************************************************
* FUNCTION agt_hello_send
*
* Send the server <hello> message to the manager on the 
* specified session
*
* INPUTS:
*   scb == session control block
*
* RETURNS:
*   status
*********************************************************************/
status_t
    agt_hello_send (ncx_instance_t *instance, ses_cb_t *scb)
{
    val_value_t  *mycaps;
    xml_msg_hdr_t msg;
    status_t      res;
    xml_attrs_t   attrs;
    boolean       anyout;
    xmlns_id_t    nc_id;
    xmlChar       numbuff[NCX_MAX_NUMLEN];

#ifdef DEBUG
    if (!scb) {
        return SET_ERROR(instance, ERR_INTERNAL_PTR);
    }
#endif

    res = NO_ERR;
    anyout = FALSE;
    xml_msg_init_hdr(instance, &msg);
    xml_init_attrs(instance, &attrs);
    nc_id = xmlns_nc_id(instance);

    /* start the hello timeout */
    (void)time(&scb->hello_time);

    /* get the server caps */
    mycaps = agt_cap_get_capsval();
    if (!mycaps) {
        res = SET_ERROR(instance, ERR_INTERNAL_PTR);
    }

    /* setup the prefix map with the NETCONF and NCX namepsaces */
    if (res == NO_ERR) {
        res = xml_msg_build_prefix_map(instance, &msg, &attrs, TRUE, FALSE);
    }

    /* send the <?xml?> directive */
    if (res == NO_ERR) {
        res = ses_start_msg(instance, scb);
    }

    /* start the hello element */
    if (res == NO_ERR) {
        anyout = TRUE;
        xml_wr_begin_elem_ex(instance, 
                             scb, 
                             &msg, 
                             0, 
                             nc_id, 
                             NCX_EL_HELLO, 
                             &attrs, 
                             TRUE, 
                             0, 
                             FALSE);
    }
    
    /* send the capabilities list */
    if (res == NO_ERR) {
        xml_wr_full_val(instance, 
                        scb, 
                        &msg, 
                        mycaps, 
                        ses_indent_count(scb));
    }

    /* send the session ID */
    if (res == NO_ERR) {
        xml_wr_begin_elem(instance, 
                          scb, 
                          &msg, 
                          nc_id, 
                          nc_id,
                          NCX_EL_SESSION_ID, 
                          ses_indent_count(scb));
    }
    if (res == NO_ERR) {
        snprintf((char *)numbuff, sizeof(numbuff), "%d", scb->sid);
        ses_putstr(instance, scb, numbuff);
    }
    if (res == NO_ERR) {
        xml_wr_end_elem(instance, scb, &msg, nc_id, NCX_EL_SESSION_ID, -1);
    }

    /* finish the hello element */
    if (res == NO_ERR) {
        xml_wr_end_elem(instance, scb, &msg, nc_id, NCX_EL_HELLO, 0);
    }

    /* finish the message */
    if (anyout) {
        ses_finish_msg(instance, scb);
    }

    xml_clean_attrs(instance, &attrs);
    xml_msg_clean_hdr(instance, &msg);
    return res;

} /* agt_hello_send */


/* END file agt_hello.c */


