#!/usr/bin/env python
"""
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# @file basic_plugin_pytest.py
# @author Anil Gunturu (anil.gunturu@riftio.com)
# @date 11/02/2014
#
"""

import argparse
import logging
import os
import sys
import unittest
import gi
gi.require_version('ExampleGi', '1.0')
gi.require_version('Peas', '1.0')
from gi.repository import GObject, Peas, GLib, GIRepository
from gi.repository import ExampleGi
import xmlrunner

logger = logging.getLogger(__name__)

def string_array_foreach(callback_boxed, string, user_data_boxed):
    # callback_boxed is a different wrapper around Boxed then boxed
    assert callback_boxed == user_data_boxed
    assert string in ('hello', 'world')

class TestExampleGi(unittest.TestCase):
    def setUp(self):
        self.boxed = ExampleGi.Boxed()
   
    def test_integer_array(self):
        self.boxed.set_integer_array(list(range(10)))
        self.assertEqual(self.boxed.get_integer_array(), list(range(10)))

    def test_string_array(self):
        self.boxed.set_string_array(['hello', 'world'])
        self.assertEqual(self.boxed.get_string_array(), ['hello', 'world'])
        self.boxed.string_array_foreach(string_array_foreach, self.boxed)

    def test_nested(self):
        self.boxed.set_nested(self.boxed)
        self.assertEqual(self.boxed.get_nested(), self.boxed)
        self.boxed.set_nested(None)
        self.assertEqual(self.boxed.get_nested(), None)

    def test_opaque_pointer(self):
        self.opaque = ExampleGi.Opaque.new()
        self.assertIsInstance(self.opaque, gi.repository.ExampleGi.Opaque)

    def test_enum(self):
        self.assertEqual(ExampleGi.MyEnumTest.VALUE_1, ExampleGi.MyEnumTest.VALUE_1)
        self.assertNotEqual(ExampleGi.MyEnumTest.VALUE_1, ExampleGi.MyEnumTest.VALUE_2)

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
