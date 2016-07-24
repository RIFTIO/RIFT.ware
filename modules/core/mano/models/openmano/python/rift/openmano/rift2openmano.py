#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import argparse
import collections
import logging
import math
import os
import sys
import tempfile
import yaml

from gi.repository import (
    RwYang,
    RwVnfdYang,
    RwNsdYang,
    )

logger = logging.getLogger("rift2openmano.py")


class VNFNotFoundError(Exception):
    pass


class RiftNSD(object):
    model = RwYang.Model.create_libncx()
    model.load_module('nsd')
    model.load_module('rw-nsd')

    def __init__(self, descriptor):
        self._nsd = descriptor

    def __str__(self):
        return str(self._nsd)

    @property
    def name(self):
        return self._nsd.name

    @property
    def id(self):
        return self._nsd.id

    @property
    def vnfd_ids(self):
        return [c.vnfd_id_ref for c in self._nsd.constituent_vnfd]

    @property
    def constituent_vnfds(self):
        return self._nsd.constituent_vnfd

    @property
    def vlds(self):
        return self._nsd.vld

    @property
    def cps(self):
        return self._nsd.connection_point

    @property
    def description(self):
        return self._nsd.description

    @classmethod
    def from_xml_file_hdl(cls, hdl):
        hdl.seek(0)
        descriptor = RwNsdYang.YangData_Nsd_NsdCatalog_Nsd()
        descriptor.from_xml_v2(RiftNSD.model, hdl.read())
        return cls(descriptor)

    @classmethod
    def from_yaml_file_hdl(cls, hdl):
        hdl.seek(0)
        descriptor = RwNsdYang.YangData_Nsd_NsdCatalog_Nsd()
        descriptor.from_yaml(RiftNSD.model, hdl.read())
        return cls(descriptor)

    @classmethod
    def from_dict(cls, nsd_dict):
        descriptor = RwNsdYang.YangData_Nsd_NsdCatalog_Nsd.from_dict(nsd_dict)
        return cls(descriptor)


class RiftVNFD(object):
    model = RwYang.Model.create_libncx()
    model.load_module('vnfd')
    model.load_module('rw-vnfd')

    def __init__(self, descriptor):
        self._vnfd = descriptor

    def __str__(self):
        return str(self._vnfd)

    @property
    def id(self):
        return self._vnfd.id

    @property
    def name(self):
        return self._vnfd.name

    @property
    def description(self):
        return self._vnfd.description

    @property
    def cps(self):
        return self._vnfd.connection_point

    @property
    def vdus(self):
        return self._vnfd.vdu

    @property
    def internal_vlds(self):
        return self._vnfd.internal_vld

    @classmethod
    def from_xml_file_hdl(cls, hdl):
        hdl.seek(0)
        descriptor = RwVnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd()
        descriptor.from_xml_v2(RiftVNFD.model, hdl.read())
        return cls(descriptor)

    @classmethod
    def from_yaml_file_hdl(cls, hdl):
        hdl.seek(0)
        descriptor = RwVnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd()
        descriptor.from_yaml(RiftVNFD.model, hdl.read())
        return cls(descriptor)

    @classmethod
    def from_dict(cls, vnfd_dict):
        descriptor = RwVnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd.from_dict(vnfd_dict)
        return cls(descriptor)


def is_writable_directory(dir_path):
    """ Returns True if dir_path is writable, False otherwise

    Arguments:
        dir_path - A directory path
    """
    if not os.path.exists(dir_path):
        raise ValueError("Directory does not exist: %s", dir_path)

    try:
        testfile = tempfile.TemporaryFile(dir=dir_path)
        testfile.close()
    except OSError:
        return False

    return True


def create_vnfd_from_xml_files(vnfd_file_hdls):
    """ Create a list of RiftVNFD instances from xml file handles

    Arguments:
        vnfd_file_hdls - Rift VNFD XML file handles

    Returns:
        A list of RiftVNFD instances
    """
    vnfd_dict = {}
    for vnfd_file_hdl in vnfd_file_hdls:
        vnfd = RiftVNFD.from_xml_file_hdl(vnfd_file_hdl)
        vnfd_dict[vnfd.id] = vnfd

    return vnfd_dict

def create_vnfd_from_yaml_files(vnfd_file_hdls):
    """ Create a list of RiftVNFD instances from xml file handles

    Arguments:
        vnfd_file_hdls - Rift VNFD YAML file handles

    Returns:
        A list of RiftVNFD instances
    """
    vnfd_dict = {}
    for vnfd_file_hdl in vnfd_file_hdls:
        vnfd = RiftVNFD.from_yaml_file_hdl(vnfd_file_hdl)
        vnfd_dict[vnfd.id] = vnfd

    return vnfd_dict


def create_nsd_from_xml_file(nsd_file_hdl):
    """ Create a list of RiftNSD instances from xml file handles

    Arguments:
        nsd_file_hdls - Rift NSD XML file handles

    Returns:
        A list of RiftNSD instances
    """
    nsd = RiftNSD.from_xml_file_hdl(nsd_file_hdl)
    return nsd

def create_nsd_from_yaml_file(nsd_file_hdl):
    """ Create a list of RiftNSD instances from yaml file handles

    Arguments:
        nsd_file_hdls - Rift NSD XML file handles

    Returns:
        A list of RiftNSD instances
    """
    nsd = RiftNSD.from_yaml_file_hdl(nsd_file_hdl)
    return nsd


def ddict():
    return collections.defaultdict(dict)

def convert_vnfd_name(vnfd_name, member_idx):
    return vnfd_name + "__" + str(member_idx)


def rift2openmano_nsd(rift_nsd, rift_vnfds):
    for vnfd_id in rift_nsd.vnfd_ids:
        if vnfd_id not in rift_vnfds:
            raise VNFNotFoundError("VNF id %s not provided" % vnfd_id)

    openmano = {}
    openmano["name"] = rift_nsd.name
    openmano["description"] = rift_nsd.description
    topology = {}
    openmano["topology"] = topology

    topology["nodes"] = {}
    for vnfd in rift_nsd.constituent_vnfds:
        vnfd_id = vnfd.vnfd_id_ref
        rift_vnfd = rift_vnfds[vnfd_id]
        member_idx = vnfd.member_vnf_index
        topology["nodes"][rift_vnfd.name + "__" + str(member_idx)] = {
                "type": "VNF",
                "VNF model": rift_vnfd.name
                }

    for vld in rift_nsd.vlds:
        # Openmano has both bridge_net and dataplane_net models for network types
        # For now, since we are using openmano in developer mode lets just hardcode
        # to bridge_net since it won't matter anyways.
        # topology["nodes"][vld.name] = {"type": "network", "model": "bridge_net"}
        pass

    topology["connections"] = {}
    for vld in rift_nsd.vlds:

        # Create a connections entry for each external VLD
        topology["connections"][vld.name] = {}
        topology["connections"][vld.name]["nodes"] = []

        if vld.provider_network.has_field("physical_network"):
            # Add the external datacenter network to the topology
            # node list if it isn't already added
            ext_net_name = vld.provider_network.physical_network
            ext_net_name_with_seg = ext_net_name
            if vld.provider_network.has_field("segmentation_id"):
                ext_net_name_with_seg += ":{}".format(vld.provider_network.segmentation_id)

            if ext_net_name not in topology["nodes"]:
                topology["nodes"][ext_net_name] = {
                        "type": "external_network",
                        "model": ext_net_name_with_seg,
                        }

            # Add the external network to the list of connection points
            topology["connections"][vld.name]["nodes"].append(
                    {ext_net_name: "0"}
                    )


        for vnfd_cp in vld.vnfd_connection_point_ref:

            # Get the RIFT VNF for this external VLD connection point
            vnfd = rift_vnfds[vnfd_cp.vnfd_id_ref]

            # For each VNF in this connection, use the same interface name
            topology["connections"][vld.name]["type"] = "link"
            # Vnf ref is the vnf name with the member_vnf_idx appended
            member_idx = vnfd_cp.member_vnf_index_ref
            vnf_ref = vnfd.name + "__" + str(member_idx)
            topology["connections"][vld.name]["nodes"].append(
                {
                    vnf_ref: vnfd_cp.vnfd_connection_point_ref
                }
            )

    return openmano


def rift2openmano_vnfd(rift_vnfd):
    openmano_vnf = {"vnf":{}}
    vnf = openmano_vnf["vnf"]

    vnf["name"] = rift_vnfd.name
    vnf["description"] = rift_vnfd.description

    vnf["external-connections"] = []

    def find_vdu_and_ext_if_by_cp_ref(cp_ref_name):
        for vdu in rift_vnfd.vdus:
            for ext_if in vdu.external_interface:
                if ext_if.vnfd_connection_point_ref == cp_ref_name:
                    return vdu, ext_if

        raise ValueError("External connection point reference %s not found" % cp_ref_name)

    def find_vdu_and_int_if_by_cp_ref(cp_ref_id):
        for vdu in rift_vnfd.vdus:
            for int_if in vdu.internal_interface:
                if int_if.vdu_internal_connection_point_ref == cp_ref_id:
                    return vdu, int_if

        raise ValueError("Internal connection point reference %s not found" % cp_ref_id)

    def rift2openmano_if_type(rift_type):
        if rift_type == "OM_MGMT":
            return "mgmt"
        elif rift_type == "VIRTIO":
            return "bridge"
        else:
            return "data"

    # Add all external connections
    for cp in rift_vnfd.cps:
        # Find the VDU and and external interface for this connection point
        vdu, ext_if = find_vdu_and_ext_if_by_cp_ref(cp.name)
        connection = {
            "name": cp.name,
            "type": rift2openmano_if_type(ext_if.virtual_interface.type_yang),
            "VNFC": vdu.name,
            "local_iface_name": ext_if.name,
            "description": "%s iface on VDU %s" % (ext_if.name, vdu.name),
            }

        vnf["external-connections"].append(connection)

    # Add all internal networks
    for vld in rift_vnfd.internal_vlds:
        connection = {
            "name": vld.name,
            "description": vld.description,
            "type": "data",
            "elements": [],
            }

        # Add the specific VDU connection points
        for int_cp_ref in vld.internal_connection_point_ref:
            vdu, int_if = find_vdu_and_int_if_by_cp_ref(int_cp_ref)
            connection["elements"].append({
                "VNFC": vdu.name,
                "local_iface_name": int_if.name,
                })
        if "internal-connections" not in vnf:
            vnf["internal-connections"] = []

        vnf["internal-connections"].append(connection)

    # Add VDU's
    vnf["VNFC"] = []
    for vdu in rift_vnfd.vdus:
        vnfc = {
            "name": vdu.name,
            "description": vdu.name,
            "VNFC image": vdu.image if os.path.isabs(vdu.image) else "/var/images/{}".format(vdu.image),
            "numas": [{
                "memory": max(int(vdu.vm_flavor.memory_mb/1024), 1),
                "interfaces":[],
                }],
            "bridge-ifaces": [],
            }

        numa_node_policy = vdu.guest_epa.numa_node_policy
        if numa_node_policy.has_field("node"):
            numa_node = numa_node_policy.node[0]

            if numa_node.has_field("paired_threads"):
                if numa_node.paired_threads.has_field("num_paired_threads"):
                    vnfc["numas"][0]["paired-threads"] = numa_node.paired_threads.num_paired_threads
                if len(numa_node.paired_threads.paired_thread_ids) > 0:
                    vnfc["numas"][0]["paired-threads-id"] = []
                    for pair in numa_node.paired_threads.paired_thread_ids:
                         vnfc["numas"][0]["paired-threads-id"].append(
                                 [pair.thread_a, pair.thread_b]
                                 )

        else:
            if vdu.vm_flavor.has_field("vcpu_count"):
                vnfc["numas"][0]["cores"] = max(vdu.vm_flavor.vcpu_count, 1)

        if vdu.has_field("hypervisor_epa"):
            vnfc["hypervisor"] = {}
            if vdu.hypervisor_epa.has_field("type"):
                if vdu.hypervisor_epa.type_yang == "REQUIRE_KVM":
                    vnfc["hypervisor"]["type"] = "QEMU-kvm"

            if vdu.hypervisor_epa.has_field("version"):
                vnfc["hypervisor"]["version"] = vdu.hypervisor_epa.version

        if vdu.has_field("host_epa"):
            vnfc["processor"] = {}
            if vdu.host_epa.has_field("om_cpu_model_string"):
                vnfc["processor"]["model"] = vdu.host_epa.om_cpu_model_string
            if vdu.host_epa.has_field("om_cpu_feature"):
                vnfc["processor"]["features"] = []
                for feature in vdu.host_epa.om_cpu_feature:
                    vnfc["processor"]["features"].append(feature)


        if vdu.vm_flavor.has_field("storage_gb"):
            vnfc["disk"] = vdu.vm_flavor.storage_gb

        vnf["VNFC"].append(vnfc)

        for int_if in list(vdu.internal_interface) + list(vdu.external_interface):
            intf = {
                "name": int_if.name,
                }
            if int_if.virtual_interface.has_field("vpci"):
                intf["vpci"] = int_if.virtual_interface.vpci

            if int_if.virtual_interface.type_yang in ["VIRTIO", "OM_MGMT"]:
                vnfc["bridge-ifaces"].append(intf)

            elif int_if.virtual_interface.type_yang == "SR-IOV":
                intf["bandwidth"] = "10 Gbps"
                intf["dedicated"] = "yes:sriov"
                vnfc["numas"][0]["interfaces"].append(intf)

            elif int_if.virtual_interface.type_yang == "PCI_PASSTHROUGH":
                intf["bandwidth"] = "10 Gbps"
                intf["dedicated"] = "yes"
                if "interfaces" not in vnfc["numas"][0]:
                    vnfc["numas"][0]["interfaces"] = []
                vnfc["numas"][0]["interfaces"].append(intf)
            else:
                raise ValueError("Interface type %s not supported" % int_if.virtual_interface)

            if int_if.virtual_interface.has_field("bandwidth"):
                if int_if.virtual_interface.bandwidth != 0:
                    bps = int_if.virtual_interface.bandwidth

                    # Calculate the bits per second conversion
                    for x in [('M', 1000000), ('G', 1000000000)]:
                        if bps/x[1] >= 1:
                            intf["bandwidth"] = "{} {}bps".format(math.ceil(bps/x[1]), x[0])


    return openmano_vnf


def parse_args(argv=sys.argv[1:]):
    """ Parse the command line arguments

    Arguments:
        arv - The list of arguments to parse

    Returns:
        Argparse Namespace instance
    """
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-o', '--outdir',
        default='-',
        help="Directory to output converted descriptors. Default is stdout",
        )

    parser.add_argument(
        '-n', '--nsd-file-hdl',
        metavar="nsd_xml_file",
        type=argparse.FileType('r'),
        help="Rift NSD Descriptor File",
        )

    parser.add_argument(
        '-v', '--vnfd-file-hdls',
        metavar="vnfd_xml_file",
        action='append',
        type=argparse.FileType('r'),
        help="Rift VNFD Descriptor File",
        )

    args = parser.parse_args(argv)

    if not os.path.exists(args.outdir):
        os.makedirs(args.outdir)

    if not is_writable_directory(args.outdir):
        logging.error("Directory %s is not writable", args.outdir)
        sys.exit(1)

    return args


def write_yaml_to_file(name, outdir, desc_dict):
    file_name = "%s.yaml" % name
    yaml_str = yaml.dump(desc_dict)
    if outdir == "-":
        sys.stdout.write(yaml_str)
        return

    file_path = os.path.join(outdir, file_name)
    dir_path = os.path.dirname(file_path)
    if not os.path.exists(dir_path):
        os.makedirs(dir_path)

    with open(file_path, "w") as hdl:
        hdl.write(yaml_str)

    logger.info("Wrote descriptor to %s", file_path)


def main(argv=sys.argv[1:]):
    args = parse_args(argv)

    nsd = None
    if args.vnfd_file_hdls is not None:
        vnf_dict = create_vnfd_from_xml_files(args.vnfd_file_hdls)

    if args.nsd_file_hdl is not None:
        nsd = create_nsd_from_xml_file(args.nsd_file_hdl)

    openmano_nsd = rift2openmano_nsd(nsd, vnf_dict)

    write_yaml_to_file(openmano_nsd["name"], args.outdir, openmano_nsd)

    for vnf in vnf_dict.values():
        openmano_vnf = rift2openmano_vnfd(vnf)
        write_yaml_to_file(openmano_vnf["vnf"]["name"], args.outdir, openmano_vnf)


if __name__ == "__main__":
    logging.basicConfig(level=logging.WARNING)
    main()
