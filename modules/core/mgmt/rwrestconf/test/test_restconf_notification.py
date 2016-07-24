#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Balaji Rajappa
# Creation Date: 02/19/2016
# 

"""
The Test setup consists of a VM containing the RESTCONF tasklet and its
dependencies. The Test setup is launched as a subprocess and the tests are
executed from this script.

The following tests are performed
 - Testing the restconf-state to get the supported streams and its attributes
 - Testing both HTTP based event streaming and Websocket based event streaming
"""

import logging
import os
import sys
import asyncio
import argparse
import xmlrunner
import unittest
import shutil
import tornado.httpclient
import tornado.websocket
import tornado.gen
import tornado.ioloop
import json
import time

import rift.vcs.api
import rift.vcs.compiler
import rift.vcs.manifest
import rift.vcs.vms

logger = logging.getLogger(__name__)

class TestWare(object):
    class TestVM(rift.vcs.VirtualMachine):
        """
        A Basic VM containing the following tasklets.
            - DtsRouter
            - uAgent (confd will be launched by this)
            - RestconfTasklet
            - RiftCli (not really required, but to satisfy certain constraints
        """
        def __init__(self, *args, **kwargs):
            super(TestWare.TestVM, self).__init__(name="TestVM", *args, **kwargs)
            self.add_proc(rift.vcs.DtsRouterTasklet())
            self.add_tasklet(rift.vcs.uAgentTasklet())
            self.add_proc(rift.vcs.procs.RiftCli(netconf_username="admin",
                                            netconf_password="admin"));
            self.add_proc(rift.vcs.RestconfTasklet())
            
    def __init__(self):
        self.rift_install = os.environ['RIFT_INSTALL'] 
        self.manifest_path = os.path.join(self.rift_install, "rest_test_manifest.xml")
        self.rwmain_path = os.path.join(self.rift_install, "usr/bin/rwmain")
        self.loop = asyncio.get_event_loop()
        self.rwmain_proc = None
    
    def generate_manifest(self):
        test_vm = TestWare.TestVM(ip="127.0.0.1")
        test_vm.lead = True

        colony = rift.vcs.core.Colony(name='top', uid=1)
        colony.append(test_vm)

        sysinfo = rift.vcs.core.SystemInfo(
            mode='ethsim',
            collapsed=True,
            zookeeper=rift.vcs.manifest.RaZookeeper(zake=True, master_ip="127.0.0.1"),
            colonies=[colony],
            multi_broker=True,
            multi_dtsrouter=False,
        )
        
        # Compile the manifest
        compiler = rift.vcs.compiler.LegacyManifestCompiler()
        _, manifest = compiler.compile(sysinfo)

        vcs = rift.vcs.api.RaVcsApi()
        vcs.manifest_generate_xml(manifest, self.manifest_path)

    def start_testware(self):
        @asyncio.coroutine
        def start():
            logger.info("Starting rwmain")
            rwmain_cmd = self.rwmain_path + " -m " + self.manifest_path

            # Launch rwmain as a session leader, so that this script is not
            # killed when rwmain terminates (different process group)
            self.rwmain_proc = yield from asyncio.subprocess.create_subprocess_shell(
                                    rwmain_cmd, loop=self.loop,
                                    preexec_fn=os.setsid,
                               )
            # wait till confd is accepting connections
            while True:
                yield from asyncio.sleep(3, loop=self.loop)
                try:
                    yield from self.loop.create_connection(
                            lambda: asyncio.Protocol(),
                            "127.0.0.1",
                            "2022",
                            )
                    break
                except OSError as e:
                    logger.warning("Connection failed: %s", str(e))

        
        pwd = os.getcwd()
        os.chdir(self.rift_install)
        # ATTN: Are these really needed?
        os.system("rm -rf ./persist*")
        self.generate_manifest()
        os.environ['RIFT_NO_SUDO_REAPER'] = '1'
        self.loop.run_until_complete(
                    asyncio.wait_for(start(), timeout=180, loop=self.loop))
        logger.info("Testware started and running.")
        os.chdir(pwd)

    def stop_testware(self):
        @asyncio.coroutine
        def stop():
            try:
                self.rwmain_proc.terminate()
                yield from self.rwmain_proc.wait()
            except ProcessLookupError:
                pass

        if self.rwmain_proc is not None:
            self.loop.run_until_complete(stop())
            self.rwmain_proc = None
        logger.info("Testware terminated.")

class TestRestconf(unittest.TestCase):
    wait_forever = False
    testware = None
    @classmethod
    def setUpClass(cls):
        cls.testware = TestWare()
        cls.testware.start_testware()
        cls.httpclient = tornado.httpclient.HTTPClient()
        cls.http_headers = {
            "Accept" : "application/vnd.yang.data+json",
            "Content-Type" : "application/vnd.yang.data+json",
        }

    @classmethod
    def tearDownClass(cls):
        if cls.wait_forever:
            try:
                # This brings the rwmain to foreground which will enable 
                # CLI command execution and system inspection or perform manual
                # testing
                cls.testware.loop.run_forever()
            except KeyboardInterrupt:
                pass
        if cls.testware is not None:
            cls.testware.stop_testware()
        cls.testware = None
        os.system("stty sane")

    def test_get_streams(self):
        """Test to check the event streams supported by RESTCONF sever

        Precondition: A running Riftware with Restconf server and its
                      dependencies.
        """
        url = "http://localhost:8888/api/operational/restconf-state" +\
              "/streams"
        get_request = tornado.httpclient.HTTPRequest(
            url,
            headers=self.http_headers,
            method="GET",
            auth_username="admin",
            auth_password="admin",
        )
        get_result = self.httpclient.fetch(get_request)
        streams = json.loads(get_result.body.decode("utf-8"))

        logger.debug("GET streams: %s", streams)

        self.assertIn("ietf-restconf-monitoring:streams", streams)

        # Confd supports NETCONF and uagent_notification streams currently
        # NETCONF stream doesn't support replay feature
        streams_list = streams["ietf-restconf-monitoring:streams"]["stream"]
        self.assertEquals(streams_list[0]["name"], "NETCONF")
        self.assertEquals(streams_list[0]["replay-support"], "false")
        self.assertEquals(streams_list[1]["name"], "uagent_notification")
        self.assertEquals(streams_list[1]["replay-support"], "true")

        for stream in streams_list:
            name = stream["name"]
            access_list = stream["access"]
            self.assertEquals(len(access_list), 4)

    def test_get_stream_location(self):
        """Test that checks the location of a given stream

        Precondition: A running Riftware with Restconf server and its
                      dependencies.
        """
        url = "http://localhost:8888/api/operational/restconf-state" +\
              "/streams/stream/NETCONF/access/ws_json/location"
        get_request = tornado.httpclient.HTTPRequest(
            url,
            headers=self.http_headers,
            method="GET",
            auth_username="admin",
            auth_password="admin",
        )
        get_result = self.httpclient.fetch(get_request)
        location = json.loads(get_result.body.decode("utf-8"))
        logger.debug("Location = %s", location)

        self.assertIn("ietf-restconf-monitoring:location", location)
        self.assertRegexpMatches(location["ietf-restconf-monitoring:location"],
                    r'ws:\/\/[^:]*:8888\/ws_streams\/NETCONF-JSON')

    def test_websocket_notification(self):
        """Opens a websocket stream and tests if notifications are received

        Precondition: A running Riftware with Restconf server and its
                      dependencies.

        In this test a websocket connection is opened, which will create
        a notification subscription for NETCONF stream. A config change is
        performed by changing the log severity of uagent. This will trigger the
        Confd to send a notification event, which in turn will be relayed to the
        websocket stream via Restconf tasklet.
        """
        url = "ws://localhost:8888/ws_streams/NETCONF-JSON"
        ws_req = tornado.httpclient.HTTPRequest(
                    url = url,
                    auth_username = "admin",
                    auth_password = "admin",
                 )

        config_url = "http://localhost:8888/api/config/logging"
        config_json_body = """{"console":{"filter":{"category":[{"name" : "rw-mgmtagt-log", "severity" : "warning" }] } } }"""
        put_request = tornado.httpclient.HTTPRequest(
            url=config_url,
            headers=self.http_headers,
            method="PUT",
            body=config_json_body,
            auth_username="admin",
            auth_password="admin",
        )
        
        @tornado.gen.coroutine
        def websocket_client():
            websock =  yield tornado.websocket.websocket_connect(ws_req)
            logger.info("Websocket connected")
            # Websocket connected, now send config and then read the
            # config change notification 
            
            httpclient = tornado.httpclient.AsyncHTTPClient()
            yield tornado.gen.sleep(0.5) # This is required for serializing 

            put_result = yield httpclient.fetch(put_request)

            while True:
                stream_msg = yield websock.read_message()
                logger.debug("Websocket message: %s", stream_msg)
                event = json.loads(stream_msg)
                self.assertIn("notification", event)
                # Wait until a config-change notification is received
                if "ietf-netconf-notifications:netconf-config-change" in event["notification"]:
                    break
            websock.close()
        
        tornado.ioloop.IOLoop.current().run_sync(websocket_client, timeout=10)

    def test_http_notification(self):
        """Opens a HTTP notification stream and test if events arrive in that.

        Precondition: A running Riftware with Restconf server and its
                      dependencies.

        In this test a HTTP streaming connection is opened, which will create
        a notification subscription for NETCONF stream. A config change is
        performed by changing the log severity of uagent. This will trigger the
        Confd to send a notification event, which in turn will be relayed to the
        HTTP stream via Restconf tasklet.
        """
        http_headers = {
            "Content-Type" : "text/eventstream",
        }
        url = "http://localhost:8888/streams/NETCONF-JSON"

        config_url = "http://localhost:8888/api/config/logging"
        config_json_body = """{"console":{"filter":{"category":[{"name" : "rw-mgmtagt-log", "severity" : "critical" }] } } }"""
        put_request = tornado.httpclient.HTTPRequest(
            url=config_url,
            headers=http_headers,
            method="PUT",
            body=config_json_body,
            auth_username="admin",
            auth_password="admin",
        )
        
        @tornado.gen.coroutine
        def http_stream_client():
            streamclient = tornado.httpclient.AsyncHTTPClient()
            def stream_cbk(chunk):
                logger.debug("CHUNK: %s", chunk)
                chunk = chunk.decode("utf-8").strip()
                self.assertTrue(chunk.startswith("data:"))
                notif = json.loads(chunk[5:])
                self.assertIn("notification", notif)
                if "ietf-netconf-notifications:netconf-config-change" in notif["notification"]:
                    stream_cbk.done = True

            stream_cbk.done = False
            req = tornado.httpclient.HTTPRequest(
                    url = url,
                    auth_username = "admin",
                    auth_password = "admin",
                    streaming_callback = stream_cbk
                  )
            streamclient.fetch(req)
            logger.info("Stream connected")
            # Websocket connected, now send config and then read the
            # config change notification 
            
            httpclient = tornado.httpclient.AsyncHTTPClient()
            yield tornado.gen.sleep(0.5) # This is required for serializing 

            put_result = yield httpclient.fetch(put_request)

            # Wait till a config change event is received
            while not stream_cbk.done:
                yield tornado.gen.sleep(0.5)

        tornado.ioloop.IOLoop.current().run_sync(http_stream_client, timeout=10)


def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-w', '--wait', action='store_true',
                help="Wait after the tests are done, to inspect the system.")
    parser.add_argument('unittest_args', nargs='*')

    args = parser.parse_args(argv)

    if args.wait: TestRestconf.wait_forever = True

    # Set the global logging level
    logging.getLogger(__name__).setLevel(logging.DEBUG if args.verbose else logging.ERROR)

    try:
        # The unittest framework requires a program name, so use the name of this
        # file instead (we do not want to have to pass a fake program name to main
        # when this is called from the interpreter).
        unittest.main(argv=[__file__] + args.unittest_args,
            testRunner=xmlrunner.XMLTestRunner(
                output=os.environ["RIFT_MODULE_TEST"]))
    except Exception as exp:
        print("Exception thrown", exp)
        if TestRestconf.testware is not None:
            os.system("stty sane")
            TestRestconf.testware.stop_testware()
        
if __name__ == "__main__":
    main()

# vim: ts=4 sw=4 et
