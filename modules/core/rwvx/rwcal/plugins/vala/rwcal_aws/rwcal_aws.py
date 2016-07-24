
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import time
import os
import subprocess
import logging
import rift.rwcal.aws as aws_drv
import rw_status
import rwlogger
import rift.rwcal.aws.exceptions as exceptions
from gi import require_version
require_version('RwCal', '1.0')

from gi.repository import (
    GObject,
    RwCal,
    RwTypes,
    RwcalYang)


PREPARE_VM_CMD = "prepare_vm.py --aws_key {key} --aws_secret {secret} --aws_region {region} --server_id {server_id}"
DELETE_VM_CMD =  "delete_vm.py --aws_key {key} --aws_secret {secret} --aws_region {region} --server_id {server_id}"

rwstatus = rw_status.rwstatus_from_exc_map({ IndexError: RwTypes.RwStatus.NOTFOUND,
                                             KeyError: RwTypes.RwStatus.NOTFOUND,
                                             NotImplementedError: RwTypes.RwStatus.NOT_IMPLEMENTED,
                                             AttributeError: RwTypes.RwStatus.FAILURE,
                                             exceptions.RWErrorNotFound: RwTypes.RwStatus.NOTFOUND,
                                             exceptions.RWErrorDuplicate: RwTypes.RwStatus.DUPLICATE,
                                             exceptions.RWErrorExists: RwTypes.RwStatus.EXISTS,
                                             exceptions.RWErrorNotConnected: RwTypes.RwStatus.NOTCONNECTED,})

class RwcalAWSPlugin(GObject.Object, RwCal.Cloud):
    """This class implements the CAL VALA methods for AWS."""

    flavor_id = 1;
    instance_num = 1
    def __init__(self):
        GObject.Object.__init__(self)
        self._driver_class = aws_drv.AWSDriver
        self._flavor_list = []
        self.log = logging.getLogger('rwcal.aws.%s' % RwcalAWSPlugin.instance_num)
        self.log.setLevel(logging.DEBUG)

        RwcalAWSPlugin.instance_num += 1

    def _get_driver(self, account):
        return self._driver_class(key     = account.aws.key,
                                  secret  = account.aws.secret,
                                  region  = account.aws.region,
                                  ssh_key = account.aws.ssh_key,
                                  vpcid   = account.aws.vpcid,
                                  availability_zone = account.aws.availability_zone,
                                  default_subnet_id = account.aws.default_subnet_id)

    @rwstatus
    def do_init(self, rwlog_ctx):
        self.log.addHandler(
                rwlogger.RwLogger(
                    category="rw-cal-log",
                    subcategory="aws",
                    log_hdl=rwlog_ctx,
                    )
                )

    @rwstatus(ret_on_failure=[None])
    def do_validate_cloud_creds(self, account):
        """
        Validates the cloud account credentials for the specified account.
        Performs an access to the resources using underlying API. If creds
        are not valid, returns an error code & reason string
        Arguments:
            account - a cloud account to validate

        Returns:
            Validation Code and Details String
        """
        status = RwcalYang.CloudConnectionStatus(
                status="success",
                details="AWS Cloud Account validation not implemented yet"
                )

        return status

    @rwstatus(ret_on_failure=[""])
    def do_get_management_network(self, account):
        """
        Returns the management network associated with the specified account.
        Arguments:
            account - a cloud account

        Returns:
            The management network
        """
        raise NotImplementedError

    @rwstatus(ret_on_failure=[""])
    def do_create_tenant(self, account, name):
        """Create a new tenant.

        Arguments:
            account - a cloud account
            name - name of the tenant

        Returns:
            The tenant id
        """
        raise NotImplementedError

    @rwstatus
    def do_delete_tenant(self, account, tenant_id):
        """delete a tenant.

        Arguments:
            account - a cloud account
            tenant_id - id of the tenant
        """
        raise NotImplementedError

    @rwstatus(ret_on_failure=[[]])
    def do_get_tenant_list(self, account):
        """List tenants.

        Arguments:
            account - a cloud account

        Returns:
            List of tenants
        """
        raise NotImplementedError

    @rwstatus(ret_on_failure=[""])
    def do_create_role(self, account, name):
        """Create a new user.

        Arguments:
            account - a cloud account
            name - name of the user

        Returns:
            The user id
        """
        raise NotImplementedError

    @rwstatus
    def do_delete_role(self, account, role_id):
        """Delete a user.

        Arguments:
            account - a cloud account
            role_id - id of the user
        """
        raise NotImplementedError

    @rwstatus(ret_on_failure=[[]])
    def do_get_role_list(self, account):
        """List roles.

        Arguments:
            account - a cloud account

        Returns:
            List of roles
        """
        raise NotImplementedError

    @rwstatus(ret_on_failure=[""])
    def do_create_image(self, account, image):
        """Create an image

        Arguments:
            account - a cloud account
            image - a description of the image to create

        Returns:
            The image id
        """
        raise NotImplementedError

    @rwstatus
    def do_delete_image(self, account, image_id):
        """Delete a vm image.

        Arguments:
            account - a cloud account
            image_id - id of the image to delete
        """
        raise NotImplementedError

    @staticmethod
    def _fill_image_info(img_info):
        """Create a GI object from image info dictionary

        Converts image information dictionary object returned by AWS
        driver into Protobuf Gi Object

        Arguments:
            account - a cloud account
            img_info - image information dictionary object from AWS

        Returns:
            The ImageInfoItem
        """
        img = RwcalYang.ImageInfoItem()
        img.name = img_info.name
        img.id   = img_info.id

        tag_fields = ['checksum']
        # Look for any properties
        if img_info.tags:
            for tag in img_info.tags:
                if tag['Key'] == 'checksum':
                    setattr(img, tag['Key'], tag['Value'])
        img.disk_format  = 'ami'
        if img_info.state == 'available':
            img.state = 'active'
        else:
            img.state = 'inactive'
        return img

    @rwstatus(ret_on_failure=[[]])
    def do_get_image_list(self, account):
        """Return a list of the names of all available images.

        Arguments:
            account - a cloud account

        Returns:
            The the list of images in VimResources object
        """
        response = RwcalYang.VimResources()
        image_list = []
        images = self._get_driver(account).list_images()
        for img in images:
            response.imageinfo_list.append(RwcalAWSPlugin._fill_image_info(img))
        return response

    @rwstatus(ret_on_failure=[None])
    def do_get_image(self, account, image_id):
        """Return a image information.

        Arguments:
            account - a cloud account
            image_id - an id of the image

        Returns:
            ImageInfoItem object containing image information.
        """
        image = self._get_driver(account).get_image(image_id)
        return RwcalAWSPlugin._fill_image_info(image)

    @rwstatus(ret_on_failure=[""])
    def do_create_vm(self, account, vminfo):
        """Create a new virtual machine.

        Arguments:
            account - a cloud account
            vminfo - information that defines the type of VM to create

        Returns:
            The image id
        """
        raise NotImplementedError

    @rwstatus
    def do_start_vm(self, account, vm_id):
        """Start an existing virtual machine.

        Arguments:
            account - a cloud account
            vm_id - an id of the VM
        """
        raise NotImplementedError

    @rwstatus
    def do_stop_vm(self, account, vm_id):
        """Stop a running virtual machine.

        Arguments:
            account - a cloud account
            vm_id - an id of the VM
        """
        raise NotImplementedError

    @rwstatus
    def do_delete_vm(self, account, vm_id):
        """Delete a virtual machine.

        Arguments:
            account - a cloud account
            vm_id - an id of the VM
        """
        raise NotImplementedError

    @rwstatus
    def do_reboot_vm(self, account, vm_id):
        """Reboot a virtual machine.

        Arguments:
            account - a cloud account
            vm_id - an id of the VM
        """
        raise NotImplementedError

    @staticmethod
    def _fill_vm_info(vm_info):
        """Create a GI object from vm info dictionary

        Converts VM information dictionary object returned by openstack
        driver into Protobuf Gi Object

        Arguments:
            vm_info - VM information from AWS

        Returns:
            Protobuf Gi object for VM
        """
        vm = RwcalYang.VMInfoItem()
        vm.vm_id     = vm_info.id
        vm.image_id  = vm_info.image_id
        vm.flavor_id = vm_info.instance_type
        if vm_info.state['Name'] == 'running':
            vm.state = 'active'
        else:
            vm.state = 'inactive'
        for network_intf in vm_info.network_interfaces:
            if 'Attachment' in network_intf and network_intf['Attachment']['DeviceIndex'] == 0:
                if 'Association' in network_intf and 'PublicIp' in network_intf['Association']:
                    vm.public_ip = network_intf['Association']['PublicIp']
                vm.management_ip = network_intf['PrivateIpAddress']
            else:
                addr = vm.private_ip_list.add()
                addr.ip_address = network_intf['PrivateIpAddress']
                if 'Association' in network_intf and 'PublicIp' in network_intf['Association']:
                    addr = vm.public_ip_list.add()
                    addr.ip_address = network_intf['Association']['PublicIp']

        if vm_info.placement and 'AvailabilityZone' in vm_info.placement:
            vm.availability_zone = vm_info.placement['AvailabilityZone']
        if vm_info.tags:
            for tag in vm_info.tags:
                if tag['Key'] == 'Name':
                    vm.vm_name   = tag['Value']
                elif tag['Key'] in vm.user_tags.fields:
                    setattr(vm.user_tags,tag['Key'],tag['Value'])
        return vm

    @rwstatus(ret_on_failure=[[]])
    def do_get_vm_list(self, account):
        """Return a list of the VMs as vala boxed objects

        Arguments:
            account - a cloud account

        Returns:
            List containing VM information
        """
        response = RwcalYang.VimResources()
        vms = self._get_driver(account).list_instances()
        for vm in vms:
            response.vminfo_list.append(RwcalAWSPlugin._fill_vm_info(vm))
        return response

    @rwstatus(ret_on_failure=[None])
    def do_get_vm(self, account, id):
        """Return vm information.

        Arguments:
            account - a cloud account
            id - an id for the VM

        Returns:
            VM information
        """
        vm = self._get_driver(account).get_instance(id)
        return RwcalAWSPlugin._fill_vm_info(vm)

    @rwstatus(ret_on_failure=[""])
    def do_create_flavor(self, account, flavor):
        """Create new flavor.
           AWS has fixed set of AWS types and so we map flavor to existing instance type
           and create local flavor for the same.

        Arguments:
            account - a cloud account
            flavor - flavor of the VM

        Returns:
            flavor id (with EC2 instance type included in id)
        """
        drv = self._get_driver(account)
        inst_type = drv.map_flavor_to_instance_type(name      = flavor.name,
			         ram       = flavor.vm_flavor.memory_mb,
			         vcpus     = flavor.vm_flavor.vcpu_count,
			         disk      = flavor.vm_flavor.storage_gb)

        new_flavor = RwcalYang.FlavorInfoItem()
        new_flavor.name = flavor.name
        new_flavor.vm_flavor.memory_mb = flavor.vm_flavor.memory_mb
        new_flavor.vm_flavor.vcpu_count = flavor.vm_flavor.vcpu_count
        new_flavor.vm_flavor.storage_gb = flavor.vm_flavor.storage_gb
        new_flavor.id = inst_type + '-' + str(RwcalAWSPlugin.flavor_id)
        RwcalAWSPlugin.flavor_id = RwcalAWSPlugin.flavor_id+1
        self._flavor_list.append(new_flavor)
        return new_flavor.id

    @rwstatus
    def do_delete_flavor(self, account, flavor_id):
        """Delete flavor.

        Arguments:
            account - a cloud account
            flavor_id - id flavor of the VM
        """

        flavor = [flav for flav in self._flavor_list if flav.id == flavor_id]
        self._flavor_list.delete(flavor[0])

    @staticmethod
    def _fill_flavor_info(flavor_info):
        """Create a GI object from flavor info dictionary

        Converts Flavor information dictionary object returned by openstack
        driver into Protobuf Gi Object

        Arguments:
            flavor_info: Flavor information from openstack

        Returns:
             Object of class FlavorInfoItem
        """
        flavor = RwcalYang.FlavorInfoItem()
        flavor.name                       = flavor_info.name
        flavor.id                         = flavor_info.id
        flavor.vm_flavor.memory_mb = flavor_info.vm_flavor.memory_mb
        flavor.vm_flavor.vcpu_count = flavor_info.vm_flavor.vcpu_count
        flavor.vm_flavor.storage_gb = flavor_info.vm_flavor.storage_gb
        return flavor

    @rwstatus(ret_on_failure=[[]])
    def do_get_flavor_list(self, account):
        """Return flavor information.

        Arguments:
            account - a cloud account

        Returns:
            List of flavors
        """
        response = RwcalYang.VimResources()
        for flv in self._flavor_list:
            response.flavorinfo_list.append(RwcalAWSPlugin._fill_flavor_info(flv))
        return response


    @rwstatus(ret_on_failure=[None])
    def do_get_flavor(self, account, id):
        """Return flavor information.

        Arguments:
            account - a cloud account
            id - an id for the flavor

        Returns:
            Flavor info item
        """
        flavor = [flav for flav in self._flavor_list if flav.id == id]
        return (RwcalAWSPlugin._fill_flavor_info(flavor[0]))

    def _fill_network_info(self, network_info, account):
        """Create a GI object from network info dictionary

        Converts Network information dictionary object returned by AWS
        driver into Protobuf Gi Object

        Arguments:
            network_info - Network information from AWS
            account - a cloud account

        Returns:
            Network info item
        """
        network                  = RwcalYang.NetworkInfoItem()
        network.network_id       = network_info.subnet_id
        network.subnet           = network_info.cidr_block
        if network_info.tags:
            for tag in network_info.tags:
                if tag['Key'] == 'Name':
                    network.network_name   = tag['Value']
        return network

    @rwstatus(ret_on_failure=[[]])
    def do_get_network_list(self, account):
        """Return a list of networks

        Arguments:
            account - a cloud account

        Returns:
            List of networks
        """
        response = RwcalYang.VimResources()
        networks = self._get_driver(account).get_subnet_list()
        for network in networks:
            response.networkinfo_list.append(self._fill_network_info(network, account))
        return response

    @rwstatus(ret_on_failure=[None])
    def do_get_network(self, account, id):
        """Return a network

        Arguments:
            account - a cloud account
            id - an id for the network

        Returns:
            Network info item
        """
        network = self._get_driver(account).get_subnet(id)
        return self._fill_network_info(network, account)

    @rwstatus(ret_on_failure=[""])
    def do_create_network(self, account, network):
        """Create a new network

        Arguments:
            account - a cloud account
            network - Network object

        Returns:
            Network id
        """
        raise NotImplementedError

    @rwstatus
    def do_delete_network(self, account, network_id):
        """Delete a network

        Arguments:
            account - a cloud account
            network_id - an id for the network
        """
        raise NotImplementedError

    @staticmethod
    def _fill_port_info(port_info):
        """Create a GI object from port info dictionary

        Converts Port information dictionary object returned by AWS
        driver into Protobuf Gi Object

        Arguments:
            port_info - Port/Network interface information from AWS

        Returns:
            Port info item
        """
        port = RwcalYang.PortInfoItem()

        port.port_id    = port_info.id
        port.network_id = port_info.subnet_id
        if port_info.attachment and 'InstanceId' in port_info.attachment:
            port.vm_id = port_info.attachment['InstanceId']
        port.ip_address = port_info.private_ip_address
        if port_info.status == 'in-use':
            port.port_state = 'active'
        elif port_info.status == 'available':
            port.port_state = 'inactive'
        else:
            port.port_state = 'unknown'
        if port_info.tag_set:
            for tag in port_info.tag_set:
                if tag['Key'] == 'Name':
                    port.port_name   = tag['Value']
        return port


    @rwstatus(ret_on_failure=[None])
    def do_get_port(self, account, port_id):
        """Return a port

        Arguments:
            account - a cloud account
            port_id - an id for the port

        Returns:
            Port info item
        """
        port = self._get_driver(account).get_network_interface(port_id)
        return RwcalAWSPlugin._fill_port_info(port)

    @rwstatus(ret_on_failure=[[]])
    def do_get_port_list(self, account):
        """Return a list of ports

        Arguments:
            account - a cloud account

        Returns:
            Port info list
        """
        response = RwcalYang.VimResources()
        ports = self._get_driver(account).get_network_interface_list()
        for port in ports:
            response.portinfo_list.append(RwcalAWSPlugin._fill_port_info(port))
        return response

    @rwstatus(ret_on_failure=[""])
    def do_create_port(self, account, port):
        """Create a new port

        Arguments:
            account - a cloud account
            port - port object

        Returns:
            Port id
        """
        raise NotImplementedError

    @rwstatus
    def do_delete_port(self, account, port_id):
        """Delete a port

        Arguments:
            account - a cloud account
            port_id - an id for port
        """
        raise NotImplementedError

    @rwstatus(ret_on_failure=[""])
    def do_add_host(self, account, host):
        """Add a new host

        Arguments:
            account - a cloud account
            host - a host object

        Returns:
            An id for the host
        """
        raise NotImplementedError

    @rwstatus
    def do_remove_host(self, account, host_id):
        """Remove a host

        Arguments:
            account - a cloud account
            host_id - an id for the host
        """
        raise NotImplementedError

    @rwstatus(ret_on_failure=[None])
    def do_get_host(self, account, host_id):
        """Return a host

        Arguments:
            account - a cloud account
            host_id - an id for host

        Returns:
            Host info item
        """
        raise NotImplementedError

    @rwstatus(ret_on_failure=[[]])
    def do_get_host_list(self, account):
        """Return a list of hosts

        Arguments:
            account - a cloud account

        Returns:
            List of hosts
        """
        raise NotImplementedError

    @rwstatus(ret_on_failure=[""])
    def do_create_virtual_link(self, account, link_params):
        """Create a new virtual link

        Arguments:
            account     - a cloud account
            link_params - information that defines the type of VDU to create

        Returns:
            The vdu_id
        """
        drv = self._get_driver(account)
        kwargs = {}
        kwargs['CidrBlock'] = link_params.subnet

        subnet =  drv.create_subnet(**kwargs)
        if link_params.name:
            subnet.create_tags(Tags=[{'Key': 'Name','Value':link_params.name}])
        if link_params.associate_public_ip:
              drv.modify_subnet(SubnetId=subnet.id,MapPublicIpOnLaunch=link_params.associate_public_ip)
        return subnet.id

    @rwstatus
    def do_delete_virtual_link(self, account, link_id):
        """Delete a virtual link

        Arguments:
            account - a cloud account
            link_id - id for the virtual-link to be deleted

        Returns:
            None
        """
        drv = self._get_driver(account)
        port_list = drv.get_network_interface_list(SubnetId=link_id)
        for port in port_list:
            if port  and port.association and 'AssociationId' in port.association:
                drv.disassociate_public_ip_from_network_interface(NetworkInterfaceId=port.id)
            if port and port.attachment and 'AttachmentId' in port.attachment:
                drv.detach_network_interface(AttachmentId = port.attachment['AttachmentId'],Force=True) #force detach as otherwise delete fails
                #detach instance takes time; so poll to check port is not in-use
                port = drv.get_network_interface(NetworkInterfaceId=port.id)
                retries = 0
                while port.status == 'in-use' and retries < 10:
                    time.sleep(5)
                    port = drv.get_network_interface(NetworkInterfaceId=port.id)
            drv.delete_network_interface(NetworkInterfaceId=port.id)
        drv.delete_subnet(link_id)

    @staticmethod
    def _fill_connection_point_info(c_point, port_info):
        """Create a GI object for RwcalYang.VDUInfoParams_ConnectionPoints()

        Converts EC2.NetworkInterface object returned by AWS driver into
        Protobuf Gi Object

        Arguments:
            port_info - Network Interface information from AWS
        Returns:
            Protobuf Gi object for RwcalYang.VDUInfoParams_ConnectionPoints
        """
        c_point.virtual_link_id = port_info.subnet_id
        c_point.connection_point_id = port_info.id
        if port_info.attachment:
            c_point.vdu_id = port_info.attachment['InstanceId']
        c_point.ip_address = port_info.private_ip_address
        if port_info.association and 'PublicIp' in port_info.association:
                c_point.public_ip = port_info.association['PublicIp']
        if port_info.tag_set:
            for tag in port_info.tag_set:
                if tag['Key'] == 'Name':
                    c_point.name   = tag['Value']
        if port_info.status == 'in-use':
            c_point.state = 'active'
        elif port_info.status == 'available':
            c_point.state = 'inactive'
        else:
            c_point.state = 'unknown'

    @staticmethod
    def _fill_virtual_link_info(network_info, port_list):
        """Create a GI object for VirtualLinkInfoParams

        Converts Subnet and NetworkInterface object
        returned by AWS driver into Protobuf Gi Object

        Arguments:
            network_info - Subnet information from AWS
            port_list - A list of network interface information from openstack
        Returns:
            Protobuf Gi object for VirtualLinkInfoParams
        """
        link = RwcalYang.VirtualLinkInfoParams()
        if network_info.state == 'available':
            link.state = 'active'
        else:
            link.state = 'inactive'
        link.virtual_link_id = network_info.subnet_id
        link.subnet = network_info.cidr_block
        if network_info.tags:
            for tag in network_info.tags:
                if tag['Key'] == 'Name':
                    link.name   = tag['Value']
        for port in port_list:
            c_point = link.connection_points.add()
            RwcalAWSPlugin._fill_connection_point_info(c_point, port)

        return link

    @staticmethod
    def _fill_vdu_info(vm_info, port_list):
        """Create a GI object for VDUInfoParams

        Converts VM information dictionary object returned by AWS
        driver into Protobuf Gi Object

        Arguments:
            vm_info - EC2 instance information from AWS
            port_list - A list of network interface information from AWS
        Returns:
            Protobuf Gi object for VDUInfoParams
        """
        vdu = RwcalYang.VDUInfoParams()
        vdu.vdu_id = vm_info.id
        mgmt_port = [port for port in port_list if port.attachment and port.attachment['DeviceIndex'] == 0]
        assert(len(mgmt_port) == 1)
        vdu.management_ip = mgmt_port[0].private_ip_address
        if mgmt_port[0].association and 'PublicIp' in mgmt_port[0].association:
            vdu.public_ip = mgmt_port[0].association['PublicIp']
            #For now set managemnet ip also to public ip
            #vdu.management_ip = vdu.public_ip
        if vm_info.tags:
            for tag in vm_info.tags:
                if tag['Key'] == 'Name':
                    vdu.name   = tag['Value']
                elif tag['Key'] == 'node_id':
                    vdu.node_id = tag['Value']
        vdu.image_id = vm_info.image_id
        vdu.flavor_id = vm_info.instance_type
        if vm_info.state['Name'] == 'running':
            vdu.state = 'active'
        else:
            vdu.state = 'inactive'
        #if vm_info.placement and 'AvailabilityZone' in vm_info.placement:
        #    vdu.availability_zone = vm_info.placement['AvailabilityZone']
        # Fill the port information
        cp_port_list = [port for port in port_list if port.attachment and port.attachment['DeviceIndex'] != 0]

        for port in cp_port_list:
            c_point = vdu.connection_points.add()
            RwcalAWSPlugin._fill_connection_point_info(c_point, port)
        return vdu


    @rwstatus(ret_on_failure=[None])
    def do_get_virtual_link(self, account, link_id):
        """Get information about virtual link.

        Arguments:
            account  - a cloud account
            link_id  - id for the virtual-link

        Returns:
            Object of type RwcalYang.VirtualLinkInfoParams
        """
        drv = self._get_driver(account)
        network = drv.get_subnet(SubnetId=link_id)
        port_list = drv.get_network_interface_list(SubnetId=link_id)
        virtual_link = RwcalAWSPlugin._fill_virtual_link_info(network, port_list)
        return virtual_link

    @rwstatus(ret_on_failure=[[]])
    def do_get_virtual_link_list(self, account):
        """Get information about all the virtual links

        Arguments:
            account  - a cloud account

        Returns:
            A list of objects of type RwcalYang.VirtualLinkInfoParams
        """
        vnf_resources = RwcalYang.VNFResources()
        drv = self._get_driver(account)
        networks = drv.get_subnet_list()
        for network in networks:
            port_list = drv.get_network_interface_list(SubnetId=network.id)
            virtual_link = RwcalAWSPlugin._fill_virtual_link_info(network, port_list)
            vnf_resources.virtual_link_info_list.append(virtual_link)
        return vnf_resources

    def _create_connection_point(self, account, c_point):
        """
        Create a connection point
        Arguments:
           account  - a cloud account
           c_point  - connection_points
        """
        drv = self._get_driver(account)
        port     = drv.create_network_interface(SubnetId=c_point.virtual_link_id)
        if c_point.name:
            port.create_tags(Tags=[{'Key': 'Name','Value':c_point.name}])
        if c_point.associate_public_ip:
                drv.associate_public_ip_to_network_interface(NetworkInterfaceId = port.id)
        return port

    def prepare_vdu_on_boot(self, account, server_id,vdu_init_params,vdu_port_list = None):
        cmd = PREPARE_VM_CMD.format(key     = account.aws.key,
                                  secret  = account.aws.secret,
                                  region  = account.aws.region,
                                  server_id = server_id)
        if vdu_init_params.has_field('name'):
            cmd += (" --vdu_name "+ vdu_init_params.name)
        if vdu_init_params.has_field('node_id'):
            cmd += (" --vdu_node_id "+ vdu_init_params.node_id)
        if vdu_port_list is not None:
            for port_id in vdu_port_list:
                cmd += (" --vdu_port_list "+ port_id)

        exec_path = 'python3 ' + os.path.dirname(aws_drv.__file__)
        exec_cmd = exec_path+'/'+cmd
        self.log.info("Running command: %s" %(exec_cmd))
        subprocess.call(exec_cmd, shell=True)

    @rwstatus(ret_on_failure=[""])
    def do_create_vdu(self, account, vdu_init):
        """Create a new virtual deployment unit

        Arguments:
            account     - a cloud account
            vdu_init  - information about VDU to create (RwcalYang.VDUInitParams)

        Returns:
            The vdu_id
        """
        drv = self._get_driver(account)
        ### First create required number of ports aka connection points
        port_list = []
        network_list = []

        ### Now Create VM
        kwargs = {}
        kwargs['ImageId'] = vdu_init.image_id
        # Get instance type from flavor id which is of form c3.xlarge-1
        inst_type =  vdu_init.flavor_id.split('-')[0]
        kwargs['InstanceType'] = inst_type
        if vdu_init.vdu_init and vdu_init.vdu_init.userdata:
            kwargs['UserData'] = vdu_init.vdu_init.userdata

        #If we need to allocate public IP address create network interface and associate elastic
        #ip  to interface
        if vdu_init.allocate_public_address:
           port_id     = drv.create_network_interface(SubnetId=drv.default_subnet_id)
           drv.associate_public_ip_to_network_interface(NetworkInterfaceId = port_id.id)
           network_interface  = {'NetworkInterfaceId':port_id.id,'DeviceIndex':0}
           kwargs['NetworkInterfaces'] = [network_interface]

        #AWS Driver will use default subnet id to create first network interface
        # if network interface is not specified and will also have associate public ip
        # if enabled for the subnet
        vm_inst = drv.create_instance(**kwargs)

        # Wait for instance to get to running state before attaching network interface
        # to instance
        #vm_inst[0].wait_until_running()

        #if vdu_init.name:
            #vm_inst[0].create_tags(Tags=[{'Key': 'Name','Value':vdu_init.name}])
        #if vdu_init.node_id is not None:
            #vm_inst[0].create_tags(Tags=[{'Key':'node_id','Value':vdu_init.node_id}])

        # Create the connection points
        port_list = []
        for index,c_point in enumerate(vdu_init.connection_points):
            port_id = self._create_connection_point(account, c_point)
            port_list.append(port_id.id)
            #drv.attach_network_interface(NetworkInterfaceId = port_id.id,InstanceId = vm_inst[0].id,DeviceIndex=index+1)

        # We wait for instance to get to running state and update name,node_id and attach network intfs
        self.prepare_vdu_on_boot(account, vm_inst[0].id, vdu_init, port_list)

        return vm_inst[0].id

    @rwstatus
    def do_modify_vdu(self, account, vdu_modify):
        """Modify Properties of existing virtual deployment unit

        Arguments:
            account     -  a cloud account
            vdu_modify  -  Information about VDU Modification (RwcalYang.VDUModifyParams)
        """
        ### First create required number of ports aka connection points
        drv = self._get_driver(account)
        port_list = []
        network_list = []

        vm_inst = drv.get_instance(vdu_modify.vdu_id)

        if vm_inst.state['Name'] != 'running':
            self.log.error("RWCAL-AWS: VM with id %s is not in running state during modify VDU",vdu_modify.vdu_id)
            raise InvalidStateError("RWCAL-AWS: VM with id %s is not in running state during modify VDU",vdu_modify.vdu_id)

        port_list = drv.get_network_interface_list(InstanceId = vdu_modify.vdu_id)
        used_device_indexs = [port.attachment['DeviceIndex'] for port in port_list if port.attachment]

        device_index = 1
        for c_point in vdu_modify.connection_points_add:
            #Get unused device index
            while device_index in used_device_indexs:
                device_index = device_index+1
            port_id = self._create_connection_point(account, c_point)
            drv.attach_network_interface(NetworkInterfaceId = port_id.id,InstanceId = vdu_modify.vdu_id,DeviceIndex =device_index)

        ### Detach the requested connection_points
        for c_point in vdu_modify.connection_points_remove:
            port = drv.get_network_interface(NetworkInterfaceId=c_point.connection_point_id)
            #Check if elastic IP is associated with interface and release it
            if port  and port.association and 'AssociationId' in port.association:
                drv.disassociate_public_ip_from_network_interface(NetworkInterfaceId=port.id)
            if port and port.attachment and port.attachment['DeviceIndex'] != 0:
                drv.detach_network_interface(AttachmentId = port.attachment['AttachmentId'],Force=True) #force detach as otherwise delete fails
            else:
                self.log.error("RWCAL-AWS: Cannot modify connection port at index 0")

        # Delete the connection points. Interfaces take time to get detached from instance and so
        # we check status before doing delete network interface
        for c_point in vdu_modify.connection_points_remove:
            port = drv.get_network_interface(NetworkInterfaceId=c_point.connection_point_id)
            retries = 0
            if port and port.attachment and port.attachment['DeviceIndex'] == 0:
                self.log.error("RWCAL-AWS: Cannot modify connection port at index 0")
                continue
            while port.status == 'in-use' and retries < 10:
                time.sleep(5)
                port = drv.get_network_interface(NetworkInterfaceId=c_point.connection_point_id)
            drv.delete_network_interface(port.id)

    def cleanup_vdu_on_term(self, account, server_id,vdu_port_list = None):
        cmd = DELETE_VM_CMD.format(key    = account.aws.key,
                                  secret  = account.aws.secret,
                                  region  = account.aws.region,
                                  server_id = server_id)
        if vdu_port_list is not None:
            for port_id in vdu_port_list:
                cmd += (" --vdu_port_list "+ port_id)

        exec_path = 'python3 ' + os.path.dirname(aws_drv.__file__)
        exec_cmd = exec_path+'/'+cmd
        self.log.info("Running command: %s" %(exec_cmd))
        subprocess.call(exec_cmd, shell=True)

    @rwstatus
    def do_delete_vdu(self, account, vdu_id):
        """Delete a virtual deployment unit

        Arguments:
            account - a cloud account
            vdu_id  - id for the vdu to be deleted

        Returns:
            None
        """
        drv = self._get_driver(account)
        ### Get list of port on VM and delete them.
        vm_inst = drv.get_instance(vdu_id)

        port_list = drv.get_network_interface_list(InstanceId = vdu_id)
        delete_port_list = [port.id for port in port_list if port.attachment and port.attachment['DeleteOnTermination'] is False]
        drv.terminate_instance(vdu_id)

        self.cleanup_vdu_on_term(account,vdu_id,delete_port_list)


    @rwstatus(ret_on_failure=[None])
    def do_get_vdu(self, account, vdu_id):
        """Get information about a virtual deployment unit.

        Arguments:
            account - a cloud account
            vdu_id  - id for the vdu

        Returns:
            Object of type RwcalYang.VDUInfoParams
        """
        drv = self._get_driver(account)

        ### Get list of ports excluding the one for management network
        vm = drv.get_instance(vdu_id)
        port_list = drv.get_network_interface_list(InstanceId = vdu_id)
        return RwcalAWSPlugin._fill_vdu_info(vm,port_list)


    @rwstatus(ret_on_failure=[[]])
    def do_get_vdu_list(self, account):
        """Get information about all the virtual deployment units

        Arguments:
            account     - a cloud account

        Returns:
            A list of objects of type RwcalYang.VDUInfoParams
        """
        vnf_resources = RwcalYang.VNFResources()
        drv = self._get_driver(account)
        vms = drv.list_instances()
        for vm in vms:
            ### Get list of ports excluding one for management network
            port_list = [p for p in drv.get_network_interface_list(InstanceId = vm.id)]
            vdu = RwcalAWSPlugin._fill_vdu_info(vm,
                                                port_list)
            vnf_resources.vdu_info_list.append(vdu)
        return vnf_resources
