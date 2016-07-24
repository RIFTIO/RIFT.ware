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

class MyNwNotFound(Exception):
    pass

class MyNodeNotFound(Exception):
    pass

class MyTpNotFound(Exception):
    pass

class MyProvNetwork(object):
    def __init__(self, nwtop, l2top, log):
        self.next_mac = 11
        self.log = log
        self.provnet1 = nwtop.network.add()
        self.provnet1.network_id = "ProviderNetwork-1"

        self.nwtop = nwtop
        self.l2top = l2top

        # L2 Network type augmentation
        self.provnet1.network_types.l2_network = self.provnet1.network_types.l2_network.new()
        # L2 Network augmentation
        self.provnet1.l2_network_attributes.name = "Rift LAB SFC-Demo Provider Network"
        ul_net = self.provnet1.supporting_network.add()
        try:
           ul_net.network_ref = l2top.find_nw_id("L2HostNetwork-1")
           self.l2netid = ul_net.network_ref
        except TypeError:
           raise MyNwNotFound()

    def get_nw_id(self, nw_name):
        for nw in self.nwtop.network:
            if (nw.network_id == nw_name):
                return nw.network_id

    def get_node(self, node_name):
        _node_id = "urn:Rift:Lab:" + node_name
        for node in self.provnet1.node:
            if (node.node_id == _node_id):
                return node

    def get_tp(self, node, tp_name):
        _tp_id = node.node_id + ":" + tp_name
        for tp in node.termination_point :
            if (tp.tp_id == _tp_id):
                return tp

    def get_link(self, link_name):
        for link in nw.link :
            if (link.l2_link_attributes.name == link_name):
                return link

    def create_node(self, node_name, description, mgmt_ip_addr = None, sup_node = None):
        logging.debug("Creating node %s", node_name)
        node = self.provnet1.node.add()
        node.node_id = "urn:Rift:Lab:" + node_name
        # L2 Node augmentation
        node.l2_node_attributes.name = node_name
        node.l2_node_attributes.description = description
        if (mgmt_ip_addr is not None):
            node.l2_node_attributes.management_address.append(mgmt_ip_addr)
        if (sup_node is not None):
            logging.debug("  Adding support node %s", sup_node.node_id)
            ul_node = node.supporting_node.add()
            ul_node.network_ref = self.l2netid
            ul_node.node_ref = sup_node.node_id
        return node

    def create_tp(self, node, cfg_tp, sup_node = None, sup_tp = None, vlan = False):
        logging.debug("   Creating termination point %s %s", node.l2_node_attributes.name, cfg_tp)
        tp = node.termination_point.add()
        tp.tp_id = ("{}:{}").format(node.node_id, cfg_tp)
        # L2 TP augmentation
        tp.l2_termination_point_attributes.description = cfg_tp
        tp.l2_termination_point_attributes.maximum_frame_size = 1500
        tp.l2_termination_point_attributes.mac_address = "00:4f:9c:ab:dd:" + str(self.next_mac)
        self.next_mac = self.next_mac + 1
        if (vlan == True):
            tp.l2_termination_point_attributes.eth_encapsulation = "l2t:vlan"
        else:
            tp.l2_termination_point_attributes.eth_encapsulation = "l2t:ethernet"
        if ((sup_tp is not None) and (sup_node is not None)):
            logging.debug("     Adding support terminaton point %s", sup_tp.tp_id)
            ul_tp = tp.supporting_termination_point.add()
            ul_tp.network_ref = self.l2netid
            ul_tp.node_ref = sup_node.node_id
            ul_tp.tp_ref = sup_tp.tp_id
        return tp

    def create_bidir_link(self, node1, tp1, node2, tp2, link_name1, link_name2):
        logging.debug("Creating links %s %s", link_name1, link_name2)
        lnk1= self.provnet1.link.add()
        lnk1.link_id = "urn:Rift:Lab:Ethernet:{}{}_{}{}".format(node1.l2_node_attributes.name, tp1.l2_termination_point_attributes.description, node2.l2_node_attributes.name, tp2.l2_termination_point_attributes.description)
        lnk1.source.source_node = node1.node_id
        lnk1.source.source_tp = tp1.tp_id
        lnk1.destination.dest_node = node2.node_id
        lnk1.destination.dest_tp = tp2.tp_id
        # L2 link augmentation
        lnk1.l2_link_attributes.name = link_name1
        #lnk1.l2_link_attributes.rate = 1000000000.00

        lnk2= self.provnet1.link.add()
        lnk2.link_id = "urn:Rift:Lab:Ethernet:{}{}_{}{}".format(node2.l2_node_attributes.name, tp2.l2_termination_point_attributes.description, node1.l2_node_attributes.name, tp1.l2_termination_point_attributes.description)
        lnk2.source.source_node = node2.node_id
        lnk2.source.source_tp = tp2.tp_id
        lnk2.destination.dest_node = node1.node_id
        lnk2.destination.dest_tp = tp1.tp_id
        # L2 link augmentation
        lnk2.l2_link_attributes.name = link_name2
        #lnk2.l2_link_attributes.rate = 1000000000.00
        return lnk1, lnk2

class MyProvTopology(MyProvNetwork):
    def __init__(self, nwtop, l2top, log):
        super(MyProvTopology, self).__init__(nwtop, l2top, log)

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
        self.pseudo_mgmt_node = self.create_node("Pseudo_mgmt_node", "Pseudo node for VM mgmt network LAN")
        self.pseudo_dp_node = self.create_node("Pseudo_DP_node", "Pseudo node for DP network LAN")

        self.g118_node = self.l2top.find_node("Grunt118")
        if (self.g118_node is None):
           raise MyNodeNotFound()
        self.g44_node = self.l2top.find_node("Grunt44")
        if (self.g44_node is None):
           raise MyNodeNotFound()
        self.g120_node = self.l2top.find_node("Grunt120")
        if (self.g120_node is None):
           raise MyNodeNotFound()

        self.g118_br_int = self.create_node("G118_Br_Int","OVS Integration bridge on Grunt118", mgmt_ip_addr="10.66.4.118", sup_node = self.g118_node)
        self.g118_br_eth1 = self.create_node("G118_Br_Eth1","OVS Integration bridge on Grunt118", mgmt_ip_addr="10.66.4.118", sup_node = self.g118_node)
        # eth2 on g118 is being used in PCI passthrough mode

        self.g44_br_int = self.create_node("G44_Br_Int","OVS Integration bridge on Grunt44", mgmt_ip_addr="10.66.4.44", sup_node = self.g44_node)
        self.g44_br_eth1 = self.create_node("G44_Br_Eth1","OVS Interface bridge on Grunt44", mgmt_ip_addr="10.66.4.44", sup_node = self.g44_node)
        self.g44_br_eth2 = self.create_node("G44_Br_Eth2","OVS Interface bridge on Grunt44", mgmt_ip_addr="10.66.4.44", sup_node = self.g44_node)
        self.g44_br_eth3 = self.create_node("G44_Br_Eth3","OVS Interface bridge on Grunt44", mgmt_ip_addr="10.66.4.44", sup_node = self.g44_node)

        self.g120_br_int = self.create_node("G120_Br_Int","OVS Integration bridge on Grunt120", mgmt_ip_addr = "10.66.4.120", sup_node = self.g120_node)
        self.g120_br_eth1 = self.create_node("G120_Br_Eth1","OVS Integration bridge on Grunt120", mgmt_ip_addr = "10.66.4.120", sup_node = self.g120_node)
        # eth2 on g120 is being used in PCI passthrough mode

    def setup_tps(self):
        logging.debug("Setting up termination points")
        self.g118_e1 = self.l2top.find_tp(self.g118_node, "eth1")
        if (self.g118_e1 is None):
           raise MyTpNotFound()
        self.g44_e1 = self.l2top.find_tp(self.g44_node, "eth1")
        if (self.g44_e1 is None):
           raise MyTpNotFound()
        self.g44_e2 = self.l2top.find_tp(self.g44_node, "eth2")
        if (self.g44_e2 is None):
           raise MyTpNotFound()
        self.g44_e3 = self.l2top.find_tp(self.g44_node, "eth3")
        if (self.g44_e3 is None):
           raise MyTpNotFound()
        self.g120_e1 = self.l2top.find_tp(self.g120_node, "eth1")
        if (self.g44_e3 is None):
           raise MyTpNotFound()

        self.g118_br_int_eth1 = self.create_tp(self.g118_br_int, "int-br-eth1")
        self.g118_br_int_tap1 = self.create_tp(self.g118_br_int, "tap1")

        self.g118_br_eth1_phyeth1 = self.create_tp(self.g118_br_eth1, "phyeth1")
        self.g118_br_eth1_eth1 = self.create_tp(self.g118_br_eth1, "eth1", sup_node=self.g118_node, sup_tp=self.g118_e1, vlan=True)

        self.g44_br_int_eth1 = self.create_tp(self.g44_br_int, "int-br-eth1")
        self.g44_br_int_vhu1 = self.create_tp(self.g44_br_int, "vhu1")
        self.g44_br_int_eth2 = self.create_tp(self.g44_br_int, "int-br-eth2")
        self.g44_br_int_vhu2 = self.create_tp(self.g44_br_int, "vhu2")
        self.g44_br_int_eth1 = self.create_tp(self.g44_br_int, "int-br-eth3")
        self.g44_br_int_vhu1 = self.create_tp(self.g44_br_int, "vhu3")

        self.g44_br_eth1_phyeth1 = self.create_tp(self.g44_br_eth1, "phyeth1")
        self.g44_br_eth1_dpdk0 = self.create_tp(self.g44_br_eth1, "dpdk0", sup_node=self.g44_node, sup_tp=self.g44_e1, vlan=True)

        self.g44_br_eth2_phyeth1 = self.create_tp(self.g44_br_eth2, "phyeth2")
        self.g44_br_eth2_dpdk1 = self.create_tp(self.g44_br_eth2, "dpdk1", sup_node=self.g44_node, sup_tp=self.g44_e2)

        self.g44_br_eth3_phyeth1 = self.create_tp(self.g44_br_eth3, "phyeth3")
        self.g44_br_eth3_dpdk2 = self.create_tp(self.g44_br_eth3, "dpdk2", sup_node=self.g44_node, sup_tp=self.g44_e3)

        self.g120_br_int_eth1 = self.create_tp(self.g120_br_int, "int-br-eth1")
        self.g120_br_int_tap1 = self.create_tp(self.g120_br_int, "tap1")

        self.g120_br_eth1_phyeth1 = self.create_tp(self.g120_br_eth1, "phyeth1")
        self.g120_br_eth1_eth1 = self.create_tp(self.g120_br_eth1, "eth1", sup_node=self.g120_node, sup_tp=self.g120_e1, vlan=True)

        self.pmn_eth1 = self.create_tp(self.pseudo_mgmt_node, "eth1")
        self.pmn_eth2 = self.create_tp(self.pseudo_mgmt_node, "eth2")
        self.pmn_eth3 = self.create_tp(self.pseudo_mgmt_node, "eth3")

    def setup_links(self):
        # Add links to provnet1 network
        # These links are unidirectional and point-to-point
        logging.debug("Setting up links")
        # Bidir Links for OVS bridges
        self.create_bidir_link(self.g118_br_eth1, self.g118_br_eth1_eth1, self.pseudo_mgmt_node, self.pmn_eth1, "Link_g118_be1_pmn_e1", "Link_pmn_e1_g118_be1")
        self.create_bidir_link(self.g44_br_eth1, self.g44_br_eth1_dpdk0, self.pseudo_mgmt_node, self.pmn_eth2, "Link_g44_be1_pmn_d0", "Link_pmn_e2_g44_d0")
        self.create_bidir_link(self.g120_br_eth1, self.g120_br_eth1_eth1, self.pseudo_mgmt_node, self.pmn_eth3, "Link_g120_be1_pmn_e3", "Link_pmn_e3_g120_be1")
        # Data path links cannot be represented here since PCI pass through is beingused on G118 and G44

    def setup_all(self):
        self.setup_nodes()
        self.setup_tps()
        self.setup_links()

def adjust_xml_file(infile, outfile, begin_marker, end_marker):
    buffer = []
    in_block = False
    max_interesting_line_toread = 1
    interesting_line = 0
    with open(infile) as inf:
        with open(outfile, 'w') as outf:
            for line in inf:
                if begin_marker in line:
                    in_block = True
                    # Go down
                if end_marker in line:
                    assert in_block is True
                    print("End of gathering line...", line)
                    buffer.append(line)  # gather lines
                    interesting_line = max_interesting_line_toread
                    in_block = False
                    continue
                if interesting_line:
                    print("Interesting line printing ...", line)
                    outf.write(line)
                    interesting_line -= 1
                    if interesting_line == 0:  # output gathered lines
                        for lbuf in buffer:
                            outf.write(lbuf)
                        buffer = []  # empty buffer 
                        print("\n\n")
                    continue

                if in_block:
                    print("Gathering line...", line)
                    buffer.append(line)  # gather lines
                else:
                    outf.write(line)


if __name__ == "__main__":
    model = RwYang.Model.create_libncx()
    model.load_schema_ypbc(RwTl.get_schema())
    # create logger 
    logger = logging.getLogger('Provider Network Topology')
    logger.setLevel(logging.DEBUG)
    logging.basicConfig(level=logging.DEBUG)

    logger.info('Creating an instance of Provider Network Topology')

    nwtop = RwTl.YangData_IetfNetwork()

    # Setup L2 topology
    l2top = MyL2Topology(nwtop, logger)
    l2top.setup_all()

    # Setup Provider network topology
    provtop = MyProvTopology(nwtop, l2top, logger)
    provtop.setup_all()

    print ("Converting to XML")
    # Convert l2nw network to XML
    xml_str = nwtop.to_xml_v2(model)
    tree = etree.XML(xml_str)
    xml_file = "/tmp/stacked_provtop.xml"
    xml_formatted_file = "/tmp/stacked_provtop2.xml"
    with open(xml_file, "w") as f:
        f.write(xml_str)
    status = subprocess.call("xmllint --format " + xml_file + " > " + xml_formatted_file, shell=True)

    status = subprocess.call("sed -i '/xml version/d' " + xml_formatted_file, shell=True)
    status = subprocess.call("sed -i '/root xmlns/d' " + xml_formatted_file, shell=True)
    status = subprocess.call("sed -i '/\/root/d' " + xml_formatted_file, shell=True)

    print ("Converting to JSON ")
    # Convert set of topologies to JSON
    json_str = nwtop.to_json(model)
    with open("/tmp/stacked_provtop.json", "w") as f:
        f.write(json_str)
    status = subprocess.call("python -m json.tool /tmp/stacked_provtop.json > /tmp/stacked_provtop2.json", shell=True)
    json_formatted_file = "/tmp/stacked_provtop2.json"
    status = subprocess.call("sed -i -e 's/\"l2t:ethernet\"/\"ethernet\"/g' " + json_formatted_file, shell=True)
    status = subprocess.call("sed -i -e 's/\"l2t:vlan\"/\"vlan\"/g' " + json_formatted_file, shell=True)
