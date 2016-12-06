
# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

import abc
import asyncio
import functools
import glob
import logging
import os
import rwlogger
import socket
import sys
import time
import types
import unittest

import gi
gi.require_version('CF', '1.0')
gi.require_version('RwMain', '1.0')
gi.require_version('RwManifestYang', '1.0')
gi.require_version('RwDtsYang', '1.0')

import gi.repository.CF as cf
import gi.repository.RwMain as rwmain
import gi.repository.RwManifestYang as rwmanifest
import gi.repository.RwDtsYang as RwDtsYang

from gi.repository import RwDts as rwdts


import rift.tasklets


if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async


class TaskletWaitError(Exception):
    pass


class AbstractDTSTest(unittest.TestCase):
    """Provides the base components for setting up DTS related unit tests.

    The class provides 3 hooks for subclasses:
    1. configure_suite(Optional): Similar to setUpClass, configs
            related to entire suite goes here
    2. configure_test(Optional): Similar to setUp
    3. configure_schema(Mandatory): Schema of the yang module.
    4. configure_timeout(Optional): timeout for each test case, defaults to 5


    Note:  Each tests uses a list of asyncio.Events for staging through the
    test.  These are required here because we are bring up each coroutine
    ("tasklet") at the same time and are not implementing any re-try
    mechanisms.  For instance, this is used in numerous tests to make sure that
    a publisher is up and ready before the subscriber sends queries.  Such
    event lists should not be used in production software.
    """
    rwmain = None
    tinfo = None
    schema = None
    id_cnt = 0
    default_timeout = 0
    top_dir = __file__[:__file__.find('/modules/core/')]
    log_level = logging.WARN

    # If log_utest_mode is set to True, RwLogger will be set turned
    # into a StreamHandler to write output to stderr instead of rwlog.
    # Otherwise no console message will be available.

    log_utest_mode = True

    @classmethod
    def setUpClass(cls):
        """
        1. create a rwmain
        2. Add DTS Router and Broker tasklets. Sets a random port for the broker
        3. Triggers the configure_suite and configure_schema hooks.
        """
        sock = socket.socket()
        # Get an available port from OS and pass it on to broker.
        sock.bind(('', 0))
        port = sock.getsockname()[1]
        sock.close()
        rwmsg_broker_port = port
        os.environ['RWMSG_BROKER_PORT'] = str(rwmsg_broker_port)

        plugin_dir = os.path.join(os.environ["RIFT_INSTALL"], "usr/lib/rift/plugins")

        if 'MESSAGE_BROKER_DIR' not in os.environ:
            os.environ['MESSAGE_BROKER_DIR'] = os.path.join(plugin_dir, 'rwmsgbroker-c')

        if 'ROUTER_DIR' not in os.environ:
            os.environ['ROUTER_DIR'] = os.path.join(plugin_dir, 'rwdtsrouter-c')

        msgbroker_dir = os.environ.get('MESSAGE_BROKER_DIR')
        router_dir = os.environ.get('ROUTER_DIR')
        [os.remove(f) for f in glob.glob(os.path.join(os.environ.get('INSTALLDIR'), './*.db'))]
        cls.manifest = cls.configure_manifest()
        cls.schema = cls.configure_schema()

        cls.dts_mgmt = None
        cls.rwmain = rwmain.Gi.new(cls.manifest)
        cls.tinfo = cls.rwmain.get_tasklet_info()

        # Run router in mainq. Eliminates some ill-diagnosed bootstrap races.
        os.environ['RWDTS_ROUTER_MAINQ'] = '1'
        if 'TEST_ENVIRON' not in os.environ:
            cls.rwmain.add_tasklet(msgbroker_dir, 'rwmsgbroker-c')
            cls.rwmain.add_tasklet(router_dir, 'rwdtsrouter-c')


        cls.log = rift.tasklets.logger_from_tasklet_info(cls.tinfo)
        cls.log.setLevel(logging.DEBUG)

        fmt = logging.Formatter(
                '%(asctime)-23s %(levelname)-5s  (%(name)s@%(process)d:%(filename)s:%(lineno)d) - %(message)s')
        stderr_handler = logging.StreamHandler(stream=sys.stderr)
        stderr_handler.setFormatter(fmt)
        stderr_handler.setLevel(cls.log_level)
        logging.getLogger().addHandler(stderr_handler)

        rwlogger.RwLogger.utest_mode = cls.log_utest_mode
        rwlogger.RwLogger.utest_mode_log_level = cls.log_level

        cls.default_timeout = cls.configure_timeout()
        cls.configure_suite(cls.rwmain)

        os.environ["PYTHONASYNCIODEBUG"] = "1"

    @abc.abstractclassmethod
    def configure_schema(cls):
        """
        Returns:
            yang schema.
        """
        raise NotImplementedError("configure_schema needs to be implemented")

    @classmethod
    def configure_manifest(cls):
        """
        Returns:
            manifest 
        """
        manifest = rwmanifest.Manifest()
        manifest.init_phase.settings.rwdtsrouter.single_dtsrouter.enable = True
        manifest.bootstrap_phase.rwmgmt.northbound_listing = [ "rwbase_schema_listing.txt" ]
        return manifest

    @classmethod
    def configure_timeout(cls):
        """
        Returns:
            Time limit for each test case, in seconds.
        """
        return 10

    @classmethod
    def configure_suite(cls, rwmain):
        """
        Args:
            rwmain (RwMain): Newly create rwmain instace, can be used to add
                    additional tasklets.
        """
        pass

    def setUp(self):
        """
        1. Creates an asyncio loop
        2. Triggers the hook configure_test
        """
        def scheduler_tick(self, *args):
            self.call_soon(self.stop)
            self.run_forever()

        # Init params: loop & timers
        self.loop = asyncio.new_event_loop()

        self.loop.scheduler_tick = types.MethodType(scheduler_tick, self.loop)

        self.asyncio_timer = None
        self.stop_timer = None
        self.__class__.id_cnt += 1
        self.dts_mgmt = rift.tasklets.DTS(self.tinfo, RwDtsYang.get_schema(), self.loop)
        self.configure_test(self.loop, self.__class__.id_cnt)

    def configure_test(self, loop, test_id):
        """
        Args:
            loop (asyncio.BaseEventLoop): Newly created asyncio event loop.
            test_id (int): Id for tests.
        """
        pass

    def run_until(self, test_done, timeout=None):
        """
        Attach the current asyncio event loop to rwsched and then run the
        scheduler until the test_done function returns True or timeout seconds
        pass.

        Args:
            test_done (function): function which should return True once the test is
                    complete and the scheduler no longer needs to run.
            timeout (int, optional): maximum number of seconds to run the test.
        """
        timeout = timeout or self.__class__.default_timeout
        tinfo = self.__class__.tinfo

        def shutdown(*args):
            if args:
                self.log.debug('Shutting down loop due to timeout')

            if self.asyncio_timer is not None:
                tinfo.rwsched_tasklet.CFRunLoopTimerRelease(self.asyncio_timer)
                self.asyncio_timer = None

            if self.stop_timer is not None:
                tinfo.rwsched_tasklet.CFRunLoopTimerRelease(self.stop_timer)
                self.stop_timer = None

            tinfo.rwsched_instance.CFRunLoopStop()

        def tick(*args):
            self.loop.call_later(0.1, self.loop.stop)

            try:
                self.loop.run_forever()
            except KeyboardInterrupt:
                self.log.debug("Shutting down loop dur to keyboard interrupt")
                shutdown()

            if test_done():
                shutdown()

        self.asyncio_timer = tinfo.rwsched_tasklet.CFRunLoopTimer(
            cf.CFAbsoluteTimeGetCurrent(),
            0.1,
            tick,
            None)

        self.stop_timer = tinfo.rwsched_tasklet.CFRunLoopTimer(
            cf.CFAbsoluteTimeGetCurrent() + timeout,
            0,
            shutdown,
            None)

        tinfo.rwsched_tasklet.CFRunLoopAddTimer(
            tinfo.rwsched_tasklet.CFRunLoopGetCurrent(),
            self.stop_timer,
            tinfo.rwsched_instance.CFRunLoopGetMainMode())

        tinfo.rwsched_tasklet.CFRunLoopAddTimer(
            tinfo.rwsched_tasklet.CFRunLoopGetCurrent(),
            self.asyncio_timer,
            tinfo.rwsched_instance.CFRunLoopGetMainMode())

        tinfo.rwsched_instance.CFRunLoopRun()

        self.assertTrue(test_done())

    def new_tinfo(self, name):
        """
        Create a new tasklet info instance with a unique instance_id per test.
        It is up to each test to use unique names if more that one tasklet info
        instance is needed.

        @param name - name of the "tasklet"
        @return     - new tasklet info instance
        """
        # Accessing using class for consistency.
        ret = self.__class__.rwmain.new_tasklet_info(
                name,
                self.__class__.id_cnt)

        log = rift.tasklets.logger_from_tasklet_info(ret)
        log.setLevel(logging.DEBUG)

        stderr_handler = logging.StreamHandler(stream=sys.stderr)
        fmt = logging.Formatter(
                '%(asctime)-23s %(levelname)-5s  (%(name)s@%(process)d:%(filename)s:%(lineno)d) - %(message)s')
        stderr_handler.setFormatter(fmt)
        log.addHandler(stderr_handler)

        return ret

    @asyncio.coroutine
    def wait_for_tasklet_running(self, tasklet_name, timeout=10):
        """ Waits for all instances of a tasklet to reach running state

        Arguments:
           tasklet_name - The name of the tasklet (e.g. rwlogd-c)
           timeout - Number of seconds to wait until the tasklet reaches run state

        Raises:
           TaskletWaitError - Tasklet did not reach run state before timeout was reached.
        """

        start_time = time.time()
        while True:
            res_iter = yield from self.dts_mgmt.query_read("D,/rwdts:dts")
            self.log.debug("Waiting for %s tasklet to reach running state", tasklet_name)
            for i in res_iter:
                res = (yield from i).result
                tasklet_members = [mbr for mbr in res.member if tasklet_name in mbr.name]
                if len(tasklet_members) == 0:
                    continue

                if all(mbr.dts_state == "run" for mbr in tasklet_members):
                    # All members for the tasklet have reached running state
                    self.log.debug("%s tasklet has reached running state", tasklet_name)
                    return

                if (start_time - time.time()) > timeout:
                    raise TaskletWaitError(
                            "Timed out waiting for tasklet %s to enter running state" % tasklet_name
                            )

                yield from asyncio.sleep(1, loop=self.loop)

def async_test(f):
    """
    Runs the testcase within a coroutine using the current test cases loop.
    """
    @functools.wraps(f)
    def wrapper(self, *args, **kwargs):
        loop = getattr(self, "loop", None) or getattr(self, "_loop", None)
        if loop is None:
            raise ValueError("Could not find loop attribute in first param")

        coro = asyncio.coroutine(f)
        future = coro(self, *args, **kwargs)
        task = asyncio.ensure_future(future, loop=loop)

        run_until = getattr(self, "run_until", None)
        if run_until is not None:
            run_until(task.done)
        else:
            loop.run_until_complete(task)

        if task.exception() is not None:
            self.log.error("Caught exception during test: %s", str(task.exception()))
            raise task.exception()

    return wrapper


class DescriptorPublisher(object):
    def __init__(self, log, dts, loop):
        self.log = log
        self.loop = loop
        self.dts = dts

        self._registrations = []

    @asyncio.coroutine
    def publish(self, w_path, path, desc):
        ready_event = asyncio.Event(loop=self.loop)

        @asyncio.coroutine
        def on_ready(regh, status):
            self.log.debug("Create element: %s, obj-type:%s obj:%s",
                           path, type(desc), desc)
            with self.dts.transaction() as xact:
                regh.create_element(path, desc, xact.xact)
            self.log.debug("Created element: %s, obj:%s", path, desc)
            ready_event.set()

        handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready
                )

        self.log.debug("Registering path: %s, obj:%s", w_path, desc)
        reg = yield from self.dts.register(
                w_path,
                handler,
                flags=rwdts.Flag.PUBLISHER | rwdts.Flag.NO_PREP_READ
                )
        self._registrations.append(reg)
        self.log.debug("Registered path : %s", w_path)
        yield from ready_event.wait()

        return reg

    def unpublish_all(self):
        self.log.debug("Deregistering all published descriptors")
        for reg in self._registrations:
            reg.deregister()
