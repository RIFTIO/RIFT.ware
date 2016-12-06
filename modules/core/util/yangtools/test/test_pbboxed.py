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
# @file test_pbboxed.py
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
gi.require_version('ExampletestYang', '1.0')

from gi.repository import RwYang, GLib
from gi.repository import RwKeyspec
from gi.repository import ExampletestYang as ey
from gi.repository import ProtobufC

logger = logging.getLogger(__name__)

class TestFieldRefs(unittest.TestCase):
    def setUp(self):
        pass

    def test_pbgi(self):
        flat = ey.RwFlat()

        flat.inner_a = ey.RwFlat_InnerA()
        flat.inner_b = ey.RwFlat_InnerB()

        a_ref = flat.inner_a
        a_ref.a = "foo"
        b_ref = flat.inner_b
        b_ref.b = "bar"

        new_a = ey.RwFlat_InnerA()
        new_a.a = "car"
        new_b = ey.RwFlat_InnerB()
        new_b.b = "dog"

        flat.inner_a = new_a
        flat.inner_b = new_b
        exceptions = 0
        try:
            print("touch a_ref.a")
            a_ref.a = "asdf"
            print("touch b_ref.b")
            b_ref.b = "qwer"
        except:
            exceptions +=1
   
        self.assertEqual(exceptions, 1)
         
 

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
