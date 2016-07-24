#!/usr/bin/env python

import argparse
import lxml.etree as etree
import os
import subprocess
import shlex
import socket
import sys
import logging
import time
import unittest
import xmlrunner

import ncclient.manager
import netconf_test_tool

logger = logging.getLogger("manager_test")

class NetconfToolConnectionError(Exception):
    pass


class SyncNetconfToolTestCase(netconf_test_tool.NetconfToolTestCase):
    def __init__(self, *args, **kwargs):
        super(SyncNetconfToolTestCase, self).__init__(*args, **kwargs)

        self.manager = None

    def start_netconf_tool(self):
        logger.debug("Starting netconf tool jar: %s", self._netconf_tool_jar)
        self._netconf_tool_proc = subprocess.Popen(
                shlex.split(self.test_tool_cmd),
                )
        logger.debug("Started netconf tool jar (pid: %s)", self._netconf_tool_proc.pid)

        start_time = time.time()
        while True:
            time.sleep(1)

            try:
                socket.create_connection(
                        (self._netconf_tool_host, self.NETCONF_TOOL_PORT),
                        timeout=1,
                        )
                break
            except socket.error as e:
                logger.warning("Connection failed: %s", str(e))

            if (time.time() - start_time) > 10:
                raise NetconfToolConnectionError("Took over 10 seconds")

    def stop_netconf_tool(self):
        self._netconf_tool_proc.terminate()
        self._netconf_tool_proc.wait()
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

    def close_ssh(self):
        logger.debug("Attempting to close SSH session.")
        self.manager.close_session()
        self.manager = None


class TestConnect(SyncNetconfToolTestCase):
    def test_single_connect(self):
        self.connect_ssh()
        self.close_ssh()

    def test_many_connect(self):
        for i in range(3):
            self.connect_ssh()
            self.close_ssh()


class TestBasic(SyncNetconfToolTestCase):
    def setUp(self):
        super(TestBasic, self).setUp()
        self.connect_ssh()

    def tearDown(self):
        self.close_ssh()
        super(TestBasic, self).tearDown()

    def test_merge_config(self):
        def merge_config():
            return self.manager.edit_config(
                    config=(
                    '<config xmlns:xc="urn:ietf:params:xml:ns:netconf:base:1.0">'
                       '<cont><l>THIS IS CONTENT</l></cont>'
                    '</config>'
                    ),
                    target="running",
                    default_operation="merge",
                    )

        def get_config():
            return self.manager.get_config(
                    source="running",
                    filter=('xpath', 'opendaylight-inventory:nodes/node/17830-sim-device/yang-ext:mount/')
                    )


        merge_reply = merge_config()
        logger.debug("Got merge config reply: %s", merge_reply)

        get_config_reply = get_config()
        logger.debug("Got get_config reply: %s", get_config_reply)

        root = etree.fromstring(str(get_config_reply).encode('utf8'))
        first = root.find(".//l")
        self.assertEquals("THIS IS CONTENT", first.text.strip())

def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-n', '--netconf-host')
    parser.add_argument('--netconf-tool-jar')
    parser.add_argument('unittest_args', nargs='*')

    args = parser.parse_args(argv)
    if args.netconf_host is not None:
        SyncNetconfToolTestCase.NETCONF_TOOL_HOST = args.netconf_host
    if args.netconf_tool_jar is not None:
        SyncNetconfToolTestCase.NETCONF_TOOL_JAR = args.netconf_tool_jar

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
