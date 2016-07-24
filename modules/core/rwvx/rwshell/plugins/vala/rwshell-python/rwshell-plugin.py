
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

from gi.repository import (
    GObject,
    RwTypes,
    RwShell)

import rw_status

import rwshell.perftools
import rwshell.crashtools

rwstatus = rw_status.rwstatus_from_exc_map({
                IndexError: RwTypes.RwStatus.NOTFOUND,
                KeyError: RwTypes.RwStatus.NOTFOUND,

                rwshell.perftools.UnlockedError: RwTypes.RwStatus.NOTCONNECTED,

                rwshell.crashtools.UnlockedError: RwTypes.RwStatus.NOTCONNECTED,
           })

class PerftoolsPlugin(GObject.Object, RwShell.Perftools):
    def __init__(self):
      GObject.Object.__init__(self)

    @rwstatus
    def do_start_perf(self, pids, frequency, perf_data_fname):
        #print ('PERF STARTING', pids, frequency, perf_data_fname)
        return rwshell.perftools.start_perf(pids, frequency, perf_data_fname)

    @rwstatus
    def do_stop_perf(self, pid):
        #print ('PERF STOPPING', pid)
        return rwshell.perftools.stop_perf(pid)


    @rwstatus
    def do_report_perf(self, percent_limit, perf_data):
        #print ('PERF REPORTING', percent_limit, perf_data)
        return rwshell.perftools.report_perf(percent_limit, perf_data)

class CrashtoolsPlugin(GObject.Object, RwShell.Crashtools):
    def __init__(self):
      GObject.Object.__init__(self)

    @rwstatus
    def do_report_crash(self, vm_name):
        #print ('CRASH REPORTING')
        return rwshell.crashtools.report_crash(vm_name)

def main():
    @rwstatus
    def blah():
        raise IndexError()

    a = blah()
    assert(a == RwTypes.RwStatus.NOTFOUND)

    @rwstatus({IndexError: RwTypes.RwStatus.NOTCONNECTED})
    def blah2():
        """Some function"""
        raise IndexError()

    a = blah2()
    assert(a == RwTypes.RwStatus.NOTCONNECTED)
    assert(blah2.__doc__ == "Some function")

if __name__ == '__main__':
    main()

