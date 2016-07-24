#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import argparse
import logging
import os
import sys
import time

import rw_peas
import rwlogger

from gi.repository import RwcalYang

import rift.rwcal.cloudsim
import rift.rwcal.cloudsim.lxc as lxc

logger = logging.getLogger('rift.cal')


def main(argv=sys.argv[1:]):
    """
    Assuming that an LVM backing-store has been created with a volume group
    called 'rift', the following creates an lxc 'image' and a pair of 'vms'.
    In the LXC based container CAL, an 'image' is container and a 'vm' is a
    snapshot of the original container.

    In addition to the LVM backing store, it is assumed that there is a network
    bridge called 'virbr0'.

    """
    logging.basicConfig(level=logging.DEBUG)

    parser = argparse.ArgumentParser()
    parser.add_argument('--rootfs', '-r')
    parser.add_argument('--num-vms', '-n', type=int, default=2)
    parser.add_argument('--terminate', '-t', action='store_true')

    args = parser.parse_args(argv)

    # Acquire the plugin from peas
    plugin = rw_peas.PeasPlugin('rwcal-plugin', 'RwCal-1.0')
    engine, info, extension = plugin()

    # Get the RwLogger context
    rwloggerctx = rwlogger.RwLog.Ctx.new("Cal-Log")

    cal = plugin.get_interface("Cloud")
    cal.init(rwloggerctx)

    # The account object is not currently used, but it is required by the CAL
    # interface, so we create an empty object here to represent it.
    account = RwcalYang.CloudAccount()
    account.account_type = "lxc"

    # Make sure that any containers that were previously created have been
    # stopped and destroyed.
    containers = lxc.containers()

    for container in containers:
        lxc.stop(container)

    for container in containers:
        lxc.destroy(container)

    template = os.path.join(
            os.environ['RIFT_INSTALL'],
            'etc/lxc-fedora-rift.lxctemplate',
            )

    logger.info(template)
    logger.info(args.rootfs)

    # Create an image that can be used to create VMs
    image = RwcalYang.ImageInfoItem()
    image.name = 'rift-master'
    image.lxc.size = '2.5G'
    image.lxc.template_path = template
    image.lxc.tarfile = args.rootfs

    cal.create_image(account, image)

    # Create a VM
    vms = []
    for index in range(args.num_vms):
        vm = RwcalYang.VMInfoItem()
        vm.vm_name = 'rift-s{}'.format(index + 1)
        vm.image_id = image.id

        cal.create_vm(account, vm)

        vms.append(vm)

    # Create the default and data networks
    network = RwcalYang.NetworkInfoItem(network_name='virbr0')
    cal.create_network(account, network)

    os.system('brctl show')

    # Create pairs of ports to connect the networks
    for index, vm in enumerate(vms):
        port = RwcalYang.PortInfoItem()
        port.port_name = "eth0"
        port.network_id = network.network_id
        port.vm_id = vm.vm_id
        port.ip_address = "192.168.122.{}".format(index + 101)
        port.lxc.veth_name = "rws{}".format(index)

        cal.create_port(account, port)

    # Swap out the current instance of the plugin to test that the data is
    # shared among different instances
    cal = plugin.get_interface("Cloud")
    cal.init()

    # Start the VMs
    for vm in vms:
        cal.start_vm(account, vm.vm_id)

    lxc.ls()

    # Exit if the containers are not supposed to be terminated
    if not args.terminate:
        return

    time.sleep(3)

    # Stop the VMs
    for vm in vms:
        cal.stop_vm(account, vm.vm_id)

    lxc.ls()

    # Delete the VMs
    for vm in vms:
        cal.delete_vm(account, vm.vm_id)

    # Delete the image
    cal.delete_image(account, image.id)


if __name__ == "__main__":
    main()
