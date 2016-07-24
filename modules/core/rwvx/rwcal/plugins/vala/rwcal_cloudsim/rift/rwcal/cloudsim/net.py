
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import collections

import netifaces

from . import shell


class VirshError(Exception):
    pass


def create(network, ip_interface=None):
    """ Create, assign ip and bring up a bridge interface

    Arguments:
        network - The network name
        ip_interface - An ipaddress.IPv4Interface instance
    """
    bridge_add(network)
    if ip_interface is not None:
        bridge_addr(
                network,
                str(ip_interface),
                str(ip_interface.network.broadcast_address),
                )
    bridge_up(network)


def delete(network):
    bridge_down(network)
    bridge_remove(network)


def bridge_add(network):
    shell.command("brctl addbr {network}".format(network=network))


def bridge_remove(network):
    shell.command("brctl delbr {network}".format(network=network))


def bridge_addr(network, addr, broadcast):
    cmd = "ip addr add {addr} broadcast {broadcast} dev {network}"
    shell.command(cmd.format(addr=addr, broadcast=broadcast, network=network))


def bridge_exists(network):
    return network in netifaces.interfaces()


def bridge_down(network):
    shell.command('ip link set {network} down'.format(network=network))


def bridge_up(network):
    shell.command('ip link set {network} up'.format(network=network))


def bridge_addresses(network):
    try:
        address = netifaces.ifaddresses(network)[netifaces.AF_INET][0]

    except KeyError:
        raise ValueError('unable to find subnet for {}'.format(network))

    cls = collections.namedtuple('BridgeAddresses', 'addr netmask broadcast')
    return cls(**address)


VirshNetwork = collections.namedtuple(
    'VirshNetwork', 'name state autostart persistant')


def virsh_list_networks():
    lines = shell.command('virsh net-list --all')
    if len(lines) < 2:
        raise Exception("Expected two lines from virsh net-list output")

    network_lines = lines[2:]
    virsh_networks = []
    for line in network_lines:
        if not line.strip():
            continue

        (name, state, autostart, persistant) = line.split()
        virsh_networks.append(
                VirshNetwork(name, state, autostart, persistant)
                )

    return virsh_networks


def virsh_list_network_names():
    virsh_networks = virsh_list_networks()
    return [n.name for n in virsh_networks]


def virsh_is_active(network_name):
    virsh_networks = virsh_list_networks()
    for network in virsh_networks:
        if network.name == network_name:
            return network.state == "active"

    raise VirshError("Did not find virsh network %s" % network_name)


def virsh_define_default():
    shell.command('virsh net-define /usr/share/libvirt/networks/default.xml')


def virsh_start(network_name):
    shell.command('virsh net-start %s' % network_name)


def virsh_initialize_default():
    if "default" not in virsh_list_network_names():
        virsh_define_default()

    if virsh_is_active("default"):
        if bridge_exists("virbr0"):
            bridge_down("virbr0")

        virsh_destroy("default")

    virsh_start("default")


def virsh_destroy(network_name):
    shell.command('virsh net-destroy %s' % network_name)

