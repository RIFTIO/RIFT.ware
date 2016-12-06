#!/usr/bin/env python3
"""
#
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
#
# @file rwrest_module_test.py
# @author Max Beckett
# @date 12/8/2015
#
"""
import json
import logging
import os
from subprocess import Popen, DEVNULL
import sys
import time
import unittest

import tornado.httpclient
import tornado.web
import xmlrunner

from rift.rwlib.util import certs

logger = logging.getLogger(__name__)

WAIT_TIME = 90 #seconds

def _collapse_string(string):
    return ''.join([line.strip() for line in string.splitlines()])

def _ordered(obj):
    '''sort json dictionaries for comparison'''
    if isinstance(obj, dict):
        for k, v in obj.items():
            if not isinstance(v, list) and not isinstance(v, dict):
                obj[k] = str(v)
        return sorted((k, _ordered(v)) for k, v in obj.items())
    if isinstance(obj, list):
        return sorted(_ordered(x) for x in obj)
    else:
        return obj

class TestRiftRest(unittest.TestCase):

    sys_proc=None

    @classmethod
    def setUpClass(cls):
    # setup http headers
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
            
        # Start the mgmt-agent system test in background
        cls.log_file = open("logfile", "w")
       
        install_dir = os.environ.get("RIFT_INSTALL")
        demo_dir = os.path.join(install_dir, 'demos')
        testbed = os.path.join(demo_dir, 'mgmt_tbed.py')
        try:
            cls.sys_proc=Popen([testbed], stdin=DEVNULL, stdout=cls.log_file, stderr=cls.log_file,
                               preexec_fn=os.setsid)
        except Exception as e:
            print("Failed to start system test.. error: {0}".format(e))
            raise

        print("Started the Mgmt Agent Mini System Test..sleeping for {} secs".format(WAIT_TIME))
        time.sleep(WAIT_TIME)



    def test_get_vcs(self):
        logger.info("starting test_get_vcs_and_logout")

        url = "%s://%s:%s/api/operational/vcs" % (self.prefix, self.rest_host, self.rest_port)

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


    @classmethod
    def tearDownClass(cls):
        print("Stopping the Mgmt Agent Mini System Test..")
        cls.sys_proc.terminate()
        cls.log_file.close()



def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__] + argv,
            testRunner=xmlrunner.XMLTestRunner(
                output=os.environ["RIFT_MODULE_TEST"]))

if __name__ == '__main__':
    main()
