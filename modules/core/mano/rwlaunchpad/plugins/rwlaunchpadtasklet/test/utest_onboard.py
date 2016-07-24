#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import argparse
import asyncio
import base64
import concurrent.futures
import io
import logging
import os
import sys
import tornado.testing
import tornado.web
import unittest
import uuid
import xmlrunner

from rift.package import convert
from rift.tasklets.rwlaunchpad import onboard
import rift.test.dts

import gi
gi.require_version('NsdYang', '1.0')
gi.require_version('VnfdYang', '1.0')

from gi.repository import (
        NsdYang,
        VnfdYang,
        )


class RestconfDescriptorHandler(tornado.web.RequestHandler):
    DESC_SERIALIZER_MAP = {
            "nsd": convert.NsdSerializer(),
            "vnfd": convert.VnfdSerializer(),
            }

    class AuthError(Exception):
        pass


    class ContentTypeError(Exception):
        pass


    class RequestBodyError(Exception):
        pass


    def initialize(self, log, auth, info):
        self._auth = auth
        # The superclass has self._log already defined so use a different name
        self._logger = log
        self._info = info
        self._logger.debug('Created restconf descriptor handler')

    def _verify_auth(self):
        if self._auth is None:
            return None

        auth_header = self.request.headers.get('Authorization')
        if auth_header is None or not auth_header.startswith('Basic '):
            self.set_status(401)
            self.set_header('WWW-Authenticate', 'Basic realm=Restricted')
            self._transforms = []
            self.finish()

            msg = "Missing Authorization header"
            self._logger.error(msg)
            raise RestconfDescriptorHandler.AuthError(msg)

        auth_header = auth_header.encode('ascii')
        auth_decoded = base64.decodebytes(auth_header[6:]).decode()
        login, password = auth_decoded.split(':', 2)
        login = login
        password = password
        is_auth = ((login, password) == self._auth)

        if not is_auth:
            self.set_status(401)
            self.set_header('WWW-Authenticate', 'Basic realm=Restricted')
            self._transforms = []
            self.finish()

            msg = "Incorrect username and password in auth header: got {}, expected {}".format(
                    (login, password), self._auth
                    )
            self._logger.error(msg)
            raise RestconfDescriptorHandler.AuthError(msg)

    def _verify_content_type_header(self):
        content_type_header = self.request.headers.get('content-type')
        if content_type_header is None:
            self.set_status(415)
            self._transforms = []
            self.finish()

            msg = "Missing content-type header"
            self._logger.error(msg)
            raise RestconfDescriptorHandler.ContentTypeError(msg)

        if content_type_header != "application/vnd.yang.data+json":
            self.set_status(415)
            self._transforms = []
            self.finish()

            msg = "Unsupported content type: %s" % content_type_header
            self._logger.error(msg)
            raise RestconfDescriptorHandler.ContentTypeError(msg)

    def _verify_headers(self):
        self._verify_auth()
        self._verify_content_type_header()

    def _verify_request_body(self, descriptor_type):
        if descriptor_type not in RestconfDescriptorHandler.DESC_SERIALIZER_MAP:
            raise ValueError("Unsupported descriptor type: %s" % descriptor_type)

        body = self.request.body
        bytes_hdl = io.BytesIO(body)

        serializer = RestconfDescriptorHandler.DESC_SERIALIZER_MAP[descriptor_type]

        try:
            message = serializer.from_file_hdl(bytes_hdl, ".json")
        except convert.SerializationError as e:
            self.set_status(400)
            self._transforms = []
            self.finish()

            msg = "Descriptor request body not valid"
            self._logger.error(msg)
            raise RestconfDescriptorHandler.RequestBodyError() from e

        self._info.last_request_message = message

        self._logger.debug("Received a valid descriptor request")

    def put(self, descriptor_type):
        self._info.last_descriptor_type = descriptor_type
        self._info.last_method = "PUT"

        try:
            self._verify_headers()
        except (RestconfDescriptorHandler.AuthError,
                RestconfDescriptorHandler.ContentTypeError):
            return None

        try:
            self._verify_request_body(descriptor_type)
        except RestconfDescriptorHandler.RequestBodyError:
            return None

        self.write("Response doesn't matter?")

    def post(self, descriptor_type):
        self._info.last_descriptor_type = descriptor_type
        self._info.last_method = "POST"

        try:
            self._verify_headers()
        except (RestconfDescriptorHandler.AuthError,
                RestconfDescriptorHandler.ContentTypeError):
            return None

        try:
            self._verify_request_body(descriptor_type)
        except RestconfDescriptorHandler.RequestBodyError:
            return None

        self.write("Response doesn't matter?")


class HandlerInfo(object):
    def __init__(self):
        self.last_request_message = None
        self.last_descriptor_type = None
        self.last_method = None


class OnboardTestCase(tornado.testing.AsyncHTTPTestCase):
    AUTH = ("admin", "admin")
    def setUp(self):
        self._log = logging.getLogger(__file__)
        self._loop = asyncio.get_event_loop()

        self._handler_info = HandlerInfo()
        super().setUp()
        self._port = self.get_http_port()
        self._onboarder = onboard.DescriptorOnboarder(
                log=self._log, port=self._port
                )

    def get_new_ioloop(self):
        return tornado.platform.asyncio.AsyncIOMainLoop()

    def get_app(self):
        attrs = dict(auth=OnboardTestCase.AUTH, log=self._log, info=self._handler_info)
        return tornado.web.Application([
            (r"/api/config/.*/(nsd|vnfd)", RestconfDescriptorHandler, attrs),
            ])

    @rift.test.dts.async_test
    def test_onboard_nsd(self):
        nsd_msg = NsdYang.YangData_Nsd_NsdCatalog_Nsd(id=str(uuid.uuid4()), name="nsd_name")
        yield from self._loop.run_in_executor(None, self._onboarder.onboard, nsd_msg)
        self.assertEqual(self._handler_info.last_request_message, nsd_msg)
        self.assertEqual(self._handler_info.last_descriptor_type, "nsd")
        self.assertEqual(self._handler_info.last_method, "POST")

    @rift.test.dts.async_test
    def test_update_nsd(self):
        nsd_msg = NsdYang.YangData_Nsd_NsdCatalog_Nsd(id=str(uuid.uuid4()), name="nsd_name")
        yield from self._loop.run_in_executor(None, self._onboarder.update, nsd_msg)
        self.assertEqual(self._handler_info.last_request_message, nsd_msg)
        self.assertEqual(self._handler_info.last_descriptor_type, "nsd")
        self.assertEqual(self._handler_info.last_method, "PUT")

    @rift.test.dts.async_test
    def test_bad_descriptor_type(self):
        nsd_msg = NsdYang.YangData_Nsd_NsdCatalog()
        with self.assertRaises(TypeError):
            yield from self._loop.run_in_executor(None, self._onboarder.update, nsd_msg)

        with self.assertRaises(TypeError):
            yield from self._loop.run_in_executor(None, self._onboarder.onboard, nsd_msg)

    @rift.test.dts.async_test
    def test_bad_port(self):
        # Use a port not used by the instantiated server
        new_port = self._port - 1
        self._onboarder.port = new_port
        nsd_msg = NsdYang.YangData_Nsd_NsdCatalog_Nsd(id=str(uuid.uuid4()), name="nsd_name")

        with self.assertRaises(onboard.OnboardError):
            yield from self._loop.run_in_executor(None, self._onboarder.onboard, nsd_msg)

        with self.assertRaises(onboard.UpdateError):
            yield from self._loop.run_in_executor(None, self._onboarder.update, nsd_msg)

    @rift.test.dts.async_test
    def test_timeout(self):
        # Set the timeout to something minimal to speed up test
        self._onboarder.timeout = .1

        nsd_msg = NsdYang.YangData_Nsd_NsdCatalog_Nsd(id=str(uuid.uuid4()), name="nsd_name")

        # Force the request to timeout by running the call synchronously so the
        with self.assertRaises(onboard.OnboardError):
            self._onboarder.onboard(nsd_msg)

        # Force the request to timeout by running the call synchronously so the
        with self.assertRaises(onboard.UpdateError):
            self._onboarder.update(nsd_msg)


def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])
    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-n', '--no-runner', action='store_true')

    args, unknown = parser.parse_known_args(argv)
    if args.no_runner:
        runner = None

    # Set the global logging level
    logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.ERROR)

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__] + unknown + ["-v"], testRunner=runner)

if __name__ == '__main__':
    main()
