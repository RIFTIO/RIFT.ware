#!/usr/bin/python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

# -*- coding: utf-8 -*-
# ex:set ts=4 et sw=4 ai:

from gi.repository import GObject, BasicPlugin

# This class implements the BasicPlugin.Api interface. The name
# of this class doesn't matter. However, it must inherit from
# the interface defined by the GType. In this case, the
# name of the Api interface is BasicPlugin.Api. If there
# are multiple classes inheriting from the BasicPlugin.Api,
# the PYGOBJECT library will pick the first class.

# Also note that the methods of the interface 
# must be prefixed with "do_"
class BasicPluginPython(GObject.Object,
                        BasicPlugin.Api):
    def __init__(self):
        GObject.Object.__init__(self)

    def do_add(self, a, b):
        return a + b
        
if __name__ == "__main__":
    #add your test code to execute this as a standalone program
    foo = BasicPluginPython()
    print(foo.add(2, 3))
