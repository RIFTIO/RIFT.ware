
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

from gi.repository import (
        GObject,
        rwcli_plugin)

import rift.pprint
import rift.rwcli_writer
import sys

class ExtensionPythonPlugin(GObject.Object, rwcli_plugin.Print):
    def __init__(self):
        GObject.Object.__init__(self)
        self._printer = rift.pprint.PrettyPrinter()
        sys.stdout.flush()

    def do_default_print(self, xml):
        self._printer.default_print(xml)
        sys.stdout.flush()

    def do_pretty_print(self, xml_str, pretty_print_routine):
        self._printer.pretty_print(xml_str, pretty_print_routine)
        sys.stdout.flush()

    def do_print_using_model(self,
            model,
            xml_str,
            pretty_print_routine,
            out_file):
        rift.rwcli_writer.pretty_print(model, xml_str, 
                            pretty_print_routine, out_file)
        sys.stdout.flush()

# vim: ts=4 et sw=4
