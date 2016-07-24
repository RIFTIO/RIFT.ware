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
/*  FILE: plock.c

   Partial lock control block
   Support for RFC 5717 operations
   Data structure definition

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
25jun10      abb      begun; split out from plock.c

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>

#ifndef _H_procdefs
#include  "procdefs.h"
#endif

#ifndef _H_cfg
#include  "cfg.h"
#endif

#ifndef _H_log
#include  "log.h"
#endif

#ifndef _H_plock_cb
#include  "plock_cb.h"
#endif

#ifndef _H_ses
#include  "ses.h"
#endif

#ifndef _H_status
#include  "status.h"
#endif

#ifndef _H_tstamp
#include  "tstamp.h"
#endif

#ifndef _H_val
#include  "val.h"
#endif

#ifndef _H_xmlns
#include  "xmlns.h"
#endif

#ifndef _H_xml_util
#include  "xml_util.h"
#endif

#ifndef _H_xpath
#include  "xpath.h"
#endif

#ifndef _H_xpath1
#include  "xpath1.h"
#endif


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/
#ifdef DEBUG
#define PLOCK_CB_DEBUG 1
#endif


/********************************************************************
*                                                                   *
*                           T Y P E S                               *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/


/********************************************************************
* FUNCTION plock_cb_new
*
* Create a new partial lock control block
*
* INPUTS:
*   sid == session ID reqauesting this partial lock
*   res == address of return status
*
* OUTPUTS:
*   *res == return status
*
* RETURNS:
*   pointer to initialized PLCB, or NULL if some error
*   this struct must be freed by the caller
*********************************************************************/
plock_cb_t *
    plock_cb_new (ncx_instance_t *instance,
                  uint32 sid,
                  status_t *res)
{
    plock_cb_t     *plcb;
    
#ifdef DEBUG
    if (res == NULL) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return NULL;
    }
#endif

    /* temp design: only 4G partial locks supported
     * until server stops giving out partial locks
     */
    if (instance->last_id == NCX_MAX_UINT) {
        *res = ERR_NCX_RESOURCE_DENIED;
        return NULL;
    }

    plcb = m__getObj(instance, plock_cb_t);
    if (plcb == NULL) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }
    memset(plcb, 0x0, sizeof(plock_cb_t));

    plcb->plock_final_result = xpath_new_result(instance, XP_RT_NODESET);
    if (plcb->plock_final_result == NULL) {
        *res = ERR_INTERNAL_MEM;
        m__free(instance, plcb);
        return NULL;
    }

    plcb->plock_id = ++instance->last_id;
    dlq_createSQue(instance, &plcb->plock_xpathpcbQ);
    dlq_createSQue(instance, &plcb->plock_resultQ);
    tstamp_datetime(instance, plcb->plock_time);
    plcb->plock_sesid = sid;
    return plcb;

}  /* plock_cb_new */


/********************************************************************
* FUNCTION plock_cb_free
*
* Free a partial lock control block
*
* INPUTS:
*   plcb == partial lock control block to free
*
*********************************************************************/
void plock_cb_free (ncx_instance_t *instance, plock_cb_t *plcb)
{
    if ( !plcb ) {
        return;
    }

    while (!dlq_empty(instance, &plcb->plock_xpathpcbQ)) {
        xpath_free_pcb(instance,  (xpath_pcb_t *) dlq_deque(instance, &plcb->plock_xpathpcbQ) );
    }

    while (!dlq_empty(instance, &plcb->plock_resultQ)) {
        xpath_free_result(instance,  (xpath_result_t *) dlq_deque(instance, &plcb->plock_resultQ) );
    }

    xpath_free_result(instance, plcb->plock_final_result);
    m__free(instance, plcb);
}  /* plock_cb_free */


/********************************************************************
* FUNCTION plock_cb_reset_id
*
* Set the next ID number back to the start
* Only the caller maintaining a queue of plcb
* can decide if the ID should rollover
*
*********************************************************************/
void plock_cb_reset_id (ncx_instance_t *instance)
{
    if (instance->last_id == NCX_MAX_UINT) {
        instance->last_id = 0;
    }

}  /* plock_cb_reset_id */


/* END file plock_cb.c */
