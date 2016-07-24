
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import collections
import itertools
import logging
import os
import uuid

import ipaddress

from gi import require_version
require_version('RwCal', '1.0')

from gi.repository import (
    GObject,
    RwCal,
    RwTypes,
    RwcalYang,
    )

import rw_status
import rwlogger

import rift.rwcal.cloudsim.lxc as lxc
import rift.rwcal.cloudsim.lvm as lvm
import rift.rwcal.cloudsim.net as net
import rift.rwcal.cloudsim.exceptions as exceptions

logger = logging.getLogger('rwcal.cloudsim')


class UnknownAccountError(Exception):
    pass


class MissingFileError(Exception):
    pass


class ImageLocationError(Exception):
    pass


class CreateNetworkError(Exception):
    pass


rwstatus = rw_status.rwstatus_from_exc_map({
    IndexError: RwTypes.RwStatus.NOTFOUND,
    KeyError: RwTypes.RwStatus.NOTFOUND,
    UnknownAccountError: RwTypes.RwStatus.NOTFOUND,
    MissingFileError: RwTypes.RwStatus.NOTFOUND,
    })


class Resources(object):
    def __init__(self):
        self.images = dict()


def rwcal_copy_object(obj):
    dup = obj.__class__()
    dup.copy_from(obj)
    return dup


MGMT_NETWORK_NAME = "virbr0"
MGMT_NETWORK_INTERFACE_IP = ipaddress.IPv4Interface("192.168.122.1/24")


class IPPoolError(Exception):
    pass


class NetworkIPPool(object):
    def __init__(self, subnet):
        self._network = ipaddress.IPv4Network(subnet)
        self._ip_gen = self._network.hosts()
        self._allocated_ips = []
        self._unallocated_ips = []

    def allocate_ip(self):
        try:
            ip = str(next(self._ip_gen))
        except StopIteration:
            try:
                ip = self._unallocated_ips.pop()
            except IndexError:
                raise IPPoolError("All ip addresses exhausted")

        self._allocated_ips.append(ip)
        return ip

    def deallocate_ip(self, ip):
        if ip not in self._allocated_ips:
            raise ValueError("Did not find IP %s in allocate ip pool")

        self._allocated_ips.remove(ip)
        self._unallocated_ips.append(ip)


class CalManager(object):
    def __init__(self):
        self._vms = {}
        self._ports = {}
        self._images = {}
        self._networks = {}
        self.flavors = {}

        self._port_to_vm = {}
        self._vm_to_image = {}
        self._port_to_network = {}
        self._network_to_ip_pool = {}

        self._vm_to_ports = collections.defaultdict(list)
        self._image_to_vms = collections.defaultdict(list)
        self._network_to_ports = collections.defaultdict(list)

        self._vm_id_gen = itertools.count(1)
        self._network_id_gen = itertools.count(1)
        self._image_id_gen = itertools.count(1)

    def add_image(self, image):
        image_id = str(next(self._image_id_gen))
        self._images[image_id] = image

        return image_id

    def remove_image(self, image_id):
        for vm_id in self.get_image_vms(image_id):
            self.remove_vm(vm_id)

        del self._images[image_id]
        del self._image_to_vms[image_id]

    def get_image(self, image_id):
        if image_id not in self._images:
            msg = "Unable to find image {}"
            raise exceptions.RWErrorNotFound(msg.format(image_id))

        return self._images[image_id]

    def get_image_list(self):
        return list(self._images.values())

    def get_image_vms(self, image_id):
        if image_id not in self._images:
            msg = "Unable to find image {}"
            raise exceptions.RWErrorNotFound(msg.format(image_id))

        return self._image_to_vms[image_id]

    def add_port(self, network_id, vm_id, port):
        if network_id not in self._networks:
            msg = "Unable to find network {}"
            raise exceptions.RWErrorNotFound(msg.format(network_id))

        if vm_id not in self._vms:
            msg = "Unable to find vm {}"
            raise exceptions.RWErrorNotFound(msg.format(vm_id))

        port_id = str(uuid.uuid4())
        self._ports[port_id] = port

        self._vm_to_ports[vm_id].append(port_id)
        self._network_to_ports[network_id].append(port_id)

        self._port_to_vm[port_id] = vm_id
        self._port_to_network[port_id] = network_id

        return port_id

    def remove_port(self, port_id):
        if port_id not in self._ports:
            msg = "Unable to find port {}"
            raise exceptions.RWErrorNotFound(msg.format(port_id))

        network_id = self._port_to_network[port_id]
        vm_id = self._port_to_vm[port_id]

        self._vm_to_ports[vm_id].remove(port_id)
        self._network_to_ports[network_id].remove(port_id)

        del self._ports[port_id]
        del self._port_to_vm[port_id]
        del self._port_to_network[port_id]

    def get_port(self, port_id):
        return self._ports[port_id]

    def get_port_list(self):
        return list(self._ports.values())

    def add_network(self, network):
        network_id = str(next(self._network_id_gen))
        self._networks[network_id] = network

        return network_id

    def remove_network(self, network_id):
        for port_id in self.get_network_ports(network_id):
            self.remove_port(port_id)

        del self._networks[network_id]

    def get_network(self, network_id):
        return self._networks[network_id]

    def add_network_ip_pool(self, network_id, ip_pool):
        self._network_to_ip_pool[network_id] = ip_pool

    def get_network_ip_pool(self, network_id):
        return self._network_to_ip_pool[network_id]

    def remove_network_ip_pool(self, network_id):
        del self._network_to_ip_pool[network_id]

    def get_network_list(self):
        return list(self._networks.values())

    def get_network_ports(self, network_id):
        return self._network_to_ports[network_id]

    def add_vm(self, image_id, vm):
        if image_id not in self._images:
            msg = "Unable to find image {}"
            raise exceptions.RWErrorNotFound(msg.format(image_id))

        vm_id = str(next(self._vm_id_gen))
        self._vms[vm_id] = vm

        self._vm_to_image[vm_id] = image_id
        self._image_to_vms[image_id].append(vm_id)

        return vm_id

    def remove_vm(self, vm_id):
        for port_id in self.get_vm_ports(vm_id):
            self.remove_port(port_id)

        image_id = self._vm_to_image[vm_id]

        self._image_to_vms[image_id].remove(vm_id)

        del self._vms[vm_id]
        del self._vm_to_image[vm_id]

    def get_vm(self, vm_id):
        return self._vms[vm_id]

    def get_vm_list(self):
        return list(self._vms.values())

    def get_vm_ports(self, vm_id):
        return self._vm_to_ports[vm_id]


class LxcManager(object):
    def __init__(self):
        self._containers = {}
        self._ports = {}
        self._bridges = {}

        self._port_to_container = {}
        self._port_to_bridge = {}

        self._container_to_ports = collections.defaultdict(list)
        self._bridge_to_ports = collections.defaultdict(list)

        # Create the management network
        self.mgmt_network = RwcalYang.NetworkInfoItem()
        self.mgmt_network.network_name = MGMT_NETWORK_NAME

        network = MGMT_NETWORK_INTERFACE_IP.network
        self.mgmt_network.subnet = str(network)

        # Create/Start the default virtd network for NAT-based
        # connectivity inside containers (http://wiki.libvirt.org/page/Networking)
        if "default" not in net.virsh_list_network_names():
            logger.debug("default virtd network not found.  Creating.")
            net.virsh_define_default()

            # The default virsh profile create a virbr0 interface
            # with a 192.168.122.1 ip address.  Also sets up iptables
            # for NAT access.
            net.virsh_start("default")

        # Create the IP pool
        mgmt_network_hosts = network.hosts()

        # Remove the management interface ip from the pool
        self._mgmt_ip_pool = list(mgmt_network_hosts)
        self._mgmt_ip_pool.remove(MGMT_NETWORK_INTERFACE_IP.ip)

    def acquire_mgmt_ip(self):
        """Returns an IP address from the available pool"""
        # TODO these ips will need to be recycled at some point
        return str(self._mgmt_ip_pool.pop())

    def add_port(self, bridge_id, container_id, port):
        if bridge_id not in self._bridges:
            msg = "Unable to find bridge {}"
            raise exceptions.RWErrorNotFound(msg.format(bridge_id))

        if container_id not in self._containers:
            msg = "Unable to find container {}"
            raise exceptions.RWErrorNotFound(msg.format(container_id))

        port_id = str(uuid.uuid4())
        self._ports[port_id] = port

        self._container_to_ports[container_id].append(port_id)
        self._bridge_to_ports[bridge_id].append(port_id)

        self._port_to_container[port_id] = container_id
        self._port_to_bridge[port_id] = bridge_id

        return port_id

    def remove_port(self, port_id):
        if port_id not in self._ports:
            msg = "Unable to find port {}"
            raise exceptions.RWErrorNotFound(msg.format(port_id))

        bridge_id = self._port_to_bridge[port_id]
        container_id = self._port_to_container[port_id]

        self._container_to_ports[container_id].remove(port_id)
        self._bridge_to_ports[bridge_id].remove(port_id)

        del self._ports[port_id]
        del self._port_to_bridge[port_id]
        del self._port_to_container[port_id]

    def get_port(self, port_id):
        return self._ports[port_id]

    def add_bridge(self, bridge):
        bridge_id = str(uuid.uuid4())
        self._bridges[bridge_id] = bridge

        return bridge_id

    def remove_bridge(self, bridge_id):
        for port_id in self._bridge_to_ports[bridge_id]:
            self.remove_port(port_id)

        del self._bridges[bridge_id]

    def get_bridge(self, bridge_id):
        return self._bridges[bridge_id]

    def get_bridge_ports(self, bridge_id):
        port_ids = self._bridge_to_ports[bridge_id]
        return [self.get_port(port_id) for port_id in port_ids]

    def add_container(self, container):
        container_id = str(uuid.uuid4())
        self._containers[container_id] = container

        return container_id

    def remove_container(self, container_id):
        for port_id in self.get_container_ports(container_id):
            self.remove_port(port_id)

        del self._containers[container_id]

    def get_container(self, container_id):
        return self._containers[container_id]

    def get_container_ports(self, container_id):
        return self._container_to_ports[container_id]



class Datastore(object):
    """
    This class is used to store data that is shared among different instance of
    the Container class.
    """
    def __init__(self):
        self.lxc_manager = LxcManager()
        self.cal_manager = CalManager()
        self.cal_to_lxc = {'image': {}, 'port': {}, 'network': {}, 'vm': {}}
        self.last_index = 0


class CloudSimPlugin(GObject.Object, RwCal.Cloud):
    # HACK this is a work-around for sharing/persisting container information.
    # This will only work for instances of CloudSimPlugin that are within the
    # same process. Thus, it works in collapsed mode, but will not work in
    # expanded mode. At the point where it is necessary to persist this
    # information in expanded mode, we will need to find a better solution.
    datastore = None

    def __init__(self):
        GObject.Object.__init__(self)
        if CloudSimPlugin.datastore is None:
            CloudSimPlugin.datastore = Datastore()

    @property
    def lxc(self):
        return CloudSimPlugin.datastore.lxc_manager

    @property
    def cal(self):
        return CloudSimPlugin.datastore.cal_manager

    @property
    def volume_group(self):
        return lvm.get("rift")

    @property
    def cal_to_lxc(self):
        return CloudSimPlugin.datastore.cal_to_lxc

    def next_snapshot_name(self):
        """Generates a new snapshot name for a container"""
        CloudSimPlugin.datastore.last_index += 1
        return 'rws{}'.format(CloudSimPlugin.datastore.last_index)

    @rwstatus
    def do_init(self, rwlog_ctx):
        if not any(isinstance(h, rwlogger.RwLogger) for h in logger.handlers):
            logger.addHandler(
                rwlogger.RwLogger(
                    category="rw-cal-log",
                    subcategory="cloudsim",
                    log_hdl=rwlog_ctx,
                )
            )

    @rwstatus(ret_on_failure=[None])
    def do_validate_cloud_creds(self, account):
        """
        Validates the cloud account credentials for the specified account.
        If creds are not valid, returns an error code & reason string
        Arguments:
            account - a cloud account to validate

        Returns:
            Validation Code and Details String
        """
        status = RwcalYang.CloudConnectionStatus(
                status="success",
                details=""
                )

        return status

    @rwstatus(ret_on_failure=[None])
    def do_get_management_network(self, account):
        """Returns the management network

        Arguments:
            account - a cloud account

        Returns:
            a NetworkInfo object

        """
        return self.lxc.mgmt_network

    @rwstatus
    def do_create_tenant(self, account, name):
        """
        Create a new tenant.

        @param name     - name to assign to the tenant.
        """
        raise NotImplementedError()

    @rwstatus
    def do_delete_tenant(self, account, tenant_id):
        """
        delete a tenant.

        @param tenant_id     - id of tenant to be deleted.
        """
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[[]])
    def do_get_tenant_list(self, account):
        """
        List tenants.

        """
        raise NotImplementedError()

    @rwstatus
    def do_create_role(self, account, name):
        """
        Create a new role.

        @param name         - name to assign to the role.
        """
        raise NotImplementedError()

    @rwstatus
    def do_delete_role(self, account, role_id):
        """
        delete a role.

        @param role_id     - id of role to be deleted.
        """
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[[]])
    def do_get_role_list(self, account):
        """
        List roles.

        """
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[None])
    def do_create_image(self, account, image):
        """Create a new image

        Creates a new container based upon the template and tarfile specified.
        Only one image is currently supported for a given instance of the CAL.

        Arguments:
            account - a cloud account
            image   - an ImageInfo object

        Raises:
            An RWErrorDuplicate is raised if create_image is called and there
            is already an image.

        Returns:
            The UUID of the new image

        """
        current_images = self.cal.get_image_list()
        lxc_name = "rwm{}".format(len(current_images))

        if not image.has_field("disk_format"):
            logger.warning("Image disk format not provided assuming qcow2")
            image.disk_format = "qcow2"

        if image.disk_format not in ["qcow2"]:
            msg = "Only qcow2 currently supported for container CAL"
            raise exceptions.RWErrorNotSupported(msg)

        # Create the base container
        if "REUSE_LXC" in os.environ and lxc_name == "rwm0":
            logger.info("REUSE_LXC set.  Not creating rwm0")
            container = lxc.Container(lxc_name)
        else:
            container = lxc.create_container(
                    name=lxc_name,
                    template_path=os.path.join(
                            os.environ['RIFT_INSTALL'],
                            "etc/lxc-fedora-rift.lxctemplate",
                            ),
                    volume="rift",
                    rootfs_qcow2file=image.location,
                    )

        # Add the images to the managers
        cal_image_id = self.cal.add_image(image)
        lxc_image_id = self.lxc.add_container(container)

        # Create the CAL to LXC mapping
        self.cal_to_lxc["image"][cal_image_id] = lxc_image_id

        image.id = cal_image_id

        return image.id

    @rwstatus
    def do_delete_image(self, account, image_id):
        """Deletes an image

        This function will remove the record of the image from the CAL and
        destroy the associated container.

        Arguments:
            account  - a cloud account
            image_id - the UUID of the image to delete

        Raises:
            An RWErrorNotEmpty exception is raised if there are VMs based on
            this image (the VMs need to be deleted first). An RWErrorNotFound
            is raised if the image_id does not match any of the known images.

        """
        container_id = self.cal_to_lxc["image"][image_id]
        container = self.lxc.get_container(container_id)

        # Stop the image and destroy it (NB: it should not be necessary to stop
        # the container, but just in case)
        container.stop()
        container.destroy()

        self.cal.remove_image(image_id)
        self.lxc.remove_container(container_id)

    @rwstatus(ret_on_failure=[None])
    def do_get_image(self, account, image_id):
        """Returns the specified image

        Arguments:
            account  - a cloud account
            image_id - the UUID of the image to retrieve

        Raises:
            An RWErrorNotFound exception is raised if the image_id does not
            match any of the known images.

        Returns:
            An image object

        """
        return self.cal.get_image(image_id)

    @rwstatus(ret_on_failure=[[]])
    def do_get_image_list(self, account):
        """Returns a list of images"""
        resources = RwcalYang.VimResources()
        for image in self.cal.get_image_list():
            resources.imageinfo_list.append(rwcal_copy_object(image))

        return resources

    @rwstatus
    def do_create_vm(self, account, vm):
        """Create a VM

        Arguments:
            vm - the VM info used to define the desire VM

        Raises:
            An RWErrorFailure is raised if there is not

        Returns:
            a string containing the unique id of the created VM

        """
        # Retrieve the container that will be used as the base of the snapshot
        container_id = self.cal_to_lxc["image"][vm.image_id]
        container = self.lxc.get_container(container_id)

        # Create a container snapshot
        snapshot = container.snapshot(self.next_snapshot_name())
        snapshot.hostname = vm.vm_name

        # Register the vm and container
        snapshot_id = self.lxc.add_container(snapshot)
        vm.vm_id = self.cal.add_vm(vm.image_id, vm)

        self.cal_to_lxc["vm"][vm.vm_id] = snapshot_id

        return vm.vm_id

    @rwstatus
    def do_start_vm(self, account, vm_id):
        """Starts the specified VM

        Arguments:
            vm_id - the id of the vm to start

        Raises:
            An RWErrorNotFound is raised if the specified vm id is not known to
            this driver.

        """
        if vm_id not in self.cal_to_lxc["vm"]:
            msg = "Unable to find the specified VM ({})"
            raise exceptions.RWErrorNotFound(msg.format(vm_id))

        container_id = self.cal_to_lxc["vm"][vm_id]

        snapshot = self.lxc.get_container(container_id)
        port_ids = self.lxc.get_container_ports(container_id)

        config = lxc.ContainerConfig(snapshot.name)

        for port_id in port_ids:
            port = self.lxc.get_port(port_id)
            config.add_network_config(port)

        vm = self.cal.get_vm(vm_id)

        # Set the management IP on the vm if not yet set
        if not vm.has_field("management_ip"):
            mgmt_ip = self.lxc.acquire_mgmt_ip()
            vm.management_ip = mgmt_ip

        # Add the management interface
        config.add_network_config(
                lxc.NetworkConfig(
                    type="veth",
                    link=self.lxc.mgmt_network.network_name,
                    name="eth0",
                    ipv4=vm.management_ip,
                    ipv4_gateway='auto',
                    )
                )

        # Add rift root as a mount point
        config.add_mount_point_config(
            lxc.MountConfig(
                local=os.environ["RIFT_ROOT"],
                remote=os.environ["RIFT_ROOT"][1:],
                read_only=False,
                )
            )

        userdata=None
        if vm.cloud_init.has_field("userdata"):
            userdata = vm.cloud_init.userdata

        snapshot.configure(config, userdata=userdata)
        snapshot.start()

    @rwstatus
    def do_stop_vm(self, account, vm_id):
        """Stops the specified VM

        Arguments:
            vm_id - the id of the vm to stop

        Raises:
            An RWErrorNotFound is raised if the specified vm id is not known to
            this driver.

        """
        if vm_id not in self.cal_to_lxc["vm"]:
            msg = "Unable to find the specified VM ({})"
            raise exceptions.RWErrorNotFound(msg.format(vm_id))

        # Stop the container
        container_id = self.cal_to_lxc["vm"][vm_id]
        snapshot = self.lxc.get_container(container_id)
        snapshot.stop()

    @rwstatus
    def do_delete_vm(self, account, vm_id):
        """Deletes the specified VM

        Arguments:
            vm_id - the id of the vm to delete

        Raises:
            An RWErrorNotFound is raised if the specified vm id is not known to
            this driver.

        """
        if vm_id not in self.cal_to_lxc["vm"]:
            msg = "Unable to find the specified VM ({})"
            raise exceptions.RWErrorNotFound(msg.format(vm_id))

        container_id = self.cal_to_lxc["vm"][vm_id]

        snapshot = self.lxc.get_container(container_id)
        snapshot.stop()
        snapshot.destroy()

        self.cal.remove_vm(vm_id)
        self.lxc.remove_container(container_id)

        # TODO: Recycle management ip

    @rwstatus
    def do_reboot_vm(self, account, vm_id):
        """
        reboot a virtual machine.

        @param vm_id     - Instance id of VM to be deleted.
        """
        self.do_stop_vm(account, vm_id, no_rwstatus=True)
        self.do_start_vm(account, vm_id, no_rwstatus=True)

    @rwstatus
    def do_get_vm(self, account, vm_id):
        """Returns the specified VM

        Arguments:
            vm_id - the id of the vm to return

        Raises:
            An RWErrorNotFound is raised if the specified vm id is not known to
            this driver.

        Returns:
            a VMInfoItem object

        """
        if vm_id not in self.cal_to_lxc["vm"]:
            msg = "Unable to find the specified VM ({})"
            raise exceptions.RWErrorNotFound(msg.format(vm_id))

        return self.cal.get_vm(vm_id)

    @rwstatus(ret_on_failure=[[]])
    def do_get_vm_list(self, account):
        """Returns the a list of the VMs known to the driver

        Returns:
            a list of VMInfoItem objects

        """
        resources = RwcalYang.VimResources()
        for vm in self.cal.get_vm_list():
            resources.vminfo_list.append(rwcal_copy_object(vm))

        return resources

    @rwstatus
    def do_create_flavor(self, account, flavor):
        """
        create new flavor.

        @param flavor   - Flavor object
        """
        flavor_id = str(uuid.uuid4())
        self.cal.flavors[flavor_id] = flavor
        logger.debug('Created flavor: {}'.format(flavor_id))
        return flavor_id

    @rwstatus
    def do_delete_flavor(self, account, flavor_id):
        """
        Delete flavor.

        @param flavor_id     - Flavor id to be deleted.
        """
        logger.debug('Deleted flavor: {}'.format(flavor_id))
        self.cal.flavors.pop(flavor_id)

    @rwstatus(ret_on_failure=[None])
    def do_get_flavor(self, account, flavor_id):
        """
        Return the specified flavor

        @param flavor_id - the id of the flavor to return
        """
        flavor = self.cal.flavors[flavor_id]
        logger.debug('Returning flavor-info for : {}'.format(flavor_id))
        return flavor

    @rwstatus(ret_on_failure=[[]])
    def do_get_flavor_list(self, account):
        """
        Return a list of flavors
        """
        vim_resources = RwcalYang.VimResources()
        for flavor in self.cal.flavors.values():
            f = RwcalYang.FlavorInfoItem()
            f.copy_from(flavor)
            vim_resources.flavorinfo_list.append(f)
        logger.debug("Returning list of flavor-info of size: %d", len(vim_resources.flavorinfo_list))
        return vim_resources

    @rwstatus
    def do_add_host(self, account, host):
        raise NotImplementedError()

    @rwstatus
    def do_remove_host(self, account, host_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[None])
    def do_get_host(self, account, host_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[[]])
    def do_get_host_list(self, account):
        raise NotImplementedError()

    @rwstatus
    def do_create_port(self, account, port):
        """Create a port between a network and a virtual machine

        Arguments:
            account - a cloud account
            port    - a description of port to create

        Raises:
            Raises an RWErrorNotFound exception if either the network or the VM
            associated with the port cannot be found.

        Returns:
            the ID of the newly created port.

        """
        if port.network_id not in self.cal_to_lxc["network"]:
            msg = 'Unable to find the specified network ({})'
            raise exceptions.RWErrorNotFound(msg.format(port.network_id))

        if port.vm_id not in self.cal_to_lxc["vm"]:
            msg = "Unable to find the specified VM ({})"
            raise exceptions.RWErrorNotFound(msg.format(port.vm_id))

        if port.has_field("ip_address"):
            raise exceptions.RWErrorFailure("IP address of the port must not be specific")

        network = self.cal.get_network(port.network_id)
        ip_pool = self.cal.get_network_ip_pool(port.network_id)
        port.ip_address = ip_pool.allocate_ip()

        net_config = lxc.NetworkConfig(
                type='veth',
                link=network.network_name[:15],
                name="veth" + str(uuid.uuid4())[:10],
                ipv4=port.ip_address,
                )

        lxc_network_id = self.cal_to_lxc["network"][port.network_id]
        lxc_vm_id = self.cal_to_lxc["vm"][port.vm_id]

        cal_port_id = self.cal.add_port(port.network_id, port.vm_id, port)
        lxc_port_id = self.lxc.add_port(lxc_network_id, lxc_vm_id, net_config)

        self.cal_to_lxc["port"][cal_port_id] = lxc_port_id
        port.port_id = cal_port_id

        return port.port_id

    @rwstatus
    def do_delete_port(self, account, port_id):
        """Delete the specified port

        Arguments:
            account - a cloud account
            port_id - the ID of the port to delete

        Raises:
            A RWErrorNotFound exception is raised if the specified port cannot
            be found.

        """
        if port_id not in self.cal_to_lxc["port"]:
            msg = "Unable to find the specified port ({})"
            raise exceptions.RWErrorNotFound(msg.format(port_id))

        lxc_port_id = self.cal_to_lxc["port"][port_id]

        # Release the port's ip address back into the network pool
        port = self.cal.get_port(port_id)
        ip_pool = self.cal.get_network_ip_pool(port.network_id)
        ip_pool.deallocate_ip(port.ip_address)

        self.cal.remove_port(port_id)
        self.lxc.remove_port(lxc_port_id)

        del self.cal_to_lxc["port"][port_id]

    @rwstatus(ret_on_failure=[None])
    def do_get_port(self, account, port_id):
        """Return the specified port

        Arguments:
            account - a cloud account
            port_id - the ID of the port to return

        Raises:
            A RWErrorNotFound exception is raised if the specified port cannot
            be found.

        Returns:
            The specified port.

        """
        if port_id not in self.cal_to_lxc["port"]:
            msg = "Unable to find the specified port ({})"
            raise exceptions.RWErrorNotFound(msg.format(port_id))

        return self.cal.get_port(port_id)

    @rwstatus(ret_on_failure=[[]])
    def do_get_port_list(self, account):
        """Returns a list of ports"""
        resources = RwcalYang.VimResources()
        for port in self.datastore.cal_manager.get_port_list():
            resources.portinfo_list.append(rwcal_copy_object(port))

        return resources

    @rwstatus
    def do_create_network(self, account, network):
        """Create a network

        Arguments:
            account - a cloud account
            network - a description of the network to create

        Returns:
            The ID of the newly created network

        """

        # Create the network
        try:
            # Setup a pool of mgmt IPv4 addresses
            if net.bridge_exists(network.network_name):
                logger.warning("Bridge %s already exists.  Removing.", network.network_name)
                net.bridge_down(network.network_name)
                net.bridge_remove(network.network_name)

            # Ensure that the subnet field was filled out and is valid
            if not network.has_field("subnet"):
                raise CreateNetworkError("subnet not provided in create network request")

            try:
                ipaddress.IPv4Network(network.subnet)
            except ValueError as e:
                raise CreateNetworkError("Could not convert subnet into a "
                                         "IPv4Network: %s" % str(network.subnet))

            ip_pool = NetworkIPPool(network.subnet)

            # Create the management bridge with interface information
            net.create(network.network_name)

        except Exception as e:
            logger.warning(str(e))

        # Register the network
        cal_network_id = self.cal.add_network(network)
        lxc_network_id = self.lxc.add_bridge(network)
        self.cal.add_network_ip_pool(cal_network_id, ip_pool)

        self.cal_to_lxc["network"][cal_network_id] = lxc_network_id

        # Set the ID of the network object
        network.network_id = cal_network_id

        return network.network_id

    @rwstatus
    def do_delete_network(self, account, network_id):
        """
        Arguments:
            account    - a cloud account
            network_id - the UUID of the network to delete

        Raises:
            An RWErrorNotFound is raised if the specified network cannot be
            found.

        """
        if network_id not in self.cal_to_lxc["network"]:
            msg = "Unable to find the specified network ({})"
            raise exceptions.RWErrorNotFound(msg.format(network_id))

        # Get the associated bridge ID
        bridge_id = self.cal_to_lxc["network"][network_id]

        # Delete the network
        network = self.cal.get_network(network_id)
        net.delete(network.network_name)

        # Remove the network records
        self.lxc.remove_bridge(bridge_id)
        self.cal.remove_network(network_id)
        del self.cal_to_lxc["network"][network_id]

    @rwstatus(ret_on_failure=[None])
    def do_get_network(self, account, network_id):
        """Returns the specified network

        Arguments:
            account    - a cloud account
            network_id - the UUID of the network to delete

        Raises:
            An RWErrorNotFound is raised if the specified network cannot be
            found.

        Returns:
            The specified network

        """
        return self.cal.get_network(network_id)

    @rwstatus(ret_on_failure=[[]])
    def do_get_network_list(self, account):
        """Returns a list of network objects"""
        resources = RwcalYang.VimResources()
        for network in self.cal.get_network_list():
            resources.networkinfo_list.append(rwcal_copy_object(network))

        return resources

    @rwstatus(ret_on_failure=[""])
    def do_create_virtual_link(self, account, link_params):
        """Create a new virtual link

        Arguments:
            account     - a cloud account
            link_params - information that defines the type of VDU to create

        Returns:
            The vdu_id
        """
        network = RwcalYang.NetworkInfoItem()
        network.network_name = link_params.name
        network.subnet = link_params.subnet

        if link_params.has_field("provider_network"):
            logger.warning("Container CAL does not implement provider network")

        rs, net_id = self.do_create_network(account, network)
        if rs != RwTypes.RwStatus.SUCCESS:
            raise exceptions.RWErrorFailure(rs)

        return net_id

    @rwstatus
    def do_delete_virtual_link(self, account, link_id):
        """Delete a virtual link

        Arguments:
            account - a cloud account
            link_id - id for the virtual-link to be deleted

        Returns:
            None
        """

        network_ports = self.cal.get_network_ports(link_id)
        for port_id in network_ports:
            self.do_delete_port(account, port_id, no_rwstatus=True)

        self.do_delete_network(account, link_id, no_rwstatus=True)

    @staticmethod
    def fill_connection_point_info(c_point, port_info):
        """Create a GI object for RwcalYang.VDUInfoParams_ConnectionPoints()

        Converts Port information dictionary object returned by container cal
        driver into Protobuf Gi Object

        Arguments:
            port_info - Port information from container cal
        Returns:
            Protobuf Gi object for RwcalYang.VDUInfoParams_ConnectionPoints
        """
        c_point.name = port_info.port_name
        c_point.connection_point_id = port_info.port_id
        c_point.ip_address = port_info.ip_address
        c_point.state = 'active'
        c_point.virtual_link_id = port_info.network_id
        c_point.vdu_id = port_info.vm_id

    @staticmethod
    def create_virtual_link_info(network_info, port_list):
        """Create a GI object for VirtualLinkInfoParams

        Converts Network and Port information dictionary object
        returned by container manager into Protobuf Gi Object

        Arguments:
            network_info - Network information from container cal
            port_list - A list of port information from container cal
            subnet: Subnet information from openstack
        Returns:
            Protobuf Gi object for VirtualLinkInfoParams
        """
        link = RwcalYang.VirtualLinkInfoParams()
        link.name = network_info.network_name
        link.state = 'active'
        link.virtual_link_id = network_info.network_id
        for port in port_list:
            c_point = link.connection_points.add()
            CloudSimPlugin.fill_connection_point_info(c_point, port)

        link.subnet = network_info.subnet

        return link

    @rwstatus(ret_on_failure=[None])
    def do_get_virtual_link(self, account, link_id):
        """Get information about virtual link.

        Arguments:
            account  - a cloud account
            link_id  - id for the virtual-link

        Returns:
            Object of type RwcalYang.VirtualLinkInfoParams
        """

        network = self.do_get_network(account, link_id, no_rwstatus=True)
        port_ids = self.cal.get_network_ports(network.network_id)
        ports = [self.cal.get_port(p_id) for p_id in port_ids]

        virtual_link = CloudSimPlugin.create_virtual_link_info(
                network, ports
                )

        return virtual_link

    @rwstatus(ret_on_failure=[None])
    def do_get_virtual_link_list(self, account):
        """Get information about all the virtual links

        Arguments:
            account  - a cloud account

        Returns:
            A list of objects of type RwcalYang.VirtualLinkInfoParams
        """
        networks = self.do_get_network_list(account, no_rwstatus=True)
        vnf_resources = RwcalYang.VNFResources()
        for network in networks.networkinfo_list:
            virtual_link = self.do_get_virtual_link(account, network.network_id, no_rwstatus=True)
            vnf_resources.virtual_link_info_list.append(virtual_link)

        return vnf_resources

    def _create_connection_point(self, account, c_point, vdu_id):
        """
        Create a connection point
        Arguments:
           account  - a cloud account
           c_point  - connection_points
        """
        port = RwcalYang.PortInfoItem()
        port.port_name = c_point.name
        port.network_id = c_point.virtual_link_id
        port.port_type = 'normal' ### Find Port type from network_profile under cloud account
        port.vm_id = vdu_id
        port_id = self.do_create_port(account, port, no_rwstatus=True)
        return port_id

    @rwstatus(ret_on_failure=[""])
    def do_create_vdu(self, account, vdu_init):
        """Create a new virtual deployment unit

        Arguments:
            account     - a cloud account
            vdu_init  - information about VDU to create (RwcalYang.VDUInitParams)

        Returns:
            The vdu_id
        """
        ### Create VM
        vm = RwcalYang.VMInfoItem()
        vm.vm_name = vdu_init.name
        vm.image_id = vdu_init.image_id
        if vdu_init.vdu_init.has_field('userdata'):
            vm.cloud_init.userdata = vdu_init.vdu_init.userdata
        vm.user_tags.node_id = vdu_init.node_id

        vm_id = self.do_create_vm(account, vm, no_rwstatus=True)

        ### Now create required number of ports aka connection points
        port_list = []
        for c_point in vdu_init.connection_points:
            virtual_link_id = c_point.virtual_link_id

            # Attempt to fetch the network to verify that the network
            # already exists.
            self.do_get_network(account, virtual_link_id, no_rwstatus=True)

            port_id = self._create_connection_point(account, c_point, vm_id)
            port_list.append(port_id)

        # Finally start the vm
        self.do_start_vm(account, vm_id, no_rwstatus=True)

        return vm_id

    @rwstatus
    def do_modify_vdu(self, account, vdu_modify):
        """Modify Properties of existing virtual deployment unit

        Arguments:
            account     -  a cloud account
            vdu_modify  -  Information about VDU Modification (RwcalYang.VDUModifyParams)
        """
        ### First create required number of ports aka connection points
        port_list = []
        network_list = []
        if not vdu_modify.has_field("vdu_id"):
            raise ValueError("vdu_id must not be empty")

        for c_point in vdu_modify.connection_points_add:
            if not c_point.has_field("virtual_link_id"):
                raise ValueError("virtual link id not provided")

            network_list.append(c_point.virtual_link_id)
            port_id = self._create_connection_point(account, c_point, vdu_modify.vdu_id)
            port_list.append(port_id)

        ### Delete the requested connection_points
        for c_point in vdu_modify.connection_points_remove:
            self.do_delete_port(account, c_point.connection_point_id, no_rwstatus=True)

        self.do_reboot_vm(account, vdu_modify.vdu_id)

    @rwstatus
    def do_delete_vdu(self, account, vdu_id):
        """Delete a virtual deployment unit

        Arguments:
            account - a cloud account
            vdu_id  - id for the vdu to be deleted

        Returns:
            None
        """
        ### Get list of port on VM and delete them.
        port_id_list = self.cal.get_vm_ports(vdu_id)
        ports = [self.cal.get_port(p_id) for p_id in port_id_list]
        for port in ports:
            self.do_delete_port(account, port.port_id, no_rwstatus=True)
        self.do_delete_vm(account, vdu_id, no_rwstatus=True)

    @staticmethod
    def fill_vdu_info(vm_info, port_list):
        """create a gi object for vduinfoparams

        converts vm information dictionary object returned by openstack
        driver into protobuf gi object

        arguments:
            vm_info - vm information from openstack
            mgmt_network - management network
            port_list - a list of port information from container cal
        returns:
            protobuf gi object for vduinfoparams
        """
        vdu = RwcalYang.VDUInfoParams()
        vdu.name = vm_info.vm_name
        vdu.vdu_id = vm_info.vm_id
        vdu.management_ip = vm_info.management_ip
        vdu.public_ip = vm_info.management_ip
        vdu.node_id = vm_info.user_tags.node_id
        vdu.image_id = vm_info.image_id
        vdu.state = 'active'

        # fill the port information
        for port in port_list:
            c_point = vdu.connection_points.add()
            CloudSimPlugin.fill_connection_point_info(c_point, port)

        vdu.vm_flavor.vcpu_count = 1
        vdu.vm_flavor.memory_mb = 8 * 1024 # 8GB
        vdu.vm_flavor.storage_gb = 10

        return vdu

    @rwstatus(ret_on_failure=[None])
    def do_get_vdu(self, account, vdu_id):
        """Get information about a virtual deployment unit.

        Arguments:
            account - a cloud account
            vdu_id  - id for the vdu

        Returns:
            Object of type RwcalYang.VDUInfoParams
        """
        port_id_list = self.cal.get_vm_ports(vdu_id)
        ports = [self.cal.get_port(p_id) for p_id in port_id_list]
        vm_info = self.do_get_vm(account, vdu_id, no_rwstatus=True)
        vdu_info = CloudSimPlugin.fill_vdu_info(vm_info, ports)

        return vdu_info

    @rwstatus(ret_on_failure=[None])
    def do_get_vdu_list(self, account):
        """Get information about all the virtual deployment units

        Arguments:
            account     - a cloud account

        Returns:
            A list of objects of type RwcalYang.VDUInfoParams
        """

        vnf_resources = RwcalYang.VNFResources()

        vm_resources = self.do_get_vm_list(account, no_rwstatus=True)
        for vm in vm_resources.vminfo_list:
            port_list = self.cal.get_vm_ports(vm.vm_id)
            vdu = CloudSimPlugin.fill_vdu_info(vm, port_list)
            vnf_resources.vdu_info_list.append(vdu)

        return vnf_resources
