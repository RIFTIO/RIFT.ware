/** \internal */


/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_queue.h
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 * @brief (Internal?) queue object for rwmsg
 * @detail Internal use only
 */

#ifndef __RWMSG_QUEUE_H__
#define __RWMSG_QUEUE_H__

__BEGIN_DECLS

/*
 * Queue object
 */

typedef struct rwmsg_queue_s rwmsg_queue_t;

/**
 * Initialize a queue.  The queue itself lives in memory owned by the caller.
 * @param ep The endpoint.
 * @param q The queue, either allocated by or embedded in a caller structure.
 * @param flavor Short string name of the queue function or purpose,
 *        used as a key to look up properties at '/rwmsg/queue/%s/...'
 * @param notify Pointer to notification object to use, NULL to use a
 *        dedicated/internal one.
 */
rw_status_t rwmsg_queue_init(rwmsg_endpoint_t *ep,
			     rwmsg_queue_t *q, 
			     const char *flavor,
			     rwmsg_notify_t *notify);
/**
 * Deinitialize a queue.  It must be empty, or this will assert!
 * @param ep The endpoint.
 * @param q The queue.  This is not freed, although any associated data is.
 */
void rwmsg_queue_deinit(rwmsg_endpoint_t *ep,
			rwmsg_queue_t *q);

/**
 * Enqueue an rwmsg_request_t object.  If the request_t is actually a
 * request (as opposed to a response) the configured queue size and
 * length limits will be enforced, mainly the form of
 * RW_STATUS_BACKPRESSURE if the queue is full.  If the queue is full, TBD,
 * use another eventfd to signal writability.
 * @param q The queue to add to
 * @param req The request to enqueue
 * @return RW_STATUS_SUCCESS or various errors, most notably
 *         RW_STATUS_BACKPRESSURE if the thing is full.
 */
rw_status_t rwmsg_queue_enqueue(rwmsg_queue_t *q, 
				rwmsg_request_t *req);
/** 
 * Caution: mpsc safe only; unsuitable for mpmc.  Dequeue the next
 * highest priority rwmsg_request_t from the queue, or return NULL if
 * there isn't one.  TBD probably need one with a floor priority for
 * some scheduler tricks.
 */
rwmsg_request_t *rwmsg_queue_dequeue(rwmsg_queue_t *q);

/** 
 * Dequeue the next rwmsg_request_t from the queue of the given
 * priority, or return NULL if there isn't one or on occasion if there
 * is a write collision.
 */
rwmsg_request_t *rwmsg_queue_dequeue_pri(rwmsg_queue_t *q, 
					 rwmsg_priority_t pri);

rwmsg_request_t *rwmsg_queue_dequeue_pri_spin(rwmsg_queue_t *q, 
					      rwmsg_priority_t pri);
rwmsg_request_t *rwmsg_queue_head_pri(rwmsg_queue_t *q, 
				      rwmsg_priority_t pri);

void rwmsg_queue_set_cap(rwmsg_queue_t *q, int len, int size);
void rwmsg_queue_set_cap_default(rwmsg_queue_t *q, const char *flavor);

rwmsg_bool_t rwmsg_queue_ready(rwmsg_queue_t *q);
uint32_t rwmsg_queue_get_length(rwmsg_queue_t *q);
uint32_t rwmsg_queue_get_length_pri(rwmsg_queue_t *q, 
                                    rwmsg_priority_t pri);
void rwmsg_queue_flush(rwmsg_queue_t *q);

void rwmsg_queue_pause(rwmsg_endpoint_t *ep,
		       rwmsg_queue_t *q);
void rwmsg_queue_resume(rwmsg_endpoint_t *ep,
			rwmsg_queue_t *q);


__END_DECLS

#endif // __RWMSG_QUEUE_H__

/** \endinternal */

