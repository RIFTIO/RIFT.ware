#!/usr/bin/env python3

# STANDARD_RIFT_IO_COPYRIGHT

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
