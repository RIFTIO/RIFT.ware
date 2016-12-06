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
gi.require_version('RwTypes', '1.0')
gi.require_version('CompanyYang', '1.0')
gi.require_version('DocumentYang', '1.0')
from gi.repository import RwTypes
from gi.repository import CompanyYang
from gi.repository import DocumentYang

logger = logging.getLogger(__name__)

class TestPbcm(unittest.TestCase):
    def setUp(self):
        pass

    def test_gi_typename(self):
        co = CompanyYang.Company()
        copbcm = co.to_pbcm()
        self.assertEqual(copbcm.get_gi_typename(), "CompanyYang.Company")
        desc = co.retrieve_descriptor()
        field_names = desc.get_field_names()
        self.assertEqual(desc.get_gi_typename(), "CompanyYang.Company")
        self.assertTrue('employee' in field_names)
        self.assertTrue('contact_info' in field_names)
        self.assertTrue('product' in field_names)
        self.assertTrue(desc.get_field_by_name('employee').is_list())
        self.assertFalse(desc.get_field_by_name('contact_info').is_list())

        keys = desc.get_key_names()
        self.assertTrue(0 == len(keys))

        person = CompanyYang.Company_Employee()
        desc = person.retrieve_descriptor()
        field_names = desc.get_field_names()

        self.assertTrue('id' in field_names)
        self.assertTrue('name' in field_names)
        self.assertTrue('title' in field_names)
        self.assertTrue('start_date' in field_names)

        keys = desc.get_key_names()
        self.assertTrue('id' in keys)
        self.assertTrue(len(keys) == 1)

        field_names = CompanyYang.Company.change_to_descriptor(CompanyYang.Company.schema()).get_field_names()
        self.assertTrue('employee' in field_names)
        self.assertTrue('contact_info' in field_names)
        self.assertTrue('product' in field_names)

    def test_protobuf_are_equal(self):
        proto_a = CompanyYang.Company()
        proto_b = CompanyYang.Company()

        self.assertEqual(proto_a, proto_b)

    def test_protobuf_are_unequal(self):
        proto_a = CompanyYang.Company()
        proto_b = DocumentYang.MainBook_Chapters()
        proto_b.set_number(3)

        self.assertNotEqual(proto_a, proto_b)


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
