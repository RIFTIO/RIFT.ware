#!/usr/bin/python

# STANDARD_RIFT_IO_COPYRIGHT

# -*- coding: utf-8 -*-
# ex:set ts=4 et sw=4 ai:

from gi.repository import GObject, AsyncCb, Peas, RwTypes
from _thread import start_new_thread

class ExtensionPythonPlugin(GObject.Object, Peas.Activatable,
                            AsyncCb.Api):
    object = GObject.property(type=GObject.Object)

    construct_only = GObject.property(type=str)
    read_only = GObject.property(type=str, default="read-only")
    write_only = GObject.property(type=str)
    readwrite = GObject.property(type=str, default="readwrite")
    prerequisite = GObject.property(type=str)

    print_done = {}

    def log_gobject_version(self):
        print("GObject version => " + str(GObject._version))
        print("PYGObject version => " + str(GObject.pygobject_version))

    def log_function(self, funcname):
        if not funcname in self.print_done:
            print ("=============================================")
            print ("This is a call to the python plugin " + funcname)
            self.log_gobject_version()
            print ("=============================================")
            self.print_done[funcname] = 1
    
    def do_activate(self):
        pass

    def do_deactivate(self):
        pass

    def do_update_state(self):
        pass

    def do_get_plugin_info(self):
        return self.plugin_info

    def do_get_settings(self):
        return self.plugin_info.get_settings(None)

    def handle_async_request(self, functor):
        functor.int_variable = 0xdeadbeef
        functor.string_variable = '0xbadbeef'
        functor.set_property('string_array', ['0xdeadbeef', '0xbadbeef'])
        functor.dispatch()

    def do_async_request(self, functor):
        # invoke the callback
        self.log_function("do_async_request")    
        args = (functor,)
        start_new_thread(self.handle_async_request, args) 
        return RwTypes.RwStatus.SUCCESS

if __name__ == "__main__":
    #add your test code to execute this as a standalone program
    foo = ExtensionPythonPlugin()
