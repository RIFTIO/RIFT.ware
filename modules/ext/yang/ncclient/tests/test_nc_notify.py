#!/usr/bin/python3

import ncclient.manager
import lxml.etree as etree

import asyncio
import argparse
import logging
import xmlrunner
import os
import sys
import unittest
import ncclient.manager
import netconf_test_tool
import time
import queue

logger = logging.getLogger(__name__)

class SyncNetconfToolTestCase(netconf_test_tool.NetconfToolTestCase):
    def start_netconf_tool(self):
        @asyncio.coroutine
        def start():
            logger.debug("Starting netconf tool jar: %s", self._netconf_tool_jar)
            logger.debug("CMD: %s", self.test_tool_cmd)
            self._netconf_tool_proc = yield from asyncio.subprocess.create_subprocess_shell(
                    self.test_tool_cmd,
                    loop=self.loop,
                    )
            logger.debug("Started netconf tool jar (pid: %s)", self._netconf_tool_proc.pid)

            while True:
                yield from asyncio.sleep(1, loop=self.loop)

                try:
                    yield from self.loop.create_connection(
                            lambda: asyncio.Protocol(),
                            self._netconf_tool_host,
                            self.NETCONF_TOOL_PORT
                            )
                    break
                except OSError as e:
                    logger.warning("Connection failed: %s", str(e))

        self.loop.run_until_complete(
                asyncio.wait_for(start(), timeout=10, loop=self.loop),
                    )

    def stop_netconf_tool(self):
        @asyncio.coroutine
        def wait():
            self._netconf_tool_proc.terminate()
            yield from self._netconf_tool_proc.wait()

        if self._netconf_tool_proc is not None:
            self.loop.run_until_complete(
                asyncio.wait_for(wait(), timeout=4, loop=self.loop))
            self._netconf_tool_proc = None

    def connect_ssh(self):
        host = self._netconf_tool_host
        port = self.NETCONF_TOOL_PORT
        logger.debug("Connecting to netconf tool SSH (%s:%s)", host, port)
        self.manager = ncclient.manager.connect(
                          host=host,
                          port=port,
                          username="admin",
                          password="admin",
                          allow_agent=False,
                          look_for_keys=False,
                          hostkey_verify=False,
                          )

        self.assertTrue(self.manager.connected)
        logger.info("Connected to netconf tool.")

    def close_ssh(self):
        logger.debug("Attempting to close SSH session.")
        self.manager.close_session()
        self.manager = None

    def tearDown(self):
        super().tearDown()

    def setUp(self):
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)    
        super().setUp()

class TestNotify(SyncNetconfToolTestCase):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._queue = queue.Queue()

    def setUp(self):
        # Launch the Netconf test tool and wait for it accept connections
        # Then connect ncclient to the test tool    
        super().setUp()
        self.connect_ssh()

    def tearDown(self):
        # Disconnect ncclient from test tool
        # Terminate the Netconf test tool    
        self.close_ssh()
        super().tearDown()        

    def test_subscribe(self):
        # Precondition: NetconfTestTool running, ncclient connected to it
        # Postcondition: ncclient disconnected, NetconfTestTool terminated
        def notification_cbk(notification):
            notification_cbk.counter += 1
            logger.debug("notification %d %s", notification_cbk.counter, notification)
            self._queue.put(notification)
            if notification_cbk.counter == 8:
                self._queue.put(None)

        notification_cbk.counter = 0

        self.manager.register_notification_callback(notification_cbk)
        logger.debug("rpc call create-subscription")
        rsp = self.manager.create_subscription()
        self.assertTrue(rsp.ok)

        self.counter = 0
        while True:
            notif = self._queue.get(timeout=5)
            if notif is None:
                break
            logger.debug("Test receive event form queue: %s %s",
                        notif.event_time, notif.event_xml)
            self.counter += 1
            self._queue.task_done()

        self.assertEqual(self.counter, 8)



def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-n', '--netconf-host')
    parser.add_argument('--netconf-tool-jar')
    parser.add_argument('-j', '--jre')
    parser.add_argument('unittest_args', nargs='*')

    args = parser.parse_args(argv)
    if args.netconf_host is not None:
        netconf_test_tool.NetconfToolTestCase.NETCONF_TOOL_HOST = args.netconf_host
    if args.netconf_tool_jar is not None:
        netconf_test_tool.NetconfToolTestCase.NETCONF_TOOL_JAR = args.netconf_tool_jar
    if args.jre is not None:
        netconf_test_tool.NetconfToolTestCase.JRE_PATH = args.jre

    # Set the global logging level
    logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.ERROR)

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__] + args.unittest_args,
            testRunner=xmlrunner.XMLTestRunner(
                output=os.environ["RIFT_MODULE_TEST"]))

if __name__ == "__main__":
    main()

# vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4
