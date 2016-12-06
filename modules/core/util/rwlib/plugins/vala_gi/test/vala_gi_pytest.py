#!/usr/bin/env python3
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
gi.require_version('Peas', '1.0')
gi.require_version('ValaGi', '1.0')

from gi.repository import GObject, Peas, GLib, GIRepository
from gi.repository import ValaGi, ExampleGi


import xmlrunner

logger = logging.getLogger(__name__)

class TestBasicPlugin(unittest.TestCase):
    def setUp(self):

        self.engine = Peas.Engine.get_default()

        # Load our plugin proxy into the g_irepository namespace
        default = GIRepository.Repository.get_default()
        GIRepository.Repository.require(default, "ExampleGi", "1.0", 0)
        GIRepository.Repository.require(default, "ValaGi", "1.0", 0)

        # Enable every possible language loader
        self.engine.enable_loader("python3");

        # Set the search path for peas engine,
        # rift-shell sets the PLUGINDIR and GI_TYPELIB_PATH
        paths = set([])
        paths = paths.union(os.environ['PLUGINDIR'].split(":"))
        for path in paths:
            self.engine.add_search_path(path, path)

    def test_basic_python_plugin(self):
        info = self.engine.get_plugin_info("vala_gi-python")
        self.engine.load_plugin(info)
        extension = self.engine.create_extension(info, ValaGi.Api, None)
        (result, box) = extension.get_box()
        self.assertEqual(box.get_integer(), 1729)
        array = box.get_string_array()
        self.assertEqual(array[0], 'hello')
        self.assertEqual(array[1], 'world')

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
