
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import datetime
import logging
import unittest

import rwlogger

# from gi.repository import IetfNetworkYang
from gi.repository import IetfL2TopologyYang as l2Tl
from gi.repository import RwTopologyYang as RwTl
# from gi.repository.RwTypes import RwStatus

from create_stackedl2topology import MyL2Topology

from rift.topmgr import (
    NwtopDataStore,
)
logger = logging.getLogger('sdntop')

NUM_NWS = 1
NUM_NODES_L2_NW = 6
NUM_TPS_L2_NW = 20
NUM_LINKS = 20

class SdnTopStoreNetworkTest(unittest.TestCase):
    def setUp(self):
        """
          Initialize Top data store
        """
        self._nwtopdata_store = NwtopDataStore(logger)
        self.test_nwtop = RwTl.YangData_IetfNetwork()

        self.l2top = MyL2Topology(self.test_nwtop, logger)
        self.l2top.setup_all()

        # Get initial test data
        self.l2net1 = self.l2top.find_nw("L2HostNetwork-1")
        # Create initial nw
        self._nwtopdata_store.create_network("L2HostNetwork-1", self.l2net1)

        # Add test data
        self.l2net1 = self.l2top.find_nw("L2HostNetwork-1")
        assert self.l2net1 is not None
        self.new_l2net = RwTl.YangData_IetfNetwork_Network()
        self.new_l2net.network_id = "L2HostNetwork-2"
        logger.info("SdnTopStoreNetworkTest: setUp")

    def tearDown(self):
        self.l2net1 = None
        self.new_l2net = None
        logger.info("SdnTopStoreNetworkTest: Done with tests")

    def test_create_network(self):
        """
           Test: Create first l2 network
        """
        num_nodes = 0
        num_tps = 0
        logger.debug("SdnTopStoreNetworkTest: Create network ")
        # Get test data
        # Created durign setup phase
        assert self.l2net1 is not None
        # Use data store APIs
        # Network already stored
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        logger.debug("...Network id %s", nw.network_id)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        logger.debug("...Network name %s", nw.l2_network_attributes.name)
        for node in nw.node:
            logger.debug("...Node id %s", node.node_id)
            num_nodes += 1
            for tp in node.termination_point:
                logger.debug("...Tp id %s", tp.tp_id)
                num_tps += 1
        self.assertEqual(num_nodes, NUM_NODES_L2_NW)
        self.assertEqual(num_tps, NUM_TPS_L2_NW)


    def test_add_network(self):
        """
           Test: Add another network, Check network id
        """
        logger.debug("SdnTopStoreNetworkTest: Add network ")
        # Use data store APIs
        self._nwtopdata_store.create_network("L2HostNetwork-2", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-2")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-2")
        self.assertEqual(len(self._nwtopdata_store._networks), 2)

    def test_add_networktype(self):
        """
           Test: Add another network, Check network type
        """
        logger.debug("SdnTopStoreTest: Add network type ")
        # Use data store APIs
        self._nwtopdata_store.create_network("L2HostNetwork-2", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-2")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-2")
        self.assertEqual(len(self._nwtopdata_store._networks), 2)
        # Add new test data
        self.new_l2net.network_types.l2_network = self.new_l2net.network_types.l2_network.new()
        logger.debug("Adding update l2net..%s", self.new_l2net)
        # Use data store APIs
        self._nwtopdata_store.update_network("L2HostNetwork-2", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-2")
        self.assertIsNotNone(nw.network_types.l2_network)

    def test_add_networkl2name(self):
        """
           Test: Add another network, Check L2 network name
        """
        logger.debug("SdnTopStoreTest: Add L2 network name ")
        # Use data store APIs
        self.new_l2net.network_types.l2_network = self.new_l2net.network_types.l2_network.new()
        self._nwtopdata_store.create_network("L2HostNetwork-2", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-2")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-2")
        self.assertEqual(len(self._nwtopdata_store._networks), 2)
        # Add new test data
        self.new_l2net.l2_network_attributes.name = "L2networkName"
        logger.debug("Adding update l2net..%s", self.new_l2net)
        # Use data store APIs
        self._nwtopdata_store.update_network("L2HostNetwork-2", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-2")
        self.assertEqual(nw.l2_network_attributes.name, "L2networkName")


class SdnTopStoreNetworkNodeTest(unittest.TestCase):
    def setUp(self):
        """
          Initialize Top data store
        """
        self._nwtopdata_store = NwtopDataStore(logger)
        self.test_nwtop = RwTl.YangData_IetfNetwork()

        self.l2top = MyL2Topology(self.test_nwtop, logger)
        self.l2top.setup_all()

        # Get initial test data
        self.l2net1 = self.l2top.find_nw("L2HostNetwork-1")
        # Create initial nw
        self._nwtopdata_store.create_network("L2HostNetwork-1", self.l2net1)
        # Get test data
        self.l2net1 = self.l2top.find_nw("L2HostNetwork-1")
        assert self.l2net1 is not None
        self.new_l2net = RwTl.YangData_IetfNetwork_Network()
        self.new_l2net.network_id = "L2HostNetwork-1"
        self.node2 = self.new_l2net.node.add()
        self.node2.node_id = "TempNode2"
        logger.info("SdnTopStoreTest: setUp NetworkNodetest")

    def tearDown(self):
        logger.info("SdnTopStoreTest: Done with  NetworkNodetest")


    def test_add_network_node(self):
        """
           Test: Add a node to existing network
                 Test all parameters
        """
        num_nodes = 0
        num_tps = 0
        logger.debug("SdnTopStoreTest: Add network node")
        # Add test data
        self.node2.node_id = "TempNode2"
        # Use data store APIs
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(len(self._nwtopdata_store._networks), 1)
        self.assertEqual(len(nw.node), NUM_NODES_L2_NW + 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].node_id, "TempNode2")

    #@unittest.skip("Skipping")
    def test_update_network_node(self):
        """
           Test: Updat a node to existing network
        """
        num_nodes = 0
        num_tps = 0
        logger.debug("SdnTopStoreTest: Update network node")
        # Add test data
        self.node2.node_id = "TempNode2"
        self.node2.l2_node_attributes.description = "TempNode2 desc"
        self.node2.l2_node_attributes.name = "Nice Name2"
        # Use data store APIs
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(len(self._nwtopdata_store._networks), 1)
        self.assertEqual(len(nw.node), NUM_NODES_L2_NW + 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].node_id, "TempNode2")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].l2_node_attributes.description, "TempNode2 desc")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].l2_node_attributes.name, "Nice Name2")

    #@unittest.skip("Skipping")
    def test_update_network_node_l2attr1(self):
        """
           Test: Update a node to existing network
        """
        num_nodes = 0
        num_tps = 0
        logger.debug("SdnTopStoreTest: Update network node")
        # Add test data
        self.node2.node_id = "TempNode2"
        self.node2.l2_node_attributes.description = "TempNode2 desc"
        self.node2.l2_node_attributes.name = "Nice Name3"
        # Use data store APIs
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(len(self._nwtopdata_store._networks), 1)
        self.assertEqual(len(nw.node), NUM_NODES_L2_NW + 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].node_id, "TempNode2")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].l2_node_attributes.description, "TempNode2 desc")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].l2_node_attributes.name, "Nice Name3")

        # Add test data
        self.node2.l2_node_attributes.name = "Nice Name4"
        logger.debug("Network %s", self.new_l2net)
        # Use data store APIs
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        logger.debug("Node %s", nw.node[NUM_NODES_L2_NW])
        self.assertEqual(nw.node[NUM_NODES_L2_NW].l2_node_attributes.name, "Nice Name4")

    def test_update_network_node_l2attr2(self):
        """
           Test: Updat a node to existing network
        """
        num_nodes = 0
        num_tps = 0
        logger.debug("SdnTopStoreTest: Update network node")
        # Add test data
        self.node2.node_id = "TempNode2"
        self.node2.l2_node_attributes.description = "TempNode2 desc"
        self.node2.l2_node_attributes.name = "Nice Name3"
        # Use data store APIs
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(len(self._nwtopdata_store._networks), 1)
        self.assertEqual(len(nw.node), NUM_NODES_L2_NW + 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].node_id, "TempNode2")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].l2_node_attributes.description, "TempNode2 desc")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].l2_node_attributes.name, "Nice Name3")

        # Add test data
        self.node2.l2_node_attributes.management_address.append("10.0.0.1")
        logger.debug("Network %s", self.new_l2net)
        # Use data store APIs
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].l2_node_attributes.name, "Nice Name3")
        self.assertEqual(len(nw.node[NUM_NODES_L2_NW].l2_node_attributes.management_address), 1)

        # Add test data
        self.node2.l2_node_attributes.management_address.append("10.0.0.2")
        logger.debug("Network %s", self.new_l2net)
        # Use data store APIs
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].l2_node_attributes.name, "Nice Name3")
        self.assertEqual(len(nw.node[NUM_NODES_L2_NW].l2_node_attributes.management_address), 2)


class SdnTopStoreNetworkNodeTpTest(unittest.TestCase):
    def setUp(self):
        """
          Initialize Top data store
        """
        self._nwtopdata_store = NwtopDataStore(logger)
        self.test_nwtop = RwTl.YangData_IetfNetwork()

        self.l2top = MyL2Topology(self.test_nwtop, logger)
        self.l2top.setup_all()

        # Get initial test data
        self.l2net1 = self.l2top.find_nw("L2HostNetwork-1")
        # Create initial nw
        self._nwtopdata_store.create_network("L2HostNetwork-1", self.l2net1)
        # Get test data
        self.l2net1 = self.l2top.find_nw("L2HostNetwork-1")
        assert self.l2net1 is not None
        self.new_l2net = RwTl.YangData_IetfNetwork_Network()
        self.new_l2net.network_id = "L2HostNetwork-1"
        self.node2 = self.new_l2net.node.add()
        self.node2.node_id = "TempNode2"
        self.tp1 = self.node2.termination_point.add()
        self.tp1.tp_id = "TempTp1"
        logger.info("SdnTopStoreTest: setUp NetworkNodeTptest")

    def tearDown(self):
        logger.info("SdnTopStoreTest: Done with  NetworkNodeTptest")
        
        self.new_l2net = None
        self.node2 = None
        self.tp1 = None

    def test_add_network_node_tp(self):
        """
           Test: Add a node to existing network
        """
        num_nodes = 0
        num_tps = 0
        logger.debug("SdnTopStoreTest: Update network ")
        # Use data store APIs
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(len(self._nwtopdata_store._networks), 1)
        self.assertEqual(len(nw.node), NUM_NODES_L2_NW + 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].node_id, "TempNode2")
        self.assertEqual(len(nw.node[NUM_NODES_L2_NW].termination_point), 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].tp_id, "TempTp1")

    def test_update_network_node_tp(self):
        """
           Test: Update a tp to existing network, add all tp elements
        """
        num_nodes = 0
        num_tps = 0
        logger.debug("SdnTopStoreTest: Update network ")
        self.tp1.tp_id = "TempTp1"
        self.tp1.l2_termination_point_attributes.description = "TempTp1 Desc"
        self.tp1.l2_termination_point_attributes.maximum_frame_size = 1296
        self.tp1.l2_termination_point_attributes.mac_address = "00:1e:67:98:28:01"
        self.tp1.l2_termination_point_attributes.eth_encapsulation = "l2t:ethernet"
        # Use data store APIs
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(len(self._nwtopdata_store._networks), 1)
        self.assertEqual(len(nw.node), NUM_NODES_L2_NW + 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].node_id, "TempNode2")
        self.assertEqual(len(nw.node[NUM_NODES_L2_NW].termination_point), 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].tp_id, "TempTp1")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].l2_termination_point_attributes.description, "TempTp1 Desc")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].l2_termination_point_attributes.maximum_frame_size, 1296)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].l2_termination_point_attributes.mac_address, "00:1e:67:98:28:01")

    def test_update_network_node_tp2(self):
        """
           Test: Update a tp to existing network, change tp elements
        """
        num_nodes = 0
        num_tps = 0
        logger.debug("SdnTopStoreTest: Update network ")
        self.tp1.tp_id = "TempTp1"
        self.tp1.l2_termination_point_attributes.description = "TempTp1 Desc"
        self.tp1.l2_termination_point_attributes.maximum_frame_size = 1296
        self.tp1.l2_termination_point_attributes.mac_address = "00:1e:67:98:28:01"
        # Use data store APIs
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(len(self._nwtopdata_store._networks), 1)
        self.assertEqual(len(nw.node), NUM_NODES_L2_NW + 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].node_id, "TempNode2")
        self.assertEqual(len(nw.node[NUM_NODES_L2_NW].termination_point), 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].tp_id, "TempTp1")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].l2_termination_point_attributes.description, "TempTp1 Desc")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].l2_termination_point_attributes.maximum_frame_size, 1296)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].l2_termination_point_attributes.mac_address, "00:1e:67:98:28:01")

        # Change frame size
        self.tp1.l2_termination_point_attributes.maximum_frame_size = 1396
        # Use data store APIs
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(len(self._nwtopdata_store._networks), 1)
        self.assertEqual(len(nw.node), NUM_NODES_L2_NW + 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].node_id, "TempNode2")
        self.assertEqual(len(nw.node[NUM_NODES_L2_NW].termination_point), 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].tp_id, "TempTp1")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].l2_termination_point_attributes.description, "TempTp1 Desc")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].l2_termination_point_attributes.maximum_frame_size, 1396)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].l2_termination_point_attributes.mac_address, "00:1e:67:98:28:01")

        # Change MAC address
        self.tp1.l2_termination_point_attributes.mac_address = "00:1e:67:98:28:02"
        # Use data store APIs
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(len(self._nwtopdata_store._networks), 1)
        self.assertEqual(len(nw.node), NUM_NODES_L2_NW + 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].node_id, "TempNode2")
        self.assertEqual(len(nw.node[NUM_NODES_L2_NW].termination_point), 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].tp_id, "TempTp1")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].l2_termination_point_attributes.description, "TempTp1 Desc")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].l2_termination_point_attributes.maximum_frame_size, 1396)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].l2_termination_point_attributes.mac_address, "00:1e:67:98:28:02")

        # Add encapsulation type
        self.tp1.l2_termination_point_attributes.eth_encapsulation = "l2t:ethernet"
        # Use data store APIs
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(len(self._nwtopdata_store._networks), 1)
        self.assertEqual(len(nw.node), NUM_NODES_L2_NW + 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].node_id, "TempNode2")
        self.assertEqual(len(nw.node[NUM_NODES_L2_NW].termination_point), 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].tp_id, "TempTp1")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].l2_termination_point_attributes.description, "TempTp1 Desc")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].l2_termination_point_attributes.maximum_frame_size, 1396)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].l2_termination_point_attributes.mac_address, "00:1e:67:98:28:02")
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].l2_termination_point_attributes.eth_encapsulation, "l2t:ethernet")

    def test_update_extra_network_node_tp2(self):
        """
           Test: Update a tp to existing network, change tp elements
        """
        num_nodes = 0
        num_tps = 0
        logger.debug("SdnTopStoreTest: Update network ")
        self.tp2 = self.node2.termination_point.add()
        self.tp2.tp_id = "TempTp2"
        # Use data store APIs
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(len(self._nwtopdata_store._networks), 1)
        self.assertEqual(len(nw.node), NUM_NODES_L2_NW + 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].node_id, "TempNode2")
        self.assertEqual(len(nw.node[NUM_NODES_L2_NW].termination_point), 2)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[1].tp_id, "TempTp2")



class SdnTopStoreNetworkLinkTest(unittest.TestCase):
    def setUp(self):
        """
          Initialize Top data store
        """
        self._nwtopdata_store = NwtopDataStore(logger)
        self.test_nwtop = RwTl.YangData_IetfNetwork()

        self.l2top = MyL2Topology(self.test_nwtop, logger)
        self.l2top.setup_all()

        # Get initial test data
        self.l2net1 = self.l2top.find_nw("L2HostNetwork-1")
        # Create initial nw
        self._nwtopdata_store.create_network("L2HostNetwork-1", self.l2net1)
        # Get test data
        self.l2net1 = self.l2top.find_nw("L2HostNetwork-1")
        assert self.l2net1 is not None
        self.new_l2net = RwTl.YangData_IetfNetwork_Network()
        self.new_l2net.network_id = "L2HostNetwork-1"

        self.src_node = self.new_l2net.node.add()
        self.src_node.node_id = "TempNode1"
        self.tp1 = self.src_node.termination_point.add()
        self.tp1.tp_id = "TempTp1"

        self.dest_node = self.new_l2net.node.add()
        self.dest_node.node_id = "TempNode2"
        self.tp2 = self.dest_node.termination_point.add()
        self.tp2.tp_id = "TempTp2"
        logger.info("SdnTopStoreTest: setUp NetworkLinkTest")

    def tearDown(self):
        logger.info("SdnTopStoreTest: Done with  NetworkLinkTest")
        
        self.new_l2net = None
        self.src_node = None
        self.tp1 = None
        self.dest_node = None
        self.tp2 = None

    def test_add_network_link(self):
        """
           Test: Add a link to existing network
        """
        logger.info("SdnTopStoreTest: Update network link")
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify data created
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(len(self._nwtopdata_store._networks), 1)
        self.assertEqual(len(nw.node), NUM_NODES_L2_NW + 2)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].node_id, "TempNode1")
        self.assertEqual(len(nw.node[NUM_NODES_L2_NW].termination_point), 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].tp_id, "TempTp1")
        self.assertEqual(nw.node[NUM_NODES_L2_NW+1].node_id, "TempNode2")
        self.assertEqual(nw.node[NUM_NODES_L2_NW+1].termination_point[0].tp_id, "TempTp2")
        self.assertEqual(len(nw.link), NUM_LINKS )
        self.link1 = self.new_l2net.link.add()
        self.link1.link_id = "Link1"
        self.link1.source.source_node = self.src_node.node_id
        self.link1.source.source_tp = self.tp1.tp_id
        self.link1.destination.dest_node = self.dest_node.node_id
        self.link1.destination.dest_tp = self.tp2.tp_id
        # Use data store APIs
        logger.info("SdnTopStoreTest: Update network link - Part 2")
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        # Verify data created
        self.assertEqual(nw.link[NUM_LINKS].link_id, "Link1")
        self.assertEqual(nw.link[NUM_LINKS].source.source_node, self.src_node.node_id)
        self.assertEqual(nw.link[NUM_LINKS].source.source_tp, self.tp1.tp_id)
        self.assertEqual(nw.link[NUM_LINKS].destination.dest_node, self.dest_node.node_id)
        self.assertEqual(nw.link[NUM_LINKS].destination.dest_tp, self.tp2.tp_id)
        self.assertEqual(len(nw.link), NUM_LINKS + 1)

    def test_add_extra_network_link(self):
        """
           Test: Add a link to existing network
        """
        logger.info("SdnTopStoreTest: Update extra network link")
        # Create initial state
        self.link1 = self.new_l2net.link.add()
        self.link1.link_id = "Link1"
        self.link1.source.source_node = self.src_node.node_id
        self.link1.source.source_tp = self.tp1.tp_id
        self.link1.destination.dest_node = self.dest_node.node_id
        self.link1.destination.dest_tp = self.tp2.tp_id
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify initial state
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(len(self._nwtopdata_store._networks), 1)
        self.assertEqual(len(nw.node), NUM_NODES_L2_NW + 2)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].node_id, "TempNode1")
        self.assertEqual(len(nw.node[NUM_NODES_L2_NW].termination_point), 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].tp_id, "TempTp1")
        self.assertEqual(nw.node[NUM_NODES_L2_NW+1].node_id, "TempNode2")
        self.assertEqual(nw.node[NUM_NODES_L2_NW+1].termination_point[0].tp_id, "TempTp2")
        self.assertEqual(nw.link[NUM_LINKS].link_id, "Link1")
        self.assertEqual(len(nw.link), NUM_LINKS  + 1)

        # Add extra link (reverse)
        self.link2 = self.new_l2net.link.add()
        self.link2.link_id = "Link2"
        self.link2.source.source_node = self.dest_node.node_id
        self.link2.source.source_tp = self.tp2.tp_id
        self.link2.destination.dest_node = self.src_node.node_id
        self.link2.destination.dest_tp = self.tp1.tp_id
        # Use data store APIs
        logger.info("SdnTopStoreTest: Update extra network link - Part 2")
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        # Verify data created
        self.assertEqual(nw.link[NUM_LINKS+1].link_id, "Link2")
        self.assertEqual(len(nw.link), NUM_LINKS + 2)
        self.assertEqual(nw.link[NUM_LINKS+1].source.source_node, self.dest_node.node_id)
        self.assertEqual(nw.link[NUM_LINKS+1].source.source_tp, self.tp2.tp_id)
        self.assertEqual(nw.link[NUM_LINKS+1].destination.dest_node, self.src_node.node_id)
        self.assertEqual(nw.link[NUM_LINKS+1].destination.dest_tp, self.tp1.tp_id)

    def test_add_network_link_l2attr(self):
        """
           Test: Check L2 link attributes
        """
        logger.info("SdnTopStoreTest: Add network link L2 attributes")
        # Create test state
        self.link1 = self.new_l2net.link.add()
        self.link1.link_id = "Link1"
        self.link1.source.source_node = self.src_node.node_id
        self.link1.source.source_tp = self.tp1.tp_id
        self.link1.destination.dest_node = self.dest_node.node_id
        self.link1.destination.dest_tp = self.tp2.tp_id
        self.link1.l2_link_attributes.name = "Link L2 name"
        self.link1.l2_link_attributes.rate = 10000
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify data state
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(len(self._nwtopdata_store._networks), 1)
        self.assertEqual(len(nw.node), NUM_NODES_L2_NW + 2)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].node_id, "TempNode1")
        self.assertEqual(len(nw.node[NUM_NODES_L2_NW].termination_point), 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].tp_id, "TempTp1")
        self.assertEqual(nw.node[NUM_NODES_L2_NW+1].node_id, "TempNode2")
        self.assertEqual(nw.node[NUM_NODES_L2_NW+1].termination_point[0].tp_id, "TempTp2")
        self.assertEqual(nw.link[NUM_LINKS].link_id, "Link1")
        self.assertEqual(len(nw.link), NUM_LINKS  + 1)
        self.assertEqual(nw.link[NUM_LINKS].l2_link_attributes.name, "Link L2 name")
        self.assertEqual(nw.link[NUM_LINKS].l2_link_attributes.rate, 10000)

    def test_change_network_link_l2attr(self):
        """
           Test: Change L2 link attributes
        """
        logger.info("SdnTopStoreTest: Change network link L2 attributes")
        # Create initial state
        self.link1 = self.new_l2net.link.add()
        self.link1.link_id = "Link1"
        self.link1.source.source_node = self.src_node.node_id
        self.link1.source.source_tp = self.tp1.tp_id
        self.link1.destination.dest_node = self.dest_node.node_id
        self.link1.destination.dest_tp = self.tp2.tp_id
        self.link1.l2_link_attributes.name = "Link L2 name"
        self.link1.l2_link_attributes.rate = 10000
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify initial state
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(len(self._nwtopdata_store._networks), 1)
        self.assertEqual(len(nw.node), NUM_NODES_L2_NW + 2)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].node_id, "TempNode1")
        self.assertEqual(len(nw.node[NUM_NODES_L2_NW].termination_point), 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].tp_id, "TempTp1")
        self.assertEqual(nw.node[NUM_NODES_L2_NW+1].node_id, "TempNode2")
        self.assertEqual(nw.node[NUM_NODES_L2_NW+1].termination_point[0].tp_id, "TempTp2")
        self.assertEqual(nw.link[NUM_LINKS].link_id, "Link1")
        self.assertEqual(len(nw.link), NUM_LINKS  + 1)
        self.assertEqual(nw.link[NUM_LINKS].l2_link_attributes.name, "Link L2 name")
        self.assertEqual(nw.link[NUM_LINKS].l2_link_attributes.rate, 10000)

        # Create initial state
        self.test_l2net = RwTl.YangData_IetfNetwork_Network()
        self.test_l2net.network_id = "L2HostNetwork-1"
        self.link1 = self.test_l2net.link.add()
        self.link1.link_id = "Link1"
        self.link1.l2_link_attributes.name = "Link L2 updated name"
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.test_l2net)
        # Verify test state
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(nw.link[NUM_LINKS].l2_link_attributes.name, "Link L2 updated name")

    def test_change_network_link_dest_tp(self):
        """
           Test: Change L2 link attributes
        """
        logger.info("SdnTopStoreTest: Change network link dest-tp")
        # Create initial state
        self.link1 = self.new_l2net.link.add()
        self.link1.link_id = "Link1"
        self.link1.source.source_node = self.src_node.node_id
        self.link1.source.source_tp = self.tp1.tp_id
        self.link1.destination.dest_node = self.dest_node.node_id
        self.link1.destination.dest_tp = self.tp2.tp_id
        self.link1.l2_link_attributes.name = "Link L2 name"
        self.link1.l2_link_attributes.rate = 10000
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.new_l2net)
        # Verify initial state
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertIsNotNone(nw)
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(len(self._nwtopdata_store._networks), 1)
        self.assertEqual(len(nw.node), NUM_NODES_L2_NW + 2)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].node_id, "TempNode1")
        self.assertEqual(len(nw.node[NUM_NODES_L2_NW].termination_point), 1)
        self.assertEqual(nw.node[NUM_NODES_L2_NW].termination_point[0].tp_id, "TempTp1")
        self.assertEqual(nw.node[NUM_NODES_L2_NW+1].node_id, "TempNode2")
        self.assertEqual(nw.node[NUM_NODES_L2_NW+1].termination_point[0].tp_id, "TempTp2")
        self.assertEqual(nw.link[NUM_LINKS].link_id, "Link1")
        self.assertEqual(len(nw.link), NUM_LINKS  + 1)
        self.assertEqual(nw.link[NUM_LINKS].l2_link_attributes.name, "Link L2 name")
        self.assertEqual(nw.link[NUM_LINKS].l2_link_attributes.rate, 10000)

        # Create test state
        self.test_l2net = RwTl.YangData_IetfNetwork_Network()
        self.test_l2net.network_id = "L2HostNetwork-1"
        self.link1 = self.test_l2net.link.add()
        self.link1.link_id = "Link1"
        # Changing dest node params
        self.link1.destination.dest_node = self.src_node.node_id
        self.link1.destination.dest_tp = self.tp1.tp_id
        self._nwtopdata_store.update_network("L2HostNetwork-1", self.test_l2net)
        # Verify test state
        nw = self._nwtopdata_store.get_network("L2HostNetwork-1")
        self.assertEqual(nw.network_id, "L2HostNetwork-1")
        self.assertEqual(nw.link[NUM_LINKS].destination.dest_node,  self.src_node.node_id)




if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    unittest.main()




