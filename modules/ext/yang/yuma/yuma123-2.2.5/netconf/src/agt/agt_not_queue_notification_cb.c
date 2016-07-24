/*  FILE: agt_not_queue_notification_cb.c 

*********************************************************************
* P U R P O S E
*********************************************************************
    NETCONF Server agt_not_queue_notification callback handler
    This file contains functions to support registering, 
    unregistering and execution of agt_not_queue_notification callbacks. 
*/

/********************************************************************
* Y U M A    H E A D E R S
*********************************************************************/
#include "agt_not.h"
#include "agt_not_queue_notification_cb.h"

#include "dlq.h"
#include "xml_util.h"

/********************************************************************
* S T A N D A R D   H E A D E R S
*********************************************************************/
#include <string.h>
#include <assert.h>

/********************************************************************
* T Y P E S
*********************************************************************/
typedef struct agt_cb_queue_notification_set_t_ {
    dlq_hdr_t                  qhdr;
    xmlChar                   *modname;
    agt_not_queue_notification_cb_t   callback;
} agt_cb_queue_notification_set_t;
    
/**************    S T A T I C   D A T A ****************************/
static bool initialised = false;
static dlq_hdr_t callbackQ;

/**************    S T A T I C   F U N C T I O N S ******************/

/********************************************************************/
static void free_callback_set(ncx_instance_t *instance,  agt_cb_queue_notification_set_t* cbSet )
{
    if ( cbSet->modname )
    {
        m__free(instance,  cbSet->modname );
    }
    m__free(instance,  cbSet );
}

/********************************************************************/
static agt_cb_queue_notification_set_t* new_callback_set(ncx_instance_t *instance,  const xmlChar *modname )
{
    agt_cb_queue_notification_set_t* cbSet = m__getObj(instance,  
            agt_cb_queue_notification_set_t );

    if ( !cbSet )
    {
        return NULL;
    }

    memset( cbSet, 0, sizeof( agt_cb_queue_notification_set_t ) );
    cbSet->modname = xml_strdup(instance,  modname );
    if ( !cbSet->modname )
    {
        m__free(instance,  cbSet );
        return NULL;
    }

    return cbSet;
}

/********************************************************************/
static agt_cb_queue_notification_set_t* find_callback_set(ncx_instance_t *instance,  const xmlChar *modname )
{
    agt_cb_queue_notification_set_t* cbSet;

    for ( cbSet = ( agt_cb_queue_notification_set_t* )dlq_firstEntry(instance,  &callbackQ );
          cbSet != NULL;
          cbSet = ( agt_cb_queue_notification_set_t* )dlq_nextEntry(instance,  cbSet ) )
    {
        if ( 0==xml_strcmp(instance,  modname, cbSet->modname ) )
        {
            return cbSet;
        }
    }

    return NULL;
}

/**************    E X T E R N A L   F U N C T I O N S **************/

/********************************************************************/
void agt_not_queue_notification_cb_init( ncx_instance_t *instance )
{
    if ( !initialised )
    {
        dlq_createSQue(instance,  &callbackQ );
        initialised = true;
    }
} /* agt_not_queue_notification_callbacks_init */

/********************************************************************/
void agt_not_queue_notification_cb_cleanup( ncx_instance_t *instance )
{
    if ( initialised )
    {
        agt_cb_queue_notification_set_t* cbSet;

        while ( !dlq_empty(instance,  &callbackQ ) )
        {
            cbSet = ( agt_cb_queue_notification_set_t* )dlq_deque(instance,  &callbackQ );
            free_callback_set(instance,  cbSet );
        }
        initialised = false;
    }
} /* agt_not_queue_notification_callbacks_cleanup */

/********************************************************************/
status_t agt_not_queue_notification_cb_register(ncx_instance_t *instance,
                                        const xmlChar *modname,
                                       agt_not_queue_notification_cb_t cb )
{
    assert( modname );

    agt_cb_queue_notification_set_t* cbSet = find_callback_set(instance,  modname );

    if ( !cbSet )
    {
        cbSet = new_callback_set(instance,  modname );
        if ( !cbSet )
        {
            return ERR_INTERNAL_MEM;
        }

        dlq_enque(instance,  cbSet, &callbackQ );
    }

    cbSet->callback = cb;
    return NO_ERR;
}

/********************************************************************/
void agt_not_queue_notification_cb_unregister(ncx_instance_t *instance,  const xmlChar *modname )
{
    assert( modname );

    agt_cb_queue_notification_set_t* cbSet = find_callback_set(instance,  modname );

    if ( cbSet )
    {
        dlq_remove(instance,  cbSet );
        free_callback_set(instance,  cbSet );
    }
}

/********************************************************************/
status_t agt_not_queue_notification_cb(ncx_instance_t *instance,  agt_not_msg_t *notif )
{
    agt_cb_queue_notification_set_t* cbSet;
    (void)instance;

    for ( cbSet = ( agt_cb_queue_notification_set_t* )dlq_firstEntry(instance,  &callbackQ );
          cbSet != NULL;
          cbSet = ( agt_cb_queue_notification_set_t* )dlq_nextEntry(instance,  cbSet ) )
    {
        if ( cbSet->callback )
        {
            status_t res = cbSet->callback(notif);

            if ( NO_ERR != res )
            {
                return res;
            }
        }
    }

    return NO_ERR;
}


/* END file agt_not_queue_notification_cb.c */

