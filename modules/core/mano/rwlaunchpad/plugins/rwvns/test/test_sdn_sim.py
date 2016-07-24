
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import datetime
import logging
import unittest

import rw_peas
import rwlogger

import gi
gi.require_version('RwTypes', '1.0')
gi.require_version('RwSdn', '1.0')
from gi.repository import RwsdnYang
from gi.repository import IetfNetworkYang
from gi.repository.RwTypes import RwStatus
from gi.repository import RwSdn


logger = logging.getLogger('sdnsim')

def get_sdn_account():
    """
    Creates an object for class RwsdnYang.SdnAccount()
    """
    account                 = RwsdnYang.SDNAccount()
    account.account_type    = "sdnsim"
    account.sdnsim.username   = "rift"
    account.sdnsim.plugin_name = "rwsdn_sim"
    return account

def get_sdn_plugin():
    """
    Loads rw.sdn plugin via libpeas
    """
    plugin = rw_peas.PeasPlugin('rwsdn_sim', 'RwSdn-1.0')
    engine, info, extension = plugin()

    # Get the RwLogger context
    rwloggerctx = rwlogger.RwLog.Ctx.new("SDN-Log")

    sdn = plugin.get_interface("Topology")
    try:
        rc = sdn.init(rwloggerctx)
        assert rc == RwStatus.SUCCESS
    except:
        logger.error("ERROR:SDN sim plugin instantiation failed. Aborting tests")
    else:
        logger.info("SDN sim plugin successfully instantiated")
    return sdn



class SdnSimTest(unittest.TestCase):
    def setUp(self):
        """
          Initialize test plugins
        """
        self._acct = get_sdn_account()
        logger.info("SDN-Sim-Test: setUp")
        self.sdn   = get_sdn_plugin()
        logger.info("SDN-Sim-Test: setUpEND")

    def tearDown(self):
        logger.info("SDN-Sim-Test: Done with tests")

    def test_get_network_list(self):
        """
           First test case
        """
        rc, nwtop = self.sdn.get_network_list(self._acct)
        self.assertEqual(rc, RwStatus.SUCCESS) 
        logger.debug("SDN-Sim-Test: Retrieved network attributes ")
        for nw in nwtop.network:
           logger.debug("...Network id %s", nw.network_id)
           logger.debug("...Network name %s", nw.l2_network_attributes.name)


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    unittest.main()




