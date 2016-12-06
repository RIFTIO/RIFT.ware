#!/usr/bin/python

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

# -*- coding: utf-8 -*-
# ex:set ts=4 et sw=4 ai:

from gi.repository import GObject, ValaGi, ExampleGi, GLib

# This class implements the ValaGi.Api interface. The name
# of this class doesn't matter. However, it must inherit from
# the interface defined by the GType. In this case, the
# name of the Api interface is ValaGi.Api. If there
# are multiple classes inheriting from the ValaGi.Api,
# the PYGOBJECT library will pick the first class.

# Also note that the methods of the interface 
# must be prefixed with "do_"
class ValaGiPlugin(GObject.Object,
                   ValaGi.Api):
    def __init__(self):
        GObject.Object.__init__(self)

    def do_add(self, a, b):
        return a + b

    def do_get_data_in_box(self, box):
        """Populate the box structure the caller will
        verify the contents.
        """
        result = 0;
        box.set_integer(4321)
        box.set_integer_array(list(range(10)))
        box.set_string_array(['hello', 'world'])
        return result

    def do_get_box(self):
        """Allocate the box and populate before
        returning. The caller will verify the contents.
        """
        result = 0 
        boxed = ExampleGi.Boxed()
        boxed.set_integer(1729)
        boxed.set_string_array(['hello', 'world'])
        return (result, boxed)
        
    def do_get_my_obj(self):
        """Populate the GObject caller will
        verify the contents.
        """
        result =  0
        obj = ValaGi.MyObj()
        obj.set_property('name', 'foo')
        obj.set_property('string_array', ['hello', 'world'])
        return (result, obj)

if __name__ == "__main__":
    #add your test code to execute this as a standalone program
    foo = ValaGiPlugin()
    print(foo.get_box())
