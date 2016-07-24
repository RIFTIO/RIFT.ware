#!/usr/bin/python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 07/24/2014
# 

"""
This is a skeletal python tool that invokes the rwcal plugin
to perform cloud operations.
"""

import argparse
import os
import socket
import sys
import logging

from gi import require_version
require_version('RwCal', '1.0')

from gi.repository import GObject, Peas, GLib, GIRepository
from gi.repository import RwCal, RwTypes

def resource_list_subcommand(rwcloud, cmdargs):
    status, flavorinfo = rwcloud.get_flavor_list()
    status, vminfo = rwcloud.get_vm_list()
    if vminfo is None:
        return

    hosts = {}

    # no yaml module installed for Python3, hack for now
    if cmdargs.hostinfo_file_name:
        with open(cmdargs.hostinfo_file_name, 'r') as f:
            lines = f.readlines()

        host = None
        for l in lines:
            l = l.strip()

            if l == 'hosts:':
                continue

            if l == '-':
                if host:
                    hosts[host['name']] = host
                    #hosts.append(host)
                host = {}
                continue

            k,v = l.split(':')
            host[k.strip()] = v.strip()

    # Collect the unique Top of Rack (TOR) swithes
    tors = set(hosts[vm.host_name]['tor'].lower() for vm in vminfo.vminfo_list)

    print("resources:")
    for vm in vminfo.vminfo_list:
        _, __, host_ip_list  = socket.gethostbyaddr(vm.host_name)

        print(" -")
        print("    name: {}".format(vm.vm_name))
        print("    osid: {}".format(vm.vm_id))
        print("    host_name: {}".format(vm.host_name))
        print("    host_ip: {}".format(host_ip_list[0]))
        controller, scratch = cmdargs.auth_url[7:].split(':')
        print("    controller: {}".format(controller))
        print("    tor: {}".format(hosts[vm.host_name]['tor']))
        print("    image_name: {}".format(vm.image_name))
        print("    flavor_name: {}".format(vm.flavor_name))
        print("    availability_zone: {}".format(vm.availability_zone))
        print("    private_ip_list: {}".format(
                sorted(v.ip_address for v in vm.private_ip_list)
        ))
        # select the 10.0 network for management ip
        for p in vm.private_ip_list:
            if p.ip_address.startswith('10.0.'):
                print("    ip_address: {}".format(p.ip_address))
                break;

        print("    public_ip_list: {}".format(
                [v.ip_address for v in vm.public_ip_list]
        ))
        for flavor in flavorinfo.flavorinfo_list:
            if flavor.name == vm.flavor_name:
                print("    vcpu: {}".format(flavor.vcpus))
                print("    ram: {}".format(flavor.memory))
                print("    disk: {}".format(flavor.disk))
                print("    host_aggregate_list: {}".format(
                        [a.name for a in flavor.host_aggregate_list]
                ))
                print("    pci_passthrough_device_list: {}".format(
                        [(d.alias,d.count) for d in flavor.pci_passthrough_device_list]
                ))
                # Number of openflow switches this resource connects to are the
                # number of TOR switches for the pool for demos
                print("    num_openflow_switches: {}".format(len(tors)))
                # The number of legacy switches are 0 for demos
                print("    num_legacy_switches: 0")
                print("    epa_attributes:")

                # HACK: rw_wag* VMs trusted_execution is always TRUE
                if vm.vm_name.startswith('rw_wag'):
                    trusted_execution = 'TRUE'
                else:
                    trusted_execution = str(flavor.trusted_host_only).upper()
                print("        trusted_execution: {}".format(trusted_execution))
                print("        ddio: {}".format(hosts[vm.host_name]['ddio']))
                print("        cat: {}".format(hosts[vm.host_name]['cat']))
                print("        ovs_acceleration: {}".format(hosts[vm.host_name]['ovs_acceleration']))
                print("        mem_page_size: {}".format(flavor.mem_page_size))
                if flavor.cpu_threads:
                    print("        cpu_threads: {}".format(flavor.cpu_threads))
                print("        cpu_pinning_policy: {}".format(flavor.cpu_policy))
                # print("            numa_policy: {{ node_cnt: {} }}".format(flavor.numa_node_cnt))
                print("        numa_node_cnt: {}".format(flavor.numa_node_cnt))

                # if any of the PCI passthrough devices are Coleto Creek
                # set qat to accel
                qat=False
                passthrough=False
                rrc=False
                for d in flavor.pci_passthrough_device_list:
                    if 'COLETO' in d.alias:
                        qat=True
                        break
                    elif '10G' in d.alias:
                        passthrough=True
                    elif '100G' in d.alias:
                        passthrough=True
                        rrc=True
                # NOTE: The following can break if SRIOV is used
                # But for the demos 1,2,3 SRIOV is not used
                # This is updated logic to set the nic to default to Niantic
                # if 100G is not in the devise list.
                if rrc:
                    print("        nic: RRC")
                else:
                    print("        nic: NIANTIC")

                if passthrough or hosts[vm.host_name]['ovs_acceleration'].upper() != 'DISABLED':
                    print("        dpdk_accelerated: TRUE")
                else:
                    print("        dpdk_accelerated: FALSE")

                if passthrough:
                    print("        pci_passthrough: TRUE")
                else:
                    print("        pci_passthrough: FALSE")

                if qat:
                    print("        quick_assist_policy: MANDATORY")
                else:
                    print("        quick_assist_policy: NOACCEL")

                break
    
def resource_subcommand(rwcloud, cmdargs):
    """Process the resources subcommand"""

    if cmdargs.which == 'list':
        resource_list_subcommand(rwcloud, cmdargs)

def vm_list_subcommand(rwcloud, cmdargs):
    status, vminfo = rwcloud.get_vm_list()
    for vm in vminfo.vminfo_list:
        print(vm)

def vm_show_subcommand(rwcloud, cmdargs):
    status, vm = rwcloud.get_vm(cmdargs.id)
    print(vm)

def vm_create_subcommand(cmdargs):
    pass

def vm_destroy_subcommand(cmdargs):
    pass

def vm_reboot_subcommand(cmdargs):
    pass

def vm_start_subcommand(cmdargs):
    pass

def vm_subcommand(rwcloud, cmdargs):
    """Process the vm subcommand"""

    if cmdargs.which == 'list':
        vm_list_subcommand(rwcloud, cmdargs)
    elif cmdargs.which == 'show':
        vm_show_subcommand(rwcloud, cmdargs)
    elif cmdargs.which == 'create':
        vm_create_subcommand(cmdargs)
    elif cmdargs.which == 'reboot':
        vm_reboot_subcommand(cmdargs)
    elif cmdargs.which == 'start':
        vm_start_subcommand(cmdargs)
    elif cmdargs.which == 'destroy':
        vm_destroy_subcommand(cmdargs)

def flavor_list_subcommand(rwcloud, cmdargs):
    status, flavorinfo = rwcloud.get_flavor_list()
    for flavor in flavorinfo.flavorinfo_list:
        print(flavor)

def flavor_show_subcommand(rwcloud, cmdargs):
    status, flavor = rwcloud.get_flavor(cmdargs.id)
    print(flavor)

def flavor_subcommand(rwcloud, cmdargs):
    """Process the flavor subcommand"""

    if cmdargs.which == 'list':
        flavor_list_subcommand(rwcloud, cmdargs)
    elif cmdargs.which == 'show':
        flavor_show_subcommand(rwcloud, cmdargs)


def main(args=sys.argv[1:]):
    logging.basicConfig(format='RWCAL %(message)s')

    ##
    # Command line argument specification
    ##
    desc="""This tool is used to manage the VMs"""
    parser = argparse.ArgumentParser(description=desc)
    subparsers = parser.add_subparsers()

    # ipaddr = socket.gethostbyname(socket.getfqdn())
    # default_auth_url = 'http://%s:35357/v2.0/tokens' % ipaddr
    default_auth_url = "http://10.64.1.31:35357/v2.0/tokens"

    parser.add_argument('-t', '--provider-type', dest='provider_type',
                        type=str, default='OPENSTACK',
                        help='Cloud provider type (default: %(default)s)')
    parser.add_argument('-u', '--user-name', dest='user',
                        type=str, default='demo',
                        help='User name (default: %(default)s)')
    parser.add_argument('-p', '--password', dest='passwd',
                        type=str, default='mypasswd',
                        help='Password (default: %(default)s)')
    parser.add_argument('-n', '--tenant-name', dest='tenant',
                        type=str, default='demo',
                        help='User name (default: %(default)s)')
    parser.add_argument('-a', '--auth-url', dest='auth_url',
                        type=str, default=default_auth_url,
                        help='Password (default: %(default)s)')

    ##
    # Subparser for Resources
    ##
    resource_parser = subparsers.add_parser('resource')
    resource_subparsers = resource_parser.add_subparsers()

    # List resource subparser
    resource_list_parser = resource_subparsers.add_parser('list')
    resource_list_parser.set_defaults(which='list')
    resource_list_parser.add_argument('-f', '--hostinfo-file-name', 
                                  dest='hostinfo_file_name', 
                                  required=True,
                                  type=str,
                                  help='name of the static yaml file containing host information')

    resource_parser.set_defaults(func=resource_subcommand)

    ##
    # Subparser for Flavor
    ##
    flavor_parser = subparsers.add_parser('flavor')
    flavor_subparsers = flavor_parser.add_subparsers()

    # List flavor subparser
    flavor_list_parser = flavor_subparsers.add_parser('list')
    flavor_list_parser.set_defaults(which='list')

    # Show flavor subparser
    flavor_show_parser = flavor_subparsers.add_parser('show')
    flavor_show_parser.add_argument('id', type=str)
    flavor_show_parser.set_defaults(which='show')

    flavor_parser.set_defaults(func=flavor_subcommand)

    ##
    # Subparser for VM
    ##
    vm_parser = subparsers.add_parser('vm')
    vm_subparsers = vm_parser.add_subparsers()

    # Create VM subparser
    vm_create_parser = vm_subparsers.add_parser('create')
    vm_create_parser.add_argument('-c', '--count',
                                  type=int, default=1,
                                  help='The number of VMs to launch '
                                       '(default: %(default)d)')
    vm_create_parser.add_argument('-i', '--image',
                                  default='rwopenstack_vm',
                                  help='Specify the image for the VM')
    vm_create_parser.add_argument('-n', '--name',
                                  help='Specify the name of the VM')
    vm_create_parser.add_argument('-f', '--flavor',
                                  help='Specify the flavor for the VM')
    vm_create_parser.add_argument('-R', '--reserve', dest='reserve_new_vms',
                                  action='store_true', help='reserve any newly created VMs')
    vm_create_parser.add_argument('-s', '--single', dest='wait_after_create',
                                  action='store_true', 
                                  help='wait for each VM to start before creating the next')

    vm_create_parser.set_defaults(which='create')

    # Reboot VM subparser
    vm_reboot_parser = vm_subparsers.add_parser('reboot')
    group = vm_reboot_parser.add_mutually_exclusive_group()
    group.add_argument('-n', '--vm-name', dest='vm_name',
                       type=str,
                       help='Specify the name of the VM')
    group.add_argument('-a', '--reboot-all',
                       dest='reboot_all', action='store_true',
                       help='Reboot all VMs')
    vm_reboot_parser.add_argument('-s', '--sleep', 
                                  dest='sleep_time', 
                                  type=int, default=4, 
                                  help='time in seconds to sleep between reboots')
    vm_reboot_parser.set_defaults(which='reboot')

    # Destroy VM subparser
    vm_destroy_parser = vm_subparsers.add_parser('destroy')
    group = vm_destroy_parser.add_mutually_exclusive_group()
    group.add_argument('-n', '--vm-name', dest='vm_name',
                       type=str,
                       help='Specify the name of the VM (accepts regular expressions)')
    group.add_argument('-a', '--destroy-all',
                       dest='destroy_all', action='store_true',
                       help='Delete all VMs')
    group.add_argument('-w', '--wait',
                       dest='wait', action='store_true',
                       help='destroy all and wait until all VMs have exited')
    vm_destroy_parser.set_defaults(which='destroy')

    # List VM subparser
    vm_list_parser = vm_subparsers.add_parser('list')
    vm_list_parser.set_defaults(which='list')
    vm_list_parser.add_argument('-i', '--ips_only', dest='ipsonly',
                                action='store_true',
                                help='only list IP addresses')

    # Show vm subparser
    vm_show_parser = vm_subparsers.add_parser('show')
    vm_show_parser.add_argument('id', type=str)
    vm_show_parser.set_defaults(which='show')
    vm_parser.set_defaults(func=vm_subcommand)

    cmdargs = parser.parse_args(args)

    # Open the peas engine
    engine = Peas.Engine.get_default()

    # Load our plugin proxy into the g_irepository namespace
    default = GIRepository.Repository.get_default()
    GIRepository.Repository.require(default, "RwCal", "1.0", 0)

    # Enable python language loader
    engine.enable_loader("python3");

    # Set the search path for peas engine,
    # rift-shell sets the PLUGINDIR and GI_TYPELIB_PATH
    paths = set([])
    paths = paths.union(os.environ['PLUGINDIR'].split(":"))
    for path in paths:
        engine.add_search_path(path, path)

    # Load the rwcal python plugin and create the extension.
    info = engine.get_plugin_info("rwcal-plugin")
    if info is None:
        print("Error loading rwcal-python plugin")
        sys.exit(1)
    engine.load_plugin(info)
    rwcloud = engine.create_extension(info, RwCal.Cloud, None)

    # For now cloud credentials are hard coded
    if cmdargs.provider_type == 'OPENSTACK':
        provider_type = RwCal.CloudType.OPENSTACK_AUTH_URL
    elif cmdargs.provider_type == 'EC2_US_EAST':
        provider_type = RwCal.CloudType.EC2_US_EAST
    elif cmdargs.provider_type == 'VSPHERE':
        provider_type = RwCal.CloudType.VSPHERE
    else:
        sys.exit("Cloud provider %s is NOT supported yet" % cmdargs.provider_type)


    if not 'RIFT_SHELL' in os.environ:
        sys.stderr.write("This tool should be run from inside a rift-shell")

    status = rwcloud.init(provider_type, 
                          cmdargs.user, 
                          cmdargs.passwd, 
                          cmdargs.auth_url,
                          cmdargs.tenant);

    assert status == RwTypes.RwStatus.SUCCESS

    cmdargs.func(rwcloud, cmdargs)

if __name__ == "__main__":
    main()

