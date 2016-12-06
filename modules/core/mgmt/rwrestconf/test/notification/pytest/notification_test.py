#!/usr/bin/env python3

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
# Author(s): Balaji Rajappa, Varun Prasad
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

import json
import logging
import os
import re

import pytest
import tornado.httpclient
import tornado.websocket
import tornado.gen
import tornado.ioloop

from rift.rwlib.util import certs
import gi
gi.require_version('IetfRestconfMonitoringYang', '1.0')
gi.require_version('RwlogMgmtYang', '1.0')

from gi.repository import IetfRestconfMonitoringYang, RwlogMgmtYang
import rift.auto.session
import rift.test.restconf_system


logger = logging.getLogger(__name__)


@pytest.fixture(scope='module', autouse=True)
def system():
    system = rift.test.restconf_system.TestSystem()
    return system


@pytest.fixture(scope='module')
def mgmt_session():
    mgmt_session = rift.auto.session.NetconfSession(host="127.0.0.1")
    mgmt_session.connect()
    return mgmt_session


@pytest.fixture(scope="module")
def proxy(mgmt_session):
    return mgmt_session.proxy


@pytest.mark.incremental
class TestNotifications:
    def test_start_system(self, system):
        system.start_testware()

    def test_get_streams(self, proxy):
        """Test to check the event streams supported by RESTCONF server.

        Precondition: A running Riftware with Restconf server and its
                      dependencies.
        """

        xpath = "/restconf-state/streams"
        streams = proxy(IetfRestconfMonitoringYang).get(xpath)
        for stream in streams.stream:
            assert len(stream.access) == 4

        # Confd supports NETCONF and uagent_notification streams currently
        # NETCONF stream doesn't support replay feature
        xpath = "/restconf-state/streams/stream[name='NETCONF']"
        netconf_stream = proxy(IetfRestconfMonitoringYang).get(xpath)

        assert netconf_stream is not None
        assert netconf_stream.replay_support is False

        xpath = "/restconf-state/streams/stream[name='uagent_notification']"
        uagent_stream = proxy(IetfRestconfMonitoringYang).get(xpath)

        assert uagent_stream is not None
        assert uagent_stream.replay_support is True

    def test_get_stream_location(self, proxy):
        """Test that checks the location of a given stream

        Precondition: A running Riftware with Restconf server and its
                      dependencies.
        """
        xpath = "/restconf-state/streams/stream[name='NETCONF']/access[encoding='ws_json']"
        netconf_access = proxy(IetfRestconfMonitoringYang).get(xpath)

        use_ssl, _, _ = certs.get_bootstrap_cert_and_key()
        if use_ssl:
            pattern = r'wss:\/\/[^:]*:8888\/ws_streams\/NETCONF-JSON'
        else:
            pattern = r'ws:\/\/[^:]*:8888\/ws_streams\/NETCONF-JSON'

        assert re.match(pattern, netconf_access.location) is not None

    def test_websocket_notification(self, proxy):
        """Opens a websocket stream and tests if notifications are received

        Precondition: A running Riftware with Restconf server and its
                      dependencies.

        In this test a websocket connection is opened, which will create
        a notification subscription for NETCONF stream. A config change is
        performed by changing the log severity of uagent. This will trigger the
        Confd to send a notification event, which in turn will be relayed to the
        websocket stream via Restconf tasklet.
        """
        use_ssl, cert, key = certs.get_bootstrap_cert_and_key()
        if use_ssl:
            prefix = "wss"
            ssl_settings = {
                "validate_cert" : False,
                "client_key" : key,
                "client_cert" : cert,
                }
        else:
            prefix = "ws"
            ssl_settings = {}

        url = "%s://localhost:8888/ws_streams/NETCONF-JSON" % prefix
        ws_req = tornado.httpclient.HTTPRequest(
                    url=url,
                    auth_username="admin",
                    auth_password="admin",
                    **ssl_settings
                 )

        logging = RwlogMgmtYang.Logging_Console.from_dict({"filter": {
                        "category": [{"name": "rw-mgmtagt-log",
                                      "severity": "warning"}]
                 }})

        @tornado.gen.coroutine
        def websocket_client():
            websock = yield tornado.websocket.websocket_connect(ws_req)
            logger.info("Websocket connected")
            # Websocket connected, now send config and then read the
            # config change notification 

            yield tornado.gen.sleep(0.5)  # This is required for serializing 

            proxy(RwlogMgmtYang).merge_config("/logging/console", logging)

            while True:
                stream_msg = yield websock.read_message()
                print (stream_msg)
                logger.debug("Websocket message: %s", stream_msg)
                event = json.loads(stream_msg)
                assert "notification" in event

                # Wait until a config-change notification is received
                if "ietf-netconf-notifications:netconf-config-change" in event["notification"]:
                    break

            websock.close()

        tornado.ioloop.IOLoop.current().run_sync(websocket_client, timeout=10)


    def test_http_notification(self, proxy):
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

        use_ssl, cert, key = certs.get_bootstrap_cert_and_key()
        if use_ssl:
            prefix = "https"
            ssl_settings = {
                "validate_cert" : False,
                "client_key" : key,
                "client_cert" : cert,
                }
        else:
            prefix = "http"
            ssl_settings = {}

        url = "%s://localhost:8888/streams/NETCONF-JSON" % prefix

        logging = RwlogMgmtYang.Logging_Console.from_dict({"filter": {
                        "category": [{"name": "rw-mgmtagt-log",
                                      "severity": "error"}]
                 }})

        @tornado.gen.coroutine
        def http_stream_client():
            streamclient = tornado.httpclient.AsyncHTTPClient()

            def stream_cbk(chunk):
                logger.debug("CHUNK: %s", chunk)
                chunk = chunk.decode("utf-8").strip()
                assert chunk.startswith("data:") is True

                notif = json.loads(chunk[5:])
                assert "notification" in notif

                if "ietf-netconf-notifications:netconf-config-change" in notif["notification"]:
                    stream_cbk.done = True

            stream_cbk.done = False
            req = tornado.httpclient.HTTPRequest(
                    url=url,
                    auth_username="admin",
                    auth_password="admin",
                    streaming_callback=stream_cbk,
                    **ssl_settings
                  )

            streamclient.fetch(req)
            logger.info("Stream connected")
            # Websocket connected, now send config and then read the
            # config change notification 

            yield tornado.gen.sleep(0.5)  # This is required for serializing 

            proxy(RwlogMgmtYang).merge_config("/logging/console", logging)

            # Wait till a config change event is received
            while not stream_cbk.done:
                yield tornado.gen.sleep(0.5)

        tornado.ioloop.IOLoop.current().run_sync(http_stream_client, timeout=10)

    def test_stop_system(self, system, wait_for_exit):
        if wait_for_exit:
            try:
                # This brings the rwmain to foreground which will enable 
                # CLI command execution and system inspection or perform manual
                # testing
                system.loop.run_forever()
            except KeyboardInterrupt:
                pass
        system.stop_testware()
        os.system("stty sane")
