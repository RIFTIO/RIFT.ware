#!/usr/bin/env python
"""
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# @file utest_component.py
# @author Austin Cormier (austin.cormier@riftio.com)
# @date 05/13/2015
#
"""

import argparse
import logging
import os
import sys
import unittest

import xmlrunner

import rift.vx.component

import gi
gi.require_version('RwDts', '1.0')
gi.require_version('RwBaseYang', '1.0')

from gi.repository import RwDts, RwBaseYang

def create_component(instance_name, component_type, parent_name=None, children_names=None):
    component = RwBaseYang.ComponentInfo()
    component.instance_name = instance_name
    component.component_type = component_type
    if parent_name is not None:
        component.rwcomponent_parent = parent_name
    if children_names is not None:
        component.rwcomponent_children = children_names

    return component


class TestSystemInfo(unittest.TestCase):
    def setUp(self):
        self.comp_db = rift.vx.component.ComponentInfoDB()

    def test_add_one_component(self):
        self.comp_db.add_component_infos([
            create_component("foo", "RWPROC", None, None)
            ])

        self.assertEqual(1, len(self.comp_db))
        self.assertIsNotNone(self.comp_db.find_component_by_name("foo"))

    def test_find_parent_component(self):
        self.comp_db.add_component_infos([
            create_component("foo_parent", "RWPROC", None, ["foo"]),
            create_component("foo", "RWPROC", "foo_parent", None),
            create_component("foo_noparent", "RWPROC", None, None),
            ])

        self.assertEqual(3, len(self.comp_db))
        self.assertIsNotNone(self.comp_db.find_parent_component("foo"))
        self.assertIsNone(self.comp_db.find_parent_component("foo_noparent"))
        self.assertRaises(rift.vx.component.ComponentNotFound,
                self.comp_db.find_parent_component, ["asdf"])

    def test_list_child_components(self):
        self.comp_db.add_component_infos([
            create_component("foo_parent", "RWPROC", None, ["foo"]),
            create_component("foo", "RWTASKLET", "foo_parent", None),
            create_component("foo_noparent", "RWTASKLET", None, None),
            ])

        self.assertEqual(3, len(self.comp_db))
        child_tasklets = self.comp_db.list_child_tasklets("foo_parent")
        self.assertEqual(1, len(child_tasklets))


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
