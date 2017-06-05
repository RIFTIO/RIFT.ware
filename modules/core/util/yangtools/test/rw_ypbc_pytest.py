#!/usr/bin/env python
"""
#
# STANDARD_RIFT_IO_COPYRIGHT #
#
# @file rw_ypbc_pytest.py
# @author Tom Seidenberg
# @date 2015/02/23
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
gi.require_version('RwYang', '1.0')
gi.require_version('RwmanifestDYang', '1.0')
from gi.repository import RwYang
from gi.repository import RwYangPb
from gi.repository import RwmanifestDYang

logger = logging.getLogger(__name__)

class TestRwYang(unittest.TestCase):
    def setUp(self):
        pass

    def test_create_schema(self):
        try:
            schema = RwYangPb.Schema()
        except TypeError as e:
            self.assertTrue("struct cannot be created directly" in str(e) )

    def test_get_schema(self):
        schema = RwmanifestDYang.get_schema()
        self.assertIsInstance(schema, gi.repository.RwYangPb.Schema)

    def test_get_schema_context(self):
        context = RwmanifestDYang.Manifest.schema()
        self.assertTrue(context)

    def test_xml_meta_data(self):
        context = RwmanifestDYang.Manifest.schema()
        desc = context.retrieve_descriptor()
        self.assertEqual(desc.xml_prefix(), "rwmanifest")
        self.assertEqual(desc.xml_ns(), "http://riftio.com/ns/riftware-1.0/rwmanifest")
        self.assertEqual(desc.xml_element_name(), "manifest")

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
