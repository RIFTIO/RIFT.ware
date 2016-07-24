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

class MyNwNotFound(Exception):
    pass

class MyNodeNotFound(Exception):
    pass

class MyTpNotFound(Exception):
    pass

class MyVMNetwork(object):
    def __init__(self, nwtop, l2top, provtop, log):
        self.next_mac = 41
        self.log = log
        self.vmnet1 = nwtop.network.add()
        self.vmnet1.network_id = "VmNetwork-1"

        self.nwtop = nwtop
        self.l2top = l2top
        self.provtop = provtop

        # L2 Network type augmentation
        self.vmnet1.network_types.l2_network = self.vmnet1.network_types.l2_network.new()
        # L2 Network augmentation
        self.vmnet1.l2_network_attributes.name = "Rift LAB SFC-Demo VM Network"
        ul_net = self.vmnet1.supporting_network.add()
        try:
           ul_net.network_ref = l2top.find_nw_id("L2HostNetwork-1")
           self.l2netid = ul_net.network_ref
        except TypeError:
           raise MyNwNotFound()
        ul_net = self.vmnet1.supporting_network.add()
        try:
           ul_net.network_ref = provtop.find_nw_id("ProviderNetwork-1")
           self.provnetid = ul_net.network_ref
        except TypeError:
           raise MyNwNotFound()

    def get_nw_id(self, nw_name):
        for nw in self.nwtop.network:
            if (nw.network_id == nw_name):
                return nw.network_id

    def get_node(self, node_name):
        _node_id = "urn:Rift:Lab:" + node_name
        for node in self.vmnet1.node:
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

    def create_node(self, node_name, description, mgmt_ip_addr=None, sup_node_list=None):
        logging.debug("Creating node %s", node_name)
        node = self.vmnet1.node.add()
        node.node_id = "urn:Rift:Lab:" + node_name
        # L2 Node augmentation
        node.l2_node_attributes.name = node_name
        node.l2_node_attributes.description = description
        if (mgmt_ip_addr is not None):
            node.l2_node_attributes.management_address.append(mgmt_ip_addr)
        if (sup_node_list is not None):
            for sup_node in sup_node_list:
                logging.debug("  Adding support node %s", sup_node[0].node_id)
                ul_node = node.supporting_node.add()
                # Second element is hardcoded as nw ref
                if (sup_node[1] is not None):
                    ul_node.network_ref = sup_node[1]
                else:
                    ul_node.network_ref = self.l2netid
                ul_node.node_ref = sup_node[0].node_id
        return node

    def create_tp(self, node, cfg_tp, sup_node = None, sup_tp = None, nw_ref = None):
        logging.debug("   Creating termination point %s %s", node.l2_node_attributes.name, cfg_tp)
        tp = node.termination_point.add()
        tp.tp_id = ("{}:{}").format(node.node_id, cfg_tp)
        # L2 TP augmentation
        tp.l2_termination_point_attributes.description = cfg_tp
        tp.l2_termination_point_attributes.maximum_frame_size = 1500
        tp.l2_termination_point_attributes.mac_address = "00:5e:8a:ab:cc:" + str(self.next_mac)
        self.next_mac = self.next_mac + 1
        tp.l2_termination_point_attributes.eth_encapsulation = "l2t:ethernet"
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

    def create_bidir_link(self, node1, tp1, node2, tp2, link_name1, link_name2):
        logging.debug("Creating links %s %s", link_name1, link_name2)
        lnk1= self.vmnet1.link.add()
        lnk1.link_id = "urn:Rift:Lab:Ethernet:{}{}_{}{}".format(node1.l2_node_attributes.name, tp1.l2_termination_point_attributes.description, node2.l2_node_attributes.name, tp2.l2_termination_point_attributes.description)
        lnk1.source.source_node = node1.node_id
        lnk1.source.source_tp = tp1.tp_id
        lnk1.destination.dest_node = node2.node_id
        lnk1.destination.dest_tp = tp2.tp_id
        # L2 link augmentation
        lnk1.l2_link_attributes.name = link_name1
        #lnk1.l2_link_attributes.rate = 1000000000.00

        lnk2= self.vmnet1.link.add()
        lnk2.link_id = "urn:Rift:Lab:Ethernet:{}{}_{}{}".format(node2.l2_node_attributes.name, tp2.l2_termination_point_attributes.description, node1.l2_node_attributes.name, tp1.l2_termination_point_attributes.description)
        lnk2.source.source_node = node2.node_id
        lnk2.source.source_tp = tp2.tp_id
        lnk2.destination.dest_node = node1.node_id
        lnk2.destination.dest_tp = tp1.tp_id
        # L2 link augmentation
        lnk2.l2_link_attributes.name = link_name2
        #lnk2.l2_link_attributes.rate = 1000000000.00
        return lnk1, lnk2

class MyVMTopology(MyVMNetwork):
    def __init__(self, nwtop, l2top, provtop, log):
        super(MyVMTopology, self).__init__(nwtop, l2top, provtop, log)

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

        self.g118_node = self.l2top.find_node("Grunt118")
        if (self.g118_node is None):
           raise MyNodeNotFound()
        self.g44_node = self.l2top.find_node("Grunt44")
        if (self.g44_node is None):
           raise MyNodeNotFound()
        self.g120_node = self.l2top.find_node("Grunt120")
        if (self.g120_node is None):
           raise MyNodeNotFound()

        self.g44_br_int_node = self.provtop.find_node("G44_Br_Int")
        if (self.g44_br_int_node is None):
           raise MyNodeNotFound()

        self.pseudo_vm = self.create_node("Pseudo_VM","Pseudo VM to manage eth0 LAN")
        sup_node_list = [[self.g118_node, self.l2netid], [self.g44_br_int_node, self.provnetid]]
        self.tg_vm = self.create_node("Trafgen_VM","Trafgen VM on Grunt118", mgmt_ip_addr="10.0.118.3", sup_node_list = sup_node_list)
        sup_node_list = [[self.g44_node, self.l2netid], [self.g44_br_int_node, self.provnetid]]
        self.lb_vm = self.create_node("LB_VM","LB VM on Grunt44", mgmt_ip_addr="10.0.118.35", sup_node_list = sup_node_list)
        sup_node_list = [[self.g120_node, self.l2netid], [self.g44_br_int_node, self.provnetid]]
        self.ts_vm = self.create_node("Trafsink_VM","Trafsink VM on Grunt120", mgmt_ip_addr="10.0.118.4", sup_node_list = sup_node_list)

    def setup_tps(self):
        logging.debug("Setting up termination points")
        # FInd L2 hosts
        self.g118_e2 = self.l2top.find_tp(self.g118_node, "eth2")
        if (self.g118_e2 is None):
           raise MyTpNotFound()
        self.g44_e2 = self.l2top.find_tp(self.g44_node, "eth2")
        if (self.g44_e2 is None):
           raise MyTpNotFound()
        # Find OVS tps
        self.g44_br_int_vhu2 = self.provtop.find_tp(self.g44_br_int_node, "vhu2")
        if (self.g44_br_int_vhu2 is None):
           raise MyTpNotFound()
        self.g44_br_int_vhu3 = self.provtop.find_tp(self.g44_br_int_node, "vhu3")
        if (self.g44_br_int_vhu3 is None):
           raise MyTpNotFound()

        self.pvm_eth1 = self.create_tp(self.pseudo_vm, "eth1") 
        self.pvm_eth2 = self.create_tp(self.pseudo_vm, "eth2") 
        self.pvm_eth3 = self.create_tp(self.pseudo_vm, "eth3") 

        self.tg_vm_eth0 = self.create_tp(self.tg_vm, "eth0")
        self.tg_vm_trafgen11 = self.create_tp(self.tg_vm, "trafgen11", sup_node=self.g118_node, sup_tp=self.g118_e2)

        self.lb_vm_eth0 = self.create_tp(self.lb_vm, "eth0")
        self.lb_vm_lb21 = self.create_tp(self.lb_vm, "load_balancer21", sup_node=self.g44_br_int_node, sup_tp=self.g44_br_int_vhu2, nw_ref=self.provnetid)
        self.lb_vm_lb22 = self.create_tp(self.lb_vm, "load_balancer22", sup_node=self.g44_br_int_node, sup_tp=self.g44_br_int_vhu3, nw_ref=self.provnetid)

        self.ts_vm_eth0 = self.create_tp(self.ts_vm, "eth0")
        self.ts_vm_trafsink31 = self.create_tp(self.ts_vm, "trafsink31", sup_node=self.g44_node, sup_tp=self.g44_e2)


    def setup_links(self):
        # Add links to vmnet1 network
        # These links are unidirectional and point-to-point
        logging.debug("Setting up links")
        # Bidir Links for OVS bridges
        self.create_bidir_link(self.tg_vm, self.tg_vm_trafgen11, self.lb_vm, self.lb_vm_lb21, "Link_tg_t11_lb_lb21", "Link_lb_lb21_tg_t11")
        self.create_bidir_link(self.ts_vm, self.ts_vm_trafsink31, self.lb_vm, self.lb_vm_lb22, "Link_ts_t31_lb_lb22", "Link_lb_lb22_tg_t31")

        self.create_bidir_link(self.pseudo_vm, self.pvm_eth1, self.tg_vm, self.tg_vm_eth0, "Link_pvm_e1_tgv_e0", "Link_tgv_e0_pvm_e1")
        self.create_bidir_link(self.pseudo_vm, self.pvm_eth2, self.lb_vm, self.lb_vm_eth0, "Link_pvm_e2_lbv_e0", "Link_lbv_e0_pvm_e2")
        self.create_bidir_link(self.pseudo_vm, self.pvm_eth3, self.ts_vm, self.ts_vm_eth0, "Link_pvm_e3_tsv_e0", "Link_tsv_e0_pvm_e3")

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
    logger = logging.getLogger('VM Network Topology')
    logger.setLevel(logging.DEBUG)
    logging.basicConfig(level=logging.DEBUG)

    logger.info('Creating an instance of VM Network Topology')

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

    print ("Converting to XML")
    # Convert l2nw network to XML
    xml_str = nwtop.to_xml_v2(model)
    tree = etree.XML(xml_str)
    xml_file = "/tmp/stacked_vmtop.xml"
    xml_formatted_file = "/tmp/stacked_vmtop2.xml"
    with open(xml_file, "w") as f:
        f.write(xml_str)
    status = subprocess.call("xmllint --format " + xml_file + " > " + xml_formatted_file, shell=True)

    status = subprocess.call("sed -i '/xml version/d' " + xml_formatted_file, shell=True)
    status = subprocess.call("sed -i '/root xmlns/d' " + xml_formatted_file, shell=True)
    status = subprocess.call("sed -i '/\/root/d' " + xml_formatted_file, shell=True)

    print ("Converting to JSON ")
    # Convert set of topologies to JSON
    json_str = nwtop.to_json(model)
    with open("/tmp/stacked_vmtop.json", "w") as f:
        f.write(json_str)
    status = subprocess.call("python -m json.tool /tmp/stacked_vmtop.json > /tmp/stacked_vmtop2.json", shell=True)
    json_formatted_file = "/tmp/stacked_vmtop2.json"
    status = subprocess.call("sed -i -e 's/\"l2t:ethernet\"/\"ethernet\"/g' " + json_formatted_file, shell=True)
    status = subprocess.call("sed -i -e 's/\"l2t:vlan\"/\"vlan\"/g' " + json_formatted_file, shell=True)

