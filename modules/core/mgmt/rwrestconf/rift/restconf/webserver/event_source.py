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
# Author(s): Balaji Rajappa
# Creation Date: 02/11/2016
# 

"""
Restconf implementation of Netconf event streaming is based on the IETF draft 
draft-ietf-netconf-restconf-09. This draft only specifies plain HTTP based 
event streaming, which again is based on the W3C.CR-eventsource spec. 

RwRestConf supports two types of event sources.
 - Websocket based event source, which relays the Netconf notifications via
   websocket transport
 - HTTP based event source, which relays the Netconf notifications via HTTP
   transport following the W3C.CR-eventsource spec.

The tornado server adds two extra routes for WebSocket and HTTP stream handlers.

Every stream can be accessed using the following methods
 - Websocket Access, streaming using XML messages
    [ws://host:port/ws_streams/STREAM-NAME]
 - Websocket Access, streaming using JSON messages
    [ws://host:port/ws_streams/STREAM-NAME-JSON]
 - HTTP Eventsource Access, streaming using XML data
    [http://host:port/streams/STREAM-NAME]
 - HTTP Eventsource Access, streaming using JSON data
    [http://host:port/streams/STREAM-NAME-JSON]

References:
    1. IETF draft-ietf-netconf-restconf-09 [RESTCONF Protocol]
    2. IETF RFC 5277 [NETCONF Event Notifications]
    3. W3C.CR-eventsource [Server-Sent Events]
"""

import asyncio
import tornado.gen
import tornado.web
import tornado.websocket
import tornado.platform.asyncio
import tornado.httputil
import urllib.parse
from .netconf_wrapper import NetconfWrapper
from .netconf_wrapper import Result
from lxml import etree

import logging
import sys

from ..util import (
    split_stream_url,
    get_username_and_password_from_auth_header,
    naive_xml_to_json,
)


class NetconfStreamHandler(object):
    """Serves as a super class for both Websocket and Http stream handlers.

    Common Netconf operations are abstracted in this class. For each opened
    stream, a new Netconf session is started. When the stream is closed, the
    Netconf session is also closed. As per RFC 5277 (Netconf Notifications),
    there can be only one subscription per session and the subscription can be
    closed only when the session is closed/killed.
    """
    def init_stream_handler(
            self, 
            logger, 
            loop, 
            netconf_ip, 
            netconf_port,
            statistics,
            xml_to_json_translator):
        """Initializes the base stream handler.

        Arguments:
            logger       - logger to be used for logging debug messages
            loop         - Asyncio event loop
            netconf_ip   - Netconf server IP
            netconf_port - Netconf server port
            statistics   - Statistics object
            xml_to_json_translator - Schema based XML to Json converter
        """
        self._logger = logger
        self._asyncio_loop = loop
        self._encoding = "xml"
        self._netconf_ip = netconf_ip
        self._netconf_port = netconf_port
        self._stat = statistics
        self._xml_to_json_translator = xml_to_json_translator

    @asyncio.coroutine
    def netconf_subscribe(self, url, auth_header):
        """Opens a netconf session and does a create-subscription for the stream
        specified in the URL.

        Arguments:
            url - stream URL (@see split_stream_url() for URL format)
            auth_header - Basic Authentication-Header

        This is an asyncio coroutine.
        """
        if auth_header is None:
            # W3C API doesn't have a provision to mention auth header
            username, password = "admin", "admin"
        else:
            username, password = get_username_and_password_from_auth_header(auth_header)
        self._nc_handler = NetconfWrapper(
                                self._logger, self._asyncio_loop,
                                self._netconf_ip, self._netconf_port, 
                                username, password,
                           )
        try:
            yield from self._nc_handler.connect()
            self._nc_handler.set_notification_callback(self.notification_cbk)
        except:
            self._logger.error("Couldn't connect to Netconf server")
            self.on_cleanup()
            return

        stream_name, encoding, filt, start_time, stop_time = split_stream_url(url)
        self._encoding = encoding

        filter = ('xpath', filt) if filt is not None else None

        result, resp = yield from self._nc_handler.create_subscription(
                            stream = stream_name,
                            filter = filter,
                            start_time = start_time,
                            stop_time = stop_time)
        if result != Result.OK:
            self._logger.error("Create netconf subscription failed")
            yield self._close_netconf()
            self.on_cleanup()
            return

        self._logger.debug("Create-Subscription for stream %s success",
                            stream_name)

    def notification_cbk(self, notification):
        """Callback that will be invoked when a Netconf notification is received
        on the session.

        Arguments:
            notification - Netconf notification object

        Internally invokes StreamHandler specific message handler.
        """
        self._logger.info("Received netconf notification %s", notification)
        if self._encoding == "json":
            # Convert XML to Json
            xml_str =  bytes(repr(notification), 'utf-8')
            resp_str = self._xml_to_json_translator.convert_notification(xml_str)
            if resp_str is None:
                # Use a schema-less conversion
                resp_str = naive_xml_to_json(xml_str)
        else:
            resp_str = repr(notification)
        self._logger.debug("Translated Notification: %s", resp_str)
        self.on_netconf_message(resp_str)

    @tornado.gen.coroutine
    def _close_netconf(self):
        """Closes the Netconf session.

        This is a tornado coroutine. Conversion of asyncio to tornado Future is
        required.
        """
        async_future = asyncio.async(self._nc_handler.close(),
                                     loop=self._asyncio_loop)
        yield tornado.platform.asyncio.to_tornado_future(async_future)

    def on_cleanup(self):
        """Invoked when a cleanup needs to be performed.

        Should be overriden by the subclasses.
        """
        raise NotImplementedError

    def on_netconf_message(self, stream_message):
        """Invoked when a Netconf stream message is received.

        StreamHandler specific implementation.
        """
        raise NotImplementedError


class WebSocketStreamHandler(
            NetconfStreamHandler,
            tornado.websocket.WebSocketHandler):
    """Tornado RequestHandler specialized to handle Websocket connection.

    Handles the route '/ws_streams'. When a stream is opened, a Netconf session
    is also opened. Notifications arriving on the Netconf session is relayed to
    the websocket stream. 
    
    Supports both XML and JSON content-types. The Content-Type is chosen based 
    on the stream name. A stream with "-JSON" suffix will have json 
    Content-Type and others will be xml Content-Type.

    JSON Event Format: 
        { 
        "notification": { 
            "eventTime" : "2016-02-24T23:11:21.81517-00:00",
            "...." : {...}
        }

    XML Event Format:
        <notification>
            <eventTime>2016-02-24T23:11:21.81517-00:00</eventTime>
            ... ...
        </notification>
    
    The notification content format is based on the YANG 'notification'
    definition.
    """
    def initialize( self, logger, loop, netconf_ip, netconf_port, statistics,
            xml_to_json_translator):
        """Initializes the WebSocket Request handler.

        Arguments:
            logger       - logger to be used for logging debug messages
            loop         - Asyncio event loop
            netconf_ip   - Netconf server IP
            netconf_port - Netconf server port
            statistics   - Statistics object
            xml_to_json_translator - Schema based XML to Json converter

        Invoked by the Tornado request handler when a request for /ws_streams
        is received.
        """
        self.init_stream_handler(logger, loop, 
                    netconf_ip, netconf_port, statistics, xml_to_json_translator)

    @tornado.gen.coroutine
    def open(self, *args, **kwargs):
        """Invoked when a Websocket streams is opened by a client.

        Arguments: (not used)

        Creates a Netconf session and a subscription for the received stream.
        This is a tornado coroutine.
        """
        self._open = True
        self._stat.websocket_stream_open += 1
        # Create subscription for the stream
        url = self.request.uri
        self._logger.info("Websocket connection %s %s", url, self)

        async_future = asyncio.async(
                            self.netconf_subscribe(
                                self.request.uri,
                                self.request.headers.get("Authorization")), 
                            loop=self._asyncio_loop)
        yield tornado.platform.asyncio.to_tornado_future(async_future)

    @tornado.gen.coroutine
    def on_message(self, message):
        """Since this Websocekt is a one-way stream, not expecting any message
        from the client.
        """
        # Not expecting any message
        if message is None:
            yield self._close_netconf()

    @tornado.gen.coroutine
    def on_close(self):
        """Invoked when the Websocket connection is closed by the client.
        """
        self._open = False
        self._stat.websocket_stream_close += 1
        self._logger.info("Websocket %s closed", self)
        yield self._close_netconf()

    @tornado.gen.coroutine
    def check_origin(self, origin):
        """Allows all cross-origin traffic.

        This method can be used to restrict traffic from specific origins.
        """
        return True
        
    def on_cleanup(self):
        """Invoked from the super class to perform Handler specific cleanup
        """
        self.close()

    def on_netconf_message(self, stream_message):
        """Invoked when a Netconf stream message is received.

        Arguments:
            stream_message - Json or Xml notification message

        Received message is written on to the Websocket stream.
        """
        if not self._open:
            # Ignore notifications once the websocket is closed. Notifications
            # still might come while the Netconf session is closing.
            return

        try:
            self.write_message(stream_message)
            self._stat.websocket_events += 1
        except tornado.websocket.WebSocketClosedError:
            self._logger.error("Websocket has closed. Close the Netconf session")
            asyncio.async(self._nc_handler.close(), loop=self._asyncio_loop)

class HttpStreamHandler(
            NetconfStreamHandler,
            tornado.websocket.WebSocketHandler):
    """Tornado RequestHandler specialized to implement HTTP EventSource.

    Handles the route '/streams'. When a stream is opened, a Netconf session
    is also opened. Notifications arriving on the Netconf session is relayed to
    the HTTP event stream. 
    
    Supports both XML and JSON content-types. The Content-Type is chosen based 
    on the stream name. A stream with "-JSON" suffix will have json 
    Content-Type and others will be xml Content-Type.

    JSON Event Format: 
        data: { 
        data: "notification": { 
        data:    "eventTime" : "2016-02-24T23:11:21.81517-00:00",
        data:    "...." : {...}
        data: }
    As per W3C.CR-EventSource spec, 'data' tag appears for each line. If the
    json is collapsed to a single line, then only one 'data' tag will be
    present.

    XML Event Format:
        data: <notification>
        data:    <eventTime>2016-02-24T23:11:21.81517-00:00</eventTime>
        data:    ... ...
        data: </notification>
    
    The notification content format is based on the YANG 'notification'
    definition.
    """
    def initialize(self, logger, loop, netconf_ip, netconf_port, statistics,
                xml_to_json_translator):
        """Initializes the HTTP Request handler.

        Arguments:
            logger       - logger to be used for logging debug messages
            loop         - Asyncio event loop
            netconf_ip   - Netconf server IP
            netconf_port - Netconf server port
            statistics   - Statistics object
            xml_to_json_translator - Schema based XML to Json converter

        Invoked by the Tornado request handler when a request for /streams
        is received.
        """
        self.init_stream_handler(logger, loop, 
                    netconf_ip, netconf_port, statistics, xml_to_json_translator)

    def set_default_headers(self):
        """Overridden method to set default response headers.
        """
        default_headers = {
            "Content-Type": "text/event-stream",
            "access-control-allow-origin": "*",
            "connection": "keep-alive",
        }
        self._headers = tornado.httputil.HTTPHeaders(default_headers)

    @tornado.web.asynchronous
    def get(self, *args, **kwargs):
        """GET request handler. Indicates opening of the event stream.

        A new Netconf session is started and subscribe for events on the given
        stream.
        """
        self._stat.http_stream_open += 1
        # Create subscription for the stream
        url = self.request.uri
        self._logger.info("HTTP Stream connection %s %s", url, self)

        auth_header = self.request.headers.get("Authorization")
        if auth_header is None:
            self.finish()
            raise ValueError("no authorization header present")

        async_future = asyncio.async(
                            self.netconf_subscribe(
                                self.request.uri,
                                auth_header), 
                            loop=self._asyncio_loop)

    def on_connection_close(self):
        """Invoked when the HTTP connection is closed.
        """
        self._stat.http_stream_close += 1
        self._logger.info("HTTP EventStream %s closed", self)
        self.finish()
        asyncio.async(self._nc_handler.close(), loop=self._asyncio_loop)

    def on_cleanup(self):
        """Invoked by the super class to perform handler specific cleanup.
        """
        self.close()

    def on_netconf_message(self, stream_message):
        """Invoked when a netconf notification message is received.

        Arguments:
            stream_message - Json/Xml notification message
        """
        self._logger.info("HttpStream: Received netconf message")
        out_str = ""
        for msg in stream_message.splitlines():
            out_str += "\ndata: " + msg
        out_str += "\n\n"
        self._logger.debug("HttpStream: %s", out_str)
        self.write(out_str)
        self.flush(callback=self.chunk_written)
        self._stat.http_events += 1

    def chunk_written(self):
        """Dummy callback to avoid self.flush() returning a future
        """
        pass

# vim: ts=4 sw=4 et
