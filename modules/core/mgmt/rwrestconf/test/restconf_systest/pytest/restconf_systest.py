#!/usr/bin/env python3
# RIFT_IO_STANDARD_COPYRIGHT_HEADER(BEGIN)
# Author(s): Max Beckett
# Creation Date: 9/1/2015
# RIFT_IO_STANDARD_COPYRIGHT_HEADER(END)

import argparse
import base64
import logging
import lxml.etree as et
import nose
import os
import shlex
import subprocess
import sys
import time
import unittest

import tornado.httpclient
import tornado.web

from rift.rwlib.util import certs

logger = logging.getLogger(__name__)

class SystemDiedError(Exception):
    pass

def compare_elements(lhs, rhs):
    lhs_tag = et.QName(lhs)
    rhs_tag = et.QName(rhs)
    if (lhs_tag.localname != rhs_tag.localname or
        lhs_tag.namespace != rhs_tag.namespace):
        logger.error("Element {} != {}".format(lhs, rhs))
        return False
    if len(lhs) != len(rhs):
        logger.error("Length of {} {} != Length of {} {}".format(
                                lhs, len(lhs), rhs, len(rhs)))
        return False
    if lhs.text != rhs.text:
        logger.error("Text mismatch {}.{} != {}.{}".format(
                                lhs, lhs.text, rhs, rhs.text))
        return False
    # Expect the elements to be in the same order
    for lhs_child, rhs_child in zip(lhs, rhs):
        result = compare_elements(lhs_child, rhs_child) 
        if not result:
            return result
    return True

class RestconfSystest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.http_client = tornado.httpclient.HTTPClient()
        cls.http_headers = {
            "Accept" : "application/vnd.yang.data+xml",
            "Content-Type" : "application/vnd.yang.data+json",
        }

        cls.rest_host = '127.0.0.1'
        cls.rest_port = '8008'
        use_ssl, cert, key = certs.get_bootstrap_cert_and_key()
        if use_ssl:
            cls.prefix = "https"
            cls.ssl_settings = {
                "validate_cert" : False,
                "client_key" : key,
                "client_cert" : cert,
                }
        else:
            cls.prefix = "http"
            cls.ssl_settings = {}
    @classmethod
    def tearDownClass(cls):
        pass

    def id(self):
        """
        Nose uses this id to derive the unittest class name which is used by
        Jenkins in reporting the test result.

        """
        org_id = super(RestconfSystest, self).id()
        return "TC_RESTCONF_SYSTEST_100%s" % (org_id)

    def test_get_vcs_and_logout(self):

        logger.info("starting test_get_vcs_and_logout")

        url = "%s://%s:%s/api/operational/vcs?deep" % (self.prefix, self.rest_host, self.rest_port)

        request = tornado.httpclient.HTTPRequest(
            url,
            headers=self.http_headers,
            method="GET",
            auth_username="admin",
            auth_password="admin",
            **self.ssl_settings
        )

        result = self.http_client.fetch(request)
        http_code= result.code

        self.assertEqual(http_code, 200, "GET request failed.")

        # logout 
        logout_url = "%s://%s:%s/api/logout" % (self.prefix, self.rest_host, self.rest_port)
        logout_request = tornado.httpclient.HTTPRequest(
            logout_url,
            headers=self.http_headers,
            method="POST",
            auth_username="admin",
            auth_password="admin",
            body="{}",
            **self.ssl_settings
        )

        logout_result = self.http_client.fetch(logout_request)
        http_code = logout_result.code

        self.assertEqual(http_code, 200,
                         "Could not logout.")

        body = b'<logout>success</logout>'
        self.assertEqual(logout_result.body, body)                         

    def test_get_bad_path(self):
        logger.info("starting test_get_bogus_path")

        url = "%s://%s:%s/api/config/bogus" % (self.prefix, self.rest_host, self.rest_port)
        request = tornado.httpclient.HTTPRequest(
            url,
            headers=self.http_headers,
            method="GET",
            auth_username="admin",
            auth_password="admin",
            **self.ssl_settings
        )

        error_code = -1

        try:
            result = self.http_client.fetch(request)
        except tornado.httpclient.HTTPError as e:
            error_code = e.code

        self.assertEqual(error_code, 404,
                         "Bogus path returned wrong error code.")

    def test_get_bad_authentication(self):

        logger.info("starting test_get_bad_authentication")

        url = "%s://%s:%s/api/config/bogus" % (self.prefix, self.rest_host, self.rest_port)
        request = tornado.httpclient.HTTPRequest(
            url,
            headers=self.http_headers,
            method="GET",
            auth_username="bad",
            auth_password="authentication",
            **self.ssl_settings
        )

        error_code = -1

        try:
            result = self.http_client.fetch(request)
        except tornado.httpclient.HTTPError as e:
            error_code = e.code

        self.assertEqual(error_code, 401,
                         "Bad authentication returned wrong error code.")

    def test_post_oper_denied(self):
        logger.info("starting test_post_oper_denied")

        json = '''
{
  "logrotate-conf": {
    "compress": "false"
  }
}'''

        url = "%s://%s:%s/api/running/uagent" % (self.prefix, self.rest_host, self.rest_port)
        request = tornado.httpclient.HTTPRequest(
            url,
            headers=self.http_headers,
            method="POST",
            auth_username="oper",
            auth_password="oper",
            body=json,
            **self.ssl_settings            
        )

        error_code = -1

        try:
            result = self.http_client.fetch(request)
        except tornado.httpclient.HTTPError as e:
            error_code = e.code

        self.assertEqual(error_code, 405,
                         "Operation denied returned wrong error code.")


    def test_get_defaults(self):
        logger.info("starting test_get_defaults")

        url = "%s://%s:%s/api/running/uagent/logrotate-conf" % (self.prefix, self.rest_host, self.rest_port)
        request = tornado.httpclient.HTTPRequest(
            url,
            headers=self.http_headers,
            method="GET",
            auth_username="admin",
            auth_password="admin",
            **self.ssl_settings
        )

        error_code = -1

        try:
            result = self.http_client.fetch(request)
            http_code = result.code
        except tornado.httpclient.HTTPError as e:
            http_code = e.code

        self.assertEqual(http_code, 200,
                         "Could not get rw-mgmt-schema config.")

        body = b'<?xml version="1.0" encoding="UTF-8"?><data xmlns="urn:ietf:params:xml:ns:netconf:base:1.0" xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0"><uagent xmlns="http://riftio.com/ns/riftware-1.0/rw-mgmtagt"><logrotate-conf><compress>true</compress><size>10</size><rotations>10</rotations></logrotate-conf></uagent></data>'
        actual = et.fromstring(result.body)
        expected = et.fromstring(body)
        self.assertTrue(compare_elements(actual[0], expected[0]),
            "Expected: {} != Actual: {}".format(
                        et.tostring(expected[0]), et.tostring(actual[0])))

def main(argv=sys.argv[1:]):

    parser = argparse.ArgumentParser()
    parser.add_argument(
            '-v', '--verbose',
            action='store_true',
            help="Logging is normally set to an INFO level. When this flag "
                 "is used logging is set to DEBUG. ")

    args = parser.parse_args(argv)

    # Configure logging
    logging_level = logging.DEBUG if args.verbose is True else logging.INFO
    logging.basicConfig()
    logging.getLogger().setLevel(logging_level)

    xunit_file = os.path.join(os.environ.get('RIFT_MODULE_TEST'),
                              "restconf_systest.xml")
    nose.runmodule(
        argv=[sys.argv[0],
             "--logging-level={}".format(logging.getLevelName(logging_level)),
             "--logging-format={}".format(
                '%(asctime)-15s %(levelname)s %(message)s'),
             "--nocapture", # display stdout immediately
             "--with-xunit",
             "--xunit-file=%s" % xunit_file])


if __name__ == '__main__':
    try:
        main()
    finally:
        os.system("stty sane")
