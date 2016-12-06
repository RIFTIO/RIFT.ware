#!/usr/bin/env python
"""
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
# @file rw_test_yang_json.py
# @date 2016/02/08
#
"""

import argparse
import logging
import os
import io
import json
import sys
import unittest
import gi
gi.require_version('RwYang', '1.0')
gi.require_version('TestJsonSchemaYang', '1.0')
from gi.repository import RwYang, TestJsonSchemaYang
import xmlrunner

class TestJsonSchema(unittest.TestCase):
    def setUp(self):
        something = False

    def basic_list_tests(self, list_json_cont):
        # validate type
        self.assertEqual(list_json_cont["type"], "list")
        # validate cardinality
        self.assertEqual(list_json_cont["cardinality"], "0..N")
        # validate presence of key(s)
        self.assertTrue(len(list_json_cont["key"]))
        # check for list elements
        self.assertTrue(len(list_json_cont["properties"]))

    def basic_leaf_tests(self, leaf_json_cont):
        self.assertEqual(leaf_json_cont["type"], "leaf")
        self.assertEqual(leaf_json_cont["cardinality"], "0..1")
        self.assertEqual(len(leaf_json_cont["properties"]), 0)

    def basic_leafref_tests(self, leafref_json_cont):
        self.basic_leaf_tests(leafref_json_cont)
        self.assertTrue(leafref_json_cont["data-type"]["leafref"]["path"])

    def test_yang_case_json(self):
        model = RwYang.Model.create_libncx()
        module = model.load_module("test-json-schema-case")
        self.assertTrue(module)
        root = model.get_root_node()
        top = root.get_first_child()

        data = top.to_json_schema(True)
        print (data)
        jdata = json.loads(data)
        choice_1 = jdata["top"]["properties"][0]["properties"][1]
        self.assertEqual(choice_1["name"], "leaf_choice")
        total_cases = len(choice_1["properties"])
        self.assertEqual(total_cases, 3)

        list_two_case = choice_1["properties"][1]
        self.assertEqual(list_two_case["name"], "list-two")

        lldb = list_two_case["properties"][0]
        self.assertEqual(lldb["name"], "lldb")
        self.assertEqual(lldb["type"], "list")

        list_ch = lldb["properties"][1]
        self.assertEqual(list_ch["type"], "choice")
        self.assertEqual(len(list_ch["properties"]), 2)

        list_case = choice_1["properties"][total_cases-1]
        self.assertEqual(list_case["name"], "list-three")
        self.assertEqual(len(list_case["properties"]), 1)

        gdb_list = list_case["properties"][0]
        opt_choice = gdb_list["properties"][1]
        self.assertEqual(opt_choice["name"], "opt")
        opt_choice_cases = len(opt_choice["properties"])
        self.assertEqual(opt_choice_cases, 2)

        choice_cont = jdata["top"]["properties"][1]
        self.assertEqual(choice_cont["name"], "choice-cont")
        choice_cont_len = len(choice_cont["properties"])

        action_choice = choice_cont["properties"][1]
        self.assertEqual(action_choice["name"], "action")
        self.assertEqual(action_choice["type"], "choice")

        action_cases = len(action_choice["properties"])
        self.assertEqual(action_cases, 3)

        annex_case = action_choice["properties"][0]
        self.assertEqual(annex_case["name"], "annex")
        self.assertEqual(len(annex_case["properties"]), 1)
        annex_cont = annex_case["properties"][0]
        self.assertEqual(len(annex_cont["properties"]), 3)

        start_case = action_choice["properties"][1]
        self.assertEqual(start_case["name"], "start")
        self.assertEqual(len(start_case["properties"]), 1)

        sleep_case = action_choice["properties"][2]
        self.assertEqual(sleep_case["name"], "sleep")
        self.assertEqual(len(sleep_case["properties"]), 1)

    def test_yang_json(self):
        model = RwYang.Model.create_libncx()
        module = model.load_module("test-json-schema")
        self.assertTrue(module)
        root = model.get_root_node()
        top = root.get_first_child()

        data = top.to_json_schema(True)
        print (data)
        jdata = json.loads(data)
        self.assertEqual(jdata["top"]["name"], "test-json-schema:top")

        # Test container items
        self.assertEqual(len(jdata["top"]["properties"]), 11)

        # Check cardinality of all leafs and lists
        cont_props = jdata["top"]["properties"]
        for prop in cont_props:
            if prop["type"] == "leaf":
                self.assertEqual(prop["cardinality"], "0..1")
            elif "list" in prop["type"]:
                self.assertEqual(prop["cardinality"], "0..N")

        # Check for null properties for leaf and leaf-lists
        for prop in cont_props:
            if "leaf" in prop["type"]:
                self.assertEqual(len(prop["properties"]), 0)

        # Check keys for list
        list_1 = jdata["top"]["properties"][6]
        self.assertEqual(list_1["name"], "list-1")
        self.basic_list_tests(list_1)

        # Container inside list
        list_list_cont = list_1["properties"][2]
        self.assertEqual(list_list_cont["name"], "list-cont")
        self.assertEqual(list_list_cont["type"], "container")
        self.assertEqual(list_list_cont["properties"][0]["name"], "yours-truly")

        # verify keys
        self.assertEqual(len(list_1["key"]), 2)
        self.assertEqual(list_1["key"][0], "id")
        self.assertEqual(list_1["key"][1], "name")

        # list inside list
        int_list_1 = list_1["properties"][3]
        self.basic_list_tests(int_list_1)
        self.assertEqual(int_list_1["name"], "int-list-1")

        # test leafref
        leafref = jdata["top"]["properties"][7]
        self.assertEqual(leafref["name"], "leaf-5")
        self.basic_leafref_tests(leafref)

        # test bit leaf
        bit_leaf = jdata["top"]["properties"][9]
        self.assertEqual(bit_leaf["name"], "bit-leaf")
        self.basic_leaf_tests(bit_leaf)

        self.assertTrue(bit_leaf["data-type"]["bits"]["bit"]["aggregate"])
        self.assertTrue(bit_leaf["data-type"]["bits"]["bit"]["timeout"])
        self.assertTrue(bit_leaf["data-type"]["bits"]["bit"]["sync"])

        bit_leaf_bits = bit_leaf["data-type"]["bits"]["bit"]
        self.assertEqual(bit_leaf_bits["aggregate"]["position"], 0)
        self.assertEqual(bit_leaf_bits["timeout"]["position"], 1)
        self.assertEqual(bit_leaf_bits["sync"]["position"], 2)

        # test binary leaf
        bin_leaf = jdata["top"]["properties"][10]
        self.assertEqual(bin_leaf["name"], "hidden")
        self.basic_leaf_tests(bin_leaf)
        self.assertEqual(bin_leaf["data-type"], "binary")

        # test enumeration
        enum_leaf = jdata["top"]["properties"][5]
        self.assertEqual(enum_leaf["name"], "leaf-4")
        self.basic_leaf_tests(enum_leaf)

        self.assertTrue(enum_leaf["data-type"]["enumeration"])
        self.assertTrue(enum_leaf["data-type"]["enumeration"]["enum"])

        enums = enum_leaf["data-type"]["enumeration"]["enum"]
        self.assertEqual(enums["I_E_A"]["value"], 991)
        self.assertEqual(enums["I_E_B"]["value"], 992)
        self.assertEqual(enums["I_E_C"]["value"], 993)


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
