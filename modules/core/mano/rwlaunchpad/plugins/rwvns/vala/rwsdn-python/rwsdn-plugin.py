
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import logging

import gi
gi.require_version('RwTypes', '1.0')
gi.require_version('RwSdn', '1.0')
from gi.repository import (
    GObject,
    RwSdn, # Vala package
    RwTypes)

import rw_status
import rwlogger

import rift.cal
import rift.sdn

logger = logging.getLogger('rwsdn')

rwstatus = rw_status.rwstatus_from_exc_map({
                IndexError: RwTypes.RwStatus.NOTFOUND,
                KeyError: RwTypes.RwStatus.NOTFOUND,

           })


class TopologyPlugin(GObject.Object, RwSdn.Topology):
    def __init__(self):
      GObject.Object.__init__(self)
      self._impl = None

    @rwstatus
    def do_init(self, rwlog_ctx):
        providers = {
            "sdnsim": rift.sdn.SdnSim,
            "mock": rift.sdn.Mock,
                }

        logger.addHandler(
            rwlogger.RwLogger(
                subcategory="rwsdn",
                log_hdl=rwlog_ctx,
            )
        )

        self._impl = {}
        for name, impl in providers.items():
            try:
                self._impl[name] = impl()

            except Exception:
                msg = "unable to load SDN implementation for {}"
                logger.exception(msg.format(name))

    @rwstatus
    def do_get_network_list(self, account, network_top):
        obj = self._impl[account.account_type]
        return obj.get_network_list(account, network_top)

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

