#!/usr/bin/env python3
"""
# STANDARD_RIFT_IO_COPYRIGHT #

@file add_vnf_vm.py
@author Karun Ganesharatnam (karun.ganesharatnam@riftio.com)
@date 02/05/2016
@summary: This script will add a VM to the Multi-VNF VM system
"""

import argparse
import logging
import json
import jsonschema
import os
import sys
import time

import kazoo.exceptions
import rift.auto.ip_utils
import rift.cal
import rift.cal.rwzk
import rift.vcs.demo

# Schema to validate VNF JSON data
jschema_file = os.path.join(os.environ.get('RIFT_INSTALL'),
                            'usr/rift/systemtest/util/multi_vm_vnf/json/vm_data_schema.json')

logging.basicConfig(format='%(asctime)-15s %(levelname)s %(message)s')
logger = logging.getLogger(__name__)

class IPAddressShortageException(Exception):
    pass

def validated_vnf_json(args, jschema_file):
    """
    Validate, if given as argument, the JSON file against the schema
    and return the parsed JSON object

    Arguments:
        args - Parsed args object (return from parser.parse_args())
        jschema_file - schema file name to validate the json

    Return:
        Parsed JSON object
    """
    vnf_info = None
    if args.vnf_json is not None:
        vnf_info = json.loads(open(args.vnf_json.name).read())
        vnf_schema = json.loads(open(jschema_file).read())
        jsonschema.validate(vnf_info, vnf_schema)
    return vnf_info

def lock_node_and_get_data(zk_client, lock_path):
    """
    To acquire Zookeeper lock for lock_path

    Arguments:
        zk_client - Zookeeper client connection
        lock_path - lock path

    """
    # Try 6 times to lock the node at 10 seconds interval
    max_tries = 6
    uniq_id = None
    for num in range(max_tries):
      try:
        zk_client.lock_node(lock_path, 100000.0)
        try:
          uniq_id = zk_client.get_node_data(lock_path)
        except kazoo.exceptions.NoNodeError:
          zk_client.unlock_node(lock_path)
          logger.debug("Re-trying to get_node_data after %d seconds" % num * 10)
          time.sleep(10)
          continue
        break
      except kazoo.exceptions.NoNodeError:
        time.sleep(10)
        logger.debug("Re-trying to lock the node after %d seconds" % num * 10)
    else:
        logger.error("Failed to lock node within %d seconds" % max_tries * 10)
        sys.exit(1)
    return uniq_id


def check_for_ip_pool_availability(vnfs, args):
    """
    Verify the number of IP addresses provided for master VMs as well as VNF VMs
    are sufficient to created required number of VMs for the VNF system

    Arguments:
        vnfs - Parsed vnf data
        args - Parsed command line argument object

    """
    err_msg = "Too short {comp} VM IP address list. {need} VMs are needed, but only {avail} were provided."
    num_vnf_vms = 0
    num_master_vms = 0

    num_avail_master_ips = len(args.master_vm_ip_address)
    num_avail_vnf_ips = len(args.vnf_vm_ip_address)

    for vnf in vnfs:
        num_master_vms += 1
        for vm in vnf["vms"]:
            num_vnf_vms += vm["vm-count"]

    if num_master_vms > num_avail_master_ips:
        raise IPAddressShortageException(err_msg.format(
                    comp="Master", need=num_master_vms, avail=num_avail_master_ips))
    if num_vnf_vms > num_avail_vnf_ips:
        raise IPAddressShortageException(err_msg.format(
                    comp="VNF", need=num_vnf_vms, avail=num_avail_vnf_ips))


def parse_command_line_arguments(cmd_arg):
    parser = argparse.ArgumentParser()
    parser.add_argument('--master-vm-ip-address',
                              type=rift.vcs.demo.DemoArgParser.parse_ip_list,
                              help="Specify a list of IP addresses to use as addresses for master VMs in the topology"
                                   "The addresses may be a comma or space delimited list, or follow the more concise  "
                                   "notation defined in rift.auto.ip_utils.ip_list_from_string.",
                              required=True)

    parser.add_argument('--vnf-vm-ip-address',
                              type=rift.vcs.demo.DemoArgParser.parse_ip_list,
                              help="Specify a list of IP addresses to use as addresses for each VM of VNFs in the topology"
                                   "The addresses may be a comma or space delimited list, or follow the more concise  "
                                   "notation defined in rift.auto.ip_utils.ip_list_from_string.",
                              required=True)

    parser.add_argument('--vnf-id',
                        help="ID of the VNF to add the VM to (cluster name defined in 'sysinfo'), and is "
                            "required unless the 'vnf-json' is specified")

    parser.add_argument('--vdu-id',
                        help="ID of the VDU (VM's name defined in 'sysinfo'), and is "
                            "required unless the 'vnf-json' is specified")

    parser.add_argument('--vnf-json',
                       type=argparse.FileType(mode='r'),
                       help="Path name of the Json file that contains detail of the VMs of VNFs. "
                            "A sample JSON file is in this directory.")

    parser.add_argument('--verbose',
                        action='store_true',
                        help="Flag to turn the debug log on")

    args = parser.parse_args(cmd_arg)

    # Arguments vnf-json or both of the vnf-id and vdu-id are required
    if args.vnf_json is None:
        if args.vnf_id is None:
            parser.error("'vnf-id' or 'vnf-json' is a required argument")
        if args.vdu_id is None:
            parser.error("'vdu-id' or 'vnf-json' is a required argument")
        if len(args.master_vm_ip_address) > 1:
            parser.error("Multiple Master IP addresses can only be used along with 'vnf-json' argument.")

    return args


if __name__ == '__main__':
    args = parse_command_line_arguments(sys.argv[1:])

    master_vm_ip_address = list(args.master_vm_ip_address)
    vnf_vm_ip_address = list(args.vnf_vm_ip_address)


    # Set debug log level
    if args.verbose:
        logger.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.INFO)

    # Validate and construct the JSON data from file
    vnf_json = validated_vnf_json(args, jschema_file)

    # If JSON file is not provided, construct the VNFs data using the arguments 'vnf-id', 'vdu-id'
    # and vnf-vm-ip-address
    if vnf_json is None:
        vnfs = [
            {
                "vnf-id": args.vnf_id,
                "vms": [
                     {
                        "vdu-id": args.vdu_id,
                        "vm-count": len(vnf_vm_ip_address)
                     }
                ]
            }]

    else:
        vnfs = vnf_json['vnfs']

    # Check if enough IP address are given as argument
    check_for_ip_pool_availability(vnfs, args)

    lock_path = '/sys/rwmain/iamup-LOCK'

    # Create VM instances
    for vnf in vnfs:
        vms = vnf["vms"]
        vnf_id = vnf["vnf-id"]

        # Set master VM address from the pool
        master_ip_addr = master_vm_ip_address.pop(0)

        # Initialize Zookeeper client and connect to server running at 'master_ip_addr'
        zk_client = rift.cal.rwzk.Kazoo()
        zk_client.client_init(unique_ports = False, server_names = [ master_ip_addr ])

        # Create VM instances
        for vm in vms:
            # Vdu id to be used to create the VM
            vdu_id = vm["vdu-id"]

            # Number of vms to be created with this vdu & vnf-id
            vm_count = vm["vm-count"]

            # Created required number instances of VMs
            for _ in range(vm_count):
                # Set VNF VM address from the pool
                vm_ip_addr = vnf_vm_ip_address.pop(0)

                logger.debug("Instantiating the VM with IP: %s, Master IP: %s, Vnf-Id: %s, Vdu-Id: %s",
                             vm_ip_addr, master_ip_addr, vnf_id, vdu_id)

                # Try to acquire Zookeeper lock for lock_path
                uniq_id = lock_node_and_get_data(zk_client, lock_path)

                new_uniq_id_val = int(uniq_id) + 1
                new_uniq_id = '%d' %(new_uniq_id_val)
                zk_client.set_node_data(lock_path, new_uniq_id.encode(), None)

                node_path = '/sys/rwmain/iamup/%s' %(vm_ip_addr)
                try:
                  zk_client.create_node(node_path)
                except kazoo.exceptions.NodeExistsError:
                  pass

                # Write the details at the node
                vm_data = '%s:%s:%s:%u' %(vnf_id, vdu_id, vm_ip_addr, new_uniq_id_val)
                zk_client.set_node_data(node_path, vm_data.encode(), None)
                vm_data = zk_client.get_node_data(node_path)

                data = vm_ip_addr
                zk_client.set_node_data('/sys/rwmain/iamup', data.encode(), None)
                logger.debug("VM Data: %s", vm_data.decode())

                zk_client.unlock_node(lock_path)
                time.sleep(45)
