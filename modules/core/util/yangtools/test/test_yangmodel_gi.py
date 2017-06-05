#!/usr/bin/env python
"""
#
# STANDARD_RIFT_IO_COPYRIGHT #
#
# @file test_yangmodel_gi.py
# @author Rajesh Ramankutty (Rajesh.Ramankutty@riftio.com)
# @date 2015/03/10
#
"""

import argparse
import logging
import os
import io
import sys
import unittest
import gi
gi.require_version('RwYang', '1.0')
from gi.repository import RwYang
import xmlrunner

class TestYangModel(unittest.TestCase):
    def setUp(self):
        something = False
        pass

    def test_yangmodel_basic(self):
        model = RwYang.Model.create_libncx()
        module = model.load_module("testncx")
        self.assertTrue(module)

        print("yang_module.location :", module.get_location())

        root = model.get_root_node()
        print("root.get_name()=", root.get_name())
        self.assertEqual(root.get_name(), "data")
        self.assertFalse(root.is_leafy())
        self.assertFalse(root.node_type())

        top = root.get_first_child()
        print("top.get_name()=", top.get_name())
        self.assertEqual(top.get_name(), "top")
        self.assertTrue(top)
        self.assertEqual(top.get_parent(),root)
        self.assertEqual(top, root.get_first_child())
        self.assertFalse(top.is_leafy())
        self.assertFalse(top.node_type())

        top_sib = top.get_next_sibling()
        self.assertFalse(top_sib)

        a = top.get_first_child()
        print("a.get_name()=", a.get_name())
        self.assertTrue(a)
        self.assertEqual(a.get_parent(),top)
        self.assertEqual(a,top.get_first_child())
        self.assertFalse(a.is_leafy())
        self.assertFalse(a.node_type())

        a1 = a.get_next_sibling()
        b = a1.get_next_sibling()
        print("b.get_name()=", b.get_name())
        str_3 = b.get_first_child()
        print("str_3.get_name()=", str_3.get_name())
        self.assertTrue(str_3.is_leafy())


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
