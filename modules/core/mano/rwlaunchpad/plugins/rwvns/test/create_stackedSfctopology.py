#!/bin/python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import gi
gi.require_version('RwYang', '1.0')
from gi.repository import IetfL2TopologyYang as l2Tl
from gi.repository import RwTopologyYang as RwTl
from gi.repository import RwYang
from xml.etree import ElementTree as etree
import subprocess
import logging

from create_stackedl2topology import MyL2Network
from create_stackedl2topology import MyL2Topology
from create_stackedProvNettopology import MyProvNetwork
from create_stackedProvNettopology import MyProvTopology
from create_stackedVMNettopology import MyVMNetwork
from create_stackedVMNettopology import MyVMTopology


class MyNwNotFound(Exception):
    pass

class MyNodeNotFound(Exception):
    pass

class MyTpNotFound(Exception):
    pass

class MySfcNetwork(object):
    def __init__(self, nwtop, l2top, provtop, vmtop, log):
        self.next_mac = 81
        self.log = log
        self.sfcnet1 = nwtop.network.add()
        self.sfcnet1.network_id = "SfcNetwork-1"

        self.l2top = l2top
        self.provtop = provtop
        self.vmtop = vmtop

        # L2 Network type augmentation
        self.sfcnet1.network_types.l2_network = self.sfcnet1.network_types.l2_network.new()
        # L2 Network augmentation
        self.sfcnet1.l2_network_attributes.name = "Rift LAB SFC-Demo SFC Network"
        try:
           self.l2netid = l2top.find_nw_id("L2HostNetwork-1")
        except TypeError:
           raise MyNwNotFound()
        ul_net = self.sfcnet1.supporting_network.add()
        try:
           ul_net.network_ref = provtop.find_nw_id("ProviderNetwork-1")
           self.provnetid = ul_net.network_ref
        except TypeError:
           raise MyNwNotFound()
        ul_net = self.sfcnet1.supporting_network.add()
        try:
           ul_net.network_ref = vmtop.find_nw_id("VmNetwork-1")
           self.vmnetid = ul_net.network_ref
        except TypeError:
           raise MyNwNotFound()

    def get_nw_id(self, nw_name):
        for nw in self.nwtop.network:
            if (nw.network_id == nw_name):
                return nw.network_id

    def get_node(self, node_name):
        _node_id = "urn:Rift:Lab:" + node_name
        for node in self.sfcnet1.node:
            if (node.node_id == _node_id):
                return node

    def get_tp(self, node, tp_name):
        _tp_id = "urn:Rift:Lab:" + node.node_id + "_" + tp_name
        for tp in node.termination_point :
            if (tp.tp_id == _tp_id):
                return tp

    def get_link(self, link_name):
        for link in nw.link :
            if (link.l2_link_attributes.name == link_name):
                return link

    def create_node(self, node_name, description, mgmt_ip_addr = None, sup_node = None, nw_ref = None):
        logging.debug("Creating node %s", node_name)
        node = self.sfcnet1.node.add()
        node.node_id = "urn:Rift:Lab:" + node_name
        # L2 Node augmentation
        node.l2_node_attributes.name = node_name
        node.l2_node_attributes.description = description
        if (mgmt_ip_addr is not None):
            node.l2_node_attributes.management_address.append(mgmt_ip_addr)
        if (sup_node is not None):
            logging.debug("  Adding support node %s", sup_node.node_id)
            ul_node = node.supporting_node.add()
            if (nw_ref is not None):
                ul_node.network_ref = nw_ref
            else:
                ul_node.network_ref = self.l2netid
            ul_node.node_ref = sup_node.node_id
        return node

    def create_tp(self, node, cfg_tp, sup_node = None, sup_tp = None, nw_ref = None):
        logging.debug("   Creating termination point %s %s", node.l2_node_attributes.name, cfg_tp)
        tp = node.termination_point.add()
        tp.tp_id = ("{}:{}").format(node.node_id, cfg_tp)
        # L2 TP augmentation
        tp.l2_termination_point_attributes.description = cfg_tp
        tp.l2_termination_point_attributes.maximum_frame_size = 1500
        #tp.l2_termination_point_attributes.mac_address = "00:5e:8a:ab:dd:" + str(self.next_mac)
        #self.next_mac = self.next_mac + 1
        tp.l2_termination_point_attributes.eth_encapsulation = "l2t:vxlan"
        if ((sup_tp is not None) and (sup_node is not None)):
            logging.debug("     Adding support terminaton point %s", sup_tp.tp_id)
            ul_tp = tp.supporting_termination_point.add()
            if (nw_ref is not None):
                ul_tp.network_ref = nw_ref
            else:
                ul_tp.network_ref = self.l2netid
            ul_tp.node_ref = sup_node.node_id
            ul_tp.tp_ref = sup_tp.tp_id
        return tp

    def create_link(self, node1, tp1, node2, tp2, link_name1, link_name2 = None):
        logging.debug("Creating links %s %s", link_name1, link_name2)
        lnk1= self.sfcnet1.link.add()
        lnk1.link_id = "urn:Rift:Lab:Ethernet:{}{}_{}{}".format(node1.l2_node_attributes.name, tp1.l2_termination_point_attributes.description, node2.l2_node_attributes.name, tp2.l2_termination_point_attributes.description)
        lnk1.source.source_node = node1.node_id
        lnk1.source.source_tp = tp1.tp_id
        lnk1.destination.dest_node = node2.node_id
        lnk1.destination.dest_tp = tp2.tp_id
        # L2 link augmentation
        lnk1.l2_link_attributes.name = link_name1
        lnk1.l2_link_attributes.rate = 1000000000.00

        # Create bidir link if second link is provided
        if (link_name2 is not None):
            lnk2= self.sfcnet1.link.add()
            lnk2.link_id = "urn:Rift:Lab:Ethernet:{}{}_{}{}".format(node2.l2_node_attributes.name, tp2.l2_termination_point_attributes.description, node1.l2_node_attributes.name, tp1.l2_termination_point_attributes.description)
            lnk2.source.source_node = node2.node_id
            lnk2.source.source_tp = tp2.tp_id
            lnk2.destination.dest_node = node1.node_id
            lnk2.destination.dest_tp = tp1.tp_id
            # L2 link augmentation
            lnk2.l2_link_attributes.name = link_name2
            lnk2.l2_link_attributes.rate = 1000000000.00


class MySfcTopology(MySfcNetwork):
    def __init__(self, nwtop, l2top, provtop, vmnet, log):
        super(MySfcTopology, self).__init__(nwtop, l2top, provtop, vmnet, log)

    def find_nw_id(self, nw_name):
        return self.get_nw_id(nw_name)

    def find_node(self, node_name):
        return self.get_node(node_name)

    def find_tp(self, node, tp_name):
        return self.get_tp(node, tp_name)

    def find_link(self, link_name):
        return self.get_link(link_name)

    def setup_nodes(self):
        logging.debug("Setting up nodes")

        self.tg_node = self.vmtop.find_node("Trafgen_VM")
        if (self.tg_node is None):
           raise MyNodeNotFound()
        self.lb_node = self.vmtop.find_node("LB_VM")
        if (self.lb_node is None):
           raise MyNodeNotFound()

        self.g44_br_int_node = self.provtop.find_node("G44_Br_Int")
        if (self.g44_br_int_node is None):
           raise MyNodeNotFound()

        self.sf1 = self.create_node("SF1","SF on LB VM", sup_node = self.lb_node, nw_ref = self.vmnetid)
        self.sfc1 = self.create_node("SFC1","SF classifier on Trafgen VM", sup_node = self.tg_node, nw_ref = self.vmnetid)
        self.sff1 = self.create_node("SFF1","SF forwarder on Grunt44 OVS integration bridge", mgmt_ip_addr="10.66.4.44", sup_node = self.g44_br_int_node, nw_ref = self.provnetid)

    def setup_tps(self):
        logging.debug("Setting up termination points")
        # FInd L2 hosts
        #self.g44_e2 = self.l2top.find_tp(self.g44_node, "eth2")
        #if (self.g44_e2 is None):
        #   raise MyTpNotFound()

        self.sfc1_vxlannsh1 = self.create_tp(self.sfc1, "vxlannsh1")
        self.sf1_vxlannsh1 = self.create_tp(self.sf1, "vxlannsh1")
        self.sff1_vxlannsh1 = self.create_tp(self.sff1, "vxlannsh1")


    def setup_links(self):
        # Add links to sfcnet1 network
        # These links are unidirectional and point-to-point
        logging.debug("Setting up links")
        # Bidir Links for OVS bridges
        self.create_link(self.sfc1, self.sfc1_vxlannsh1, self.sff1, self.sff1_vxlannsh1, "Link_sfc1_sff1")
        self.create_link(self.sfc1, self.sfc1_vxlannsh1, self.sf1, self.sf1_vxlannsh1, "Link_sff1_sf1", "Link_sf1_sff1")

    def setup_all(self):
        self.setup_nodes()
        self.setup_tps()
        #self.setup_links()


if __name__ == "__main__":
    model = RwYang.Model.create_libncx()
    model.load_schema_ypbc(RwTl.get_schema())
    # create logger 
    logger = logging.getLogger('SFC Network Topology')
    logger.setLevel(logging.DEBUG)
    logging.basicConfig(level=logging.DEBUG)

    logger.info('Creating an instance of SFC Network Topology')

    nwtop = RwTl.YangData_IetfNetwork()

    # Setup L2 topology
    l2top = MyL2Topology(nwtop, logger)
    l2top.setup_all()

    # Setup Provider network topology
    provtop = MyProvTopology(nwtop, l2top, logger)
    provtop.setup_all()

    # Setup VM network topology
    vmtop = MyVMTopology(nwtop, l2top, provtop, logger)
    vmtop.setup_all()

    # Setup SFC network topology
    sfctop = MySfcTopology(nwtop, l2top, provtop, vmtop, logger)
    sfctop.setup_all()

    print ("Converting to XML")
    # Convert l2nw network to XML
    xml_str = nwtop.to_xml_v2(model)
    tree = etree.XML(xml_str)
    xml_file = "/tmp/stacked_sfctop.xml"
    xml_formatted_file = "/tmp/stacked_sfctop2.xml"
    with open(xml_file, "w") as f:
        f.write(xml_str)
    status = subprocess.call("xmllint --format " + xml_file + " > " + xml_formatted_file, shell=True)

    status = subprocess.call("sed -i '/xml version/d' " + xml_formatted_file, shell=True)
    status = subprocess.call("sed -i '/root xmlns/d' " + xml_formatted_file, shell=True)
    status = subprocess.call("sed -i '/\/root/d' " + xml_formatted_file, shell=True)

    print ("Converting to JSON ")
    # Convert set of topologies to JSON
    json_str = nwtop.to_json(model)
    with open("/tmp/stacked_sfctop.json", "w") as f:
        f.write(json_str)
    status = subprocess.call("python -m json.tool /tmp/stacked_sfctop.json > /tmp/stacked_sfctop2.json", shell=True)
    json_formatted_file = "/tmp/stacked_sfctop2.json"
    status = subprocess.call("sed -i -e 's/\"l2t:ethernet\"/\"ethernet\"/g' " + json_formatted_file, shell=True)
    status = subprocess.call("sed -i -e 's/\"l2t:vlan\"/\"vlan\"/g' " + json_formatted_file, shell=True)
    status = subprocess.call("sed -i -e 's/\"l2t:vxlan\"/\"vxlan\"/g' " + json_formatted_file, shell=True)

