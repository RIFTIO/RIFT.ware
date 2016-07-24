
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import random
import socket
import struct
import collections
import logging
import os
import uuid

from gi import require_version
require_version('RwCal', '1.0')

from gi.repository import (
    GObject,
    RwCal,
    RwTypes,
    RwcalYang)

import rw_status
import rwlogger

logger = logging.getLogger('rwcal.mock')

class UnknownAccountError(Exception):
    pass


class MissingFileError(Exception):
    pass


class ImageLocationError(Exception):
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
        self.vlinks = dict()
        self.vdus  = dict()
        self.flavors = dict()

class MockPlugin(GObject.Object, RwCal.Cloud):
    """This class implements the abstract methods in the Cloud class.
    Mock is used for unit testing."""

    def __init__(self):
        GObject.Object.__init__(self)
        self.resources = collections.defaultdict(Resources)

    @staticmethod
    def get_uuid(name):
        if name == None:
            raise ValueError("Name can not be None")
        return str(uuid.uuid3(uuid.NAMESPACE_DNS, name))

    @rwstatus
    def do_init(self, rwlog_ctx):
        if not any(isinstance(h, rwlogger.RwLogger) for h in logger.handlers):
            logger.addHandler(
                rwlogger.RwLogger(
                    category="rw-cal-log",
                    subcategory="rwcal.mock",
                    log_hdl=rwlog_ctx,
                )
            )

        account = RwcalYang.CloudAccount()
        account.name = 'mock_account'
        account.account_type = 'mock'
        account.mock.username = 'mock_user'
        self.create_default_resources(account)

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
        """
        Returns the management network

        @param account - a cloud account

        """
        raise NotImplementedError()

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
        """
        Create a VM image

        @param account - cloud account information
        @param image   - information about the image
        """
        if image.location is None:
            raise ImageLocationError("uninitialized image location")

        if not os.path.exists(image.location):
            raise MissingFileError("{} does not exist".format(image.location))

        image.id = self.get_uuid(image.name)
        self.resources[account.name].images[image.id] = image
        logger.debug('created image: {}'.format(image.id))
        return image.id

    @rwstatus
    def do_delete_image(self, account, image_id):
        """
        delete a vm image.

        @param image_id     - Instance id of VM image to be deleted.
        """
        if account.name not in self.resources:
            raise UnknownAccountError()

        del self.resources[account.name].images[image_id]

    @rwstatus(ret_on_failure=[None])
    def do_get_image(self, account, image_id):
        return self.resources[account.name].images[image_id]

    @rwstatus(ret_on_failure=[[]])
    def do_get_image_list(self, account):
        """
        Return a list of the names of all available images.
        """
        boxed_image_list = RwcalYang.VimResources()
        for image in self.resources[account.name].images.values():
            image_entry = RwcalYang.ImageInfoItem()
            image_entry.id = image.id
            image_entry.name = image.name
            if image.has_field('checksum'):
                image_entry.checksum = image.checksum
            boxed_image_list.imageinfo_list.append(image_entry)

        return boxed_image_list

    @rwstatus
    def do_create_vm(self, account, vm):
        """
        Create a new virtual machine.

        @param name     - name to assign to the VM.  This does not have to be unique.
        @param image    - name of image to load on the VM.
        @param size     - name of the size of the VM to create.
        @param location - name of the location to launch the VM in.
        """
        raise NotImplementedError()

    @rwstatus
    def do_start_vm(self, account, vm_id):
        """
        Start a virtual machine.

        @param vm_id - id of VM to start
        """
        raise NotImplementedError()

    @rwstatus
    def do_stop_vm(self, account, vm_id):
        """
        Stop a virtual machine.

        @param vm_id - id of VM to stop
        """
        raise NotImplementedError()

    @rwstatus
    def do_delete_vm(self, account, vm_id):
        """
        delete a virtual machine.

        @param vm_id     - Instance id of VM to be deleted.
        """
        raise NotImplementedError()

    @rwstatus
    def do_reboot_vm(self, account, vm_id):
        """
        reboot a virtual machine.

        @param vm_id     - Instance id of VM to be deleted.
        """
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[[]])
    def do_get_vm_list(self, account):
        raise NotImplementedError()

    @rwstatus
    def do_create_flavor(self, account, flavor):
        """
        create new flavor.

        @param flavor   - Flavor object
        """
        flavor_id = self.get_uuid(flavor.name)
        self.resources[account.name].flavors[flavor_id] = flavor
        logger.debug('Created flavor: {}'.format(flavor_id))
        return flavor_id

    @rwstatus
    def do_delete_flavor(self, account, flavor_id):
        """
        Delete flavor.

        @param flavor_id     - Flavor id to be deleted.
        """
        logger.debug('Deleted flavor: {}'.format(flavor_id))
        self.resources[account.name].flavors.pop(flavor_id)

    @rwstatus(ret_on_failure=[None])
    def do_get_flavor(self, account, flavor_id):
        """
        Return the specified flavor

        @param flavor_id - the id of the flavor to return
        """
        flavor = self.resources[account.name].flavors[flavor_id]
        logger.debug('Returning flavor-info for : {}'.format(flavor_id))
        return flavor

    @rwstatus(ret_on_failure=[[]])
    def do_get_flavor_list(self, account):
        """
        Return a list of flavors
        """
        vim_resources = RwcalYang.VimResources()
        for flavor in self.resources[account.name].flavors.values():
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
        raise NotImplementedError()

    @rwstatus
    def do_delete_port(self, account, port_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[None])
    def do_get_port(self, account, port_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[[]])
    def do_get_port_list(self, account):
        raise NotImplementedError()

    @rwstatus
    def do_create_network(self, account, network):
        raise NotImplementedError()

    @rwstatus
    def do_delete_network(self, account, network_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[None])
    def do_get_network(self, account, network_id):
        raise NotImplementedError()

    @rwstatus(ret_on_failure=[[]])
    def do_get_network_list(self, account):
        raise NotImplementedError()

    def create_default_resources(self, account):
        """
        Create default resources
        """
        link_list = []
        ### Add virtual links
        #for i in range(1):
        #    vlink = RwcalYang.VirtualLinkReqParams()
        #    vlink.name = 'link-'+str(i)
        #    vlink.subnet = '10.0.0.0/24'
        #    rs, vlink_id = self.do_create_virtual_link(account, vlink)
        #    assert vlink_id != ''
        #    logger.debug("Creating static virtual-link with name: %s", vlink.name)
        #    link_list.append(vlink_id)

        #### Add VDUs
        #for i in range(8):
        #    vdu = RwcalYang.VDUInitParams()
        #    vdu.name = 'vdu-'+str(i)
        #    vdu.node_id = str(i)
        #    vdu.image_id = self.get_uuid('image-'+str(i))
        #    vdu.flavor_id = self.get_uuid('flavor'+str(i))
        #    vdu.vm_flavor.vcpu_count = 4
        #    vdu.vm_flavor.memory_mb = 4096*2
        #    vdu.vm_flavor.storage_gb = 40
        #    for j in range(2):
        #        c = vdu.connection_points.add()
        #        c.name = vdu.name+'-port-'+str(j)
        #        c.virtual_link_id = link_list[j]
        #    rs, vdu_id = self.do_create_vdu(account, vdu)
        #    assert vdu_id != ''
        #    logger.debug("Creating static VDU with name: %s", vdu.name)

        for i in range(2):
            flavor = RwcalYang.FlavorInfoItem()
            flavor.name = 'flavor-'+str(i)
            flavor.vm_flavor.vcpu_count = 4
            flavor.vm_flavor.memory_mb = 4096*2
            flavor.vm_flavor.storage_gb = 40
            rc, flavor_id = self.do_create_flavor(account, flavor)

        for i in range(2):
            image = RwcalYang.ImageInfoItem()
            image.name = "rwimage"
            image.id = self.get_uuid('image-'+str(i))
            image.checksum = self.get_uuid('rwimage'+str(i))
            image.location = "/dev/null"
            rc, image_id = self.do_create_image(account, image)

        image = RwcalYang.ImageInfoItem()
        image.name = "Fedora-x86_64-20-20131211.1-sda.qcow2"
        image.id = self.get_uuid(image.name)
        image.checksum = self.get_uuid(image.name)
        image.location = "/dev/null"
        rc, image_id = self.do_create_image(account, image)

        image = RwcalYang.ImageInfoItem()
        image.name = "Fedora-x86_64-20-20131211.1-sda-ping.qcow2"
        image.id = self.get_uuid(image.name)
        image.checksum = self.get_uuid(image.name)
        image.location = "/dev/null"
        rc, image_id = self.do_create_image(account, image)

        image = RwcalYang.ImageInfoItem()
        image.name = "Fedora-x86_64-20-20131211.1-sda-pong.qcow2"
        image.id = self.get_uuid(image.name)
        image.checksum = self.get_uuid(image.name)
        image.location = "/dev/null"
        rc, image_id = self.do_create_image(account, image)


    @rwstatus(ret_on_failure=[""])
    def do_create_virtual_link(self, account, link_params):
        vlink_id = self.get_uuid("%s_%s" % (link_params.name, len(self.resources[account.name].vlinks)))
        vlink = RwcalYang.VirtualLinkInfoParams()
        vlink.name = link_params.name
        vlink.state = 'active'
        vlink.virtual_link_id = vlink_id
        vlink.subnet = link_params.subnet
        vlink.connection_points = []
        for field in link_params.provider_network.fields:
            if link_params.provider_network.has_field(field):
                setattr(vlink.provider_network, field, getattr(link_params.provider_network, field))

        self.resources[account.name].vlinks[vlink_id] = vlink
        logger.debug('created virtual-link: {}'.format(vlink_id))
        return vlink_id

    @rwstatus
    def do_delete_virtual_link(self, account, link_id):
        self.resources[account.name].vlinks.pop(link_id)
        logger.debug('deleted virtual-link: {}'.format(link_id))


    @rwstatus(ret_on_failure=[None])
    def do_get_virtual_link(self, account, link_id):
        vlink = self.resources[account.name].vlinks[link_id]
        logger.debug('Returning virtual-link-info for : {}'.format(link_id))
        return vlink

    @rwstatus(ret_on_failure=[""])
    def do_get_virtual_link_list(self, account):
        vnf_resources = RwcalYang.VNFResources()
        for r in self.resources[account.name].vlinks.values():
            vlink = RwcalYang.VirtualLinkInfoParams()
            vlink.copy_from(r)
            vnf_resources.virtual_link_info_list.append(vlink)
        logger.debug("Returning list of virtual-link-info of size: %d", len(vnf_resources.virtual_link_info_list))
        return vnf_resources

    @rwstatus(ret_on_failure=[""])
    def do_create_vdu(self, account, vdu_init):
        vdu_id = self.get_uuid("%s_%s" % (vdu_init.name, len(self.resources[account.name].vdus)))
        vdu = RwcalYang.VDUInfoParams()
        vdu.vdu_id = vdu_id
        vdu.name = vdu_init.name
        vdu.node_id = vdu_init.node_id
        vdu.image_id = vdu_init.image_id
        if vdu_init.has_field('flavor_id'):
            vdu.flavor_id = vdu_init.flavor_id

        if vdu_init.has_field('vm_flavor'):
            xx = vdu.vm_flavor.new()
            xx.from_pbuf(vdu_init.vm_flavor.to_pbuf())
            vdu.vm_flavor = xx

        if vdu_init.has_field('guest_epa'):
            xx = vdu.guest_epa.new()
            xx.from_pbuf(vdu_init.guest_epa.to_pbuf())
            vdu.guest_epa = xx

        if vdu_init.has_field('vswitch_epa'):
            xx = vdu.vswitch_epa.new()
            xx.from_pbuf(vdu_init.vswitch_epa.to_pbuf())
            vdu.vswitch_epa = xx

        if vdu_init.has_field('hypervisor_epa'):
            xx = vdu.hypervisor_epa.new()
            xx.from_pbuf(vdu_init.hypervisor_epa.to_pbuf())
            vdu.hypervisor_epa = xx

        if vdu_init.has_field('host_epa'):
            xx = vdu.host_epa.new()
            xx.from_pbuf(vdu_init.host_epa.to_pbuf())
            vdu.host_epa = xx

        vdu.state = 'active'
        vdu.management_ip = socket.inet_ntoa(struct.pack('>I', random.randint(1, 0xffffffff)))
        vdu.public_ip = vdu.management_ip

        for c in vdu_init.connection_points:
            p = vdu.connection_points.add()
            p.connection_point_id = self.get_uuid(c.name)
            p.name = c.name
            p.vdu_id = vdu_id
            p.state = 'active'
            p.ip_address = socket.inet_ntoa(struct.pack('>I', random.randint(1, 0xffffffff)))
            p.virtual_link_id = c.virtual_link_id
            # Need to add this connection_point to virtual link
            vlink = self.resources[account.name].vlinks[c.virtual_link_id]
            v = vlink.connection_points.add()
            for field in p.fields:
                if p.has_field(field):
                    setattr(v, field, getattr(p, field))

        self.resources[account.name].vdus[vdu_id] = vdu
        logger.debug('Created vdu: {}'.format(vdu_id))
        return vdu_id


    @rwstatus
    def do_modify_vdu(self, account, vdu_modify):
        vdu = self.resources[account.name].vdus[vdu_modify.vdu_id]
        for c in vdu_modify.connection_points_add:
            p = vdu.connection_points.add()
            p.connection_point_id = self.get_uuid(c.name)
            p.name = c.name
            p.vdu_id = vdu.vdu_id
            p.state = 'active'
            p.ip_address = socket.inet_ntoa(struct.pack('>I', random.randint(1, 0xffffffff)))
            p.virtual_link_id = c.virtual_link_id
            # Need to add this connection_point to virtual link
            vlink = self.resources[account.name].vlinks[c.virtual_link_id]
            aa = RwcalYang.VirtualLinkInfoParams_ConnectionPoints()
            aa.connection_point_id = p.connection_point_id
            aa.name = p.name
            aa.virtual_link_id = vlink.virtual_link_id
            aa.state = 'active'
            aa.ip_address = p.ip_address
            aa.vdu_id = p.vdu_id
            vlink.connection_points.append(aa)

        for c in vdu_modify.connection_points_remove:
            for d in vdu.connection_points:
                if c.connection_point_id == d.connection_point_id:
                    vdu.connection_points.remove(d)
                    break
            for k, vlink in self.resources[account.name].vlinks.items():
                for z in vlink.connection_points:
                    if z.connection_point_id == c.connection_point_id:
                        vlink.connection_points.remove(z)
                        break
        logger.debug('modified vdu: {}'.format(vdu_modify.vdu_id))

    @rwstatus
    def do_delete_vdu(self, account, vdu_id):
        vdu = self.resources[account.name].vdus.pop(vdu_id)
        for c in vdu.connection_points:
            vlink = self.resources[account.name].vlinks[c.virtual_link_id]
            z = [p for p in vlink.connection_points if p.connection_point_id == c.connection_point_id]
            assert len(z) == 1
            vlink.connection_points.remove(z[0])

        logger.debug('deleted vdu: {}'.format(vdu_id))

    @rwstatus(ret_on_failure=[None])
    def do_get_vdu(self, account, vdu_id):
        vdu = self.resources[account.name].vdus[vdu_id]
        logger.debug('Returning vdu-info for : {}'.format(vdu_id))
        return vdu.copy()

    @rwstatus(ret_on_failure=[""])
    def do_get_vdu_list(self, account):
        vnf_resources = RwcalYang.VNFResources()
        for r in self.resources[account.name].vdus.values():
            vdu = RwcalYang.VDUInfoParams()
            vdu.copy_from(r)
            vnf_resources.vdu_info_list.append(vdu)
        logger.debug("Returning list of vdu-info of size: %d", len(vnf_resources.vdu_info_list))
        return vnf_resources

