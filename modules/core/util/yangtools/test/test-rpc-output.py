#!/usr/bin/env python
"""
# STANDARD_RIFT_IO_COPYRIGHT #
# @file testoutput.py
# @date 2016/07/30
#
"""

import argparse
import gi
import logging
import os
import sys
import unittest
import xmlrunner

gi.require_version('RwYang', '1.0')
from gi.repository import RwYang
gi.require_version('TestOutputYang', '1.0')
from gi.repository import TestOutputYang

logger = logging.getLogger(__name__)

class TestOutput(unittest.TestCase):
    def setUp(self):
        self.model_ = RwYang.Model.create_libncx()
        schema = TestOutputYang.get_schema()
        self.model_.load_schema_ypbc(schema)

    def test_input(self):
        input = TestOutputYang.YangInput_TestOutput_TestRpc()

        input_str = '''<?xml version="1.0" ?>
        <to:test-rpc xmlns:to="http://riftio.com/ns/core/util/yangtools/test/yang/test-output">
          <to:output>abc123</to:output>
          <to:input>789XYZ</to:input>
        </to:test-rpc>'''

        input.from_xml_v2(self.model_, input_str)
        logger.debug("RPC input string '%s'", input);

    def test_output(self):
        output = TestOutputYang.YangOutput_TestOutput_TestRpc()

        output_str2 = '''<?xml version="1.0" ?>
        <to:test-rpc xmlns:to="http://riftio.com/ns/core/util/yangtools/test/yang/test-output">
          <to:output>ABCDEF</to:output>
          <to:input>xyzXYZ</to:input>
        </to:test-rpc>'''

        output.from_xml_v2(self.model_, output_str2)
        logger.debug("RPC output string '%s'", output);

def main(argv = sys.argv[1:]):
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

if __name__ == "__main__":
    main()

