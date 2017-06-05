"""
# STANDARD_RIFT_IO_COPYRIGHT #

@file component.py
@author Joshua Downer (joshua.downer@riftio.com)
@date 03/04/2015

"""
import asyncio
import logging
import sys

import gi
gi.require_version('CF', '1.0')
gi.require_version('RwSched', '1.0')
gi.require_version('RwTaskletPlugin', '1.0')

import gi.repository.CF as cf
import gi.repository.GObject as GObject
import gi.repository.RwSched as rwsched
import gi.repository.RwTaskletPlugin as rwtasklet
import six

from . import monkey

import rwlogger


class UnknownTaskletError(Exception):
    def __init__(self, component_handle, instance_handle):
        msg = "Unable to find tasklet ({},{})".format(
                component_handle,
                instance_handle,
                )
        super(UnknownTaskletError, self).__init__(msg)


class StackedEventLoop(asyncio.SelectorEventLoop):
    def __init__(self, sched, sched_tasklet, log):
        """
        Create an asyncio EventLoop that integrates with the core Rift.io
        scheduler.

        The StackedEventLoop is a fully compliant asyncio.BaseEventLoop that
        will be started and stopped as necessary by the Rift.io scheduler.  It
        uses epoll to monitor file handles owned by the event loop.  The epoll
        file handle itself is attached to the Rift scheduler so that the
        scheduler is informed of when the event loop has work to perform.

        Each run of the event loop by the scheduler is referred to as a tick.  A
        tick will run all Tasks that are in the ready state on the event loop
        and then stop.  Prior to completing the tick the event loop may
        reschedule itself on the Rift scheduler if the prior run moved new tasks
        to the ready state or if there are any scheduled events.  In the case of
        ready events, it will request to be run again as soon as possible.  If
        only scheduled events remain, the event loop will schedule itself to
        run again when the nearest event will be ready.

        @param sched          - Rift scheduler handle
        @param sched_tasklet  - Rift scheduler tasklet handle
        @param log            - log handle
        """

        super(StackedEventLoop, self).__init__(selector=asyncio.selectors.EpollSelector())

        self._sched = sched
        self._sched_tasklet = sched_tasklet
        self._log = log
        self._cfsocket = None
        self._cfsocket_src = None
        self._cftimer = None
        self._scheduler_tick_enter_hooks = []
        self._scheduler_tick_exit_hooks = []

        monkey.patch()

        def set_event_loop():
            """ Set the current loop as the current context's event loop

            This allows callers to use asyncio without passing in the explicit
            event loop.
            """
            asyncio.set_event_loop(self)

        def unset_event_loop():
            """ Unset the current loop as the current context's event loop

            This prevents any other tasklets/library from inadvertantly using
            this tasklets event loop.
            """
            asyncio.set_event_loop(None)

        self.register_tick_enter_hook(set_event_loop)
        self.register_tick_exit_hook(unset_event_loop)

    def debug(self, msg, *args, **kwargs):
        if self._debug:
            self._log.debug(msg, *args, **kwargs)

    def register_tick_enter_hook(self, hook):
        """ Register a function to be called on each scheduler_tick() invocation

        Arguments:
            hook - A function to call on scheduler tick
        """
        if hook not in self._scheduler_tick_enter_hooks:
            self._scheduler_tick_enter_hooks.append(hook)

    def unregister_tick_enter_hook(self, hook):
        """ Unregister a previously register enter scheduler tick hook

        Arguments:
            hook - A previously registered hook
        """
        if hook in self._scheduler_tick_enter_hooks:
            self._scheduler_tick_enter_hooks.remove(hook)

    def register_tick_exit_hook(self, hook):
        """ Register a function to be called on scheduler_tick() exit

        Arguments:
            hook - A function to call on scheduler tick
        """
        if hook not in self._scheduler_tick_exit_hooks:
            self._scheduler_tick_exit_hooks.append(hook)

    def unregister_tick_exit_hook(self, hook):
        """ Unregister a previously registered scheduler exit tick hook

        Arguments:
            hook - A previously registered hook
        """
        if hook in self._scheduler_tick_exit_hooks:
            self._scheduler_tick_exit_hooks.remove(hook)

    def scheduler_tick(self, *args):
        """
        Process all ready events on the loop and reschedule if necessary:
            - If any tasks are in the ready state, reschedule as soon as
              possible.
            - If events are scheduled to transition to ready in the future,
              scheduled to run again at the timeout for the closest.
        """
        self.debug('Scheduler tick:  tasks: %d, ready: %d, scheduled: %d',
                len(asyncio.Task.all_tasks(self)),
                len(self._ready),
                len(self._scheduled))

        # Call all registered enter tick hooks
        for enter_hook in self._scheduler_tick_enter_hooks:
            enter_hook()

        self.call_soon(self.stop)
        self.run_forever()

        if self._ready:
            self.debug('Scheduler has events ready to run, running again soon')

            self._sched_tasklet.CFRunLoopTimerSetNextFireDate(
                    self._cftimer,
                    cf.CFAbsoluteTimeGetCurrent())
        elif self._scheduled:
            when = self._scheduled[0]._when
            timeout = max(0, (when - self.time() + self._clock_resolution))

            self.debug('Running scheduler again in %f seconds', timeout)
            self._sched_tasklet.CFRunLoopTimerSetNextFireDate(
                    self._cftimer,
                    cf.CFAbsoluteTimeGetCurrent() + timeout)

        # Call all registered exit tick hooks
        for exit_hook in self._scheduler_tick_exit_hooks:
            exit_hook()

    def _on_signal(self, tasklet, signal):
        self.debug("Got signal handler callback for signal: %s", signal)
        self._handle_signal(signal)

    def add_signal_handler(self, sig, callback):
        """
        Connect a signal handler to the Rift scheduler.

        This method overrides the default add_signal_handler() to play properly
        with the CFRunLoop and potentially allow many tasklets to register for
        the same signal (see RIFT-7743)

        This implementation has limitations in removing an added signal handler.
        """
        self.debug("Adding signal %s handler %s to tasklet", sig, callback)

        self._sched_tasklet.signal(sig, self._on_signal)

        super().add_signal_handler(sig, callback)

    def connect_to_scheduler(self):
        """
        Connect this event loop to the Rift scheduler.
        """
        # Create a timer that can be called at irregular intervals from the
        # scheduler_tick() as necessary.

        # See discussion url below, we have random intervals at which we want
        # this timer to fire so we set a massive interval and then call
        # SetNextFireDate() when we actually need to trigger it.
        # https://developer.apple.com/library/mac/documentation/CoreFoundation/Reference/CFRunLoopTimerRef/index.html#//apple_ref/c/func/CFRunLoopTimerCreate
        self._cftimer = self._sched_tasklet.CFRunLoopTimer(
            fireDate=0,
            interval=60 * 60 * 24 * 365 * 100,
            callback=self.scheduler_tick,
            user_data=None)

        self._sched_tasklet.CFRunLoopAddTimer(
                self._sched_tasklet.CFRunLoopGetCurrent(),
                self._cftimer,
                self._sched.CFRunLoopGetMainMode())


        # Create a CFSource monitoring the epoll file descriptor for any events
        # on the set of file descriptors being monitored by epoll.

        cb_flags = rwsched.Cfsocketcallbacktype.KCFSOCKETREADCALLBACK

        socket_flags = rwsched.Cfsocketcallbacktype.KCFSOCKETAUTOMATICALLYREENABLEREADCALLBACK

        self._cfsocket = self._sched_tasklet.CFSocketBindToNative(
                self._selector._epoll.fileno(),
                cb_flags,
                self.scheduler_tick,
                None)
        self._sched_tasklet.CFSocketSetSocketFlags(
                self._cfsocket,
                socket_flags)

        self._cfsocket_src = self._sched_tasklet.CFSocketCreateRunLoopSource(
                self._sched.CFAllocatorGetDefault(),
                self._cfsocket,
                0)

        self._sched_tasklet.CFRunLoopAddSource(
            self._sched_tasklet.CFRunLoopGetCurrent(),
            self._cfsocket_src,
            self._sched.CFRunLoopGetMainMode())

    def disconnect_from_scheduler(self):
        """
        Disconnect this event loop from the Rift scheduler and clean up any
        artifacts.
        """
        self._sched_tasklet.CFRunLoopRemoveSource(
                self._sched_tasklet.CFRunLoopGetCurrent(),
                self._cfsocket_src,
                self._sched.CFRunLoopGetMainMode())
        self._sched_tasklet.CFSocketReleaseRunLoopSource(self._cfsocket_src)
        self._sched_tasklet.CFSocketRelease(self._cfsocket)
        self._sched_tasklet.CFRunLoopTimerRelease(self._cftimer)
        self._cfsocket_src = None
        self._cfsocket = None
        self._cftimer = None

    def create_task(self, coro):

        def capture_exception(fut):
            try:
                fut.exception()
            except asyncio.CancelledError:
                pass
            except Exception as ex:
                self._log.error("Scheduled task: {} failed with exception {}"
                                .format(fut, ex))


        task = super().create_task(coro)
        task.add_done_callback(capture_exception)

        return task


class MetaTasklet(type):
    """
    This is a metaclass that is used to automatically create a component class
    to be associated with a tasklet class. The component class can be loaded as
    a plugin and used by VCS to create and control tasklets.
    """

    def __new__(meta, name, bases, dct):
        """Create a new tasklet type

        Arguments:
            meta  - this meta class
            name  - the name of the new type
            bases - a tuple of base classes that the new type will derived from
            dct   - a dictionary of attributes

        Returns:
            A new tasklet type

        """
        # This line creates an object that represents the tasklet class. This
        # is what normally happens implicitly. This new type is then used by
        # the Component class that we create below.
        tasklet_type = super(MetaTasklet, meta).__new__(meta, name, bases, dct)

        class Component(GObject.Object, rwtasklet.Component):
            def __init__(self):
                """Create a Component object"""
                GObject.Object.__init__(self)
                self.tasklets = {}

            def do_component_init(self):
                """Initialize the component

                This function initializes the component and returns the
                component handle.

                Returns:
                    A component handle

                """
                return rwtasklet.ComponentHandle()

            def do_component_deinit(self, component_handle):
                """Deinitialize the component

                Currently this is a no-op.

                Arguments:
                    component_handle - the handle for this component

                """
                pass

            def do_instance_alloc(self,
                    component_handle,
                    tasklet_info,
                    instance_url,
                    ):
                """Create a tasklet instance

                Arguments:
                    component_handle - the handle associated with this
                                       component
                    tasklet_info     - information about the tasklet that is
                                       being created
                    instance_url     - the instance URL

                Returns:
                    An instance handle

                """
                instance_handle = rwtasklet.InstanceHandle()

                # NB: this is essentially the reason for the metaclass. Here we
                # are using the new tasklet type that was created above.
                tasklet = tasklet_type(tasklet_info)

                # Add the new tasklet to the dict of tasklets and use the
                # component and instances handles (as a pair) to define the
                # key.
                self.tasklets[(component_handle, instance_handle)] = tasklet

                return instance_handle

            def do_instance_free(self, component_handle, instance_handle):
                """Free the memory associated with an instance

                Arguments:
                    component_handle - the handle of this component
                    instance_handle  - the handle of the instance to free

                Raises:
                    An UnknownTaskletError is raised if the tasklet was not
                    created by this component or has already been freed.

                """
                try:
                    del self.tasklets[(component_handle, instance_handle)]
                except KeyError:
                    raise UnknownTaskletError(component_handle, instance_handle)

            def do_instance_start(self, component_handle, instance_handle):
                """Start a tasklet

                Finds the tasklet associated with the specified component and
                instance handles and invokes the 'start' function.

                Arguments:
                    component_handle - the handle of this component
                    instance_handle  - the handle of the instance to free

                Raises:
                    An UnknownTaskletError is raised if the tasklet was not
                    created by this component or has already been freed.

                """
                try:
                    self.tasklets[(component_handle, instance_handle)].start()
                except KeyError:
                    raise UnknownTaskletError(component_handle, instance_handle)

            def do_instance_stop(self, component_handle, instance_handle):
                """Stop a tasklet

                Finds the tasklet associated with the specified component and
                instance handles and invokes the 'stop' function.

                Arguments:
                    component_handle - the handle of this component
                    instance_handle  - the handle of the instance to free

                Raises:
                    An UnknownTaskletError is raised if the tasklet was not
                    created by this component or has already been freed.

                """
                try:
                    self.tasklets[(component_handle, instance_handle)].stop()
                except KeyError:
                    raise UnknownTaskletError(component_handle, instance_handle)

        # Add the component type to the module that contains the tasklet. This
        # is essential so that the plugin can be found and loaded. The GI
        # framework searches for plugins at the top-level of the module after
        # the module has been imported. To help GI find our component class we
        # added it to the module as an attribute.
        module = sys.modules[tasklet_type.__module__]
        setattr(module, 'rift_gi_component_{}'.format(name), Component)

        return tasklet_type


@six.add_metaclass(MetaTasklet)
class Tasklet(object):
    """
    This is the base class for tasklets. Tasklets have only 2 functions on
    their interface: 'start' and 'stop'. There is no assumption that a tasklet
    cannot be restarted by calling 'start' following a call to 'stop'.
    """
    def __init__(self, tasklet_info):
        """
        Create a Tasklet object

        Tasklets are not directly created by the user, but are created by the
        associated Component class via VCS.

        @param tasklet_info - info relevant to this tasklet

        """
        self._tasklet_info = tasklet_info

        self._log = logger_from_tasklet_info(tasklet_info)
        self._log.setLevel(logging.DEBUG)

        self._log_hdl = tasklet_info.get_rwlog_ctx()
        self._rwlog_handler = rwlogger.RwLogger(
                        log_hdl=self._log_hdl
                        )
        self._log.addHandler(self._rwlog_handler)

        # We want set up the root logger to log to this tasklets rwlog handler
        # in order to catch any logs emitted via third party libraries.
        # Because of this, we want to ensure that any logs sent to this logger do
        # not propogate to the root logger which would cause the messages to get
        # logged twice.
        self._log.propagate = False

        self._loop = StackedEventLoop(
                self._tasklet_info.rwsched_instance,
                self._tasklet_info.rwsched_tasklet,
                self._log)

        self._add_root_logger_handling()

    def _add_root_logger_handling(self):
        """
        Create a scheduler hook to set the root logger for this
        tasklet on entry.  This enables us to capture the logs
        used by libraries without having to manually find
        the library's logger and set the handler.
        """
        root_logger = logging.getLogger()
        root_logger.setLevel(logging.DEBUG)
        root_rwlog_handler = rwlogger.RwLogger(
                        log_hdl=self._log_hdl,
                        )

        # We need to filter this handler by the core CFRunLoop thread id
        # so only messages being logged specifically during this tasklets
        # scheduler invocation are logged using it's handler.
        root_rwlog_handler.addFilter(rwlogger.ThreadFilter.from_current())

        def add_root_logger_handler():
            """ Set the root logger's handler to this tasklets rwlog handler """

            # Clone the current category/subcategory assignment for the tasklet
            root_rwlog_handler.set_category(self._rwlog_handler.category)
            root_rwlog_handler.set_subcategory(self._rwlog_handler.subcategory)

            root_logger.addHandler(root_rwlog_handler)

        def remove_root_logger_handler():
            """ Remove the root logger's handler to this tasklets rwlog handler """
            root_logger.removeHandler(root_rwlog_handler)

        self._loop.register_tick_enter_hook(add_root_logger_handler)
        self._loop.register_tick_exit_hook(remove_root_logger_handler)

    @property
    def tasklet_info(self):
        """The tasklet info associated with this tasklet"""
        return self._tasklet_info

    @property
    def log(self):
        """The log instance used by this tasklet"""
        return self._log

    @property
    def rwlog(self):
        """The rwlogger instance used by this tasklet"""
        return self._rwlog_handler

    @property
    def log_hdl(self):
        """The rwlog handle used by this tasklet"""
        return self._log_hdl

    @property
    def loop(self):
        """
        The asyncio event loop being used by this tasklet.

        This loop instance needs to be passes into all asyncio calls which take
        an optional 'loop' argument.

        Typicaly loop does not need to be specified because all calls because
        the loop is shared across all python threads and accessible via
        asyncio.get_event_loop().  In tasklets, this is not desirable as it
        would mean that other tasklets in the same process would be executing
        events when this tasklets event loop is running.
        """
        return self._loop

    def add_log_stderr_handler(self):
        """
        Add a stderr handler to the Tasklet's log instance.

        This shouldn't be necessary if rwlog is working correctly, but can
        be useful for development in certain situations.
        """
        stderr_handler = logging.StreamHandler(stream=sys.stderr)
        fmt = logging.Formatter(
                '%(asctime)-23s %(levelname)-5s  (%(name)s@%(process)d:%(filename)s:%(lineno)d) - %(message)s')
        stderr_handler.setFormatter(fmt)
        self._log.addHandler(stderr_handler)

    def start(self):
        """
        Start the tasklet

        The default implementation will hook the tasklet's event loop to the
        Rift scheduler.
        """
        self.loop.connect_to_scheduler()

    def stop(self):
        """
        Stop the tasklet

        If the default implementation was used to hook the tasklet's event loop
        to the Rift scheduler, this method must be called to unhook.
        """
        self.loop.disconnect_from_scheduler()


def logger_from_tasklet_info(tasklet_info):
    """
    Get the logger for the specified tasklet.

    @param tasklet_info - tasklet-info for which to get a logger
    @return - a logger for the specified tasklet
    """
    log_name = tasklet_info.instance_name.replace('.', '_')
    log = logging.getLogger('rw.tasklet.%s' % (log_name,))


    return log


# vim: sw=4
