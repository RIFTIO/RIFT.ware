/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include "gioscheduler.h"
#include "gcancellable.h"


/**
 * SECTION:gioscheduler
 * @short_description: I/O Scheduler
 * @include: gio/gio.h
 * 
 * Schedules asynchronous I/O operations. #GIOScheduler integrates 
 * into the main event loop (#GMainLoop) and uses threads.
 * 
 * <para id="io-priority"><indexterm><primary>I/O priority</primary></indexterm>
 * Each I/O operation has a priority, and the scheduler uses the priorities
 * to determine the order in which operations are executed. They are 
 * <emphasis>not</emphasis> used to determine system-wide I/O scheduling.
 * Priorities are integers, with lower numbers indicating higher priority. 
 * It is recommended to choose priorities between %G_PRIORITY_LOW and 
 * %G_PRIORITY_HIGH, with %G_PRIORITY_DEFAULT as a default.
 * </para>
 **/

struct _GIOSchedulerJob {
  GList *active_link;
  GIOSchedulerJobFunc job_func;
  GSourceFunc cancel_func; /* Runs under job map lock */
  gpointer data;
  GDestroyNotify destroy_notify;

  gint io_priority;
  GCancellable *cancellable;
  gulong cancellable_id;
  GMainContext *context;
};

G_LOCK_DEFINE_STATIC(active_jobs);
static GList *active_jobs = NULL;

static GThreadPool *job_thread_pool = NULL;

static void io_job_thread (gpointer data,
			   gpointer user_data);

static void
g_io_job_free (GIOSchedulerJob *job)
{
  if (job->cancellable)
    {
      if (job->cancellable_id)
	g_cancellable_disconnect (job->cancellable, job->cancellable_id);
      g_object_unref (job->cancellable);
    }
  g_main_context_unref (job->context);
  g_free (job);
}

static gint
g_io_job_compare (gconstpointer a,
		  gconstpointer b,
		  gpointer      user_data)
{
  const GIOSchedulerJob *aa = a;
  const GIOSchedulerJob *bb = b;

  /* Cancelled jobs are set prio == -1, so that
     they are executed as quickly as possible */
  
  /* Lower value => higher priority */
  if (aa->io_priority < bb->io_priority)
    return -1;
  if (aa->io_priority == bb->io_priority)
    return 0;
  return 1;
}

static gpointer
init_scheduler (gpointer arg)
{
  if (job_thread_pool == NULL)
    {
      /* TODO: thread_pool_new can fail */
      job_thread_pool = g_thread_pool_new (io_job_thread,
					   NULL,
					   10,
					   FALSE,
					   NULL);
      if (job_thread_pool != NULL)
	{
	  g_thread_pool_set_sort_function (job_thread_pool,
					   g_io_job_compare,
					   NULL);
	}
    }
  return NULL;
}

static void
on_job_canceled (GCancellable    *cancellable,
		 gpointer         user_data)
{
  GIOSchedulerJob *job = user_data;

  /* This might be called more than once */
  job->io_priority = -1;

  if (job_thread_pool != NULL)
    g_thread_pool_set_sort_function (job_thread_pool,
				     g_io_job_compare,
				     NULL);
}

static void
job_destroy (gpointer data)
{
  GIOSchedulerJob *job = data;

  if (job->destroy_notify)
    job->destroy_notify (job->data);

  G_LOCK (active_jobs);
  active_jobs = g_list_delete_link (active_jobs, job->active_link);
  G_UNLOCK (active_jobs);
  g_io_job_free (job);
}

static void
io_job_thread (gpointer data,
	       gpointer user_data)
{
  GIOSchedulerJob *job = data;
  gboolean result;

  if (job->cancellable)
    g_cancellable_push_current (job->cancellable);

  do 
    {
      result = job->job_func (job, job->cancellable, job->data);
    }
  while (result);

  if (job->cancellable)
    g_cancellable_pop_current (job->cancellable);

  job_destroy (job);
}

/**
 * g_io_scheduler_push_job:
 * @job_func: a #GIOSchedulerJobFunc.
 * @user_data: data to pass to @job_func
 * @notify: (allow-none): a #GDestroyNotify for @user_data, or %NULL
 * @io_priority: the <link linkend="gioscheduler">I/O priority</link> 
 * of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 *
 * Schedules the I/O job to run in another thread.
 *
 * @notify will be called on @user_data after @job_func has returned,
 * regardless whether the job was cancelled or has run to completion.
 * 
 * If @cancellable is not %NULL, it can be used to cancel the I/O job
 * by calling g_cancellable_cancel() or by calling 
 * g_io_scheduler_cancel_all_jobs().
 **/
void
g_io_scheduler_push_job (GIOSchedulerJobFunc  job_func,
			 gpointer             user_data,
			 GDestroyNotify       notify,
			 gint                 io_priority,
			 GCancellable        *cancellable)
{
  static GOnce once_init = G_ONCE_INIT;
  GIOSchedulerJob *job;

  g_return_if_fail (job_func != NULL);

  job = g_new0 (GIOSchedulerJob, 1);
  job->job_func = job_func;
  job->data = user_data;
  job->destroy_notify = notify;
  job->io_priority = io_priority;
    
  if (cancellable)
    {
      job->cancellable = g_object_ref (cancellable);
      job->cancellable_id = g_cancellable_connect (job->cancellable, (GCallback)on_job_canceled,
						   job, NULL);
    }

  job->context = g_main_context_ref_thread_default ();

  G_LOCK (active_jobs);
  active_jobs = g_list_prepend (active_jobs, job);
  job->active_link = active_jobs;
  G_UNLOCK (active_jobs);

  g_once (&once_init, init_scheduler, NULL);
  g_thread_pool_push (job_thread_pool, job, NULL);
}

/**
 * g_io_scheduler_cancel_all_jobs:
 * 
 * Cancels all cancellable I/O jobs. 
 *
 * A job is cancellable if a #GCancellable was passed into
 * g_io_scheduler_push_job().
 **/
void
g_io_scheduler_cancel_all_jobs (void)
{
  GList *cancellable_list, *l;
  
  G_LOCK (active_jobs);
  cancellable_list = NULL;
  for (l = active_jobs; l != NULL; l = l->next)
    {
      GIOSchedulerJob *job = l->data;
      if (job->cancellable)
	cancellable_list = g_list_prepend (cancellable_list,
					   g_object_ref (job->cancellable));
    }
  G_UNLOCK (active_jobs);

  for (l = cancellable_list; l != NULL; l = l->next)
    {
      GCancellable *c = l->data;
      g_cancellable_cancel (c);
      g_object_unref (c);
    }
  g_list_free (cancellable_list);
}

typedef struct {
  GSourceFunc func;
  gboolean ret_val;
  gpointer data;
  GDestroyNotify notify;

  GMutex ack_lock;
  GCond ack_condition;
  gboolean ack;
} MainLoopProxy;

static gboolean
mainloop_proxy_func (gpointer data)
{
  MainLoopProxy *proxy = data;

  proxy->ret_val = proxy->func (proxy->data);

  if (proxy->notify)
    proxy->notify (proxy->data);

  g_mutex_lock (&proxy->ack_lock);
  proxy->ack = TRUE;
  g_cond_signal (&proxy->ack_condition);
  g_mutex_unlock (&proxy->ack_lock);

  return FALSE;
}

static void
mainloop_proxy_free (MainLoopProxy *proxy)
{
  g_mutex_clear (&proxy->ack_lock);
  g_cond_clear (&proxy->ack_condition);
  g_free (proxy);
}

/**
 * g_io_scheduler_job_send_to_mainloop:
 * @job: a #GIOSchedulerJob
 * @func: a #GSourceFunc callback that will be called in the original thread
 * @user_data: data to pass to @func
 * @notify: (allow-none): a #GDestroyNotify for @user_data, or %NULL
 * 
 * Used from an I/O job to send a callback to be run in the thread
 * that the job was started from, waiting for the result (and thus
 * blocking the I/O job).
 *
 * Returns: The return value of @func
 **/
gboolean
g_io_scheduler_job_send_to_mainloop (GIOSchedulerJob *job,
				     GSourceFunc      func,
				     gpointer         user_data,
				     GDestroyNotify   notify)
{
  GSource *source;
  MainLoopProxy *proxy;
  gboolean ret_val;

  g_return_val_if_fail (job != NULL, FALSE);
  g_return_val_if_fail (func != NULL, FALSE);

  proxy = g_new0 (MainLoopProxy, 1);
  proxy->func = func;
  proxy->data = user_data;
  proxy->notify = notify;
  g_mutex_init (&proxy->ack_lock);
  g_cond_init (&proxy->ack_condition);
  g_mutex_lock (&proxy->ack_lock);

  source = g_idle_source_new ();
  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_set_callback (source, mainloop_proxy_func, proxy,
			 NULL);

  g_source_attach (source, job->context);
  g_source_unref (source);

  while (!proxy->ack)
    g_cond_wait (&proxy->ack_condition, &proxy->ack_lock);
  g_mutex_unlock (&proxy->ack_lock);

  ret_val = proxy->ret_val;
  mainloop_proxy_free (proxy);
  
  return ret_val;
}

/**
 * g_io_scheduler_job_send_to_mainloop_async:
 * @job: a #GIOSchedulerJob
 * @func: a #GSourceFunc callback that will be called in the original thread
 * @user_data: data to pass to @func
 * @notify: (allow-none): a #GDestroyNotify for @user_data, or %NULL
 * 
 * Used from an I/O job to send a callback to be run asynchronously in
 * the thread that the job was started from. The callback will be run
 * when the main loop is available, but at that time the I/O job might
 * have finished. The return value from the callback is ignored.
 *
 * Note that if you are passing the @user_data from g_io_scheduler_push_job()
 * on to this function you have to ensure that it is not freed before
 * @func is called, either by passing %NULL as @notify to 
 * g_io_scheduler_push_job() or by using refcounting for @user_data.
 **/
void
g_io_scheduler_job_send_to_mainloop_async (GIOSchedulerJob *job,
					   GSourceFunc      func,
					   gpointer         user_data,
					   GDestroyNotify   notify)
{
  GSource *source;
  MainLoopProxy *proxy;

  g_return_if_fail (job != NULL);
  g_return_if_fail (func != NULL);

  proxy = g_new0 (MainLoopProxy, 1);
  proxy->func = func;
  proxy->data = user_data;
  proxy->notify = notify;
  g_mutex_init (&proxy->ack_lock);
  g_cond_init (&proxy->ack_condition);

  source = g_idle_source_new ();
  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_set_callback (source, mainloop_proxy_func, proxy,
			 (GDestroyNotify)mainloop_proxy_free);

  g_source_attach (source, job->context);
  g_source_unref (source);
}
