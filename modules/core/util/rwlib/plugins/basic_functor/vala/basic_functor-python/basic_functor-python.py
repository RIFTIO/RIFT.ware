#!/usr/bin/python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

# -*- coding: utf-8 -*-
# ex:set ts=4 et sw=4 ai:

from gi.repository import GObject, BasicFunctor

# This class implements the BasicFunctor.Api interface. The name
# of this class doesn't matter. However, it must inherit from
# the interface defined by the GType. In this case, the
# name of the Api interface is BasicFunctor.Api. If there
# are multiple classes inheriting from the BasicFunctor.Api,
# the PYGOBJECT library will pick the first class.

# Also note that the methods of the interface 
# must be prefixed with "do_"
class BasicFunctorPythonPlugin(GObject.Object,
                               BasicFunctor.Api):
    def __init__(self):
        GObject.Object.__init__(self)

    def do_callback_example(self, functor):
        functor.callback()
        return 0
        
if __name__ == "__main__":
    #add your test code to execute this as a standalone program
    foo = BasicFunctorPythonPlugin()
    print(foo.add(2, 3))
