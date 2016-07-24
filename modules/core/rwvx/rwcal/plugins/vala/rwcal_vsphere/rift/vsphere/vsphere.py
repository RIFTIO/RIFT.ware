
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import libcloud.compute.providers
import libcloud.compute.types

from gi.repository import RwcalYang


from . import core


class Vsphere(core.Cloud):
    """This class implements the abstract methods in the Cloud class.
    This is the Vsphere CAL driver."""

    def __init__(self):
        super(Vsphere, self).__init__()
        self._driver_class = libcloud.compute.providers.get_driver(
                libcloud.compute.providers.Provider.VSPHERE)

    def driver(self, account):
        return self._driver_class(
                username=account.username,
                passwork=account.password,
                url=url,
                )

    def get_image_list(self, account):
        """
        Return a list of the names of all available images.
        """
        images = self.driver(account).list_images()
        return [image.name for image in images]

    def create_vm(self, account, vminfo):
        """
        Create a new virtual machine.

        @param account  - account information used authenticate the create of
                          the virtual machine 
        @param vmfinfo  - information about the virtual machine to create

        """
        node = self.driver(account).ex_create_node_from_template(
                name=vminfo.vm_name,
                template=vminfo.vsphere.template,
                )

        vminfo.vm_id = node.id

        return node.id

    def delete_vm(self, account, vm_id):
        """
        delete a virtual machine.

        @param vm_id     - Instance id of VM to be deleted.
        """
        node = Node()
        node.id = vm_id
        self.driver(account).destroy_node(node)

    def reboot_vm(self, account, vm_id):
        """
        Reboot a virtual machine.

        @param vm_id     - Instance id of VM to be deleted.
        """
        node = Node()
        node.id = vm_id
        self.driver(account).reboot_node(node)
