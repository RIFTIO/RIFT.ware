#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import asyncio
import logging
import os
import sys
import threading
import time
import unittest

import xmlrunner

import gi.repository.CF as cf
import gi.repository.RwMain as rwmain
import gi.repository.RwManifestYang as rwmanifest

import rift.tasklets

if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async


class StackedEventLoopTestCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        manifest = rwmanifest.Manifest()
        cls.rwmain = rwmain.Gi.new(manifest)
        cls.tinfo = cls.rwmain.get_tasklet_info()

        cls.log = rift.tasklets.logger_from_tasklet_info(cls.tinfo)
        cls.log.setLevel(logging.DEBUG)

        stderr_handler = logging.StreamHandler(stream=sys.stderr)
        fmt = logging.Formatter(
                '%(asctime)-23s %(levelname)-5s  (%(name)s@%(process)d:%(filename)s:%(lineno)d) - %(message)s')
        stderr_handler.setFormatter(fmt)
        cls.log.addHandler(stderr_handler)

    def setUp(self):
        self.done_check = None
        self.stop_timer = None

        self.loop = rift.tasklets.StackedEventLoop(
                self.tinfo.rwsched_instance,
                self.tinfo.rwsched_tasklet,
                self.log)

        self.loop.connect_to_scheduler()

    def tearDown(self):
        self.loop.disconnect_from_scheduler()

    def run_until(self, test_done, timeout=5):
        """
        Run the rift scheduler until test_done() returns true or timeout
        seconds pass.

        Calls assertTrue() on test_done().

        @param test_done  - function which should return True once the test is
                            complete and the scheduler no longer needs to run.
        @param timeout    - maximum number of seconds to run the test.
        """
        def shutdown(*args):
            if args:
                self.log.error('Shutting down loop due to timeout')

            if self.done_check is not None:
                self.tinfo.rwsched_tasklet.CFRunLoopTimerRelease(self.done_check)
                self.done_check = None

            if self.stop_timer is not None:
                self.tinfo.rwsched_tasklet.CFRunLoopTimerRelease(self.stop_timer)
                self.stop_timer = None

            self.tinfo.rwsched_instance.CFRunLoopStop()

        def tick(*args):
            if test_done():
                shutdown()


        self.done_check = self.tinfo.rwsched_tasklet.CFRunLoopTimer(
            cf.CFAbsoluteTimeGetCurrent(),
            0.01,
            tick,
            None)

        self.stop_timer = self.tinfo.rwsched_tasklet.CFRunLoopTimer(
            cf.CFAbsoluteTimeGetCurrent() + timeout,
            0,
            shutdown,
            None)

        self.tinfo.rwsched_tasklet.CFRunLoopAddTimer(
            self.tinfo.rwsched_tasklet.CFRunLoopGetCurrent(),
            self.stop_timer,
            self.tinfo.rwsched_instance.CFRunLoopGetMainMode())

        self.tinfo.rwsched_tasklet.CFRunLoopAddTimer(
            self.tinfo.rwsched_tasklet.CFRunLoopGetCurrent(),
            self.done_check,
            self.tinfo.rwsched_instance.CFRunLoopGetMainMode())

        self.loop.scheduler_tick()
        self.tinfo.rwsched_instance.CFRunLoopRun()

        self.assertTrue(test_done())

    def test_call_later(self):
        """
        Verify that call_later will be triggered correctly
        """
        done = asyncio.Event(loop=self.loop)
        self.loop.call_later(0.1, done.set)
        self.run_until(done.is_set)

    def test_sleep(self):
        """
        Verify that asyncio.sleep() functions correctly
        """
        done = asyncio.Event(loop=self.loop)

        @asyncio.coroutine
        def test():
            yield from asyncio.sleep(0.1, loop=self.loop)
            done.set()

        asyncio.ensure_future(test(), loop=self.loop)
        self.run_until(done.is_set)

    def test_reader(self):
        """
        Verify that add_reader() works
        """
        done = asyncio.Event(loop=self.loop)
        msg = ''
        r, w = os.pipe()

        def thread_main():
            time.sleep(0.1)
            os.write(w, b'blah\n')

        def on_read():
            nonlocal msg

            msg = os.read(r, 100)
            done.set()

        th = threading.Thread(target=thread_main)
        self.loop.add_reader(r, on_read)

        th.start()
        self.run_until(done.is_set)

        th.join()
        os.close(r)
        os.close(w)

        self.assertEqual(msg, b'blah\n')

    def test_call_order(self):
        """
        Verify that functions passed to call_soon() are a FIFO
        """
        order = []
        done = asyncio.Event(loop=self.loop)

        for i in range(3):
            self.loop.call_soon(order.append, i)
        self.loop.call_soon(done.set)

        self.run_until(done.is_set)
        self.assertEqual(order, [0, 1, 2])


def main():
    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])
    unittest.main(testRunner=runner)

if __name__ == '__main__':
    main()

# vim: sw=4
