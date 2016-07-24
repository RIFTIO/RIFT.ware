#!/usr/bin/python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


from gi.repository import (
        GObject,
        PythonPlugin,)

class Plugin(GObject.Object, PythonPlugin.PythonValueStore):
    _class_values = {}

    def __init__(self):
        GObject.Object.__init__(self)
        self._instance_values = {}

    def do_get_class_value(self, key):
        try:
            return self._class_values[key]
        except KeyError:
            return ''

    def do_set_class_value(self, key, value):
        self._class_values[key] = value

    def do_get_instance_value(self, key):
        try:
            return self._instance_values[key]
        except KeyError:
            return ''

    def do_set_instance_value(self, key, value):
        self._instance_values[key] = value


# vim: ts=4 sw=4

