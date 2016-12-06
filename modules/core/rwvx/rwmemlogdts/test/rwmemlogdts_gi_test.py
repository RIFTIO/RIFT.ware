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

import argparse
import os
import sys
import types
import unittest
import xmlrunner
import gi
gi.require_version('RwMemlog', '1.0')
gi.require_version('ProtobufC', '1.0')
import gi.repository.RwMemlog as rwmemlog
from gi.repository import ProtobufC

class RwMemlogTestCase(unittest.TestCase):
    def test_one(self):
        memlog_instance = rwmemlog.instance_new("memlogGtest",0,0)
        memlogbuffer = memlog_instance.get_buffer("memlogGtestObj", 0)
        memlogbuffer.buffer_log("abcdefgh")
#       memlogbuffer.buffer_output()
        results = memlogbuffer.buffer_string()
#       print(results)
        self.assertTrue(len(results) > 0) 
#       memlog_instance.instance_output() 
        instresults = memlog_instance.instance_string() 
#       print(instresults)
        self.assertTrue(len(instresults) > 0) 

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    args, _ = parser.parse_known_args()
    runner = xmlrunner.XMLTestRunner(output=os.environ["RIFT_MODULE_TEST"])
    unittest.main(testRunner=runner)


if __name__ == '__main__':
    main()
