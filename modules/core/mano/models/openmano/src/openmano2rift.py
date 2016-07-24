#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import argparse
import itertools
import logging
import os
import sys
import tempfile
import uuid
import yaml

import gi
gi.require_version('RwYang', '1.0')
gi.require_version('RwVnfdYang', '1.0')
gi.require_version('RwNsdYang', '1.0')
from gi.repository import (
    RwYang,
    RwVnfdYang,
    RwNsdYang,
    )

logging.basicConfig(level=logging.WARNING)
logger = logging.getLogger("openmano2rift.py")


class UnknownVNFError(Exception):
    pass


class DescriptorFileWriter(object):
    def __init__(self, module_list, output_dir, output_format):
        self._model = RwYang.Model.create_libncx()
        for module in module_list:
            self._model.load_module(module)

        self._output_dir = output_dir
        self._output_format = output_format

    def _write_file(self, file_name, output):
        file_path = os.path.join(self._output_dir, file_name)
        dir_path = os.path.dirname(file_path)
        if not os.path.exists(dir_path):
            os.makedirs(dir_path)

        with open(file_path, "w") as hdl:
            hdl.write(output)

        logger.info("Wrote descriptor to %s", file_path)

    def _write_json(self, descriptor, subdir):
        self._write_file(
            '%s.json' % os.path.join(descriptor.name, subdir, descriptor.name),
            descriptor.descriptor.to_json(self._model)
            )

    def _write_xml(self, descriptor, subdir):
        self._write_file(
            '%s.xml' % os.path.join(descriptor.name, subdir, descriptor.name),
            descriptor.descriptor.to_xml_v2(self._model, pretty_print=True)
            )

    def _write_yaml(self, descriptor, subdir):
        self._write_file(
            '%s.yaml' % os.path.join(descriptor.name, subdir, descriptor.name),
            yaml.dump(descriptor.descriptor.as_dict()),
            )

    def write_descriptor(self, descriptor, subdir=""):
        if self._output_format == 'json':
            self._write_json(descriptor, subdir=subdir)

        elif self._output_format == 'xml':
            self._write_xml(descriptor, subdir=subdir)

        elif self._output_format == 'yaml':
            self._write_yaml(descriptor, subdir=subdir)


class RiftManoDescriptor(object):
    def __init__(self, openmano=None):
        self.openmano = openmano
        self.descriptor = None


class RiftNS(RiftManoDescriptor):
    def __init__(self, openmano=None):
        super().__init__(openmano)
        self.nsd_catalog = None
        self.nsd = None
        self.name = None

    def get_vnfd_id(self, vnf_list, vnf_name):
        for vnf in vnf_list:
            if vnf.name == vnf_name:
                return vnf.vnfd.id

        # Didn't find the vnf just return the vnf_name
        return vnf_name

    def openmano2rift(self, vnf_list):
        self.descriptor = RwNsdYang.YangData_Nsd_NsdCatalog()
        openmano_nsd = self.openmano.dictionary
        self.name = openmano_nsd['name']
        nsd = self.descriptor.nsd.add()
        nsd.id = str(uuid.uuid1())
        nsd.name = self.name
        nsd.short_name = self.name
        nsd.description = openmano_nsd['description']

        nodes = openmano_nsd['topology']['nodes']
        connections = openmano_nsd['topology']['connections']

        def create_consituent_vnfds():
            vnf_member_index_dict = {}

            vnfd_idx_gen = itertools.count(1)
            for key in nodes:
                node = nodes[key]
                if node['type'] != 'VNF':
                    continue

                vnfd_idx = next(vnfd_idx_gen)
                constituent_vnfd = nsd.constituent_vnfd.add()
                constituent_vnfd.member_vnf_index = vnfd_idx
                constituent_vnfd.vnfd_id_ref = self.get_vnfd_id(vnf_list, node['VNF model'])
                vnf_member_index_dict[key] = vnfd_idx

            return vnf_member_index_dict

        def create_connections(vnf_member_index_dict):
            keys = connections.keys()
            for key in keys:
                # TODO: Need clarification from TEF
                # skip the mgmtnet in OpenMANO descriptor
                if key == 'mgmtnet':
                    continue
                conn = connections[key]
                vld = nsd.vld.add()
                vld.from_dict(dict(
                    id=str(uuid.uuid1()),
                    name=key,
                    short_name=key,
                    type_yang='ELAN',
                    ))

                nodes = conn['nodes']
                for node, node_keys in [(node, node.keys()) for node in nodes]:
                    for node_key in node_keys:
                        topo_node = openmano_nsd['topology']['nodes'][node_key]
                        if topo_node['type'] == 'VNF':
                            cpref = vld.vnfd_connection_point_ref.add()
                            cpref.from_dict(dict(
                                member_vnf_index_ref=vnf_member_index_dict[node_key],
                                vnfd_id_ref=self.get_vnfd_id(vnf_list, topo_node['VNF model']),
                                #vnfd_id_ref=topo_node['VNF model'],
                                vnfd_connection_point_ref=node[node_key],
                                ))
                            if key != 'control-net':
                                vld.provider_network.physical_network = 'physnet_sriov'
                                vld.provider_network.overlay_type = 'VLAN'

        vnf_member_index_dict = create_consituent_vnfds()
        create_connections(vnf_member_index_dict)


class RiftVnfd(RiftManoDescriptor):
    def __init__(self, openmano=None):
        super().__init__(openmano)
        self.vnfd_catalog = None
        self.vnfd = None

    def find_external_connection(self, vdu_name, if_name):
        """
        Find if the vdu interface has an external connection.
        """
        openmano_vnfd = self.openmano.dictionary['vnf']
        if 'external-connections' not in openmano_vnfd:
            return None

        ext_conn_list = openmano_vnfd['external-connections']
        for ext_conn in ext_conn_list:
            if ((ext_conn['VNFC'] == vdu_name) and
                    (ext_conn['local_iface_name'] == if_name)):
                return ext_conn

        return None

    def openmano2rift(self):
        self.descriptor = RwVnfdYang.YangData_Vnfd_VnfdCatalog()
        vnfd = self.descriptor.vnfd.add()
        self.vnfd = vnfd
        vnfd.id = str(uuid.uuid1())

        openmano_vnfd = self.openmano.dictionary['vnf']
        self.name = openmano_vnfd['name']
        vnfd.name = self.name
        if "description" in openmano_vnfd:
            vnfd.description = openmano_vnfd['description']

        # Parse and add all the external connection points
        if 'external-connections' in openmano_vnfd:
            ext_conn_list = openmano_vnfd['external-connections']

            for ext_conn in ext_conn_list:
                # TODO: Fix this
                if ext_conn['name'] == 'eth0':
                    continue
                conn_point = vnfd.connection_point.add()
                conn_point.name = ext_conn['name']
                conn_point.type_yang = 'VPORT'

        # TODO: Need a concrete example of how openmano descriptor
        # uses internal connections.
        if 'internal-connections' in openmano_vnfd:
            int_conn_list = openmano_vnfd['internal-connections']

        def add_external_interfaces(vdu, numa):
            if 'interfaces' not in numa:
                return

            numa_if_list = numa['interfaces']
            for numa_if in numa_if_list:
                ext_conn = self.find_external_connection(vdu.name, numa_if['name'])
                if not ext_conn:
                    continue

                ext_iface = vdu.external_interface.add()
                ext_iface.name = numa_if['name']
                ext_iface.vnfd_connection_point_ref = ext_conn['name']
                ext_iface.virtual_interface.vpci = numa_if['vpci']
                if numa_if['dedicated'] == 'no':
                    ext_iface.virtual_interface.type_yang = 'SR_IOV'
                else:
                    ext_iface.virtual_interface.type_yang = 'PCI_PASSTHROUGH'

        vnfc_list = openmano_vnfd['VNFC']
        for vnfc in vnfc_list:
            vdu = vnfd.vdu.add()
            vdu_dict = dict(
                id=str(uuid.uuid1()),
                name=vnfc['name'],
                image=vnfc['VNFC image'],
                vm_flavor={"storage_gb": vnfc["disk"] if "disk" in vnfc else 20},
                )
            if "description" in vnfc:
                vdu_dict["description"] = vnfc['description']

            vdu.from_dict(vdu_dict)

            vnfd.mgmt_interface.vdu_id = vdu.id

            numa_list = vnfc['numas']
            memory = 0
            vcpu_count = 0
            numa_node_cnt = 0

            for numa in numa_list:
                node = vdu.guest_epa.numa_node_policy.node.add()
                node.id = numa_node_cnt
                # node.memory_mb = int(numa['memory']) * 1024
                numa_node_cnt += 1

                memory = memory + node.memory_mb
                # Need a better explanation of "cores", "paired-threads", "threads"
                # in openmano descriptor. Particularly how they map to cpu and
                # thread pinning policies
                if 'paired-threads' in numa:
                    vcpu_count = vcpu_count + int(numa['paired-threads']) * 2

                if 'cores' in numa:
                    vcpu_count = vcpu_count + int(numa['cores'])

                add_external_interfaces(vdu, numa)


            # vdu.vm_flavor.memory_mb = memory
            vdu.vm_flavor.memory_mb = 12 * 1024
            vdu.vm_flavor.vcpu_count = vcpu_count
            vdu.guest_epa.numa_node_policy.node_cnt = numa_node_cnt
            vdu.guest_epa.numa_node_policy.mem_policy = 'STRICT'
            vdu.guest_epa.mempage_size = 'LARGE'
            vdu.guest_epa.cpu_pinning_policy = 'DEDICATED'
            vdu.guest_epa.cpu_thread_pinning_policy = 'PREFER'

            # TODO: Enable hypervisor epa
            # vdu.hypervisor_epa.version = vnfc['hypervisor']['version']
            # if vnfc['hypervisor']['type'] == 'QEMU-kvm':
            #     vdu.hypervisor_epa.type_yang = 'REQUIRE_KVM'
            # else:
            #     vdu.hypervisor_epa.type_yang = 'PREFER_KVM'

            # TODO: Enable host epa
            # vdu.host_epa.cpu_feature = vnfc['processor']['features']

            # Parse the bridge interfaces
            if 'bridge-ifaces' in vnfc:
                bridge_ifaces = vnfc['bridge-ifaces']


                for bridge_iface in bridge_ifaces:
                    # TODO: Fix this
                    if bridge_iface['name'] == 'eth0':
                        continue

                    ext_conn = self.find_external_connection(vdu.name,
                                                             bridge_iface['name'])
                    if ext_conn:
                        ext_iface = vdu.external_interface.add()
                        ext_iface.name = bridge_iface['name']
                        ext_iface.vnfd_connection_point_ref = ext_conn['name']
                        if 'vpci' in bridge_iface:
                            ext_iface.virtual_interface.vpci = bridge_iface['vpci']
                        ext_iface.virtual_interface.type_yang = 'VIRTIO'

            # set vpci information for the 'default' network
            # TODO: This needs to be inferred gtom bridge ifaces, 
            # need input from TEF
            vdu.mgmt_vpci = "0000:00:0a.0"


class OpenManoDescriptor(object):
    def __init__(self, yaml_file_hdl):
        self.dictionary = yaml.load(yaml_file_hdl)

    @property
    def type(self):
        """ The descriptor type (ns or vnf)"""
        if 'vnf' in self.dictionary:
            return "vnf"
        else:
            return "ns"

    def dump(self):
        """ Dump the Descriptor out to stdout """
        print(yaml.dump(self.dictionary))


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


def create_vnfs_from_yaml_files(yaml_file_hdls):
    """ Create a list of RiftVnfd instances from yaml file handles

    Arguments:
        yaml_file_hdls - OpenMano Yaml file handles

    Returns:
        A list of RiftVnfd instances
    """
    vnf_list = []
    for yaml_file_hdl in yaml_file_hdls:
        openmano = OpenManoDescriptor(yaml_file_hdl)
        yaml_file_hdl.seek(0)

        if openmano.type != "vnf":
            continue

        vnf = RiftVnfd(openmano)
        vnf.openmano2rift()
        vnf_list.append(vnf)

    return vnf_list


def create_ns_from_yaml_files(yaml_file_hdls, vnf_list):
    """ Create a list of RiftNS instances from yaml file handles

    Arguments:
        yaml_file_hdls - OpenMano Yaml file handles
        vnf_list - list of RiftVnfd

    Returns:
        A list of RiftNS instances
    """
    ns_list = []
    for yaml_file_hdl in yaml_file_hdls:
        openmano = OpenManoDescriptor(yaml_file_hdl)
        if openmano.type != "ns":
            continue

        net_svc = RiftNS(openmano)
        net_svc.openmano2rift(vnf_list)
        ns_list.append(net_svc)

    return ns_list


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
        default='.',
        help="Directory to output converted descriptors",
        )

    parser.add_argument(
        '-f', '--format',
        choices=['yaml', 'xml', 'json'],
        default='xml',
        help="Descriptor output format",
        )

    parser.add_argument(
        'yaml_file_hdls',
        metavar="yaml_file",
        nargs="+",
        type=argparse.FileType('r'),
        help="OpenMano YAML Descriptor File",
        )

    args = parser.parse_args(argv)

    if not os.path.exists(args.outdir):
        os.makedirs(args.outdir)

    if not is_writable_directory(args.outdir):
        logging.error("Directory %s is not writable", args.outdir)
        sys.exit(1)

    return args


def main(argv=sys.argv[1:]):
    args = parse_args(argv)

    vnf_list = create_vnfs_from_yaml_files(args.yaml_file_hdls)
    ns_list = create_ns_from_yaml_files(args.yaml_file_hdls, vnf_list)

    writer = DescriptorFileWriter(
        module_list=['nsd', 'rw-nsd', 'vnfd', 'rw-vnfd'],
        output_dir=args.outdir,
        output_format=args.format,
        )

    for nw_svc in ns_list:
        writer.write_descriptor(nw_svc, subdir="nsd")

    for vnf in vnf_list:
        writer.write_descriptor(vnf, subdir="vnfd")


if __name__ == "__main__":
    main()

