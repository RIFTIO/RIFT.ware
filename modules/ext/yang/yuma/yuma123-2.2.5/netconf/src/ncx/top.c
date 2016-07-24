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
/*  FILE: top.c

  NCX Common Top Element Handler

  This module uses a simple queue of top-level entries
  because there are not likely to be very many of them.

  Each top-level node is keyed by the owner name and 
  the element name.  
                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
30dec05      abb      begun
11-feb-07    abb      Move some common fns back to ncx/top.h

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <memory.h>

#include <xmlstring.h>
#include <xmlreader.h>

#ifndef _H_procdefs
#include  "procdefs.h"
#endif

#ifndef _H_dlq
#include "dlq.h"
#endif

#ifndef _H_log
#include "log.h"
#endif

#ifndef _H_ncxconst
#include "ncxconst.h"
#endif

#ifndef _H_ncx
#include "ncx.h"
#endif

#ifndef _H_status
#include  "status.h"
#endif

#ifndef _H_top
#include "top.h"
#endif

#ifndef _H_xmlns
#include "xmlns.h"
#endif

#ifndef _H_xml_util
#include "xml_util.h"
#endif

/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

#ifdef DEBUG
#define TOP_DEBUG 1
#endif

/********************************************************************
*                                                                   *
*                             T Y P E S                             *
*                                                                   *
*********************************************************************/

/* callback registry entry for (owner, element-name) pairs */
typedef struct top_entry_t_ {
    dlq_hdr_t      qhdr;          /* Q header for topQ */
    xmlChar       *owner;
    xmlChar       *elname;
    top_handler_t  handler;
} top_entry_t;

/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/


/********************************************************************
* FUNCTION find_entry
* 
* Find an entry in the topQ
*
* INPUTS:
*   owner == owner name
*   elname == element name
* RETURNS:
*  pointer to top_entry_t if found, NULL if not found
*********************************************************************/
static top_entry_t *
    find_entry (ncx_instance_t *instance,
                const xmlChar *owner,
                const xmlChar *elname)
{
    top_entry_t *en;

    for (en = (top_entry_t *)dlq_firstEntry(instance, &instance->topQ);
         en != NULL;
         en = (top_entry_t *)dlq_nextEntry(instance, en)) {
        if (!xml_strcmp(instance, en->owner, owner) &&
            !xml_strcmp(instance, en->elname, elname)) {
            return en;
        }
    }
    return NULL;

}  /* find_entry */


/**************    E X T E R N A L   F U N C T I O N S **********/


/********************************************************************
* FUNCTION top_init
* 
* Initialize the Top Elelment Handler
* 
*********************************************************************/
void
    top_init (ncx_instance_t *instance)
{
    if (!instance->top_init_done) {
        dlq_createSQue(instance, &instance->topQ);
        instance->top_init_done = TRUE;
    }
}  /* top_init */


/********************************************************************
* FUNCTION top_cleanup
*
*  cleanup Top Element Handler
*********************************************************************/
void
    top_cleanup (ncx_instance_t *instance)
{
    top_entry_t *en;

    if (instance->top_init_done) {
        while (!dlq_empty(instance, &instance->topQ)) {
            en = (top_entry_t *)dlq_deque(instance, &instance->topQ);
            m__free(instance, en);
        }
        memset(&instance->topQ, 0x0, sizeof(dlq_hdr_t));
        instance->top_init_done = FALSE;
    }
}   /* top_cleanup */


/********************************************************************
* FUNCTION top_register_node
* 
* Register a top entry handler function
*
* INPUTS:
*   owner == owner name  (really module name)
*   elname == element name
*   handler == callback function for this element
* RETURNS:
*  status
*********************************************************************/
status_t 
    top_register_node (ncx_instance_t *instance,
                       const xmlChar *owner,
                       const xmlChar *elname,
                       top_handler_t handler)
{
    top_entry_t *en;

    /* validate the parameters */
    if (!instance->top_init_done) {
        top_init(instance);
    }
    if (!owner || !elname || !handler) {
        return SET_ERROR(instance, ERR_INTERNAL_PTR);
    }
    if (!ncx_valid_name(owner, xml_strlen(instance, owner)) ||
        !ncx_valid_name(elname, xml_strlen(instance, elname))) {
        return SET_ERROR(instance, ERR_NCX_INVALID_NAME);
    }
    if (find_entry(instance, owner, elname)) {
        return SET_ERROR(instance, ERR_NCX_DUP_ENTRY);
    }

    /* add the new entry */
    en = m__getObj(instance, top_entry_t);
    if (!en) {
        return SET_ERROR(instance, ERR_INTERNAL_MEM);
    }
    memset(en, 0x0, sizeof(top_entry_t));

    en->owner = xml_strdup(instance, owner);
    if (!en->owner) {
        m__free(instance, en);
        return SET_ERROR(instance, ERR_INTERNAL_MEM);
    }

    en->elname = xml_strdup(instance, elname);
    if (!en->elname) {
        m__free(instance, en->owner);
        m__free(instance, en);
        return SET_ERROR(instance, ERR_INTERNAL_MEM);
    }

    en->handler = handler;
    dlq_enque(instance, en, &instance->topQ);
    return NO_ERR;

} /* top_register_node */


/********************************************************************
* FUNCTION top_unregister_node
* 
* Remove a top entry handler function
* This will not affect calls to the handler currently
* in progress, but new calls to top_dispatch_msg
* will no longer find this node.
*
* INPUTS:
*   owner == owner name
*   elname == element name
* RETURNS:
*   none
*********************************************************************/
void
    top_unregister_node (ncx_instance_t *instance,
                             const xmlChar *owner,
                             const xmlChar *elname)
{
    top_entry_t *en;

    if (!instance->top_init_done) {
        top_init(instance);
        SET_ERROR(instance, ERR_NCX_NOT_FOUND);
        return;
    }
    if (!owner || !elname) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
    
    en = find_entry(instance, owner, elname);
    if (!en) {
        SET_ERROR(instance, ERR_NCX_NOT_FOUND);
        return;
    }

    dlq_remove(instance, en);

    if (en->owner) {
        m__free(instance, en->owner);
    }
    if (en->elname) {
        m__free(instance, en->elname);
    }
    m__free(instance, en);

} /* top_unregister_node */


/********************************************************************
* FUNCTION top_find_handler
* 
* Find the top_handler in the topQ
*
* INPUTS:
*   owner == owner name
*   elname == element name
*
* RETURNS:
*  pointer to top_handler or NULL if not found
*********************************************************************/
top_handler_t
    top_find_handler (ncx_instance_t *instance,
                      const xmlChar *owner,
                      const xmlChar *elname)
{
    top_entry_t *en;

    en = find_entry(instance, owner, elname);
    return (en) ? en-> handler : NULL;

}  /* top_find_handler */


/* END file top.c */
