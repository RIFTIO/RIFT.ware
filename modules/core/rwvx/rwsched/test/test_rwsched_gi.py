#!/usr/bin/env python3
"""
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# @file rwlog_pytest.py
# @author Anil Gunturu (anil.gunturu@riftio.com)
# @date 11/02/2014
#
"""
import argparse
import io
import gi
import logging
import os
import signal
import sys
import unittest
import xmlrunner
import threading
import time

import rwlogger

import gi
gi.require_version('RwSched', '1.0')
gi.require_version('CF', '1.0')

from gi.repository import RwSched, CF

class Callback:
    def rwsched_TIMER_callback(self, timer, info):
        print ("Inside Callback::rwsched_TIMER_callback")
        print (self, timer, info)
        TestRwSched.TIMER_calledback = True

    def rwsched_IO_callback(self, s, cb_type, address, data, info):
        print ("Inside Callback::rwsched_IO_callback")
        print (self, s, cb_type, address, data, info)
        fd = int(info)
        print ("Read 8 chars from fd(%d) - %s\n" %(fd, os.read(fd, 8)))  # read 8 chars and print
        TestRwSched.IO_calledback = True

    def rwsched_SIGNAL_callback(self, a, b):
        print ("Inside Callback::rwsched_SIGNAL_callback")
        print ("self=", self, "a=", a, "b=", b)
        print ("\n")

class TestRwSched(unittest.TestCase):
    def setUp(self):
        TIMER_calledback = False
        IO_calledback = False
        pass

    def test_rwsched_basic(self):
        instance = RwSched.Instance()
        self.assertIsInstance(instance, gi.repository.RwSched.Instance)
        tasklet = RwSched.Tasklet.new(instance)
        self.assertIsInstance(tasklet, gi.repository.RwSched.Tasklet)

        runloop = tasklet.CFRunLoopGetCurrent()
        self.assertIsInstance(runloop, gi.repository.RwSched.CFRunLoop)

        interval = 2.0

        foo = Callback()
        timer = tasklet.CFRunLoopTimer(CF.CFAbsoluteTimeGetCurrent() + interval, 
                                       interval,
                                       foo.rwsched_TIMER_callback, 
                                       instance)
        tasklet.CFRunLoopAddTimer(tasklet.CFRunLoopGetCurrent(), 
                                  timer, 
                                  instance.CFRunLoopGetMainMode())
        pipe_fds = os.pipe()
        print ("Created pipe with fd[0]=%d fd[1]=%d\n" % pipe_fds)
        os.write(pipe_fds[1], bytes("Hello GObject Introspection", 'UTF-8'))

        cf_callback_flags = 1   #kCFSocketReadCallBack
        cf_option_flags = 1     #kCFSocketAutomaticallyReenableReadCallBack

        socket = tasklet.CFSocketBindToNative(pipe_fds[0],
                                              cf_callback_flags,
                                              foo.rwsched_IO_callback,
                                              pipe_fds[0])
        self.assertIsInstance(socket, gi.repository.RwSched.CFSocket)

        tasklet.CFSocketSetSocketFlags(socket, cf_option_flags)

        source = tasklet.CFSocketCreateRunLoopSource(instance.CFAllocatorGetDefault(), socket, 0)

        tasklet.CFRunLoopAddSource(runloop, source, instance.CFRunLoopGetMainMode())

        #tasklet.signal(signal.SIGUSER1, foo.rwsched_SIGNAL_callback)
        tasklet.signal(signal.SIGALRM, foo.rwsched_SIGNAL_callback)

        signal.alarm(1)

        #instance.CFRunLoopRun()
        instance.CFRunLoopRunInMode(instance.CFRunLoopGetMainMode(),
                                    5.0, # How many seconds
                                    0)
        print (self.TIMER_calledback, self.IO_calledback)
        self.assertTrue(self.TIMER_calledback == True)
        self.assertTrue(self.IO_calledback == True)

    @unittest.skipUnless("FORCE" in os.environ, "Requires FORCE envvar")
    def test_signal_function_cleanup(self):
        instance = RwSched.Instance()
        self.assertIsInstance(instance, gi.repository.RwSched.Instance)
        tasklet = RwSched.Tasklet.new(instance)
        self.assertIsInstance(tasklet, gi.repository.RwSched.Tasklet)

        runloop = tasklet.CFRunLoopGetCurrent()
        self.assertIsInstance(runloop, gi.repository.RwSched.CFRunLoop)

        start_time = time.time()
        while (time.time() - start_time) < 5:
            event = threading.Event()

            def test_signal(*args, **kwargs):
                #print("Got signal callback (*args=%s, **kwargs=%s)" % (args, kwargs))
                event.set()

            tasklet.signal(signal.SIGUSR1, test_signal)
            #signal.signal(signal.SIGUSR1, test_signal)
            os.kill(0, signal.SIGUSR1)
            a = event.wait(1)
            assert a


def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')

    args, _ = parser.parse_known_args(argv)

    # Set the global logging level
    logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.ERROR)

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__] + argv,
            testRunner=xmlrunner.XMLTestRunner(
                output=os.environ["RIFT_MODULE_TEST"]))

if __name__ == '__main__':
    main()
