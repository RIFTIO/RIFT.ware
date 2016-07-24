#!/usr/bin/env python
"""
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# @file rw_pbcm_test.py
#
"""
import argparse
import logging
import os
import sys
import unittest
import xmlrunner
import gi
import re
import threading
import time
import random
import gc
from gi import require_version
require_version('CompanyYang', '1.0')
from gi.repository import CompanyYang

logger = logging.getLogger(__name__)


class TestThread(threading.Thread):
   def __init__(self, co):
       threading.Thread.__init__(self)
       self.co = co

   def run(self):
       for i in range (1000):
          time.sleep(random.randint(100,1000)/1000000.0)
          self.look_at_co()
          gc.collect()

   def look_at_co(self):
       peeps = self.co.employee
       peeps[ random.randint(0,31) ].title = "head honcho";

class TestThreads(unittest.TestCase):
    def setUp(self):
        self.co = CompanyYang.Company()
        
        for i in range(32):
            person = CompanyYang.Company_Employee()
            person.id = i
            person.name = "bob"
            self.co.employee.append(person)

    def test_threads(self):
        threads = []
        for i in range(32):
            t = TestThread (self.co)
            threads.append(t)

        for t in threads:
            t.start()

        for t in threads:
            t.join()

def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose')

    args = parser.parse_args(argv)

    # Set the global logging level
    logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.ERROR)

    # The unittest framework requires a program name, so use the name of this
    # file instead (we do not want to have to pass a fake program name to main
    # when this is called from the interpreter).
    unittest.main(argv=[__file__] + argv,
            testRunner=xmlrunner.XMLTestRunner(
                output=os.environ["RIFT_MODULE_TEST"]))

if __name__ == '__main__':
    main()
