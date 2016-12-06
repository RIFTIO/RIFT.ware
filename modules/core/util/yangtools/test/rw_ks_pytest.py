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
# @file rw_ks_pytest.py
# @author Sujithra Periasamy
# @date 2015/05/06
#
"""
import argparse
import logging
import unittest
import sys
import os
import xmlrunner
import gi
gi.require_version('RwYang', '1.0')
gi.require_version('RwKeyspec', '1.0')
gi.require_version('RwpersonDbYang', '1.0')
gi.require_version('RwFpathDYang', '1.0')
gi.require_version('TestAugmentA1Yang', '1.0')
from gi.repository import RwYang
from gi.repository import RwKeyspec
from gi.repository import RwpersonDbYang
from gi.repository import RwFpathDYang
from gi.repository import TestAugmentA1Yang

logger = logging.getLogger(__name__)

class TestKeySpec(unittest.TestCase):
    def setUp(self):
        pass

    def test_keyspec_xpath(self):
        xpath = "/ps:person/ps:phone[ps:number=\'1234\']"
        schema = RwpersonDbYang.get_schema()
        keyspec = RwKeyspec.path_from_xpath(schema, xpath, RwKeyspec.RwXpathType.KEYSPEC)
        self.assertTrue(keyspec)
        xpath_r = keyspec.to_xpath(schema)
        self.assertTrue(xpath_r)
        self.assertEqual(xpath_r, xpath)
        print(xpath_r)

    def test_gi_schema_context_methods(self):
        #create a keyspec that points to a leaf node
        xpath = "/rw-base:colony[rw-base:name=\'trafgen\']/rw-fpath:bundle-ether[rw-fpath:name=\'bundle1\']/rw-fpath:mtu"
        schema = RwFpathDYang.get_schema()
        keyspec = RwKeyspec.path_from_xpath(schema, xpath, RwKeyspec.RwXpathType.KEYSPEC)
        self.assertTrue(keyspec)
        gi_desc = RwFpathDYang.ConfigColony_BundleEther.schema()
        self.assertTrue(gi_desc);
        lname = gi_desc.keyspec_leaf(keyspec)
        self.assertTrue(lname)
        print("Leaf " + lname)
        self.assertEqual(lname, "mtu")
        #Error case not a leaf keyspec
        xpath = "/rw-base:colony[rw-base:name=\'trafgen\']/rw-fpath:bundle-ether[rw-fpath:name=\'bundle1\']"
        keyspec = RwKeyspec.path_from_xpath(schema, xpath, RwKeyspec.RwXpathType.KEYSPEC)
        self.assertTrue(keyspec)
        lname = gi_desc.keyspec_leaf(keyspec)
        self.assertFalse(lname);
        #Error wrong schema
        gi_desc = RwFpathDYang.ConfigColony.schema()
        lname = gi_desc.keyspec_leaf(keyspec)
        self.assertFalse(lname)

    def test_gi_keyspec_string_method(self):
        #create a keyspec that points to a leaf node
        xpath = "D,/rw-base:colony[rw-base:name=\'abc\']/rw-fpath:bundle-ether[rw-fpath:name=\'xyz\']/rw-base:mtu"
        schema = RwFpathDYang.get_schema()
        keyspec = RwKeyspec.path_from_xpath(schema, xpath, RwKeyspec.RwXpathType.KEYSPEC)
        self.assertFalse(keyspec)
        xpath = "D,/rw-base:colony[rw-base:name=\'abc\']/rw-fpath:bundle-ether[rw-fpath:name=\'xyz\']/rw-fpath:mtu"
        keyspec = RwKeyspec.path_from_xpath(schema, xpath, RwKeyspec.RwXpathType.KEYSPEC)
        self.assertTrue(keyspec)
        v_create_string =  keyspec.create_string(schema)
        v_create_string_no_schema =  keyspec.create_string()
        print("create_string           = %s" % (v_create_string,))
        print("create_string_no_schema = %s" % (v_create_string_no_schema,))
        self.assertEqual(v_create_string, xpath)

    def test_gi_xpath_for_leaf(self):
        # some augment testcases for leaf yang nodes
        xpath = "D,/taugb1:b1_c/tauga1:a1_f1"
        schema = TestAugmentA1Yang.get_schema()
        keyspec = RwKeyspec.path_from_xpath(schema, xpath, RwKeyspec.RwXpathType.KEYSPEC)
        self.assertTrue(keyspec)
        v_create_string =  keyspec.create_string(schema)
        self.assertEqual(v_create_string, xpath)
        xpath = "D,/taugb1:b1_c/taugb1:b1_c1/taugb1:b1_f1"
        keyspec = RwKeyspec.path_from_xpath(schema, xpath, RwKeyspec.RwXpathType.KEYSPEC)
        self.assertTrue(keyspec)
        v_create_string =  keyspec.create_string(schema)
        self.assertEqual(v_create_string, xpath)
        xpath = "D,/taugb1:b1_c/taugb1:b1_c1/tauga1:a1_c1/tauga1:a1_f1"
        keyspec = RwKeyspec.path_from_xpath(schema, xpath, RwKeyspec.RwXpathType.KEYSPEC)
        self.assertTrue(keyspec)
        v_create_string =  keyspec.create_string(schema)
        self.assertEqual(v_create_string, xpath)

    def test_gi_get_pbcmd_from_xpath(self):
        xpath = "D,/rw-base:colony[rw-base:name='trafgen']"
        schema = RwFpathDYang.get_schema()
        pbcmd = RwKeyspec.get_pbcmd_from_xpath(xpath, schema)
        self.assertTrue(pbcmd)
        self.assertEqual(pbcmd, RwFpathDYang.ConfigColony().retrieve_descriptor())
        xpath = "/rw-base:colony[rw-base:name='trafgen']/rw-fpath:bundle-ether[rw-fpath:name='bundle1']"
        pbcmd = RwKeyspec.get_pbcmd_from_xpath(xpath, schema)
        self.assertTrue(pbcmd)
        self.assertEqual(pbcmd, RwFpathDYang.ConfigColony_BundleEther().retrieve_descriptor())
        #container
        xpath = "/rw-base:colony[rw-base:name='trafgen']/rw-fpath:bundle-ether[rw-fpath:name='bundle1']/rw-fpath:lacp"
        pbcmd = RwKeyspec.get_pbcmd_from_xpath(xpath, schema)
        self.assertTrue(pbcmd)
        self.assertEqual(pbcmd, RwFpathDYang.ConfigColony_BundleEther_Lacp().retrieve_descriptor())
        #leaf, should give error
        xpath = "/rw-base:colony[rw-base:name='trafgen']/rw-fpath:bundle-ether[rw-fpath:name='bundle1']/rw-fpath:open"
        pbcmd = RwKeyspec.get_pbcmd_from_xpath(xpath, schema)
        self.assertFalse(pbcmd);

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
