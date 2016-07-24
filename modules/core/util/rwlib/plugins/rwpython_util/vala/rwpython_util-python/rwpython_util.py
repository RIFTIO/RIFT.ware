
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
import gi
gi.require_version('rwpython_util', '1.0')

from gi.repository import (
    GObject,
    Peas,
    rwpython_util,
    RwTypes)


class Plugin(GObject.Object, rwpython_util.Api):
    def __init__(self):
        GObject.Object.__init__(self)
        self._data_store = {}

    def do_python_run_string(self, python_util):
        status = RwTypes.RwStatus.SUCCESS
        try:
            exec(python_util, self._data_store)
        except Exception:
            status = RwTypes.RwStatus.FAILURE
        return status

    def do_python_eval_integer(self, python_variable):
        try:
            ret = eval(python_variable, self._data_store)
        except NameError:
            return (RwTypes.RwStatus.FAILURE, 0)
        return (RwTypes.RwStatus.SUCCESS, ret)

    def do_python_eval_string(self, python_variable):
        try:
            ret = eval(python_variable, self._data_store)
        except NameError:
            return (RwTypes.RwStatus.FAILURE, '')
        return (RwTypes.RwStatus.SUCCESS, ret)

# vim: et sw=4
