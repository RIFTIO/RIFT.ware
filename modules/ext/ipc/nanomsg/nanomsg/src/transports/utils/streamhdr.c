/*
    Copyright (c) 2013 250bpm s.r.o.  All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom
    the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#include "streamhdr.h"

#include "../../aio/timer.h"

#include "../../utils/err.h"
#include "../../utils/cont.h"
#include "../../utils/fast.h"
#include "../../utils/wire.h"
#include "../../utils/attr.h"

#include <stddef.h>
#include <string.h>

#define NN_STREAMHDR_STATE_IDLE 1
#define NN_STREAMHDR_STATE_SENDING 2
#define NN_STREAMHDR_STATE_RECEIVING 3
#define NN_STREAMHDR_STATE_STOPPING_TIMER_ERROR 4
#define NN_STREAMHDR_STATE_STOPPING_TIMER_DONE 5
#define NN_STREAMHDR_STATE_DONE 6
#define NN_STREAMHDR_STATE_STOPPING 7

static const char *nn_streamhdr_state_strtab[] = {
  NULL,
  "NN_STREAMHDR_STATE_IDLE",
  "NN_STREAMHDR_STATE_SENDING",
  "NN_STREAMHDR_STATE_RECEIVING",
  "NN_STREAMHDR_STATE_STOPPING_TIMER_ERROR",
  "NN_STREAMHDR_STATE_STOPPING_TIMER_DONE",
  "NN_STREAMHDR_STATE_DONE",
  "NN_STREAMHDR_STATE_STOPPING",
};

#define NN_STREAMHDR_SRC_USOCK 1
#define NN_STREAMHDR_SRC_TIMER 2

static const char *nn_streamhdr_src_strtab[] = {
  NULL,
  "NN_STREAMHDR_SRC_USOCK",
  "NN_STREAMHDR_SRC_TIMER",
};

static const char *nn_getstr_streamhdr_state(int state) {
  if (state > 0 && state < (sizeof(nn_streamhdr_state_strtab) / sizeof(const char *))) {
    return nn_streamhdr_state_strtab[state];
  } else if (state < 0) {
    return "<negative>";
  } else {
    return "0";
  }
};
static const char *nn_getstr_streamhdr_src(int src) {
  if (src > 0 && src < (sizeof(nn_streamhdr_src_strtab) / sizeof(const char *))) {
    return nn_streamhdr_src_strtab[src];
  } else if (src == -2) {
    return "NN_FSM_ACTION";
  } else if (src < 0) {
    return "<negative>";
  } else {
    return "0";
  }
}
static const char *nn_getstr_src_type(int src, int type) {
  switch (src) {
  case NN_STREAMHDR_SRC_USOCK:
    return nn_getstr_usock_type(type);
    break;
  case NN_STREAMHDR_SRC_TIMER:
    return "<timer-event>";
    break;
  case NN_FSM_ACTION:
    return nn_getstr_fsm_type(type);
    break;
  default:
    return "unk src";
    break;
  }
}



/*  Private functions. */
static void nn_streamhdr_handler (struct nn_fsm *self, int src, int type,
    void *srcptr);
static void nn_streamhdr_shutdown (struct nn_fsm *self, int src, int type,
    void *srcptr);

void nn_streamhdr_init (struct nn_streamhdr *self, int src,
    struct nn_fsm *owner)
{
    nn_fsm_init (&self->fsm, nn_streamhdr_handler, nn_streamhdr_shutdown,
        src, self, owner);
    self->state = NN_STREAMHDR_STATE_IDLE;
    nn_timer_init (&self->timer, NN_STREAMHDR_SRC_TIMER, &self->fsm);
    nn_fsm_event_init (&self->done);

    self->usock = NULL;
    self->usock_owner.src = -1;
    self->usock_owner.fsm = NULL;
    self->pipebase = NULL;
}

void nn_streamhdr_term (struct nn_streamhdr *self)
{
    nn_assert_state (self, NN_STREAMHDR_STATE_IDLE);

    nn_fsm_event_term (&self->done);
    nn_timer_term (&self->timer);
    nn_fsm_term (&self->fsm);
}

int nn_streamhdr_isidle (struct nn_streamhdr *self)
{
    return nn_fsm_isidle (&self->fsm);
}

void nn_streamhdr_start (struct nn_streamhdr *self, struct nn_usock *usock,
    struct nn_pipebase *pipebase)
{
    size_t sz;
    int protocol;

    /*  Take ownership of the underlying socket. */
    nn_assert (self->usock == NULL && self->usock_owner.fsm == NULL);
    self->usock_owner.src = NN_STREAMHDR_SRC_USOCK;
    self->usock_owner.fsm = &self->fsm;
    nn_usock_swap_owner (usock, &self->usock_owner);
    self->usock = usock;
    self->pipebase = pipebase;

    /*  Get the protocol identifier. */
    sz = sizeof (protocol);
    nn_pipebase_getopt (pipebase, NN_SOL_SOCKET, NN_PROTOCOL, &protocol, &sz);
    nn_assert (sz == sizeof (protocol));

    uint64_t hs[2];
    sz = sizeof(hs);
    nn_pipebase_getopt (pipebase, NN_SOL_SOCKET, NN_RWHANDSHAKE, &hs, &sz);

    /*  Compose the protocol header. */
    if (sz) {

      //fprintf(stderr, "usock=%p have RWHANDSHAKE sz=%d\n", usock, (int)sz);

      nn_assert(sz == 16);
      memcpy(self->protohder, hs, sz);
      memcpy (self->protohder+sz, "\0SP\0\0\0\0\0", 8);
      self->protohder_len = 24;
      nn_puts (self->protohder + 20, (uint16_t) protocol);
    } else {
      memcpy (self->protohder, "\0SP\0\0\0\0\0", 8);
      nn_puts (self->protohder + 4, (uint16_t) protocol);
      self->protohder_len = 8;
    }

    /*  Launch the state machine. */
    nn_fsm_start (&self->fsm);
}

void nn_streamhdr_stop (struct nn_streamhdr *self)
{
    nn_fsm_stop (&self->fsm);
}

static void nn_streamhdr_shutdown (struct nn_fsm *self, int src, int type,
    NN_UNUSED void *srcptr)
{
    struct nn_streamhdr *streamhdr;

    streamhdr = nn_cont (self, struct nn_streamhdr, fsm);

    if (nn_slow (src == NN_FSM_ACTION && type == NN_FSM_STOP)) {
        nn_timer_stop (&streamhdr->timer);
        streamhdr->state = NN_STREAMHDR_STATE_STOPPING;
    }
    if (nn_slow (streamhdr->state == NN_STREAMHDR_STATE_STOPPING)) {
        if (!nn_timer_isidle (&streamhdr->timer))
            return;
        streamhdr->state = NN_STREAMHDR_STATE_IDLE;
        nn_fsm_stopped (&streamhdr->fsm, NN_STREAMHDR_STOPPED);
        return;
    }

    nn_fsm_bad_state (streamhdr->state, src, type);
}

static void nn_streamhdr_handler (struct nn_fsm *self, int src, int type,
    NN_UNUSED void *srcptr)
{
    struct nn_streamhdr *streamhdr;
    struct nn_iovec iovec;
    int protocol;

    streamhdr = nn_cont (self, struct nn_streamhdr, fsm);

    if (0) {
      fprintf(stderr, "FSM streamhdr=%p (state=%d/%s), src=%d/%s type=%d/%s\n", 
	      streamhdr,
	      streamhdr->state,
	      nn_getstr_streamhdr_state(streamhdr->state),
	      src,
	      nn_getstr_streamhdr_src(src),
	      type,
	      nn_getstr_src_type(src, type));
    }

    switch (streamhdr->state) {

/******************************************************************************/
/*  IDLE state.                                                               */
/******************************************************************************/
    case NN_STREAMHDR_STATE_IDLE:
        switch (src) {

        case NN_FSM_ACTION:
            switch (type) {
            case NN_FSM_START:
  	        nn_timer_start (&streamhdr->timer, 10000);   /* FIXME!!! should be 1s but sleep et al borks us and somehow it never recovers */
                iovec.iov_base = streamhdr->protohder;
                iovec.iov_len = streamhdr->protohder_len;
		nn_usock_send (streamhdr->usock, &iovec, 1);
                streamhdr->state = NN_STREAMHDR_STATE_SENDING;
                return;
            default:
                nn_fsm_bad_action (streamhdr->state, src, type);
            }

        default:
            nn_fsm_bad_source (streamhdr->state, src, type);
        }

/******************************************************************************/
/*  SENDING state.                                                            */
/******************************************************************************/
    case NN_STREAMHDR_STATE_SENDING:
        switch (src) {

        case NN_STREAMHDR_SRC_USOCK:
            switch (type) {
            case NN_USOCK_SENT:
                nn_usock_recv (streamhdr->usock, streamhdr->protohder,
			       8/*sizeof (streamhdr->protohdr)*/);
                streamhdr->state = NN_STREAMHDR_STATE_RECEIVING;
                return;
            case NN_USOCK_SHUTDOWN:
                /*  Ignore it. Wait for ERROR event  */
                return;
            case NN_USOCK_ERROR:
                nn_timer_stop (&streamhdr->timer);
		//fprintf(stderr, "streamhdr.c STATE_SENDING / USOCK / ERROR -> STOPPING_TIMER \n");
                streamhdr->state = NN_STREAMHDR_STATE_STOPPING_TIMER_ERROR;
                return;
            default:
                nn_fsm_bad_action (streamhdr->state, src, type);
            }

        case NN_STREAMHDR_SRC_TIMER:
            switch (type) {
            case NN_TIMER_TIMEOUT:
                nn_timer_stop (&streamhdr->timer);
		//fprintf(stderr, "streamhdr.c STATE_SENDING / TIMER / TIMEOUT -> STOPPING_TIMER \n");
                streamhdr->state = NN_STREAMHDR_STATE_STOPPING_TIMER_ERROR;
                return;
            default:
                nn_fsm_bad_action (streamhdr->state, src, type);
            }

        default:
            nn_fsm_bad_source (streamhdr->state, src, type);
        }

/******************************************************************************/
/*  RECEIVING state.                                                          */
/******************************************************************************/
    case NN_STREAMHDR_STATE_RECEIVING:
        switch (src) {

        case NN_STREAMHDR_SRC_USOCK:
            switch (type) {
            case NN_USOCK_RECEIVED:

                /*  Here we are checking whether the peer speaks the same
                    protocol as this socket. */
	      if (memcmp (streamhdr->protohder, "\0SP\0", 4) != 0) {
		//fprintf(stderr, "streamhdr.c STATE_RECEIVING / RECEIVED / protohder != '0SP0' thus invalidhdr -> STOPPING_TIMER\n");
                    goto invalidhdr;
	      }
                protocol = nn_gets (streamhdr->protohder + 4);
                if (!nn_pipebase_ispeer (streamhdr->pipebase, protocol)) {
		  //fprintf(stderr, "streamhdr.c STATE_RECEIVING / RECEIVED / !nn_pipebase_ispeer() aka invalidhdr -> STOPPING_TIMER\n");
                    goto invalidhdr;
		}
                nn_timer_stop (&streamhdr->timer);
                streamhdr->state = NN_STREAMHDR_STATE_STOPPING_TIMER_DONE;
                return;
            case NN_USOCK_SHUTDOWN:
                /*  Ignore it. Wait for ERROR event  */
                return;
            case NN_USOCK_ERROR:
	      //fprintf(stderr, "streamhdr.c STATE_RECEIVING / USOCK / USOCK_ERROR -> STOPPING_TIMER\n");
invalidhdr:
                nn_timer_stop (&streamhdr->timer);
                streamhdr->state = NN_STREAMHDR_STATE_STOPPING_TIMER_ERROR;
                return;
            default:
                nn_assert (0);
            }

        case NN_STREAMHDR_SRC_TIMER:
            switch (type) {
            case NN_TIMER_TIMEOUT:
                nn_timer_stop (&streamhdr->timer);
		//fprintf(stderr, "streamhdr.c STATE_RECEIVING / TIMER / TIMEOUT -> STOPPING_TIMER\n");
                streamhdr->state = NN_STREAMHDR_STATE_STOPPING_TIMER_ERROR;
                return;
            default:
                nn_fsm_bad_action (streamhdr->state, src, type);
            }

        default:
            nn_fsm_bad_source (streamhdr->state, src, type);
        }

/******************************************************************************/
/*  STOPPING_TIMER_ERROR state.                                               */
/******************************************************************************/
    case NN_STREAMHDR_STATE_STOPPING_TIMER_ERROR:
        switch (src) {

        case NN_STREAMHDR_SRC_USOCK:
            /*  It's safe to ignore usock event when we are stopping  */
            return;

        case NN_STREAMHDR_SRC_TIMER:
            switch (type) {
            case NN_TIMER_STOPPED:
                nn_usock_swap_owner (streamhdr->usock, &streamhdr->usock_owner);
                streamhdr->usock = NULL;
                streamhdr->usock_owner.src = -1;
                streamhdr->usock_owner.fsm = NULL;
                streamhdr->state = NN_STREAMHDR_STATE_DONE;
		//fprintf(stderr, "streamhdr.c STATE_STOPPING_TIMER_ERROR / TIMER / STOPPED -> STREAMHDR_ERROR\n");
                nn_fsm_raise (&streamhdr->fsm, &streamhdr->done,
                    NN_STREAMHDR_ERROR);
                return;
            default:
                nn_fsm_bad_action (streamhdr->state, src, type);
            }

        default:
            nn_fsm_bad_source (streamhdr->state, src, type);
        }

/******************************************************************************/
/*  STOPPING_TIMER_DONE state.                                                */
/******************************************************************************/
    case NN_STREAMHDR_STATE_STOPPING_TIMER_DONE:
        switch (src) {

        case NN_STREAMHDR_SRC_TIMER:
            switch (type) {
            case NN_TIMER_STOPPED:
                nn_usock_swap_owner (streamhdr->usock, &streamhdr->usock_owner);
                streamhdr->usock = NULL;
                streamhdr->usock_owner.src = -1;
                streamhdr->usock_owner.fsm = NULL;
                streamhdr->state = NN_STREAMHDR_STATE_DONE;
                nn_fsm_raise (&streamhdr->fsm, &streamhdr->done,
                    NN_STREAMHDR_OK);
                return;
            default:
                nn_fsm_bad_action (streamhdr->state, src, type);
            }

        default:
            nn_fsm_bad_source (streamhdr->state, src, type);
        }

/******************************************************************************/
/*  DONE state.                                                               */
/*  The header exchange was either done successfully of failed. There's       */
/*  nothing that can be done in this state except stopping the object.        */
/******************************************************************************/
    case NN_STREAMHDR_STATE_DONE:
        nn_fsm_bad_source (streamhdr->state, src, type);

/******************************************************************************/
/*  Invalid state.                                                            */
/******************************************************************************/
    default:
        nn_fsm_bad_state (streamhdr->state, src, type);
    }
}

