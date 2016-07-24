#!/usr/bin/env python3
"""
#
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
#
# @file lua_coroutine_pytest.py
# @author Anil Gunturu (anil.gunturu@riftio.com)
# @date 11/02/2014
#
"""

import argparse
import logging
import os
import sys
import unittest
from gi.repository import GObject, Peas, GLib, GIRepository
from gi.repository import LuaCoroutine


import xmlrunner

logger = logging.getLogger(__name__)

class TestLuaCoroutine(unittest.TestCase):
    def setUp(self):
   
        self.engine = Peas.Engine.get_default()
        # self.engine = Peas.Engine.new_with_nonglobal_loaders()
  
        # Load our plugin proxy into the g_irepository namespace
        default = GIRepository.Repository.get_default()
        GIRepository.Repository.require(default, "LuaCoroutine", "1.0", 0)

        # Enable every possible language loader
        self.engine.enable_loader("python3");
        self.engine.enable_loader("c");
        self.engine.enable_loader("lua5.1");

        # Set the search path for peas engine,
        # rift-shell sets the PLUGINDIR and GI_TYPELIB_PATH
        paths = set([])
        paths = paths.union(os.environ['PLUGINDIR'].split(":"))
        for path in paths:
            self.engine.add_search_path(path, path)
 
    def test_lua_coroutine_wrap(self):
         info = self.engine.get_plugin_info("lua_coroutine-lua")
         self.engine.load_plugin(info)
         extension = self.engine.create_extension(info, LuaCoroutine.Api, None)
         total, functor = extension.add_coroutine_wrap(10, 20)
         self.assertEqual(total, 10)
         total = functor.callback()
         self.assertEqual(total, 30)

    def test_lua_coroutine_create(self):
         info = self.engine.get_plugin_info("lua_coroutine-lua")
         self.engine.load_plugin(info)
         extension = self.engine.create_extension(info, LuaCoroutine.Api, None)
         total, functor = extension.add_coroutine_create(10, 20)
         self.assertEqual(total, 10)
         total = functor.callback()
         self.assertEqual(total, 30)

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
