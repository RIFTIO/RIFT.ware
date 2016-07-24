
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import logging

import requests

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

logger = logging.getLogger('rwcal.cloudsimproxy')


rwstatus = rw_status.rwstatus_from_exc_map({
    IndexError: RwTypes.RwStatus.NOTFOUND,
    KeyError: RwTypes.RwStatus.NOTFOUND,
    })


class CloudsimProxyError(Exception):
    pass


class CloudSimProxyPlugin(GObject.Object, RwCal.Cloud):
    DEFAULT_PROXY_HOST = "localhost"
    DEFAULT_PROXY_PORT = 9002

    def __init__(self):
        self._session = None
        self._host = None
        self._port = CloudSimProxyPlugin.DEFAULT_PROXY_PORT

    @property
    def session(self):
        if self._session is None:
            self._session = requests.Session()

        return self._session

    @property
    def host(self):
        return self._host

    @host.setter
    def host(self, host):
        if self._host is not None:
            if host != self._host:
                raise CloudsimProxyError("Cloudsim host changed during execution")

        self._host = host

    def _set_host_from_account(self, account):
        self.host = account.cloudsim_proxy.host

    def _proxy_rpc_call(self, api, **kwargs):
        url = "http://{host}:{port}/api/{api}".format(
                host=self._host,
                port=self._port,
                api=api,
                )

        post_dict = {}
        for key, val in kwargs.items():
            post_dict[key] = val

        logger.debug("Sending post to url %s with json data: %s", url, post_dict)
        r = self.session.post(url, json=post_dict)
        r.raise_for_status()

        response_dict = r.json()
        logger.debug("Got json response: %s", response_dict)

        return_vals = []
        for return_val in response_dict["return_vals"]:
            value = return_val["value"]
            proto_type = return_val["proto_type"]
            if proto_type is not None:
                gi_cls = getattr(RwcalYang, proto_type)
                logger.debug("Deserializing into %s", proto_type)
                gi_obj = gi_cls.from_dict(value)
                value = gi_obj

            return_vals.append(value)

        logger.debug("Returning RPC return values: %s", return_vals)

        if len(return_vals) == 0:
            return None

        elif len(return_vals) == 1:
            return return_vals[0]

        else:
            return tuple(return_vals[1:])

    @rwstatus
    def do_init(self, rwlog_ctx):
        logger.addHandler(
            rwlogger.RwLogger(
                category="rw-cal-log",
                subcategory="cloudsimproxy",
                log_hdl=rwlog_ctx,
            )
        )

    @rwstatus(ret_on_failure=[None])
    def do_get_management_network(self, account):
        """Returns the management network

        Arguments:
            account - a cloud account

        Returns:
            a NetworkInfo object

        """

        self._set_host_from_account(account)
        return self._proxy_rpc_call("get_management_network")

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
        self._set_host_from_account(account)
        return self._proxy_rpc_call("create_image", image=image.as_dict())

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
        self._set_host_from_account(account)
        return self._proxy_rpc_call("get_management_network")

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
        self._set_host_from_account(account)
        return self._proxy_rpc_call("get_image", image_id=image_id)

    @rwstatus(ret_on_failure=[[]])
    def do_get_image_list(self, account):
        """Returns a list of images"""
        self._set_host_from_account(account)
        return self._proxy_rpc_call("get_image_list")

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
        self._set_host_from_account(account)
        return self._proxy_rpc_call("create_vm", vm=vm.as_dict())

    @rwstatus
    def do_start_vm(self, account, vm_id):
        """Starts the specified VM

        Arguments:
            vm_id - the id of the vm to start

        Raises:
            An RWErrorNotFound is raised if the specified vm id is not known to
            this driver.

        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("start_vm", vm_id=vm_id)

    @rwstatus
    def do_stop_vm(self, account, vm_id):
        """Stops the specified VM

        Arguments:
            vm_id - the id of the vm to stop

        Raises:
            An RWErrorNotFound is raised if the specified vm id is not known to
            this driver.

        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("stop_vm", vm_id=vm_id)

    @rwstatus
    def do_delete_vm(self, account, vm_id):
        """Deletes the specified VM

        Arguments:
            vm_id - the id of the vm to delete

        Raises:
            An RWErrorNotFound is raised if the specified vm id is not known to
            this driver.

        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("delete_vm", vm_id=vm_id)

    @rwstatus
    def do_reboot_vm(self, account, vm_id):
        """
        reboot a virtual machine.

        @param vm_id     - Instance id of VM to be deleted.
        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("reboot_vm", vm_id=vm_id)

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
        self._set_host_from_account(account)
        return self._proxy_rpc_call("get_vm", vm_id=vm_id)

    @rwstatus(ret_on_failure=[[]])
    def do_get_vm_list(self, account):
        """Returns the a list of the VMs known to the driver

        Returns:
            a list of VMInfoItem objects

        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("get_vm_list")

    @rwstatus
    def do_create_flavor(self, account, flavor):
        """
        create new flavor.

        @param flavor   - Flavor object
        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("create_flavor", flavor=flavor.as_dict())

    @rwstatus
    def do_delete_flavor(self, account, flavor_id):
        """
        Delete flavor.

        @param flavor_id     - Flavor id to be deleted.
        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("delete_flavor", flavor_id=flavor_id)

    @rwstatus(ret_on_failure=[None])
    def do_get_flavor(self, account, flavor_id):
        """
        Return the specified flavor

        @param flavor_id - the id of the flavor to return
        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("get_flavor", flavor_id=flavor_id)

    @rwstatus(ret_on_failure=[[]])
    def do_get_flavor_list(self, account):
        """
        Return a list of flavors
        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("get_flavor_list")

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
        self._set_host_from_account(account)
        return self._proxy_rpc_call("create_port", port=port.as_dict())

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
        self._set_host_from_account(account)
        return self._proxy_rpc_call("delete_port", port_id=port_id)

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
        self._set_host_from_account(account)
        return self._proxy_rpc_call("get_port", port_id=port_id)

    @rwstatus(ret_on_failure=[[]])
    def do_get_port_list(self, account):
        """Returns a list of ports"""

        self._set_host_from_account(account)
        return self._proxy_rpc_call("get_port_list")

    @rwstatus
    def do_create_network(self, account, network):
        """Create a network

        Arguments:
            account - a cloud account
            network - a description of the network to create

        Returns:
            The ID of the newly created network

        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("create_network", network=network.as_dict())

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
        self._set_host_from_account(account)
        return self._proxy_rpc_call("delete_network", network_id=network_id)

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
        self._set_host_from_account(account)
        return self._proxy_rpc_call("get_network", network_id=network_id)

    @rwstatus(ret_on_failure=[[]])
    def do_get_network_list(self, account):
        """Returns a list of network objects"""
        self._set_host_from_account(account)
        return self._proxy_rpc_call("get_network_list")

    @rwstatus(ret_on_failure=[""])
    def do_create_virtual_link(self, account, link_params):
        """Create a new virtual link

        Arguments:
            account     - a cloud account
            link_params - information that defines the type of VDU to create

        Returns:
            The vdu_id
        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("create_virtual_link", link_params=link_params.as_dict())

    @rwstatus(ret_on_failure=[None])
    def do_get_virtual_link(self, account, link_id):
        """Get information about virtual link.

        Arguments:
            account  - a cloud account
            link_id  - id for the virtual-link

        Returns:
            Object of type RwcalYang.VirtualLinkInfoParams
        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("get_virtual_link", link_id=link_id)

    @rwstatus(ret_on_failure=[[]])
    def do_get_virtual_link_list(self, account):
        """Returns the a list of the Virtual links

        Returns:
            a list of RwcalYang.VirtualLinkInfoParams objects

        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("get_virtual_link_list")

    @rwstatus(ret_on_failure=[None])
    def do_delete_virtual_link(self, account, link_id):
        """Delete the virtual link

        Arguments:
            account  - a cloud account
            link_id  - id for the virtual-link
        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("delete_virtual_link", link_id=link_id)

    @rwstatus(ret_on_failure=[""])
    def do_create_virtual_link(self, account, link_params):
        """Create a new virtual link

        Arguments:
            account     - a cloud account
            link_params - information that defines the type of VDU to create

        Returns:
            The vdu_id
        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("create_virtual_link", link_params=link_params.as_dict())

    @rwstatus(ret_on_failure=[""])
    def do_create_vdu(self, account, vdu_init):
        """Create a new virtual deployment unit

        Arguments:
            account     - a cloud account
            vdu_init  - information about VDU to create (RwcalYang.VDUInitParams)

        Returns:
            The vdu_id
        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("create_vdu", vdu_params=vdu_init.as_dict())

    @rwstatus
    def do_modify_vdu(self, account, vdu_modify):
        """Modify Properties of existing virtual deployment unit

        Arguments:
            account     -  a cloud account
            vdu_modify  -  Information about VDU Modification (RwcalYang.VDUModifyParams)
        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("modify_vdu", vdu_params=vdu_modify.as_dict())

    @rwstatus
    def do_delete_vdu(self, account, vdu_id):
        """Delete a virtual deployment unit

        Arguments:
            account - a cloud account
            vdu_id  - id for the vdu to be deleted

        Returns:
            None
        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("delete_vdu", vdu_id=vdu_id)

    @rwstatus(ret_on_failure=[None])
    def do_get_vdu(self, account, vdu_id):
        """Get information about a virtual deployment unit.

        Arguments:
            account - a cloud account
            vdu_id  - id for the vdu

        Returns:
            Object of type RwcalYang.VDUInfoParams
        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("get_vdu", vdu_id=vdu_id)

    @rwstatus(ret_on_failure=[None])
    def do_get_vdu_list(self, account):
        """Get information about all the virtual deployment units

        Arguments:
            account     - a cloud account

        Returns:
            A list of objects of type RwcalYang.VDUInfoParams
        """
        self._set_host_from_account(account)
        return self._proxy_rpc_call("get_vdu_list")
