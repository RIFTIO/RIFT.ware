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
# Author(s): Max Beckett
# Creation Date: 7/10/2015
# 

import asyncio
import functools
import lxml.etree 
import time
import traceback
import sys

import tornado.gen
import tornado.httpserver
import tornado.ioloop
import tornado.platform.asyncio
import tornado.web
import tornado.websocket
import urllib

from ..translation import (
    convert_rpc_to_json_output,
    convert_rpc_to_xml_output,
    convert_xml_to_collection,
    convert_get_request_to_xml,
)
from ..util import (
    create_xpath_from_url,
    create_dts_xpath_from_url,    
    is_config,
    is_schema_api,
    find_child_by_name,
    find_target_node,
    Result,
    PayloadType,
    determine_payload_type,
    map_error_to_http_code,
    format_error_message,
    naive_xml_to_json,    
    NetconfOperation,
    map_request_to_netconf_operation,
    get_json_schema,
)

from .connection_manager import AuthenticationError

class RestError(Exception):
    def __init__(self, status, message):
        super(RestError, self).__init__(message)
        self.status = status
        self.message = message
        self._base_error_message = "<error>%s</error>"    

    def get_message_xml(self):
        if self.message.startswith("<"):
            return self.message
        else:    
            return self._base_error_message % self.message

def exception_wrapper(method):
    @functools.wraps(method)
    @asyncio.coroutine
    def _impl(self):
        try:
            status, content_type, payload = yield from method(self)
            return status, content_type, payload
        except RestError as e:
            error_payload = format_error_message(self._accept_type, e.get_message_xml())
            return e.status, self._accept_type.value, error_payload
        except Exception as e:
            raise e
    
    return _impl

class HttpHandler(tornado.web.RequestHandler):
    def initialize(
            self,
            asyncio_loop,
            confd_url_converter,
            configuration,
            connection_manager,
            logger,
            schema_root,
            statistics,
            xml_to_json_translator):

        self._asyncio_loop = asyncio_loop
        self._confd_url_converter = confd_url_converter
        self._configuration = configuration
        self._connection_manager = connection_manager
        # ATTN: can't call it _log becuase the base class has a _log instance already
        self._logger = logger
        self._schema_root = schema_root
        self._statistics = statistics
        self._xml_to_json_translator = xml_to_json_translator

        self._default_accept_type = PayloadType.XML

    def _handle_request_exception(self, exception):
        # log uncaught exceptions
        if not isinstance(exception, tornado.web.HTTPError):
            exc_type, exc_value, exc_traceback = sys.exc_info()
            stack_trace = traceback.format_exception(exc_type, exc_value, exc_traceback)

            self._logger.error(repr(stack_trace))
            raise RestError(500,repr(stack_trace))

    def _get_encoded_username_and_password(self):
        auth_header = self.request.headers.get("Authorization")        
        if auth_header is None:
            self._statistics.req_401_rsp += 1
            self._logger.error("Authentication failure. Authorization header missing.")
            raise RestError(401, "no authorization header present")
        if not auth_header.startswith("Basic "):
            self._statistics.req_401_rsp += 1
            self._logger.error("Authentication failure. Only basic authorization is supported.")
            raise RestError(401, "only supoprt for basic authorization")

        return auth_header[6:]

    @asyncio.coroutine
    def connect(self):
        ''' must be called from inside a coroutine impl '''
        self._netconf = yield from self._connection_manager.get_connection(self._get_encoded_username_and_password())

    @asyncio.coroutine
    def _handle_upgrade(self):
        ''' must be called from inside a coroutine impl '''
        yield from self._connection_manager.reconnect(self._get_encoded_username_and_password())

    def prepare(self):
        # setup the state needed to do the netconf generation
        self._body = self.request.body.decode("utf-8")
        self._body_header = self.request.headers.get("Content-Type")
        self._netconf_operation = map_request_to_netconf_operation(self.request)

        # bump the request statistics
        self._statistics.increment_http_code(self.request.method)

        # determine the response payload type
        accept_type = self.request.headers.get("Accept")
        self._accept_type = determine_payload_type(accept_type)
        if self._accept_type is None:
            self._accept_type = PayloadType.XML
            self._logger.debug("Unknown Accept type %s, defaulting to %s",
                               accept_type,
                               self._accept_type)

        self._logger.debug("%s: %s %s" % (self.request.method, self.request.uri, self._accept_type))

    def on_finish(self):
        # bump the request statistics
        self._statistics.increment_http_code(self.request.method, self._status_code)
        
        if self._configuration.log_timing:
            self._logger.debug("total transaction time for %s %s: %s " % (self.method, self.uri, self.request_time()))


    def _convert_response_and_get_status(self, netconf_operation, accept_type, xml_blob):
        if netconf_operation in (NetconfOperation.DELETE,
                                 NetconfOperation.CREATE,
                                 NetconfOperation.REPLACE,
                                 NetconfOperation.RPC):
            #ATTN: rpc needs to use the schema-aware translation
            if accept_type in (PayloadType.JSON, PayloadType.JSON_COLLECTION):
                response = naive_xml_to_json(bytes(xml_blob,"utf-8"))
                if netconf_operation == NetconfOperation.RPC:
                    response = convert_rpc_to_json_output(response)
            else:
                response = xml_blob
                if netconf_operation == NetconfOperation.RPC:
                    response = convert_rpc_to_xml_output(response)
            return 201, response
        else: # GET or GET_CONFIG
            if accept_type == PayloadType.XML:
                # strip off <data></data> tags
                root = lxml.etree.fromstring(xml_blob)
                try:
                    lxml.etree.tostring(root[0])
                except:
                    return 204, ""
                response = xml_blob

            elif accept_type == PayloadType.XML_COLLECTION:
                try:
                    # check for no content
                    root = lxml.etree.fromstring(xml_blob)
                    lxml.etree.tostring(root[0])
                except:
                    return 204, ""

                response = convert_xml_to_collection(self.request.uri, xml_blob)

            elif accept_type == PayloadType.JSON or accept_type == PayloadType.JSON_COLLECTION:
                xpath = create_xpath_from_url(self.request.uri, self._schema_root)

                try:
                    # check for no content
                    root = lxml.etree.fromstring(xml_blob)
                    lxml.etree.tostring(root[0])
                except:
                    return 204, ""

                try:
                    is_collection = accept_type == PayloadType.JSON_COLLECTION
                    response = self._xml_to_json_translator.convert(is_collection, self.request.uri, xpath, xml_blob)
                except IndexError:
                    self._statistics.get_204_rsp += 1
                    return 204, "", ""
                except Exception as e:
                    raise RestError(500, str(xml_blob))
        return 200, response

    @asyncio.coroutine
    @exception_wrapper
    def _generic_impl(self):
        try:
            yield from self.connect()
        except AuthenticationError as e:
            self._statistics.req_401_rsp += 1
            self._logger.error("Authentication failed. Bad username/password.")
            raise RestError(401, "bad username/password")
        except ValueError as e:
            self._statistics.req_401_rsp += 1
            self._logger.error("Authentication failed. Missing BasicAuth.")
            raise RestError(401, "must supply BasicAuth")

        # ATTN: special checks for POST and PUT
        if self.request.method == "DELETE" and not is_config(self.request.uri):
            self._logger.error("Operational data DELETE not supported.")
            raise RestError(405, "can't delete operational data")

        # ATTN: make json it's own request handler
        if self.request.method == "GET" and is_schema_api(self.request.uri):
            return 200, "application/vnd.yang.data+json", get_json_schema(self._schema_root, self.request.uri)

        # convert url and body into xml payload 
        body = self.request.body.decode("utf-8")
        body_header = self.request.headers.get("Content-Type")
        convert_payload = (body, body_header)

        try:
            if self.request.method in ("GET"):
                converted_url = convert_get_request_to_xml(self._schema_root, self.request.uri)
            else:
                converted_url = self._confd_url_converter.convert(self.request.method, self.request.uri, convert_payload)
        except Exception as e:
            self._logger.error("invalid url requested %s, %s", self.request.uri, e)
            raise RestError(404, "Resource target or resource node not found")

        self._logger.debug("converted url: %s" % converted_url)

        if self._configuration.log_timing:
            sb_start_time = time.clock_gettime(time.CLOCK_REALTIME)

        result, netconf_response = yield from self._netconf.execute(self._netconf_operation, self.request.uri, converted_url)

        if self._configuration.log_timing:
            sb_end_time = time.clock_gettime(time.CLOCK_REALTIME)
            self._logger.debug("southbound transaction time: %s " % sb_end_time - sb_start_time)

        if result != Result.OK:
            code = map_error_to_http_code(result)
            raise RestError(code, netconf_response)

        # convert response to match the accept type
        code, response = self._convert_response_and_get_status(self._netconf_operation, self._accept_type, netconf_response)
        return code, self._accept_type.value, response


    @tornado.gen.coroutine
    def delete(self, *args, **kwargs):

        f = asyncio.async(self._generic_impl(), loop=self._asyncio_loop)
        response = yield tornado.platform.asyncio.to_tornado_future(f)
        
        http_code, content_type, message = response[:]

        self.clear()
        self.set_status(http_code)
        self.set_header("Content-Type", content_type)
        self.write(message)

    @tornado.gen.coroutine
    def get(self, *args, **kwargs):
        f = asyncio.async(self._generic_impl(), loop=self._asyncio_loop)
        response = yield tornado.platform.asyncio.to_tornado_future(f)
        
        http_code, content_type, message = response[:]
        self.clear()

        self.set_status(http_code)
        if http_code != 204:
            self.set_header("Content-Type", content_type)
            self.write(message)

    @tornado.gen.coroutine
    def put(self, *args, **kwargs):
        self._statistics.put_req += 1
        f = asyncio.async(self._generic_impl(), loop=self._asyncio_loop)
        response = yield tornado.platform.asyncio.to_tornado_future(f)

        http_code, content_type, message = response[:]
        self.clear()
        self.set_status(http_code)
        self.set_header("Content-Type", content_type)
        self.write(message)

    @tornado.gen.coroutine
    def post(self, *args, **kwargs):
        self._statistics.post_req += 1

        f = asyncio.async(self._generic_impl(), loop=self._asyncio_loop)
        response = yield tornado.platform.asyncio.to_tornado_future(f)

        http_code, content_type, message = response[:]
        self.clear()
        self.set_status(http_code)
        self.set_header("Content-Type", content_type)

        self.write(message)

class ReadOnlyHandler(HttpHandler):
    SUPPORTED_METHODS = ("GET")
    def prepare(self):
        super(ReadOnlyHandler, self).prepare()
        # hack the url to work with everything else we know about
        self.request.uri = self.request.uri.replace("/readonly", "")


