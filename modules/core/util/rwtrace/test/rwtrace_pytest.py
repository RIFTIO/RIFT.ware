#!/usr/bin/env python
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
# @file rwtrace_pytest.py
# @author Anil Gunturu (anil.gunturu@riftio.com)
# @date 11/02/2014
#
"""

import argparse
import logging
import os
import io
import sys
import unittest
import gi

import gi
gi.require_version('RwTrace', '1.0')

from gi.repository import RwTrace
import xmlrunner

class TestRwTrace(unittest.TestCase):
    def setUp(self):
        pass

    def test_rwsched_basic(self):
        trace_ctx = RwTrace.Ctx()
        self.assertIsInstance(trace_ctx, gi.repository.RwTrace.Ctx)
        trace_ctx.category_severity_set(RwTrace.Category.RWCAL, 
                                        RwTrace.Severity.DEBUG)
        trace_ctx.category_destination_set(RwTrace.Category.RWCAL, 
                                           RwTrace.Destination.CONSOLE)
        trace_ctx.trace(RwTrace.Category.RWCAL,
                        RwTrace.Severity.EMERG,
                       "foo")
        pass

def main(argv=sys.argv[1:]):
    logging.basicConfig(format='TEST %(message)s')

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')

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
