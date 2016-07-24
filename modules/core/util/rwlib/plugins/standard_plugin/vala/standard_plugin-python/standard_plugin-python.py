#!/usr/bin/python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

# -*- coding: utf-8 -*-
# ex:set ts=4 et sw=4 ai:

#need to export GI_TYPELIB_PATH and LD_LIBARARY_PATH 
#for this script to run standalone
#
# NOTE:
# When an interface method has in/out or out parameters
# the return for a method shoudl be of the form:
# return (retval, arg_a, ... arg_z)
# where arg_a .. arg_z are the in/out OR out parameters


from gi.repository import GObject, Rwplugin, StandardPlugin, Peas
import inspect

def caller_name(skip=2):
    """Get a name of a caller in the format module.class.method
    `skip` specifies how many levels of stack to skip while getting caller
    name. skip=1 means "who calls me", skip=2 "who calls my caller" etc.
    An empty string is returned if skipped levels exceed stack height
    """
    stack = inspect.stack()
    start = 0 + skip
    if len(stack) < start + 1:
        return ''
    parentframe = stack[start][0]
    name = []
    module = inspect.getmodule(parentframe)
    # `modname` can be None when frame is executed directly in console
    # TODO(techtonik): consider using __main__
    if module:
        name.append(module.__name__)
    # detect classname
    if 'self' in parentframe.f_locals:
        # I don't know any way to detect call from the object method
        # XXX: there seems to be no way to detect static method call - it will
        # be just a function call
        name.append(parentframe.f_locals['self'].__class__.__name__)
    codename = parentframe.f_code.co_name
    if codename != '<module>': # top level usually
        name.append( codename ) # function or a method
    del parentframe
    return ".".join(name)

class LogFunction():
    print_done = {}

    def log_function(self):
        caller = caller_name()
        if not caller in self.print_done:
            callstring = "This is a call to the python plugin " + caller
            print("=" * len(callstring))
            print(callstring)
            print("=" * len(callstring))
            self.print_done[caller] = 1
        pass

class ExtensionPythonPlugin(GObject.Object,
                            LogFunction,
                            Rwplugin.Api,
                            StandardPlugin.Api):
    object = GObject.property(type=GObject.Object)

    construct_only = GObject.property(type=str)
    read_only = GObject.property(type=str, default="read-only")
    write_only = GObject.property(type=str)
    readwrite = GObject.property(type=str, default="readwrite")
    prerequisite = GObject.property(type=str)

    _log = LogFunction()

    print_done = {}

    def log_function(self, funcname):
        if not funcname in self.print_done:
            print ("=============================================")
            print ("This is a call to the python plugin " + funcname)
            print ("=============================================")
            self.print_done[funcname] = 1
        pass

    def do_module_init(self, name, alloc_cfg):
        self._log.log_function()
        a = Module()
        a.set_name(name)
        a.set_alloc_cfg(alloc_cfg)
        return a

    def do_module_deinit(self, mod):
        self._log.log_function()
        del mod
        pass

    def do_module_instance_alloc(self, mod, name, alloc_cfg):
        self._log.log_function()
        a = ModuleInstance()
        a.set_name(name)
        a.set_alloc_cfg(alloc_cfg)
    	return a

    def do_module_instance_free(self, modinst):
        self._log.log_function()
        del modinst
        pass

    def do_module_instance_prelink_check(self, module_instance_handle, linkid, iface_provider, iface_mih, apitype, apistr):
        self._log.log_function()
        print(module_instance_handle)
        print(linkid)
        # As an example, invoke the add() method from the interface
        print(iface_provider.add(1,99))
        print(iface_mih)
        print(apitype)
        print(apistr)
        ret = True
        return ret

    def do_module_instance_prelink(self, module_instance_handle, linkid, iface_provider, iface_mih, apitype, apistr):
        self._log.log_function()
        pass

    def do_module_instance_postlink(self, module_instance_handle, linkid, iface_provider, iface_mih, apitype, apistr):
        self._log.log_function()
        pass

    def do_module_instance_preunlink(self, module_instance_handle, linkid, iface_provider, iface_mih, apitype, apistr):
        self._log.log_function()
        pass

    def do_module_instance_postunlink(self, module_instance_handle, linkid, iface_provider, iface_mih, apitype, apistr):
        self._log.log_function()
        pass


    def do_add(self, a, b):
        # Log the function entry
        self._log.log_function()
        return a + b

    def do_add3(self, a, b):
        # Log the function entry
        self._log.log_function()
        c = a + b
        return c

    def do_invoke_module(self, mod):
        # Log the function entry
        self._log.log_function()
        mod.invoke()
        pass

    def do_invoke_module_instance(self, modinst):
        # Log the function entry
        self._log.log_function()
        modinst.invoke()
        pass


class Module(GObject.Object):

    _name = ""
    _alloc_cfg = ""
    _log = LogFunction()

    def set_name(self, name):
        self._name = name
        pass

    def set_alloc_cfg(self, alloc_cfg):
        self._alloc_cfg = alloc_cfg
        pass

    def invoke(self):
        self._log.log_function()
        print("Module INVOKED!!!")
        pass


class ModuleInstance(GObject.Object):

    _name = ""
    _alloc_cfg = ""
    _log = LogFunction()

    def set_name(self, name):
        self._name = name
        pass

    def set_alloc_cfg(self, alloc_cfg):
        self._alloc_cfg = alloc_cfg
        pass

    def invoke(self):
        self._log.log_function()
        print("ModuleInstance " , self._name , " INVOKED!!!")
        pass
