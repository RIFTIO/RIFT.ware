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
Implements the RESTCONF-state as specified in the ietf-restconf-monitoring YANG
module (defined in IETF draft-ietf-netconf-restconf-09). This state is used to
view the list of notification streams supported and the means to access these
streams.

A subset of the ietf-restconf-monitoring YANG definitions are defined in the
rw-restconf.yang. This will enable the operational data to be viewed from all
Netconf based north-bound (CLI, netconf client etc...)

The 'capabilities' state mentioned in the YANG is not implemented. Only 
'streams' state is implemented.

Every stream can be accessed using the following methods
 - Websocket Access, streaming using XML messages
    [ws://host:port/ws_streams/STREAM-NAME]
 - Websocket Access, streaming using JSON messages
    [ws://host:port/ws_streams/STREAM-NAME-JSON]
 - HTTP Eventsource Access, streaming using XML data
    [http://host:port/streams/STREAM-NAME]
 - HTTP Eventsource Access, streaming using JSON data
    [http://host:port/streams/STREAM-NAME-JSON]

To get the location for a particular protocol and content, use the following
query.
  /api/operational/restconf-state/streams/stream/{stream-name}/access/{encoding}/location
Here encoding can be one of [xml, json, ws_xml, ws_json]

References:
  1. IETF draft-ietf-netconf-restconf-09 [RESTCONF Protocol]
  2. rw-restconf.yang
"""

import asyncio
import logging
import sys
from lxml import etree
from .netconf_wrapper import NetconfWrapper
from .netconf_wrapper import Result

import gi
gi.require_version('IetfRestconfMonitoringYang', '1.0')
from gi.repository import IetfRestconfMonitoringYang

NC_NETMOD_NS = "urn:ietf:params:xml:ns:netmod:notification"
etree.register_namespace('netmod', NC_NETMOD_NS)

NOTIF_NS = {'netmod' : NC_NETMOD_NS}

class StateProvider(object):
    """Provides the RESTCONF state.

    The list of streams supported is not available with the RESTCONF. It is
    queried from the Netconf server using a standard netconf-streams query. On
    top of it, the access methods and the location is added.
    """
    URL_TEMPLATE = "{scheme}://{host}/{proto_prefix}streams/{stream}{en_suffix}"
    def __init__(
            self,
            logger,
            loop,
            netconf_host,
            netconf_port,
            webhost,
            use_https):
        """Construct the RESTCONF state provider.

        Arguments:
            logger       - logger used for debug trace logging
            loop         - asyncio event loop (Restconf Tasklet loop)
            netconf_host - Netconf server host-name/IP-address
            webhost      - Host:Port on which the RESTCONF webserver is listening
            use_https    - The web application is using secure transport
        """
        self._logger = logger
        self._loop   = loop
        self._netconf = None
        self._netconf_host = netconf_host
        self._netconf_port = netconf_port
        self._webhost = webhost
        self._ws_scheme = "wss" if use_https else "ws"
        self._http_scheme = "https" if use_https else "http"

    @asyncio.coroutine
    def get_state(self):
        """Get the stream states.

        Performs a Netconf query to get the Netconf notification streams and
        adds access method on top of it.
        """
        if self._netconf is None:
            # Always use the 'admin'. Access control not required for a Restconf
            # initiated query to Netconf
            self._netconf = NetconfWrapper(
                    self._logger, self._loop, self._netconf_host,
                    self._netconf_port, "admin", "admin")
            yield from self._netconf.connect()

        restconf_state = IetfRestconfMonitoringYang.RestconfState()
        data_ele = etree.Element("data")
        netconf_ele = etree.SubElement(data_ele, "{%s}netconf" % NC_NETMOD_NS)
        etree.SubElement(netconf_ele, "{%s}streams" % NC_NETMOD_NS)
        xml_str = etree.tostring(data_ele)

        result, resp = yield from self._netconf.get(url=None, xml=xml_str)
        if result != Result.OK:
            self._logger.error("Failed to get netconf/streams %s %s", result, resp)
            return restconf_state

        streams = restconf_state.create_streams()
        resp = etree.fromstring(resp)

        for stream in resp.xpath("//netmod:stream", namespaces=NOTIF_NS):
            pb_stream = streams.create_stream()

            st_name = stream.find("netmod:name", namespaces=NOTIF_NS)
            if st_name is not None: pb_stream.name = st_name.text

            st_descr = stream.find("netmod:description", namespaces=NOTIF_NS)
            if st_descr is not None: pb_stream.description = st_descr.text

            st_replay = stream.find("netmod:replaySupport", namespaces=NOTIF_NS)
            if st_replay is not None: 
                pb_stream.replay_support = True if st_replay.text == "true" else False

            st_log_time = stream.find("netmod:replayLogCreationTime", namespaces=NOTIF_NS)
            if st_log_time is not None: 
                pb_stream.replay_log_creation_time = st_log_time.text

            self.add_access(pb_stream, self._ws_scheme, "ws_xml")
            self.add_access(pb_stream, self._ws_scheme, "ws_json")
            self.add_access(pb_stream, self._http_scheme, "xml")
            self.add_access(pb_stream, self._http_scheme, "json")
            
            restconf_state.streams.stream.append(pb_stream)

        return restconf_state

    def add_access(self, stream, scheme, encoding):
        """Constructs the access URL and adds it to the Restconf Stream
        protobuf.

        Arguments:
            stream   - Protobuf representing the RestconfState Stream
            schema   - URL scheme to be used (cane be ws or http)
            encoding - Encoding to be used (can be xml, json, ws_xml, ws_json)
        """
        access = stream.create_access()
        en_suffix = "-JSON" if encoding.endswith("json") else ""
        proto_prefix = "ws_" if scheme.startswith("ws") else ""
        access.encoding = encoding
        access.location = self.URL_TEMPLATE.format(
                                scheme = scheme,
                                host   = self._webhost,
                                proto_prefix = proto_prefix,
                                stream = stream.name,
                                en_suffix = en_suffix)     
        stream.access.append(access)

# vim: ts=4 sw=4 et
