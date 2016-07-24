
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import collections
import logging

import gi
gi.require_version('RwTypes', '1.0')
gi.require_version('RwcalYang', '1.0')
gi.require_version('RwSdn', '1.0')
from gi.repository import (
    GObject,
    RwSdn, # Vala package
    RwTypes,
    RwTopologyYang as RwTl,
    RwsdnYang
    )

import rw_status
import rwlogger

logger = logging.getLogger('rwsdn.mock')


class UnknownAccountError(Exception):
    pass


class MissingFileError(Exception):
    pass


rwstatus = rw_status.rwstatus_from_exc_map({
    IndexError: RwTypes.RwStatus.NOTFOUND,
    KeyError: RwTypes.RwStatus.NOTFOUND,
    UnknownAccountError: RwTypes.RwStatus.NOTFOUND,
    MissingFileError: RwTypes.RwStatus.NOTFOUND,
    })

GRUNT118 = {"name": "grunt118", "ip_addr": "10.66.4.118", "tps": ["eth0"]}
GRUNT44 = {"name": "grunt44", "ip_addr": "10.66.4.44", "tps": ["eth0"]}
AS1 = {"name":"AristaSw1", "ip_addr": "10.66.4.54", "tps": ["Ethernet8/7","Ethernet8/8"]}
NW_NODES = [GRUNT118, GRUNT44, AS1]
NW_BIDIR_LINKS = [{"src" : ("grunt118","eth0"), "dest" : ("AristaSw1","Ethernet8/7")}, 
            {"src" : ("grunt44","eth0"), "dest" : ("AristaSw1","Ethernet8/8")}]


class DataStore(object):
    def __init__(self):
        self.topology = None
        self.nw = None
        self.next_mac = 11

    def create_link(self, cfg_src_node, cfg_src_tp, cfg_dest_node, cfg_dest_tp):
        lnk= self.nw.link.add()
        lnk.link_id = "urn:Rift:Lab:Ethernet:{}{}_{}{}".format(cfg_src_node, cfg_src_tp, cfg_dest_node, cfg_dest_tp)
        lnk.source.source_node = cfg_src_node
        lnk.source.source_tp = cfg_src_tp
        lnk.destination.dest_node = cfg_dest_node
        lnk.destination.dest_tp = cfg_dest_tp
        # L2 link augmentation
        lnk.l2_link_attributes.name = cfg_src_tp + cfg_dest_tp
        lnk.l2_link_attributes.rate = 1000000000.00

    def create_tp(self, node, cfg_tp):
        tp = node.termination_point.add()
        tp.tp_id = ("urn:Rift:Lab:{}:{}").format(node.node_id, cfg_tp)
        # L2 TP augmentation
        tp.l2_termination_point_attributes.description = cfg_tp
        tp.l2_termination_point_attributes.maximum_frame_size = 1500
        tp.l2_termination_point_attributes.mac_address = "00:1e:67:d8:48:" + str(self.next_mac)
        self.next_mac = self.next_mac + 1
        tp.l2_termination_point_attributes.tp_state = "in_use"
        tp.l2_termination_point_attributes.eth_encapsulation = "ethernet"

    def create_node(self, cfg_node):
        node = self.nw.node.add()
        node.node_id = cfg_node['name']
        # L2 Node augmentation
        node.l2_node_attributes.name = cfg_node['name']
        node.l2_node_attributes.description = "Host with OVS-DPDK"
        node.l2_node_attributes.management_address.append(cfg_node['ip_addr'])
        for cfg_tp in cfg_node['tps']:
            self.create_tp(node, cfg_tp)
        
    def create_default_topology(self):
        logger.debug('Creating default topology: ')

        self.topology = RwTl.YangData_IetfNetwork()
        self.nw = self.topology.network.add()
        self.nw.network_id = "L2HostTopology-Def1"
        self.nw.server_provided = 'true'

        # L2 Network type augmentation
        self.nw.network_types.l2_network = self.nw.network_types.l2_network.new()
        # L2 Network augmentation
        self.nw.l2_network_attributes.name = "Rift LAB SFC-Demo Host Network"

        for cfg_node in NW_NODES:
            self.create_node(cfg_node)

        for cfg_link in NW_BIDIR_LINKS:
            self.create_link(cfg_link['src'][0], cfg_link['src'][1], cfg_link['dest'][0], cfg_link['dest'][1])
            self.create_link(cfg_link['src'][1], cfg_link['src'][0], cfg_link['dest'][1], cfg_link['dest'][0])

        return self.topology
 
        
class Resources(object):
    def __init__(self):
        self.networks = dict()
        

class MockPlugin(GObject.Object, RwSdn.Topology):
    """This class implements the abstract methods in the Topology class.
    Mock is used for unit testing."""

    def __init__(self):
        GObject.Object.__init__(self)
        self.resources = collections.defaultdict(Resources)
        self.datastore = None

    @rwstatus
    def do_init(self, rwlog_ctx):
        if not any(isinstance(h, rwlogger.RwLogger) for h in logger.handlers):
            logger.addHandler(
                rwlogger.RwLogger(
                    subcategory="rwsdn.mock",
                    log_hdl=rwlog_ctx,
                )
            )

        account = RwsdnYang.SDNAccount()
        account.name = 'mock'
        account.account_type = 'mock'
        account.mock.username = 'rift'

        self.datastore = DataStore()
        self.topology = self.datastore.create_default_topology()
            
    @rwstatus(ret_on_failure=[None])
    def do_get_network_list(self, account):
        """
        Returns the list of discovered network

        @param account - a SDN account

        """
        logger.debug('Get network list: ')

        if (self.topology):
            logger.debug('Returning network list: ')
            return self.topology

        logger.debug('Returning empty network list: ')
        return None

        
