/*-
 * Copyright (c) 2010 Kip Macy
 * All rights reserved.
 * Copyright (c) 2013 Patrick Kelsey. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Derived in part from libplebnet's pn_kern_intr.c.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cpuset.h>
#include <sys/rtprio.h>
#include <sys/systm.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/ktr.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/unistd.h>
#include <sys/vmmeter.h>
#include <machine/atomic.h>
#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/stdarg.h>
#ifdef DDB
#include <ddb/ddb.h>
#include <ddb/db_sym.h>
#endif



/*
 * Describe an interrupt thread.  There is one of these per interrupt event.
 */
struct intr_thread {
	struct intr_event *it_event;
	struct thread *it_thread;	/* Kernel thread. */
	int	it_flags;		/* (j) IT_* flags. */
	int	it_need;		/* Needs service. */
};

/* Interrupt thread flags kept in it_flags */
#define	IT_DEAD		0x000001	/* Thread is waiting to exit. */

struct	intr_entropy {
	struct	thread *td;
	uintptr_t event;
};


extern struct mtx uinet_pcpu_locks[MAXCPU];

void
critical_enter(void)
{
	/* XXX grab per-cpu mutex 
	   The best performing reimplementation is TBD.  For now, a per-cpu
	   mutex is used to prevent another thread from entering the same
	   critical section on the current cpu.
	 */
	KASSERT(curthread->td_oncpu < mp_ncpus, ("curthread->td_oncpu >= mp_ncpus"));
	mtx_lock(&uinet_pcpu_locks[curthread->td_oncpu]);
}

void
critical_exit(void)
{
	KASSERT(curthread->td_oncpu < mp_ncpus, ("curthread->td_oncpu >= mp_ncpus"));
	mtx_unlock(&uinet_pcpu_locks[curthread->td_oncpu]);
}

#undef thread_lock
#undef thread_unlock
#define thread_lock(tdp)      	mtx_lock((tdp)->td_lock)
#define thread_unlock(tdp)      mtx_unlock((tdp)->td_lock)



struct	intr_event *clk_intr_event;
struct	intr_event *tty_intr_event;
void	*vm_ih;
struct proc *intrproc;

static MALLOC_DEFINE(M_ITHREAD, "ithread", "Interrupt Threads");

static int intr_storm_threshold = 1000;
static TAILQ_HEAD(, intr_event) event_list =
    TAILQ_HEAD_INITIALIZER(event_list);
static struct mtx event_lock;
MTX_SYSINIT(intr_event_list, &event_lock, "intr event list", MTX_DEF);

static void	intr_event_update(struct intr_event *ie);
#ifdef INTR_FILTER
static int	intr_event_schedule_thread(struct intr_event *ie,
		    struct intr_thread *ithd);
static int	intr_filter_loop(struct intr_event *ie,
		    struct trapframe *frame, struct intr_thread **ithd);
static struct intr_thread *ithread_create(const char *name,
			      struct intr_handler *ih);
#else
static int	intr_event_schedule_thread(struct intr_event *ie);
static struct intr_thread *ithread_create(const char *name);
#endif
static void	ithread_destroy(struct intr_thread *ithread);
static void	ithread_execute_handlers(struct proc *p, 
		    struct intr_event *ie);
#ifdef INTR_FILTER
static void	priv_ithread_execute_handler(struct proc *p, 
		    struct intr_handler *ih);
#endif
static void	ithread_loop(void *);
static void	ithread_update(struct intr_thread *ithd);
#if 0
static void	start_softintr(void *);
#endif

/* Map an interrupt type to an ithread priority. */
u_char
intr_priority(enum intr_type flags)
{
	u_char pri;

	flags &= (INTR_TYPE_TTY | INTR_TYPE_BIO | INTR_TYPE_NET |
	    INTR_TYPE_CAM | INTR_TYPE_MISC | INTR_TYPE_CLK | INTR_TYPE_AV);
	switch (flags) {
	case INTR_TYPE_TTY:
		pri = PI_TTY;
		break;
	case INTR_TYPE_BIO:
		pri = PI_DISK;
		break;
	case INTR_TYPE_NET:
		pri = PI_NET;
		break;
	case INTR_TYPE_CAM:
		pri = PI_DISK;          
		break;
	case INTR_TYPE_AV:		
		pri = PI_AV;
		break;
	case INTR_TYPE_CLK:
		pri = PI_REALTIME;
		break;
	case INTR_TYPE_MISC:
		pri = PI_DULL;          /* don't care */
		break;
	default:
		/* We didn't specify an interrupt level. */
		panic("intr_priority: no interrupt type in flags");
	}

	return pri;
}

/*
 * Update an ithread based on the associated intr_event.
 */
static void
ithread_update(struct intr_thread *ithd)
{
	struct intr_event *ie;
	struct thread *td;
	u_char pri;

	ie = ithd->it_event;
	td = ithd->it_thread;

	/* Determine the overall priority of this event. */
	if (TAILQ_EMPTY(&ie->ie_handlers))
		pri = PRI_MAX_ITHD;
	else
		pri = TAILQ_FIRST(&ie->ie_handlers)->ih_pri;

	/* Update name and priority. */
	strlcpy(td->td_name, ie->ie_fullname, sizeof(td->td_name));

	/* XXX set priority */
}

/*
 * Regenerate the full name of an interrupt event and update its priority.
 */
static void
intr_event_update(struct intr_event *ie)
{
	struct intr_handler *ih;
	char *last;
	int missed, space;

	/* Start off with no entropy and just the name of the event. */
	mtx_assert(&ie->ie_lock, MA_OWNED);
	strlcpy(ie->ie_fullname, ie->ie_name, sizeof(ie->ie_fullname));
	missed = 0;
	space = 1;

	/* Run through all the handlers updating values. */
	TAILQ_FOREACH(ih, &ie->ie_handlers, ih_next) {
		if (strlen(ie->ie_fullname) + strlen(ih->ih_name) + 1 <
		    sizeof(ie->ie_fullname)) {
			strcat(ie->ie_fullname, " ");
			strcat(ie->ie_fullname, ih->ih_name);
			space = 0;
		} else
			missed++;
	}

	/*
	 * If the handler names were too long, add +'s to indicate missing
	 * names. If we run out of room and still have +'s to add, change
	 * the last character from a + to a *.
	 */
	last = &ie->ie_fullname[sizeof(ie->ie_fullname) - 2];
	while (missed-- > 0) {
		if (strlen(ie->ie_fullname) + 1 == sizeof(ie->ie_fullname)) {
			if (*last == '+') {
				*last = '*';
				break;
			} else
				*last = '+';
		} else if (space) {
			strcat(ie->ie_fullname, " +");
			space = 0;
		} else
			strcat(ie->ie_fullname, "+");
	}

	/*
	 * If this event has an ithread, update it's priority and
	 * name.
	 */
#ifndef RIFT_UINET
	if (ie->ie_thread != NULL)
		ithread_update(ie->ie_thread);
#endif
	CTR2(KTR_INTR, "%s: updated %s", __func__, ie->ie_fullname);
}

int
intr_event_create(struct intr_event **event, void *source, int flags, int irq,
    void (*pre_ithread)(void *), void (*post_ithread)(void *),
    void (*post_filter)(void *), int (*assign_cpu)(void *, u_char),
    const char *fmt, ...)
{
	struct intr_event *ie;
	va_list ap;

	/* The only valid flag during creation is IE_SOFT. */
	if ((flags & ~IE_SOFT) != 0)
		return (EINVAL);
	ie = malloc(sizeof(struct intr_event), M_ITHREAD, M_WAITOK | M_ZERO);
	ie->ie_source = source;
	ie->ie_pre_ithread = pre_ithread;
	ie->ie_post_ithread = post_ithread;
	ie->ie_post_filter = post_filter;
	ie->ie_assign_cpu = assign_cpu;
	ie->ie_flags = flags;
	ie->ie_irq = irq;
	ie->ie_cpu = NOCPU;
	TAILQ_INIT(&ie->ie_handlers);
	mtx_init(&ie->ie_lock, "intr event", NULL, MTX_DEF);

	va_start(ap, fmt);
	vsnprintf(ie->ie_name, sizeof(ie->ie_name), fmt, ap);
	va_end(ap);
	strlcpy(ie->ie_fullname, ie->ie_name, sizeof(ie->ie_fullname));
	mtx_lock(&event_lock);
	TAILQ_INSERT_TAIL(&event_list, ie, ie_list);
	mtx_unlock(&event_lock);
	if (event != NULL)
		*event = ie;
	CTR2(KTR_INTR, "%s: created %s", __func__, ie->ie_name);
	return (0);
}

/*
 * Bind an interrupt event to the specified CPU.  Note that not all
 * platforms support binding an interrupt to a CPU.  For those
 * platforms this request will fail.  For supported platforms, any
 * associated ithreads as well as the primary interrupt context will
 * be bound to the specificed CPU.  Using a cpu id of NOCPU unbinds
 * the interrupt event.
 */
int
intr_event_bind(struct intr_event *ie, u_char cpu)
{

	return (EOPNOTSUPP);
}

#if 0
static struct intr_event *
intr_lookup(int irq)
{
	struct intr_event *ie;

	mtx_lock(&event_lock);
	TAILQ_FOREACH(ie, &event_list, ie_list)
		if (ie->ie_irq == irq &&
		    (ie->ie_flags & IE_SOFT) == 0 &&
		    TAILQ_FIRST(&ie->ie_handlers) != NULL)
			break;
	mtx_unlock(&event_lock);
	return (ie);
}
#endif

int
intr_event_destroy(struct intr_event *ie)
{

	mtx_lock(&event_lock);
	mtx_lock(&ie->ie_lock);
	if (!TAILQ_EMPTY(&ie->ie_handlers)) {
		mtx_unlock(&ie->ie_lock);
		mtx_unlock(&event_lock);
		return (EBUSY);
	}
	TAILQ_REMOVE(&event_list, ie, ie_list);
	if (ie->ie_thread != NULL) {
		ithread_destroy(ie->ie_thread);
		ie->ie_thread = NULL;
	}
	mtx_unlock(&ie->ie_lock);
	mtx_unlock(&event_lock);
	mtx_destroy(&ie->ie_lock);
	free(ie, M_ITHREAD);
	return (0);
}

#ifndef INTR_FILTER
static struct intr_thread *
ithread_create(const char *name)
{
	struct intr_thread *ithd;
	struct intr_event *ie;
	struct thread *td;
	int error;

	ithd = malloc(sizeof(struct intr_thread), M_ITHREAD, M_WAITOK | M_ZERO);
	ie = malloc(sizeof(struct intr_event), M_ITHREAD, M_WAITOK | M_ZERO);
	ithd->it_event = ie;
	
	error = kproc_kthread_add(ithread_loop, ithd, &intrproc,
		    &td, RFSTOPPED | RFHIGHPID,
	    	    0, "intr", "%s", name);

	
	if (error)
		panic("kproc_create() failed with %d", error);
	td->td_pflags |= TDP_ITHREAD;
	ithd->it_thread = td;

	CTR2(KTR_INTR, "%s: created %s", __func__, name);
	return (ithd);
}
#else
static struct intr_thread *
ithread_create(const char *name, struct intr_handler *ih)
{
	struct intr_thread *ithd;
	struct thread *td;
	int error;

	ithd = malloc(sizeof(struct intr_thread), M_ITHREAD, M_WAITOK | M_ZERO);

	error = kproc_kthread_add(ithread_loop, ih, &intrproc,
		    &td, RFSTOPPED | RFHIGHPID,
	    	    0, "intr", "%s", name);
	if (error)
		panic("kproc_create() failed with %d", error);
	td->td_pflags |= TDP_ITHREAD;
	ithd->it_thread = td;
	CTR2(KTR_INTR, "%s: created %s", __func__, name);
	return (ithd);
}
#endif

static void
ithread_destroy(struct intr_thread *ithread)
{
	;
	
}

#ifndef INTR_FILTER
int
intr_event_add_handler(struct intr_event *ie, const char *name,
    driver_filter_t filter, driver_intr_t handler, void *arg, u_char pri,
    enum intr_type flags, void **cookiep)
{
	struct intr_handler *ih, *temp_ih;
	struct intr_thread *it;

	if (ie == NULL || name == NULL || (handler == NULL && filter == NULL))
		return (EINVAL);

	/* Allocate and populate an interrupt handler structure. */
	ih = malloc(sizeof(struct intr_handler), M_ITHREAD, M_WAITOK | M_ZERO);
	ih->ih_filter = filter;
	ih->ih_handler = handler;
	ih->ih_argument = arg;
	ih->ih_event = ie;
	ih->ih_pri = pri;
	strncpy(ih->ih_name, name, MAXCOMLEN);

	if (flags & INTR_EXCL)
		ih->ih_flags = IH_EXCLUSIVE;
	if (flags & INTR_MPSAFE)
		ih->ih_flags |= IH_MPSAFE;

	/* We can only have one exclusive handler in a event. */
	mtx_lock(&ie->ie_lock);
	if (!TAILQ_EMPTY(&ie->ie_handlers)) {
		if ((flags & INTR_EXCL) ||
		    (TAILQ_FIRST(&ie->ie_handlers)->ih_flags & IH_EXCLUSIVE)) {
			mtx_unlock(&ie->ie_lock);
			free(ih, M_ITHREAD);
			return (EINVAL);
		}
	}

	/* Add the new handler to the event in priority order. */
	TAILQ_FOREACH(temp_ih, &ie->ie_handlers, ih_next) {
		if (temp_ih->ih_pri > ih->ih_pri)
			break;
	}
	if (temp_ih == NULL)
		TAILQ_INSERT_TAIL(&ie->ie_handlers, ih, ih_next);
	else
		TAILQ_INSERT_BEFORE(temp_ih, ih, ih_next);
	intr_event_update(ie);

	/* Create a thread if we need one. */
	while (ie->ie_thread == NULL && handler != NULL) {
		if (ie->ie_flags & IE_ADDING_THREAD)
			msleep(ie, &ie->ie_lock, 0, "ithread", 0);
		else {
			ie->ie_flags |= IE_ADDING_THREAD;
			mtx_unlock(&ie->ie_lock);
			it = ithread_create("intr: newborn");
			mtx_lock(&ie->ie_lock);
			ie->ie_flags &= ~IE_ADDING_THREAD;
			ie->ie_thread = it;
			it->it_event = ie;
			ithread_update(it);
			wakeup(ie);
		}
	}
	CTR3(KTR_INTR, "%s: added %s to %s", __func__, ih->ih_name,
	    ie->ie_name);
	mtx_unlock(&ie->ie_lock);

	if (cookiep != NULL)
		*cookiep = ih;
	return (0);
}
#else
int
intr_event_add_handler(struct intr_event *ie, const char *name,
    driver_filter_t filter, driver_intr_t handler, void *arg, u_char pri,
    enum intr_type flags, void **cookiep)
{
	struct intr_handler *ih, *temp_ih;
	struct intr_thread *it;

	if (ie == NULL || name == NULL || (handler == NULL && filter == NULL))
		return (EINVAL);

	/* Allocate and populate an interrupt handler structure. */
	ih = malloc(sizeof(struct intr_handler), M_ITHREAD, M_WAITOK | M_ZERO);
	ih->ih_filter = filter;
	ih->ih_handler = handler;
	ih->ih_argument = arg;
	ih->ih_name = name;
	ih->ih_event = ie;
	ih->ih_pri = pri;
	if (flags & INTR_EXCL)
		ih->ih_flags = IH_EXCLUSIVE;
	if (flags & INTR_MPSAFE)
		ih->ih_flags |= IH_MPSAFE;

	/* We can only have one exclusive handler in a event. */
	mtx_lock(&ie->ie_lock);
	if (!TAILQ_EMPTY(&ie->ie_handlers)) {
		if ((flags & INTR_EXCL) ||
		    (TAILQ_FIRST(&ie->ie_handlers)->ih_flags & IH_EXCLUSIVE)) {
			mtx_unlock(&ie->ie_lock);
			free(ih, M_ITHREAD);
			return (EINVAL);
		}
	}

	/* Add the new handler to the event in priority order. */
	TAILQ_FOREACH(temp_ih, &ie->ie_handlers, ih_next) {
		if (temp_ih->ih_pri > ih->ih_pri)
			break;
	}
	if (temp_ih == NULL)
		TAILQ_INSERT_TAIL(&ie->ie_handlers, ih, ih_next);
	else
		TAILQ_INSERT_BEFORE(temp_ih, ih, ih_next);
	intr_event_update(ie);

	/* For filtered handlers, create a private ithread to run on. */
	if (filter != NULL && handler != NULL) { 
		mtx_unlock(&ie->ie_lock);
		it = ithread_create("intr: newborn", ih);		
		mtx_lock(&ie->ie_lock);
		it->it_event = ie; 
		ih->ih_thread = it;
		ithread_update(it); // XXX - do we really need this?!?!?
	} else { /* Create the global per-event thread if we need one. */
		while (ie->ie_thread == NULL && handler != NULL) {
			if (ie->ie_flags & IE_ADDING_THREAD)
				msleep(ie, &ie->ie_lock, 0, "ithread", 0);
			else {
				ie->ie_flags |= IE_ADDING_THREAD;
				mtx_unlock(&ie->ie_lock);
				it = ithread_create("intr: newborn", ih);
				mtx_lock(&ie->ie_lock);
				ie->ie_flags &= ~IE_ADDING_THREAD;
				ie->ie_thread = it;
				it->it_event = ie;
				ithread_update(it);
				wakeup(ie);
			}
		}
	}
	CTR3(KTR_INTR, "%s: added %s to %s", __func__, ih->ih_name,
	    ie->ie_name);
	mtx_unlock(&ie->ie_lock);

	if (cookiep != NULL)
		*cookiep = ih;
	return (0);
}
#endif

/*
 * Return the ie_source field from the intr_event an intr_handler is
 * associated with.
 */
void *
intr_handler_source(void *cookie)
{
	struct intr_handler *ih;
	struct intr_event *ie;

	ih = (struct intr_handler *)cookie;
	if (ih == NULL)
		return (NULL);
	ie = ih->ih_event;
	KASSERT(ie != NULL,
	    ("interrupt handler \"%s\" has a NULL interrupt event",
	    ih->ih_name));
	return (ie->ie_source);
}

#ifndef INTR_FILTER
int
intr_event_remove_handler(void *cookie)
{
	struct intr_handler *handler = (struct intr_handler *)cookie;
	struct intr_event *ie;
#ifdef INVARIANTS
	struct intr_handler *ih;
#endif

	if (handler == NULL)
		return (EINVAL);
	ie = handler->ih_event;
	KASSERT(ie != NULL,
	    ("interrupt handler \"%s\" has a NULL interrupt event",
	    handler->ih_name));
	mtx_lock(&ie->ie_lock);
	CTR3(KTR_INTR, "%s: removing %s from %s", __func__, handler->ih_name,
	    ie->ie_name);
#ifdef INVARIANTS
	TAILQ_FOREACH(ih, &ie->ie_handlers, ih_next)
		if (ih == handler)
			goto ok;
	mtx_unlock(&ie->ie_lock);
	panic("interrupt handler \"%s\" not found in interrupt event \"%s\"",
	    ih->ih_name, ie->ie_name);
ok:
#endif
	/*
	 * If there is no ithread, then just remove the handler and return.
	 * XXX: Note that an INTR_FAST handler might be running on another
	 * CPU!
	 */
	if (ie->ie_thread == NULL) {
		TAILQ_REMOVE(&ie->ie_handlers, handler, ih_next);
		mtx_unlock(&ie->ie_lock);
		free(handler, M_ITHREAD);
		return (0);
	}

	/*
	 * If the interrupt thread is already running, then just mark this
	 * handler as being dead and let the ithread do the actual removal.
	 *
	 * During a cold boot while cold is set, msleep() does not sleep,
	 * so we have to remove the handler here rather than letting the
	 * thread do it.
	 */
	thread_lock(ie->ie_thread->it_thread);
	if (!TD_AWAITING_INTR(ie->ie_thread->it_thread)) {
		handler->ih_flags |= IH_DEAD;

		/*
		 * Ensure that the thread will process the handler list
		 * again and remove this handler if it has already passed
		 * it on the list.
		 */
		ie->ie_thread->it_need = 1;
	}
	thread_unlock(ie->ie_thread->it_thread);
	while (handler->ih_flags & IH_DEAD)
		msleep(handler, &ie->ie_lock, 0, "iev_rmh", 0);
	intr_event_update(ie);
	mtx_unlock(&ie->ie_lock);
	free(handler, M_ITHREAD);
	return (0);
}

static int
intr_event_schedule_thread(struct intr_event *ie)
{
#ifdef notyet
	struct intr_entropy entropy;
#endif
	struct intr_thread *it;
	struct thread *td;
	struct thread *ctd;
	struct proc *p;

	/*
	 * If no ithread or no handlers, then we have a stray interrupt.
	 */
	if (ie == NULL || TAILQ_EMPTY(&ie->ie_handlers) ||
	    ie->ie_thread == NULL)
		return (EINVAL);

	ctd = curthread;
	it = ie->ie_thread;
	td = it->it_thread;
	p = td->td_proc;

	KASSERT(p != NULL, ("ithread %s has no process", ie->ie_name));

	/*
	 * Set it_need to tell the thread to keep running if it is already
	 * running.  Then, lock the thread and see if we actually need to
	 * put it on the runqueue.
	 */
	it->it_need = 1;
	thread_lock(td);
	if (TD_AWAITING_INTR(td)) {
		TD_CLR_IWAIT(td);
		cv_signal((struct cv *)td->td_sleepqueue);
	}
	thread_unlock(td);
	return (0);
}
#else
int
intr_event_remove_handler(void *cookie)
{
	struct intr_handler *handler = (struct intr_handler *)cookie;
	struct intr_event *ie;
	struct intr_thread *it;
#ifdef INVARIANTS
	struct intr_handler *ih;
#endif

	if (handler == NULL)
		return (EINVAL);
	ie = handler->ih_event;
	KASSERT(ie != NULL,
	    ("interrupt handler \"%s\" has a NULL interrupt event",
	    handler->ih_name));
	mtx_lock(&ie->ie_lock);
	CTR3(KTR_INTR, "%s: removing %s from %s", __func__, handler->ih_name,
	    ie->ie_name);
#ifdef INVARIANTS
	TAILQ_FOREACH(ih, &ie->ie_handlers, ih_next)
		if (ih == handler)
			goto ok;
	mtx_unlock(&ie->ie_lock);
	panic("interrupt handler \"%s\" not found in interrupt event \"%s\"",
	    ih->ih_name, ie->ie_name);
ok:
#endif
	/*
	 * If there are no ithreads (per event and per handler), then
	 * just remove the handler and return.  
	 * XXX: Note that an INTR_FAST handler might be running on another CPU!
	 */
	if (ie->ie_thread == NULL && handler->ih_thread == NULL) {
		TAILQ_REMOVE(&ie->ie_handlers, handler, ih_next);
		mtx_unlock(&ie->ie_lock);
		free(handler, M_ITHREAD);
		return (0);
	}

	/* Private or global ithread? */
	it = (handler->ih_thread) ? handler->ih_thread : ie->ie_thread;
	/*
	 * If the interrupt thread is already running, then just mark this
	 * handler as being dead and let the ithread do the actual removal.
	 *
	 * During a cold boot while cold is set, msleep() does not sleep,
	 * so we have to remove the handler here rather than letting the
	 * thread do it.
	 */
	thread_lock(it->it_thread);
	if (!TD_AWAITING_INTR(it->it_thread)) {
		handler->ih_flags |= IH_DEAD;

		/*
		 * Ensure that the thread will process the handler list
		 * again and remove this handler if it has already passed
		 * it on the list.
		 */
		it->it_need = 1;
	} 
	thread_unlock(it->it_thread);
	while (handler->ih_flags & IH_DEAD)
		msleep(handler, &ie->ie_lock, 0, "iev_rmh", 0);
	/* 
	 * At this point, the handler has been disconnected from the event,
	 * so we can kill the private ithread if any.
	 */
	if (handler->ih_thread) {
		ithread_destroy(handler->ih_thread);
		handler->ih_thread = NULL;
	}
	intr_event_update(ie);
	mtx_unlock(&ie->ie_lock);
	free(handler, M_ITHREAD);
	return (0);
}

static int
intr_event_schedule_thread(struct intr_event *ie, struct intr_thread *it)
{
#ifdef notyet
	struct intr_entropy entropy;
#endif
	struct thread *td;
	struct thread *ctd;
	struct proc *p;

	/*
	 * If no ithread or no handlers, then we have a stray interrupt.
	 */
	if (ie == NULL || TAILQ_EMPTY(&ie->ie_handlers) || it == NULL)
		return (EINVAL);

	ctd = curthread;
	td = it->it_thread;
	p = td->td_proc;

	KASSERT(p != NULL, ("ithread %s has no process", ie->ie_name));

	/*
	 * Set it_need to tell the thread to keep running if it is already
	 * running.  Then, lock the thread and see if we actually need to
	 * put it on the runqueue.
	 */
	it->it_need = 1;
	thread_lock(td);
	if (TD_AWAITING_INTR(td)) {
		CTR3(KTR_INTR, "%s: schedule pid %d (%s)", __func__, p->p_pid,
		    td->td_name);
		TD_CLR_IWAIT(td);
		sched_add(td, SRQ_INTR);
	} else {
		CTR5(KTR_INTR, "%s: pid %d (%s): it_need %d, state %d",
		    __func__, p->p_pid, td->td_name, it->it_need, td->td_state);
	}
	thread_unlock(td);

	return (0);
}
#endif

/*
 * Allow interrupt event binding for software interrupt handlers -- a no-op,
 * since interrupts are generated in software rather than being directed by
 * a PIC.
 */
static int
swi_assign_cpu(void *arg, u_char cpu)
{

	return (0);
}

/*
 * Add a software interrupt handler to a specified event.  If a given event
 * is not specified, then a new event is created.
 */
int
swi_add(struct intr_event **eventp, const char *name, driver_intr_t handler,
	    void *arg, int pri, enum intr_type flags, void **cookiep)
{
	struct intr_event *ie;
	int error;

	ie = (eventp != NULL) ? *eventp : NULL;

	if (ie != NULL) {
		if (!(ie->ie_flags & IE_SOFT))
			return (EINVAL);
	} else {
		error = intr_event_create(&ie, NULL, IE_SOFT, 0,
		    NULL, NULL, NULL, swi_assign_cpu, "swi%d:", pri);
		if (error)
			return (error);
		if (eventp != NULL)
			*eventp = ie;
	}
	error = intr_event_add_handler(ie, name, NULL, handler, arg,
	    (pri * RQ_PPQ) + PI_SOFT, flags, cookiep);
	
	
	return (error);
}

/*
 * Schedule a software interrupt thread.
 */
void
swi_sched(void *cookie, int flags)
{
	struct intr_handler *ih = (struct intr_handler *)cookie;
	struct intr_event *ie = ih->ih_event;
	int error;

	CTR3(KTR_INTR, "swi_sched: %s %s need=%d", ie->ie_name, ih->ih_name,
	    ih->ih_need);

	/*
	 * Set ih_need for this handler so that if the ithread is already
	 * running it will execute this handler on the next pass.  Otherwise,
	 * it will execute it the next time it runs.
	 */
	atomic_store_rel_int(&ih->ih_need, 1);

	if (!(flags & SWI_DELAY)) {
		PCPU_INC(cnt.v_soft);
#ifdef INTR_FILTER
		error = intr_event_schedule_thread(ie, ie->ie_thread);
#else
		error = intr_event_schedule_thread(ie);
#endif
		KASSERT(error == 0, ("stray software interrupt"));
	}
}

/*
 * Remove a software interrupt handler.  Currently this code does not
 * remove the associated interrupt event if it becomes empty.  Calling code
 * may do so manually via intr_event_destroy(), but that's not really
 * an optimal interface.
 */
int
swi_remove(void *cookie)
{

	return (intr_event_remove_handler(cookie));
}

#ifdef INTR_FILTER
static void
priv_ithread_execute_handler(struct proc *p, struct intr_handler *ih)
{
	struct intr_event *ie;

	ie = ih->ih_event;
	/*
	 * If this handler is marked for death, remove it from
	 * the list of handlers and wake up the sleeper.
	 */
	if (ih->ih_flags & IH_DEAD) {
		mtx_lock(&ie->ie_lock);
		TAILQ_REMOVE(&ie->ie_handlers, ih, ih_next);
		ih->ih_flags &= ~IH_DEAD;
		wakeup(ih);
		mtx_unlock(&ie->ie_lock);
		return;
	}
	
	/* Execute this handler. */
	CTR6(KTR_INTR, "%s: pid %d exec %p(%p) for %s flg=%x",
	     __func__, p->p_pid, (void *)ih->ih_handler, ih->ih_argument,
	     ih->ih_name, ih->ih_flags);
	
	if (!(ih->ih_flags & IH_MPSAFE))
		mtx_lock(&Giant);
	ih->ih_handler(ih->ih_argument);
	if (!(ih->ih_flags & IH_MPSAFE))
		mtx_unlock(&Giant);
}
#endif

/*
 * This is a public function for use by drivers that mux interrupt
 * handlers for child devices from their interrupt handler.
 */
void
intr_event_execute_handlers(struct proc *p, struct intr_event *ie)
{
	struct intr_handler *ih, *ihn;

	TAILQ_FOREACH_SAFE(ih, &ie->ie_handlers, ih_next, ihn) {
		/*
		 * If this handler is marked for death, remove it from
		 * the list of handlers and wake up the sleeper.
		 */
		if (ih->ih_flags & IH_DEAD) {
			mtx_lock(&ie->ie_lock);
			TAILQ_REMOVE(&ie->ie_handlers, ih, ih_next);
			ih->ih_flags &= ~IH_DEAD;
			wakeup(ih);
			mtx_unlock(&ie->ie_lock);
			continue;
		}

		/* Skip filter only handlers */
		if (ih->ih_handler == NULL)
			continue;

		/*
		 * For software interrupt threads, we only execute
		 * handlers that have their need flag set.  Hardware
		 * interrupt threads always invoke all of their handlers.
		 */
		if (ie->ie_flags & IE_SOFT) {
			if (!ih->ih_need)
				continue;
			else
				atomic_store_rel_int(&ih->ih_need, 0);
		}

		/* Execute this handler. */
		CTR6(KTR_INTR, "%s: pid %d exec %p(%p) for %s flg=%x",
		    __func__, p->p_pid, (void *)ih->ih_handler, 
		    ih->ih_argument, ih->ih_name, ih->ih_flags);

		if (!(ih->ih_flags & IH_MPSAFE))
			mtx_lock(&Giant);
		ih->ih_handler(ih->ih_argument);
		if (!(ih->ih_flags & IH_MPSAFE))
			mtx_unlock(&Giant);
	}
}

static void
ithread_execute_handlers(struct proc *p, struct intr_event *ie)
{

	/* Interrupt handlers should not sleep. */
	if (!(ie->ie_flags & IE_SOFT))
		THREAD_NO_SLEEPING();
	intr_event_execute_handlers(p, ie);
	if (!(ie->ie_flags & IE_SOFT))
		THREAD_SLEEPING_OK();

	/*
	 * Interrupt storm handling:
	 *
	 * If this interrupt source is currently storming, then throttle
	 * it to only fire the handler once  per clock tick.
	 *
	 * If this interrupt source is not currently storming, but the
	 * number of back to back interrupts exceeds the storm threshold,
	 * then enter storming mode.
	 */
	if (intr_storm_threshold != 0 && ie->ie_count >= intr_storm_threshold &&
	    !(ie->ie_flags & IE_SOFT)) {
		/* Report the message only once every second. */
		if (ppsratecheck(&ie->ie_warntm, &ie->ie_warncnt, 1)) {
			printf(
	"interrupt storm detected on \"%s\"; throttling interrupt source\n",
			    ie->ie_name);
		}
		pause("istorm", 1);
	} else
		ie->ie_count++;

	/*
	 * Now that all the handlers have had a chance to run, reenable
	 * the interrupt source.
	 */
	if (ie->ie_post_ithread != NULL)
		ie->ie_post_ithread(ie->ie_source);
}

#ifndef INTR_FILTER
/*
 * This is the main code for interrupt threads.
 */
static void
ithread_loop(void *arg)
{
	struct intr_thread *ithd;
	struct intr_event *ie;
	struct thread *td;
	struct proc *p;

	td = curthread;
	p = td->td_proc;
	ithd = (struct intr_thread *)arg;
	KASSERT(ithd->it_thread == td,
	    ("%s: ithread and proc linkage out of sync", __func__));
	ie = ithd->it_event;
	ie->ie_count = 0;

	/*
	 * As long as we have interrupts outstanding, go through the
	 * list of handlers, giving each one a go at it.
	 */
	for (;;) {
		/*
		 * If we are an orphaned thread, then just die.
		 */
		if (ithd->it_flags & IT_DEAD) {
			CTR3(KTR_INTR, "%s: pid %d (%s) exiting", __func__,
			    p->p_pid, td->td_name);
			free(ithd, M_ITHREAD);
			kthread_exit();
		}

		/*
		 * Service interrupts.  If another interrupt arrives while
		 * we are running, it will set it_need to note that we
		 * should make another pass.
		 */
		while (ithd->it_need) {
			/*
			 * This might need a full read and write barrier
			 * to make sure that this write posts before any
			 * of the memory or device accesses in the
			 * handlers.
			 */
			atomic_store_rel_int(&ithd->it_need, 0);
			ithread_execute_handlers(p, ie);
		}
		WITNESS_WARN(WARN_PANIC, NULL, "suspending ithread");
		mtx_assert(&Giant, MA_NOTOWNED);

		/*
		 * Processed all our interrupts.  Now get the sched
		 * lock.  This may take a while and it_need may get
		 * set again, so we have to check it again.
		 */
		/* XXX */
		thread_lock(td);
		if (!ithd->it_need && !(ithd->it_flags & IT_DEAD)) {
			TD_SET_IWAIT(td);
			ie->ie_count = 0;
			cv_wait((struct cv *)td->td_sleepqueue, 
				td->td_lock);
		}
		thread_unlock(td);
	}
}

/*
 * Main interrupt handling body.
 *
 * Input:
 * o ie:                        the event connected to this interrupt.
 * o frame:                     some archs (i.e. i386) pass a frame to some.
 *                              handlers as their main argument.
 * Return value:
 * o 0:                         everything ok.
 * o EINVAL:                    stray interrupt.
 */
int
intr_event_handle(struct intr_event *ie, struct trapframe *frame)
{
	struct intr_handler *ih;
	struct thread *td;
	int error, ret, thread;

	td = curthread;

	/* An interrupt with no event or handlers is a stray interrupt. */
	if (ie == NULL || TAILQ_EMPTY(&ie->ie_handlers))
		return (EINVAL);

	/*
	 * Execute fast interrupt handlers directly.
	 * To support clock handlers, if a handler registers
	 * with a NULL argument, then we pass it a pointer to
	 * a trapframe as its argument.
	 */
	td->td_intr_nesting_level++;
	thread = 0;
	ret = 0;
	critical_enter();
	TAILQ_FOREACH(ih, &ie->ie_handlers, ih_next) {
		if (ih->ih_filter == NULL) {
			thread = 1;
			continue;
		}
		CTR4(KTR_INTR, "%s: exec %p(%p) for %s", __func__,
		    ih->ih_filter, ih->ih_argument == NULL ? frame :
		    ih->ih_argument, ih->ih_name);
		if (ih->ih_argument == NULL)
			ret = ih->ih_filter(frame);
		else
			ret = ih->ih_filter(ih->ih_argument);
		/* 
		 * Wrapper handler special handling:
		 *
		 * in some particular cases (like pccard and pccbb), 
		 * the _real_ device handler is wrapped in a couple of
		 * functions - a filter wrapper and an ithread wrapper.
		 * In this case (and just in this case), the filter wrapper 
		 * could ask the system to schedule the ithread and mask
		 * the interrupt source if the wrapped handler is composed
		 * of just an ithread handler.
		 *
		 * TODO: write a generic wrapper to avoid people rolling 
		 * their own
		 */
		if (!thread) {
			if (ret == FILTER_SCHEDULE_THREAD)
				thread = 1;
		}
	}

	if (thread) {
		if (ie->ie_pre_ithread != NULL)
			ie->ie_pre_ithread(ie->ie_source);
	} else {
		if (ie->ie_post_filter != NULL)
			ie->ie_post_filter(ie->ie_source);
	}
	
	/* Schedule the ithread if needed. */
	if (thread) {
		error = intr_event_schedule_thread(ie);
#ifndef XEN		
		KASSERT(error == 0, ("bad stray interrupt"));
#else
		if (error != 0)
			log(LOG_WARNING, "bad stray interrupt");
#endif		
	}
	critical_exit();
	td->td_intr_nesting_level--;
	return (0);
}
#else
/*
 * This is the main code for interrupt threads.
 */
static void
ithread_loop(void *arg)
{
	struct intr_thread *ithd;
	struct intr_handler *ih;
	struct intr_event *ie;
	struct thread *td;
	struct proc *p;
	int priv;

	td = curthread;
	p = td->td_proc;
	ih = (struct intr_handler *)arg;
	priv = (ih->ih_thread != NULL) ? 1 : 0;
	ithd = (priv) ? ih->ih_thread : ih->ih_event->ie_thread;
	KASSERT(ithd->it_thread == td,
	    ("%s: ithread and proc linkage out of sync", __func__));
	ie = ithd->it_event;
	ie->ie_count = 0;

	/*
	 * As long as we have interrupts outstanding, go through the
	 * list of handlers, giving each one a go at it.
	 */
	for (;;) {
		/*
		 * If we are an orphaned thread, then just die.
		 */
		if (ithd->it_flags & IT_DEAD) {
			CTR3(KTR_INTR, "%s: pid %d (%s) exiting", __func__,
			    p->p_pid, td->td_name);
			free(ithd, M_ITHREAD);
			kthread_exit();
		}

		/*
		 * Service interrupts.  If another interrupt arrives while
		 * we are running, it will set it_need to note that we
		 * should make another pass.
		 */
		while (ithd->it_need) {
			/*
			 * This might need a full read and write barrier
			 * to make sure that this write posts before any
			 * of the memory or device accesses in the
			 * handlers.
			 */
			atomic_store_rel_int(&ithd->it_need, 0);
			if (priv)
				priv_ithread_execute_handler(p, ih);
			else 
				ithread_execute_handlers(p, ie);
		}
		WITNESS_WARN(WARN_PANIC, NULL, "suspending ithread");
		mtx_assert(&Giant, MA_NOTOWNED);

		/*
		 * Processed all our interrupts.  Now get the sched
		 * lock.  This may take a while and it_need may get
		 * set again, so we have to check it again.
		 */
		thread_lock(td);
		if (!ithd->it_need && !(ithd->it_flags & IT_DEAD)) {
			TD_SET_IWAIT(td);
			ie->ie_count = 0;
			mi_switch(SW_VOL | SWT_IWAIT, NULL);
		}
		thread_unlock(td);
	}
}

/* 
 * Main loop for interrupt filter.
 *
 * Some architectures (i386, amd64 and arm) require the optional frame 
 * parameter, and use it as the main argument for fast handler execution
 * when ih_argument == NULL.
 *
 * Return value:
 * o FILTER_STRAY:              No filter recognized the event, and no
 *                              filter-less handler is registered on this 
 *                              line.
 * o FILTER_HANDLED:            A filter claimed the event and served it.
 * o FILTER_SCHEDULE_THREAD:    No filter claimed the event, but there's at
 *                              least one filter-less handler on this line.
 * o FILTER_HANDLED | 
 *   FILTER_SCHEDULE_THREAD:    A filter claimed the event, and asked for
 *                              scheduling the per-handler ithread.
 *
 * In case an ithread has to be scheduled, in *ithd there will be a 
 * pointer to a struct intr_thread containing the thread to be
 * scheduled.
 */

static int
intr_filter_loop(struct intr_event *ie, struct trapframe *frame, 
		 struct intr_thread **ithd) 
{
	struct intr_handler *ih;
	void *arg;
	int ret, thread_only;

	ret = 0;
	thread_only = 0;
	TAILQ_FOREACH(ih, &ie->ie_handlers, ih_next) {
		/*
		 * Execute fast interrupt handlers directly.
		 * To support clock handlers, if a handler registers
		 * with a NULL argument, then we pass it a pointer to
		 * a trapframe as its argument.
		 */
		arg = ((ih->ih_argument == NULL) ? frame : ih->ih_argument);
		
		CTR5(KTR_INTR, "%s: exec %p/%p(%p) for %s", __func__,
		     ih->ih_filter, ih->ih_handler, arg, ih->ih_name);

		if (ih->ih_filter != NULL)
			ret = ih->ih_filter(arg);
		else {
			thread_only = 1;
			continue;
		}

		if (ret & FILTER_STRAY)
			continue;
		else { 
			*ithd = ih->ih_thread;
			return (ret);
		}
	}

	/*
	 * No filters handled the interrupt and we have at least
	 * one handler without a filter.  In this case, we schedule
	 * all of the filter-less handlers to run in the ithread.
	 */	
	if (thread_only) {
		*ithd = ie->ie_thread;
		return (FILTER_SCHEDULE_THREAD);
	}
	return (FILTER_STRAY);
}

/*
 * Main interrupt handling body.
 *
 * Input:
 * o ie:                        the event connected to this interrupt.
 * o frame:                     some archs (i.e. i386) pass a frame to some.
 *                              handlers as their main argument.
 * Return value:
 * o 0:                         everything ok.
 * o EINVAL:                    stray interrupt.
 */
int
intr_event_handle(struct intr_event *ie, struct trapframe *frame)
{
	struct intr_thread *ithd;
	struct thread *td;
	int thread;

	ithd = NULL;
	td = curthread;

	if (ie == NULL || TAILQ_EMPTY(&ie->ie_handlers))
		return (EINVAL);

	td->td_intr_nesting_level++;
	thread = 0;
	critical_enter();
	thread = intr_filter_loop(ie, frame, &ithd);	
	if (thread & FILTER_HANDLED) {
		if (ie->ie_post_filter != NULL)
			ie->ie_post_filter(ie->ie_source);
	} else {
		if (ie->ie_pre_ithread != NULL)
			ie->ie_pre_ithread(ie->ie_source);
	}
	critical_exit();
	
	/* Interrupt storm logic */
	if (thread & FILTER_STRAY) {
		ie->ie_count++;
		if (ie->ie_count < intr_storm_threshold)
			printf("Interrupt stray detection not present\n");
	}

	/* Schedule an ithread if needed. */
	if (thread & FILTER_SCHEDULE_THREAD) {
		if (intr_event_schedule_thread(ie, ithd) != 0)
			panic("%s: impossible stray interrupt", __func__);
	}
	td->td_intr_nesting_level--;
	return (0);
}
#endif
