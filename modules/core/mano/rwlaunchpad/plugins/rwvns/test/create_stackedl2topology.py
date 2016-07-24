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


class MyL2Network(object):
    def __init__(self, nwtop, log):
        self.next_mac = 11
        self.log = log
        self.nwtop = nwtop
        self.l2net1 = nwtop.network.add()
        self.l2net1.network_id = "L2HostNetwork-1"

        # L2 Network type augmentation
        self.l2net1.network_types.l2_network = self.l2net1.network_types.l2_network.new()
        # L2 Network augmentation
        self.l2net1.l2_network_attributes.name = "Rift LAB SFC-Demo Host Network"

    def get_nw_id(self, nw_name):
        for nw in self.nwtop.network:
            if (nw.network_id == nw_name):
                return nw.network_id

    def get_nw(self, nw_name):
        for nw in self.nwtop.network:
            if (nw.network_id == nw_name):
                return nw

    def get_node(self, node_name):
        _node_id = "urn:Rift:Lab:" + node_name
        for node in self.l2net1.node:
            if (node.node_id == _node_id):
                return node

    def get_tp(self, node, tp_name):
        _tp_id = node.node_id + "_" + tp_name
        for tp in node.termination_point :
            if (tp.tp_id == _tp_id):
                return tp

    def get_link(self, link_name):
        for link in nw.link :
            if (link.l2_link_attributes.name == link_name):
                return link

    def create_node(self, node_name, mgmt_ip_addr, description):
        logging.debug("Creating node %s", node_name)
        node = self.l2net1.node.add()
        node.node_id = "urn:Rift:Lab:" + node_name
        # L2 Node augmentation
        node.l2_node_attributes.name = node_name
        node.l2_node_attributes.description = description
        node.l2_node_attributes.management_address.append(mgmt_ip_addr)
        return node

    def create_tp(self, node, cfg_tp):
        logging.debug("    Creating termination point %s %s", node.l2_node_attributes.name, cfg_tp)
        tp = node.termination_point.add()
        tp.tp_id = ("{}_{}").format(node.node_id, cfg_tp)
        # L2 TP augmentation
        tp.l2_termination_point_attributes.description = cfg_tp
        tp.l2_termination_point_attributes.maximum_frame_size = 1500
        tp.l2_termination_point_attributes.mac_address = "00:1e:67:d8:48:" + str(self.next_mac)
        self.next_mac = self.next_mac + 1
        tp.l2_termination_point_attributes.eth_encapsulation = "l2t:ethernet"
        return tp

    def create_bidir_link(self, node1, tp1, node2, tp2, link_name1, link_name2):
        logging.debug("Creating links %s %s", link_name1, link_name2)
        lnk1= self.l2net1.link.add()
        lnk1.link_id = "urn:Rift:Lab:Ethernet:{}{}_{}{}".format(node1.l2_node_attributes.name, tp1.l2_termination_point_attributes.description, node2.l2_node_attributes.name, tp2.l2_termination_point_attributes.description)
        lnk1.source.source_node = node1.node_id
        lnk1.source.source_tp = tp1.tp_id
        lnk1.destination.dest_node = node2.node_id
        lnk1.destination.dest_tp = tp2.tp_id
        # L2 link augmentation
        lnk1.l2_link_attributes.name = link_name1
        #lnk1.l2_link_attributes.rate = 1000000000.00

        lnk2= self.l2net1.link.add()
        lnk2.link_id = "urn:Rift:Lab:Ethernet:{}{}_{}{}".format(node2.l2_node_attributes.name, tp2.l2_termination_point_attributes.description, node1.l2_node_attributes.name, tp1.l2_termination_point_attributes.description)
        lnk2.source.source_node = node2.node_id
        lnk2.source.source_tp = tp2.tp_id
        lnk2.destination.dest_node = node1.node_id
        lnk2.destination.dest_tp = tp1.tp_id
        # L2 link augmentation
        lnk2.l2_link_attributes.name = link_name2
        #lnk2.l2_link_attributes.rate = 1000000000.00
        return lnk1, lnk2

class MyL2Topology(MyL2Network):
    def __init__(self, nwtop, log):
        super(MyL2Topology, self).__init__(nwtop, log)

    def find_nw_id(self, nw_name):
        return self.get_nw_id(nw_name)

    def find_nw(self, nw_name):
        return self.get_nw(nw_name)

    def find_node(self, node_name):
        return self.get_node(node_name)

    def find_tp(self, node, tp_name):
        return self.get_tp(node, tp_name)

    def find_link(self, link_name):
        return self.get_link(link_name)

    def setup_nodes(self):
        self.g118 = self.create_node("Grunt118","10.66.4.118", "Host with OVS and PCI")
        self.g44 = self.create_node("Grunt44","10.66.4.44", "Host with OVS-DPDK")
        self.g120 = self.create_node("Grunt120","10.66.4.120", "Host with OVS and PCI")
        self.hms = self.create_node("HostMgmtSwitch","10.66.4.98", "Switch for host eth0")
        self.vms = self.create_node("VMMgmtSwitch","10.66.4.55", "Switch for VMs eth0")
        self.ads = self.create_node("AristaDPSwitch","10.66.4.90", "10 Gbps Switch")

    def setup_tps(self):
        self.g118_e0 = self.create_tp(self.g118, "eth0")
        self.g118_e1 = self.create_tp(self.g118, "eth1")
        self.g118_e2 = self.create_tp(self.g118, "eth2")

        self.g44_e0 = self.create_tp(self.g44, "eth0")
        self.g44_e1 = self.create_tp(self.g44, "eth1")
        self.g44_e2 = self.create_tp(self.g44, "eth2")
        self.g44_e3 = self.create_tp(self.g44, "eth3")

        self.g120_e0 = self.create_tp(self.g120, "eth0")
        self.g120_e1 = self.create_tp(self.g120, "eth1")
        self.g120_e2 = self.create_tp(self.g120, "eth2")

        self.hms_e1 = self.create_tp(self.hms, "eth1")
        self.hms_e2 = self.create_tp(self.hms, "eth2")
        self.hms_e3 = self.create_tp(self.hms, "eth3")

        self.vms_e1 = self.create_tp(self.vms, "eth1")
        self.vms_e2 = self.create_tp(self.vms, "eth2")
        self.vms_e3 = self.create_tp(self.vms, "eth3")

        self.ads_57 = self.create_tp(self.ads, "Card_5:Port_7")
        self.ads_58 = self.create_tp(self.ads, "Card_8:Port_8")
        self.ads_47 = self.create_tp(self.ads, "Card_4:Port_7")
        self.ads_48 = self.create_tp(self.ads, "Card_4:Port_8")

    def setup_links(self):
        # Add links to l2net1 network
        # These links are unidirectional and point-to-point
        # Bidir Links for Grunt118
        self.create_bidir_link(self.g118, self.g118_e0, self.hms, self.hms_e1, "Link_g118_e0_hms_e1", "Link_hms_e1_g118_e0")
        self.create_bidir_link(self.g118, self.g118_e1, self.vms, self.vms_e1, "Link_g118_e1_vms_e1", "Link_vms_e1_g118_e1")
        self.create_bidir_link(self.g118, self.g118_e2, self.ads, self.ads_57, "Link_g118_e2_ads_47", "Link_ads_47_g118_e2")
        # Bidir Links for Grunt44
        self.create_bidir_link(self.g44, self.g44_e0, self.hms, self.hms_e2, "Link_g44_e0_hms_e1", "Link_hms_e1_g44_e0")
        self.create_bidir_link(self.g44, self.g44_e1, self.vms, self.vms_e2, "Link_g44_e1_vms_e1", "Link_vms_e1_g44_e1")
        self.create_bidir_link(self.g44, self.g44_e2, self.ads, self.ads_47, "Link_g44_e2_ads_47", "Link_ads_47_g44_e2")
        self.create_bidir_link(self.g44, self.g44_e3, self.ads, self.ads_48, "Link_g44_e3_ads_48", "Link_ads_48_g44_e3")
        # Bidir Links for Grunt120
        self.create_bidir_link(self.g120, self.g120_e0, self.hms, self.hms_e3, "Link_g120_e0_hms_e1", "Link_hms_e1_g120_e0")
        self.create_bidir_link(self.g120, self.g120_e1, self.vms, self.vms_e3, "Link_g120_e1_vms_e1", "Link_vms_e1_g120_e1")
        self.create_bidir_link(self.g120, self.g120_e2, self.ads, self.ads_58, "Link_g120_e2_ads_58", "Link_ads_58_g120_e2")

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
    logger = logging.getLogger(__file__)
    logger.setLevel(logging.DEBUG)
    logging.basicConfig(level=logging.DEBUG)

    logging.info('Creating an instance of L2 Host Topology')
    nwtop = RwTl.YangData_IetfNetwork()

    l2top = MyL2Topology(nwtop, logger)
    l2top.setup_all()

    logging.info ("Converting to XML")
    # Convert l2nw network to XML
    xml_str = nwtop.to_xml_v2(model)
    tree = etree.XML(xml_str)
    xml_file = "/tmp/stacked_top.xml"
    xml_formatted_file = "/tmp/stacked_top2.xml"
    with open(xml_file, "w") as f:
        f.write(xml_str)
    status = subprocess.call("xmllint --format " + xml_file + " > " + xml_formatted_file, shell=True)
    status = subprocess.call("sed -i '/xml version/d' " + xml_formatted_file, shell=True)
    status = subprocess.call("sed -i '/root xmlns/d' " + xml_formatted_file, shell=True)
    status = subprocess.call("sed -i '/\/root/d' " + xml_formatted_file, shell=True)

    logging.info ("Converting to JSON")
    # Convert set of topologies to JSON
    json_str = nwtop.to_json(model)
    with open("/tmp/stacked_top.json", "w") as f:
        f.write(json_str)
    status = subprocess.call("python -m json.tool /tmp/stacked_top.json > /tmp/stacked_top2.json", shell=True)
    json_formatted_file = "/tmp/stacked_top2.json"
    status = subprocess.call("sed -i -e 's/\"l2t:ethernet\"/\"ethernet\"/g' " + json_formatted_file, shell=True)

