
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import functools

from . import exceptions


def unsupported(f):
    @functools.wraps(f)
    def impl(*args, **kwargs):
        msg = '{} not supported'.format(f.__name__)
        raise exceptions.RWErrorNotSupported(msg)

    return impl


class Cloud(object):
    """
    Cloud defines a base class for cloud driver implementations. Note that
    not all drivers will support the complete set of functionality presented
    here.
    """

    @unsupported
    def get_management_network(self, account):
        """
        Returns the management network associated with the specified account.

        @param account - a cloud account

        @return a management network
        """
        pass

    @unsupported
    def create_tenant(self, account, name):
        """
        Create a new tenant.

        @param account - a cloud account
        @param name    - name to assign to the tenant.
        """
        pass

    @unsupported
    def delete_tenant(self, account, tenant_id):
        """
        delete a tenant.

        @param account   - a cloud account
        @param tenant_id - id of tenant to be deleted.
        """
        pass

    @unsupported
    def get_tenant_list(self, account):
        """
        List tenants.

        @param account - a cloud account
        """
        pass

    @unsupported
    def create_role(self, account, name):
        """
        Create a new role.

        @param account - a cloud account
        @param name    - name to assign to the role.
        """
        pass

    @unsupported
    def delete_role(self, account, role_id):
        """
        delete a role.

        @param account - a cloud account
        @param role_id - id of role to be deleted.
        """
        pass

    @unsupported
    def get_role_list(self, account):
        """
        List roles.

        @param account - a cloud account
        """
        pass

    @unsupported
    def create_image(self, account, image):
        """
        Create an image

        @param account - a cloud account
        @param image   - a description of the image to create
        """
        pass

    @unsupported
    def delete_image(self, account, image_id):
        """
        delete a vm image.

        @param account  - a cloud account
        @param image_id - Instance id of VM image to be deleted.
        """
        pass

    @unsupported
    def get_image_list(self, account):
        """
        Return a list of the names of all available images.

        @param account - a cloud account
        """
        pass

    @unsupported
    def get_image(self, account, image_id):
        """
        Returns image information.

        @param account - a cloud account
        """
        pass

    @unsupported
    def create_vm(self, account, vm):
        """
        Create a new virtual machine.

        @param account - a cloud account
        @param vm      - The info required to create a VM
        """
        pass

    @unsupported
    def start_vm(self, account, vm_id):
        """
        start an existing virtual machine.

        @param account - a cloud account
        @param vm_id   - The id of the VM to start
        """
        pass

    @unsupported
    def stop_vm(self, account, vm_id):
        """
        Stop a running virtual machine.

        @param account - a cloud account
        @param vm_id   - The id of the VM to stop
        """
        pass

    @unsupported
    def delete_vm(self, account, vm_id):
        """
        delete a virtual machine.

        @param account - a cloud account
        @param vm_id   - Instance id of VM to be deleted.
        """
        pass

    @unsupported
    def reboot_vm(self, account, vm_id):
        """
        reboot a virtual machine.

        @param account - a cloud account
        @param vm_id   - Instance id of VM to be deleted.
        """
        pass

    @unsupported
    def get_vm_list(self, account):
        """
        Return a list of vms.

        @param account - a cloud account
        """
        pass

    @unsupported
    def get_vm(self, account):
        """
        Return vm information.

        @param account - a cloud account
        """
        pass

    @unsupported
    def create_flavor(self, account, flavor):
        """
        create new flavor.

        @param account - a cloud account
        @param flavor  - Flavor object
        """
        pass

    @unsupported
    def delete_flavor(self, account, flavor_id):
        """
        Delete flavor.

        @param account   - a cloud account
        @param flavor_id - Flavor id to be deleted.
        """
        pass

    @unsupported
    def get_flavor_list(self, account):
        """
        Return a list of flavors.

        @param account - a cloud account
        """
        pass

    @unsupported
    def get_flavor(self, account):
        """
        Return flavor information.

        @param account - a cloud account
        """
        pass

    @unsupported
    def get_network(self, account, network_id):
        """
        Return a network

        @param account    - a cloud account
        @param network_id - unique network identifier
        """
        pass

    @unsupported
    def get_network_list(self, account):
        """
        Return a list of networks

        @param account - a cloud account
        """
        pass

    @unsupported
    def create_network(self, account, network):
        """
        Create a new network

        @param account - a cloud account
        @param network - Network object
        """
        pass

    @unsupported
    def delete_network(self, account, network_id):
        """
        Delete a network

        @param account    - a cloud account
        @param network_id - unique network identifier
        """
        pass

    @unsupported
    def get_port(self, account, port_id):
        """
        Return a port

        @param account - a cloud account
        @param port_id - unique port identifier
        """
        pass

    @unsupported
    def get_port_list(self, account):
        """
        Return a list of ports

        @param account - a cloud account
        """
        pass

    @unsupported
    def create_port(self, account, port):
        """
        Create a new port

        @param account - a cloud account
        @param port    - port object
        """
        pass

    @unsupported
    def delete_port(self, account, port_id):
        """
        Delete a port

        @param account - a cloud account
        @param port_id - unique port identifier
        """
        pass

    @unsupported
    def add_host(self, account, host):
        """
        Add a new host

        @param account - a cloud account
        @param host    - a host object
        """
        pass

    @unsupported
    def remove_host(self, account, host_id):
        """
        Remove a host

        @param account - a cloud account
        @param host_id - unique host identifier
        """
        pass

    @unsupported
    def get_host(self, account, host_id):
        """
        Return a host

        @param account - a cloud account
        @param host_id - unique host identifier
        """
        pass

    @unsupported
    def get_host_list(self, account):
        """
        Return a list of hosts

        @param account - a cloud account
        """
        pass
