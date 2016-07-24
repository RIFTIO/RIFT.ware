
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import contextlib
import logging
import os
import subprocess
import threading
import time

import rift.rwcal.openstack as openstack_drv
import rw_status
import rwlogger
from gi.repository import (
    GObject,
    RwCal,
    RwTypes,
    RwcalYang)

PREPARE_VM_CMD = "prepare_vm.py --auth_url {auth_url} --username {username} --password {password} --tenant_name {tenant_name} --mgmt_network {mgmt_network} --server_id {server_id} --port_metadata"


rwstatus = rw_status.rwstatus_from_exc_map({ IndexError: RwTypes.RwStatus.NOTFOUND,
                                             KeyError: RwTypes.RwStatus.NOTFOUND,
                                             NotImplementedError: RwTypes.RwStatus.NOT_IMPLEMENTED,})

espec_utils = openstack_drv.OpenstackExtraSpecUtils()

class UninitializedPluginError(Exception):
    pass

class OpenstackServerGroupError(Exception):
    pass


class RwcalOpenstackPlugin(GObject.Object, RwCal.Cloud):
    """This class implements the CAL VALA methods for openstack."""

    instance_num = 1

    def __init__(self):
        GObject.Object.__init__(self)
        self._driver_class = openstack_drv.OpenstackDriver
        self.log = logging.getLogger('rwcal.openstack.%s' % RwcalOpenstackPlugin.instance_num)
        self.log.setLevel(logging.DEBUG)

        self._rwlog_handler = None
        RwcalOpenstackPlugin.instance_num += 1


    @contextlib.contextmanager
    def _use_driver(self, account):
        if self._rwlog_handler is None:
            raise UninitializedPluginError("Must call init() in CAL plugin before use.")

        with rwlogger.rwlog_root_handler(self._rwlog_handler):
            try:
                drv = self._driver_class(username      = account.openstack.key,
                                         password      = account.openstack.secret,
                                         auth_url      = account.openstack.auth_url,
                                         tenant_name   = account.openstack.tenant,
                                         mgmt_network  = account.openstack.mgmt_network,
                                         cert_validate = account.openstack.cert_validate )
            except Exception as e:
                self.log.error("RwcalOpenstackPlugin: OpenstackDriver init failed. Exception: %s" %(str(e)))
                raise

            yield drv


    @rwstatus
    def do_init(self, rwlog_ctx):
        self._rwlog_handler = rwlogger.RwLogger(
                category="rw-cal-log",
                subcategory="openstack",
                log_hdl=rwlog_ctx,
                )
        self.log.addHandler(self._rwlog_handler)
        self.log.propagate = False

    @rwstatus(ret_on_failure=[None])
    def do_validate_cloud_creds(self, account):
        """
        Validates the cloud account credentials for the specified account.
        Performs an access to the resources using Keystone API. If creds
        are not valid, returns an error code & reason string
        Arguments:
            account - a cloud account to validate

        Returns:
            Validation Code and Details String
        """
        status = RwcalYang.CloudConnectionStatus()

        try:
            with self._use_driver(account) as drv:
                drv.validate_account_creds()

        except openstack_drv.ValidationError as e:
            self.log.error("RwcalOpenstackPlugin: OpenstackDriver credential validation failed. Exception: %s", str(e))
            status.status = "failure"
            status.details = "Invalid Credentials: %s" % str(e)

        except Exception as e:
            msg = "RwcalOpenstackPlugin: OpenstackDriver connection failed. Exception: %s" %(str(e))
            self.log.error(msg)
            status.status = "failure"
            status.details = msg

        else:
            status.status = "success"
            status.details = "Connection was successful"

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
        return account.openstack.mgmt_network

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

        try:
            # If the use passed in a file descriptor, use that to
            # upload the image.
            if image.has_field("fileno"):
                new_fileno = os.dup(image.fileno)
                hdl = os.fdopen(new_fileno, 'rb')
            else:
                hdl = open(image.location, "rb")
        except Exception as e:
            self.log.error("Could not open file for upload. Exception received: %s", str(e))
            raise

        with hdl as fd:
            kwargs = {}
            kwargs['name'] = image.name

            if image.disk_format:
                kwargs['disk_format'] = image.disk_format
            if image.container_format:
                kwargs['container_format'] = image.container_format

            with self._use_driver(account) as drv:
                # Create Image
                image_id = drv.glance_image_create(**kwargs)
                # Upload the Image
                drv.glance_image_upload(image_id, fd)

        return image_id

    @rwstatus
    def do_delete_image(self, account, image_id):
        """Delete a vm image.

        Arguments:
            account - a cloud account
            image_id - id of the image to delete
        """
        with self._use_driver(account) as drv:
            drv.glance_image_delete(image_id=image_id)


    @staticmethod
    def _fill_image_info(img_info):
        """Create a GI object from image info dictionary

        Converts image information dictionary object returned by openstack
        driver into Protobuf Gi Object

        Arguments:
            account - a cloud account
            img_info - image information dictionary object from openstack

        Returns:
            The ImageInfoItem
        """
        img = RwcalYang.ImageInfoItem()
        img.name = img_info['name']
        img.id = img_info['id']
        img.checksum = img_info['checksum']
        img.disk_format = img_info['disk_format']
        img.container_format = img_info['container_format']
        if img_info['status'] == 'active':
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
        with self._use_driver(account) as drv:
            images = drv.glance_image_list()
        for img in images:
            response.imageinfo_list.append(RwcalOpenstackPlugin._fill_image_info(img))
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
        with self._use_driver(account) as drv:
            image = drv.glance_image_get(image_id)
        return RwcalOpenstackPlugin._fill_image_info(image)

    @rwstatus(ret_on_failure=[""])
    def do_create_vm(self, account, vminfo):
        """Create a new virtual machine.

        Arguments:
            account - a cloud account
            vminfo - information that defines the type of VM to create

        Returns:
            The image id
        """
        kwargs = {}
        kwargs['name']      = vminfo.vm_name
        kwargs['flavor_id'] = vminfo.flavor_id
        kwargs['image_id']  = vminfo.image_id

        with self._use_driver(account) as drv:
            ### If floating_ip is required and we don't have one, better fail before any further allocation
            if vminfo.has_field('allocate_public_address') and vminfo.allocate_public_address:
                if account.openstack.has_field('floating_ip_pool'):
                    pool_name = account.openstack.floating_ip_pool
                else:
                    pool_name = None
                floating_ip = self._allocate_floating_ip(drv, pool_name)
            else:
                floating_ip = None

        if vminfo.has_field('cloud_init') and vminfo.cloud_init.has_field('userdata'):
            kwargs['userdata']  = vminfo.cloud_init.userdata
        else:
            kwargs['userdata'] = ''

        if account.openstack.security_groups:
            kwargs['security_groups'] = account.openstack.security_groups

        port_list = []
        for port in vminfo.port_list:
            port_list.append(port.port_id)

        if port_list:
            kwargs['port_list'] = port_list

        network_list = []
        for network in vminfo.network_list:
            network_list.append(network.network_id)

        if network_list:
            kwargs['network_list'] = network_list

        metadata = {}
        for field in vminfo.user_tags.fields:
            if vminfo.user_tags.has_field(field):
                metadata[field] = getattr(vminfo.user_tags, field)
        kwargs['metadata']  = metadata

        if vminfo.has_field('availability_zone'):
            kwargs['availability_zone']  = vminfo.availability_zone
        else:
            kwargs['availability_zone'] = None

        if vminfo.has_field('server_group'):
            kwargs['scheduler_hints'] = {'group': vminfo.server_group }
        else:
            kwargs['scheduler_hints'] = None

        with self._use_driver(account) as drv:
            vm_id = drv.nova_server_create(**kwargs)
            if floating_ip:
                self.prepare_vdu_on_boot(account, vm_id, floating_ip)

        return vm_id

    @rwstatus
    def do_start_vm(self, account, vm_id):
        """Start an existing virtual machine.

        Arguments:
            account - a cloud account
            vm_id - an id of the VM
        """
        with self._use_driver(account) as drv:
            drv.nova_server_start(vm_id)

    @rwstatus
    def do_stop_vm(self, account, vm_id):
        """Stop a running virtual machine.

        Arguments:
            account - a cloud account
            vm_id - an id of the VM
        """
        with self._use_driver(account) as drv:
            drv.nova_server_stop(vm_id)

    @rwstatus
    def do_delete_vm(self, account, vm_id):
        """Delete a virtual machine.

        Arguments:
            account - a cloud account
            vm_id - an id of the VM
        """
        with self._use_driver(account) as drv:
            drv.nova_server_delete(vm_id)

    @rwstatus
    def do_reboot_vm(self, account, vm_id):
        """Reboot a virtual machine.

        Arguments:
            account - a cloud account
            vm_id - an id of the VM
        """
        with self._use_driver(account) as drv:
            drv.nova_server_reboot(vm_id)

    @staticmethod
    def _fill_vm_info(vm_info, mgmt_network):
        """Create a GI object from vm info dictionary

        Converts VM information dictionary object returned by openstack
        driver into Protobuf Gi Object

        Arguments:
            vm_info - VM information from openstack
            mgmt_network - Management network

        Returns:
            Protobuf Gi object for VM
        """
        vm = RwcalYang.VMInfoItem()
        vm.vm_id     = vm_info['id']
        vm.vm_name   = vm_info['name']
        vm.image_id  = vm_info['image']['id']
        vm.flavor_id = vm_info['flavor']['id']
        vm.state     = vm_info['status']
        for network_name, network_info in vm_info['addresses'].items():
            if network_info:
                if network_name == mgmt_network:
                    vm.public_ip = next((item['addr']
                                            for item in network_info
                                            if item['OS-EXT-IPS:type'] == 'floating'),
                                        network_info[0]['addr'])
                    vm.management_ip = network_info[0]['addr']
                else:
                    for interface in network_info:
                        addr = vm.private_ip_list.add()
                        addr.ip_address = interface['addr']

        for network_name, network_info in vm_info['addresses'].items():
            if network_info and network_name == mgmt_network and not vm.public_ip:
                for interface in network_info:
                    if 'OS-EXT-IPS:type' in interface and interface['OS-EXT-IPS:type'] == 'floating':
                        vm.public_ip = interface['addr']

        # Look for any metadata
        for key, value in vm_info['metadata'].items():
            if key in vm.user_tags.fields:
                setattr(vm.user_tags, key, value)
        if 'OS-EXT-SRV-ATTR:host' in vm_info:
            if vm_info['OS-EXT-SRV-ATTR:host'] != None:
                vm.host_name = vm_info['OS-EXT-SRV-ATTR:host']
        if 'OS-EXT-AZ:availability_zone' in vm_info:
            if vm_info['OS-EXT-AZ:availability_zone'] != None:
                vm.availability_zone = vm_info['OS-EXT-AZ:availability_zone']
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
        with self._use_driver(account) as drv:
            vms = drv.nova_server_list()
        for vm in vms:
            response.vminfo_list.append(RwcalOpenstackPlugin._fill_vm_info(vm, account.openstack.mgmt_network))
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
        with self._use_driver(account) as drv:
            vm = drv.nova_server_get(id)
        return RwcalOpenstackPlugin._fill_vm_info(vm, account.openstack.mgmt_network)

    @staticmethod
    def _get_guest_epa_specs(guest_epa):
        """
        Returns EPA Specs dictionary for guest_epa attributes
        """
        epa_specs = {}
        if guest_epa.has_field('mempage_size'):
            mempage_size = espec_utils.guest.mano_to_extra_spec_mempage_size(guest_epa.mempage_size)
            if mempage_size is not None:
                epa_specs['hw:mem_page_size'] = mempage_size

        if guest_epa.has_field('cpu_pinning_policy'):
            cpu_pinning_policy = espec_utils.guest.mano_to_extra_spec_cpu_pinning_policy(guest_epa.cpu_pinning_policy)
            if cpu_pinning_policy is not None:
                epa_specs['hw:cpu_policy'] = cpu_pinning_policy

        if guest_epa.has_field('cpu_thread_pinning_policy'):
            cpu_thread_pinning_policy = espec_utils.guest.mano_to_extra_spec_cpu_thread_pinning_policy(guest_epa.cpu_thread_pinning_policy)
            if cpu_thread_pinning_policy is None:
                epa_specs['hw:cpu_threads_policy'] = cpu_thread_pinning_policy

        if guest_epa.has_field('trusted_execution'):
            trusted_execution = espec_utils.guest.mano_to_extra_spec_trusted_execution(guest_epa.trusted_execution)
            if trusted_execution is not None:
                epa_specs['trust:trusted_host'] = trusted_execution

        if guest_epa.has_field('numa_node_policy'):
            if guest_epa.numa_node_policy.has_field('node_cnt'):
                numa_node_count = espec_utils.guest.mano_to_extra_spec_numa_node_count(guest_epa.numa_node_policy.node_cnt)
                if numa_node_count is not None:
                    epa_specs['hw:numa_nodes'] = numa_node_count

            if guest_epa.numa_node_policy.has_field('mem_policy'):
                numa_memory_policy = espec_utils.guest.mano_to_extra_spec_numa_memory_policy(guest_epa.numa_node_policy.mem_policy)
                if numa_memory_policy is not None:
                    epa_specs['hw:numa_mempolicy'] = numa_memory_policy

            if guest_epa.numa_node_policy.has_field('node'):
                for node in guest_epa.numa_node_policy.node:
                    if node.has_field('vcpu') and node.vcpu:
                        epa_specs['hw:numa_cpus.'+str(node.id)] = ','.join([str(j) for j in node.vcpu])
                    if node.memory_mb:
                        epa_specs['hw:numa_mem.'+str(node.id)] = str(node.memory_mb)

        if guest_epa.has_field('pcie_device'):
            pci_devices = []
            for device in guest_epa.pcie_device:
                pci_devices.append(device.device_id +':'+str(device.count))
            epa_specs['pci_passthrough:alias'] = ','.join(pci_devices)

        return epa_specs

    @staticmethod
    def _get_host_epa_specs(host_epa):
        """
        Returns EPA Specs dictionary for host_epa attributes
        """

        epa_specs = {}

        if host_epa.has_field('cpu_model'):
            cpu_model = espec_utils.host.mano_to_extra_spec_cpu_model(host_epa.cpu_model)
            if cpu_model is not None:
                epa_specs['capabilities:cpu_info:model'] = cpu_model

        if host_epa.has_field('cpu_arch'):
            cpu_arch = espec_utils.host.mano_to_extra_spec_cpu_arch(host_epa.cpu_arch)
            if cpu_arch is not None:
                epa_specs['capabilities:cpu_info:arch'] = cpu_arch

        if host_epa.has_field('cpu_vendor'):
            cpu_vendor = espec_utils.host.mano_to_extra_spec_cpu_vendor(host_epa.cpu_vendor)
            if cpu_vendor is not None:
                epa_specs['capabilities:cpu_info:vendor'] = cpu_vendor

        if host_epa.has_field('cpu_socket_count'):
            cpu_socket_count = espec_utils.host.mano_to_extra_spec_cpu_socket_count(host_epa.cpu_socket_count)
            if cpu_socket_count is not None:
                epa_specs['capabilities:cpu_info:topology:sockets'] = cpu_socket_count

        if host_epa.has_field('cpu_core_count'):
            cpu_core_count = espec_utils.host.mano_to_extra_spec_cpu_core_count(host_epa.cpu_core_count)
            if cpu_core_count is not None:
                epa_specs['capabilities:cpu_info:topology:cores'] = cpu_core_count

        if host_epa.has_field('cpu_core_thread_count'):
            cpu_core_thread_count = espec_utils.host.mano_to_extra_spec_cpu_core_thread_count(host_epa.cpu_core_thread_count)
            if cpu_core_thread_count is not None:
                epa_specs['capabilities:cpu_info:topology:threads'] = cpu_core_thread_count

        if host_epa.has_field('cpu_feature'):
            cpu_features = []
            espec_cpu_features = []
            for feature in host_epa.cpu_feature:
                cpu_features.append(feature)
            espec_cpu_features = espec_utils.host.mano_to_extra_spec_cpu_features(cpu_features)
            if espec_cpu_features is not None:
                epa_specs['capabilities:cpu_info:features'] = espec_cpu_features
        return epa_specs

    @staticmethod
    def _get_hypervisor_epa_specs(guest_epa):
        """
        Returns EPA Specs dictionary for hypervisor_epa attributes
        """
        hypervisor_epa = {}
        return hypervisor_epa

    @staticmethod
    def _get_vswitch_epa_specs(guest_epa):
        """
        Returns EPA Specs dictionary for vswitch_epa attributes
        """
        vswitch_epa = {}
        return vswitch_epa

    @staticmethod
    def _get_host_aggregate_epa_specs(host_aggregate):
        """
        Returns EPA Specs dictionary for host aggregates
        """
        epa_specs = {}
        for aggregate in host_aggregate:
            epa_specs['aggregate_instance_extra_specs:'+aggregate.metadata_key] = aggregate.metadata_value

        return epa_specs

    @staticmethod
    def _get_epa_specs(flavor):
        """
        Returns epa_specs dictionary based on flavor information
        """
        epa_specs = {}
        if flavor.has_field('guest_epa'):
            guest_epa = RwcalOpenstackPlugin._get_guest_epa_specs(flavor.guest_epa)
            epa_specs.update(guest_epa)
        if flavor.has_field('host_epa'):
            host_epa = RwcalOpenstackPlugin._get_host_epa_specs(flavor.host_epa)
            epa_specs.update(host_epa)
        if flavor.has_field('hypervisor_epa'):
            hypervisor_epa = RwcalOpenstackPlugin._get_hypervisor_epa_specs(flavor.hypervisor_epa)
            epa_specs.update(hypervisor_epa)
        if flavor.has_field('vswitch_epa'):
            vswitch_epa = RwcalOpenstackPlugin._get_vswitch_epa_specs(flavor.vswitch_epa)
            epa_specs.update(vswitch_epa)
        if flavor.has_field('host_aggregate'):
            host_aggregate = RwcalOpenstackPlugin._get_host_aggregate_epa_specs(flavor.host_aggregate)
            epa_specs.update(host_aggregate)
        return epa_specs

    @rwstatus(ret_on_failure=[""])
    def do_create_flavor(self, account, flavor):
        """Create new flavor.

        Arguments:
            account - a cloud account
            flavor - flavor of the VM

        Returns:
            flavor id
        """
        epa_specs = RwcalOpenstackPlugin._get_epa_specs(flavor)
        with self._use_driver(account) as drv:
            return drv.nova_flavor_create(name      = flavor.name,
                                          ram       = flavor.vm_flavor.memory_mb,
                                          vcpus     = flavor.vm_flavor.vcpu_count,
                                          disk      = flavor.vm_flavor.storage_gb,
                                          epa_specs = epa_specs)


    @rwstatus
    def do_delete_flavor(self, account, flavor_id):
        """Delete flavor.

        Arguments:
            account - a cloud account
            flavor_id - id flavor of the VM
        """
        with self._use_driver(account) as drv:
            drv.nova_flavor_delete(flavor_id)

    @staticmethod
    def _fill_epa_attributes(flavor, flavor_info):
        """Helper function to populate the EPA attributes

        Arguments:
              flavor     : Object with EPA attributes
              flavor_info: A dictionary of flavor_info received from openstack
        Returns:
              None
        """
        getattr(flavor, 'vm_flavor').vcpu_count  = flavor_info['vcpus']
        getattr(flavor, 'vm_flavor').memory_mb   = flavor_info['ram']
        getattr(flavor, 'vm_flavor').storage_gb  = flavor_info['disk']

        ### If extra_specs in flavor_info
        if not 'extra_specs' in flavor_info:
            return

        for attr in flavor_info['extra_specs']:
            if attr == 'hw:cpu_policy':
                cpu_pinning_policy = espec_utils.guest.extra_spec_to_mano_cpu_pinning_policy(flavor_info['extra_specs']['hw:cpu_policy'])
                if cpu_pinning_policy is not None:
                    getattr(flavor, 'guest_epa').cpu_pinning_policy = cpu_pinning_policy

            elif attr == 'hw:cpu_threads_policy':
                cpu_thread_pinning_policy = espec_utils.guest.extra_spec_to_mano_cpu_thread_pinning_policy(flavor_info['extra_specs']['hw:cpu_threads_policy'])
                if cpu_thread_pinning_policy is not None:
                    getattr(flavor, 'guest_epa').cpu_thread_pinning_policy = cpu_thread_pinning_policy

            elif attr == 'hw:mem_page_size':
                mempage_size = espec_utils.guest.extra_spec_to_mano_mempage_size(flavor_info['extra_specs']['hw:mem_page_size'])
                if mempage_size is not None:
                    getattr(flavor, 'guest_epa').mempage_size = mempage_size


            elif attr == 'hw:numa_nodes':
                numa_node_count = espec_utils.guest.extra_specs_to_mano_numa_node_count(flavor_info['extra_specs']['hw:numa_nodes'])
                if numa_node_count is not None:
                    getattr(flavor,'guest_epa').numa_node_policy.node_cnt = numa_node_count

            elif attr.startswith('hw:numa_cpus.'):
                node_id = attr.split('.')[1]
                nodes = [ n for n in flavor.guest_epa.numa_node_policy.node if n.id == int(node_id) ]
                if nodes:
                    numa_node = nodes[0]
                else:
                    numa_node = getattr(flavor,'guest_epa').numa_node_policy.node.add()
                    numa_node.id = int(node_id)

                numa_node.vcpu = [ int(x) for x in flavor_info['extra_specs'][attr].split(',') ]

            elif attr.startswith('hw:numa_mem.'):
                node_id = attr.split('.')[1]
                nodes = [ n for n in flavor.guest_epa.numa_node_policy.node if n.id == int(node_id) ]
                if nodes:
                    numa_node = nodes[0]
                else:
                    numa_node = getattr(flavor,'guest_epa').numa_node_policy.node.add()
                    numa_node.id = int(node_id)

                numa_node.memory_mb =  int(flavor_info['extra_specs'][attr])

            elif attr == 'hw:numa_mempolicy':
                numa_memory_policy = espec_utils.guest.extra_to_mano_spec_numa_memory_policy(flavor_info['extra_specs']['hw:numa_mempolicy'])
                if numa_memory_policy is not None:
                    getattr(flavor,'guest_epa').numa_node_policy.mem_policy = numa_memory_policy

            elif attr == 'trust:trusted_host':
                trusted_execution = espec_utils.guest.extra_spec_to_mano_trusted_execution(flavor_info['extra_specs']['trust:trusted_host'])
                if trusted_execution is not None:
                    getattr(flavor,'guest_epa').trusted_execution = trusted_execution

            elif attr == 'pci_passthrough:alias':
                device_types = flavor_info['extra_specs']['pci_passthrough:alias']
                for device in device_types.split(','):
                    dev = getattr(flavor,'guest_epa').pcie_device.add()
                    dev.device_id = device.split(':')[0]
                    dev.count = int(device.split(':')[1])

            elif attr == 'capabilities:cpu_info:model':
                cpu_model = espec_utils.host.extra_specs_to_mano_cpu_model(flavor_info['extra_specs']['capabilities:cpu_info:model'])
                if cpu_model is not None:
                    getattr(flavor, 'host_epa').cpu_model = cpu_model

            elif attr == 'capabilities:cpu_info:arch':
                cpu_arch = espec_utils.host.extra_specs_to_mano_cpu_arch(flavor_info['extra_specs']['capabilities:cpu_info:arch'])
                if cpu_arch is not None:
                    getattr(flavor, 'host_epa').cpu_arch = cpu_arch

            elif attr == 'capabilities:cpu_info:vendor':
                cpu_vendor = espec_utils.host.extra_spec_to_mano_cpu_vendor(flavor_info['extra_specs']['capabilities:cpu_info:vendor'])
                if cpu_vendor is not None:
                    getattr(flavor, 'host_epa').cpu_vendor = cpu_vendor

            elif attr == 'capabilities:cpu_info:topology:sockets':
                cpu_sockets = espec_utils.host.extra_spec_to_mano_cpu_socket_count(flavor_info['extra_specs']['capabilities:cpu_info:topology:sockets'])
                if cpu_sockets is not None:
                    getattr(flavor, 'host_epa').cpu_socket_count = cpu_sockets

            elif attr == 'capabilities:cpu_info:topology:cores':
                cpu_cores = espec_utils.host.extra_spec_to_mano_cpu_core_count(flavor_info['extra_specs']['capabilities:cpu_info:topology:cores'])
                if cpu_cores is not None:
                    getattr(flavor, 'host_epa').cpu_core_count = cpu_cores

            elif attr == 'capabilities:cpu_info:topology:threads':
                cpu_threads = espec_utils.host.extra_spec_to_mano_cpu_core_thread_count(flavor_info['extra_specs']['capabilities:cpu_info:topology:threads'])
                if cpu_threads is not None:
                    getattr(flavor, 'host_epa').cpu_core_thread_count = cpu_threads

            elif attr == 'capabilities:cpu_info:features':
                cpu_features = espec_utils.host.extra_spec_to_mano_cpu_features(flavor_info['extra_specs']['capabilities:cpu_info:features'])
                if cpu_features is not None:
                    for feature in cpu_features:
                        getattr(flavor, 'host_epa').cpu_feature.append(feature)
            elif attr.startswith('aggregate_instance_extra_specs:'):
                    aggregate = getattr(flavor, 'host_aggregate').add()
                    aggregate.metadata_key = ":".join(attr.split(':')[1::])
                    aggregate.metadata_value = flavor_info['extra_specs'][attr]

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
        flavor.name                       = flavor_info['name']
        flavor.id                         = flavor_info['id']
        RwcalOpenstackPlugin._fill_epa_attributes(flavor, flavor_info)
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
        with self._use_driver(account) as drv:
            flavors = drv.nova_flavor_list()
        for flv in flavors:
            response.flavorinfo_list.append(RwcalOpenstackPlugin._fill_flavor_info(flv))
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
        with self._use_driver(account) as drv:
            flavor = drv.nova_flavor_get(id)
        return RwcalOpenstackPlugin._fill_flavor_info(flavor)


    def _fill_network_info(self, network_info, account):
        """Create a GI object from network info dictionary

        Converts Network information dictionary object returned by openstack
        driver into Protobuf Gi Object

        Arguments:
            network_info - Network information from openstack
            account - a cloud account

        Returns:
            Network info item
        """
        network                  = RwcalYang.NetworkInfoItem()
        network.network_name     = network_info['name']
        network.network_id       = network_info['id']
        if ('provider:network_type' in network_info) and (network_info['provider:network_type'] != None):
            network.provider_network.overlay_type = network_info['provider:network_type'].upper()
        if ('provider:segmentation_id' in network_info) and (network_info['provider:segmentation_id']):
            network.provider_network.segmentation_id = network_info['provider:segmentation_id']
        if ('provider:physical_network' in network_info) and (network_info['provider:physical_network']):
            network.provider_network.physical_network = network_info['provider:physical_network'].upper()

        if 'subnets' in network_info and network_info['subnets']:
            subnet_id = network_info['subnets'][0]
            with self._use_driver(account) as drv:
                subnet = drv.neutron_subnet_get(subnet_id)
            network.subnet = subnet['cidr']
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
        with self._use_driver(account) as drv:
            networks = drv.neutron_network_list()
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
        with self._use_driver(account) as drv:
            network = drv.neutron_network_get(id)
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
        kwargs = {}
        kwargs['name']            = network.network_name
        kwargs['admin_state_up']  = True
        kwargs['external_router'] = False
        kwargs['shared']          = False

        if network.has_field('provider_network'):
            if network.provider_network.has_field('physical_network'):
                kwargs['physical_network'] = network.provider_network.physical_network
            if network.provider_network.has_field('overlay_type'):
                kwargs['network_type'] = network.provider_network.overlay_type.lower()
            if network.provider_network.has_field('segmentation_id'):
                kwargs['segmentation_id'] = network.provider_network.segmentation_id

        with self._use_driver(account) as drv:
            network_id = drv.neutron_network_create(**kwargs)
            drv.neutron_subnet_create(network_id = network_id,
                                      cidr = network.subnet)
        return network_id

    @rwstatus
    def do_delete_network(self, account, network_id):
        """Delete a network

        Arguments:
            account - a cloud account
            network_id - an id for the network
        """
        with self._use_driver(account) as drv:
            drv.neutron_network_delete(network_id)

    @staticmethod
    def _fill_port_info(port_info):
        """Create a GI object from port info dictionary

        Converts Port information dictionary object returned by openstack
        driver into Protobuf Gi Object

        Arguments:
            port_info - Port information from openstack

        Returns:
            Port info item
        """
        port = RwcalYang.PortInfoItem()

        port.port_name  = port_info['name']
        port.port_id    = port_info['id']
        port.network_id = port_info['network_id']
        port.port_state = port_info['status']
        if 'device_id' in port_info:
            port.vm_id = port_info['device_id']
        if 'fixed_ips' in port_info:
            port.ip_address = port_info['fixed_ips'][0]['ip_address']
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
        with self._use_driver(account) as drv:
            port = drv.neutron_port_get(port_id)

        return RwcalOpenstackPlugin._fill_port_info(port)

    @rwstatus(ret_on_failure=[[]])
    def do_get_port_list(self, account):
        """Return a list of ports

        Arguments:
            account - a cloud account

        Returns:
            Port info list
        """
        response = RwcalYang.VimResources()
        with self._use_driver(account) as drv:
            ports = drv.neutron_port_list(*{})
        for port in ports:
            response.portinfo_list.append(RwcalOpenstackPlugin._fill_port_info(port))
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
        kwargs = {}
        kwargs['name'] = port.port_name
        kwargs['network_id'] = port.network_id
        kwargs['admin_state_up'] = True
        if port.has_field('vm_id'):
            kwargs['vm_id'] = port.vm_id
        if port.has_field('port_type'):
            kwargs['port_type'] = port.port_type
        else:
            kwargs['port_type'] = "normal"

        with self._use_driver(account) as drv:
            return drv.neutron_port_create(**kwargs)

    @rwstatus
    def do_delete_port(self, account, port_id):
        """Delete a port

        Arguments:
            account - a cloud account
            port_id - an id for port
        """
        with self._use_driver(account) as drv:
            drv.neutron_port_delete(port_id)

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

    @staticmethod
    def _fill_connection_point_info(c_point, port_info):
        """Create a GI object for RwcalYang.VDUInfoParams_ConnectionPoints()

        Converts Port information dictionary object returned by openstack
        driver into Protobuf Gi Object

        Arguments:
            port_info - Port information from openstack
        Returns:
            Protobuf Gi object for RwcalYang.VDUInfoParams_ConnectionPoints
        """
        c_point.name = port_info['name']
        c_point.connection_point_id = port_info['id']
        if ('fixed_ips' in port_info) and (len(port_info['fixed_ips']) >= 1):
            if 'ip_address' in port_info['fixed_ips'][0]:
                c_point.ip_address = port_info['fixed_ips'][0]['ip_address']
        if port_info['status'] == 'ACTIVE':
            c_point.state = 'active'
        else:
            c_point.state = 'inactive'
        if 'network_id' in port_info:
            c_point.virtual_link_id = port_info['network_id']
        if ('device_id' in port_info) and (port_info['device_id']):
            c_point.vdu_id = port_info['device_id']

    @staticmethod
    def _fill_virtual_link_info(network_info, port_list, subnet):
        """Create a GI object for VirtualLinkInfoParams

        Converts Network and Port information dictionary object
        returned by openstack driver into Protobuf Gi Object

        Arguments:
            network_info - Network information from openstack
            port_list - A list of port information from openstack
            subnet: Subnet information from openstack
        Returns:
            Protobuf Gi object for VirtualLinkInfoParams
        """
        link = RwcalYang.VirtualLinkInfoParams()
        link.name  = network_info['name']
        if network_info['status'] == 'ACTIVE':
            link.state = 'active'
        else:
            link.state = 'inactive'
        link.virtual_link_id = network_info['id']
        for port in port_list:
            if port['device_owner'] == 'compute:None':
                c_point = link.connection_points.add()
                RwcalOpenstackPlugin._fill_connection_point_info(c_point, port)

        if subnet != None:
            link.subnet = subnet['cidr']

        if ('provider:network_type' in network_info) and (network_info['provider:network_type'] != None):
            link.provider_network.overlay_type = network_info['provider:network_type'].upper()
        if ('provider:segmentation_id' in network_info) and (network_info['provider:segmentation_id']):
            link.provider_network.segmentation_id = network_info['provider:segmentation_id']
        if ('provider:physical_network' in network_info) and (network_info['provider:physical_network']):
            link.provider_network.physical_network = network_info['provider:physical_network'].upper()

        return link

    @staticmethod
    def _fill_vdu_info(vm_info, flavor_info, mgmt_network, port_list, server_group):
        """Create a GI object for VDUInfoParams

        Converts VM information dictionary object returned by openstack
        driver into Protobuf Gi Object

        Arguments:
            vm_info - VM information from openstack
            flavor_info - VM Flavor information from openstack
            mgmt_network - Management network
            port_list - A list of port information from openstack
            server_group - A list (with one element or empty list) of server group to which this VM belongs
        Returns:
            Protobuf Gi object for VDUInfoParams
        """
        vdu = RwcalYang.VDUInfoParams()
        vdu.name = vm_info['name']
        vdu.vdu_id = vm_info['id']
        for network_name, network_info in vm_info['addresses'].items():
            if network_info and network_name == mgmt_network:
                for interface in network_info:
                    if 'OS-EXT-IPS:type' in interface:
                        if interface['OS-EXT-IPS:type'] == 'fixed':
                            vdu.management_ip = interface['addr']
                        elif interface['OS-EXT-IPS:type'] == 'floating':
                            vdu.public_ip = interface['addr']

        # Look for any metadata
        for key, value in vm_info['metadata'].items():
            if key == 'node_id':
                vdu.node_id = value
        if ('image' in vm_info) and ('id' in vm_info['image']):
            vdu.image_id = vm_info['image']['id']
        if ('flavor' in vm_info) and ('id' in vm_info['flavor']):
            vdu.flavor_id = vm_info['flavor']['id']

        if vm_info['status'] == 'ACTIVE':
            vdu.state = 'active'
        elif vm_info['status'] == 'ERROR':
            vdu.state = 'failed'
        else:
            vdu.state = 'inactive'

        if 'availability_zone' in vm_info:
            vdu.availability_zone = vm_info['availability_zone']

        if server_group:
            vdu.server_group.name = server_group[0]

        vdu.cloud_type  = 'openstack'
        # Fill the port information
        for port in port_list:
            c_point = vdu.connection_points.add()
            RwcalOpenstackPlugin._fill_connection_point_info(c_point, port)

        if flavor_info is not None:
            RwcalOpenstackPlugin._fill_epa_attributes(vdu, flavor_info)
        return vdu

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
        network.subnet       = link_params.subnet

        if link_params.provider_network:
            for field in link_params.provider_network.fields:
                if link_params.provider_network.has_field(field):
                    setattr(network.provider_network,
                            field,
                            getattr(link_params.provider_network, field))
        net_id = self.do_create_network(account, network, no_rwstatus=True)
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
        if not link_id:
            self.log.error("Empty link_id during the virtual link deletion")
            raise Exception("Empty link_id during the virtual link deletion")

        with self._use_driver(account) as drv:
            port_list = drv.neutron_port_list(**{'network_id': link_id})

        for port in port_list:
            if ((port['device_owner'] == 'compute:None') or (port['device_owner'] == '')):
                self.do_delete_port(account, port['id'], no_rwstatus=True)
        self.do_delete_network(account, link_id, no_rwstatus=True)

    @rwstatus(ret_on_failure=[None])
    def do_get_virtual_link(self, account, link_id):
        """Get information about virtual link.

        Arguments:
            account  - a cloud account
            link_id  - id for the virtual-link

        Returns:
            Object of type RwcalYang.VirtualLinkInfoParams
        """
        if not link_id:
            self.log.error("Empty link_id during the virtual link get request")
            raise Exception("Empty link_id during the virtual link get request")

        with self._use_driver(account) as drv:
            network = drv.neutron_network_get(link_id)
            if network:
                port_list = drv.neutron_port_list(**{'network_id': network['id']})
                if 'subnets' in network:
                    subnet = drv.neutron_subnet_get(network['subnets'][0])
                else:
                    subnet = None
                virtual_link = RwcalOpenstackPlugin._fill_virtual_link_info(network, port_list, subnet)
            else:
                virtual_link = None
            return virtual_link

    @rwstatus(ret_on_failure=[None])
    def do_get_virtual_link_list(self, account):
        """Get information about all the virtual links

        Arguments:
            account  - a cloud account

        Returns:
            A list of objects of type RwcalYang.VirtualLinkInfoParams
        """
        vnf_resources = RwcalYang.VNFResources()
        with self._use_driver(account) as drv:
            networks = drv.neutron_network_list()
            for network in networks:
                port_list = drv.neutron_port_list(**{'network_id': network['id']})
                if ('subnets' in network) and (network['subnets']):
                    subnet = drv.neutron_subnet_get(network['subnets'][0])
                else:
                    subnet = None
                virtual_link = RwcalOpenstackPlugin._fill_virtual_link_info(network, port_list, subnet)
                vnf_resources.virtual_link_info_list.append(virtual_link)
            return vnf_resources

    def _create_connection_point(self, account, c_point):
        """
        Create a connection point
        Arguments:
           account  - a cloud account
           c_point  - connection_points
        """
        port            = RwcalYang.PortInfoItem()
        port.port_name  = c_point.name
        port.network_id = c_point.virtual_link_id

        if c_point.type_yang == 'VIRTIO':
            port.port_type = 'normal'
        elif c_point.type_yang == 'SR_IOV':
            port.port_type = 'direct'
        else:
            raise NotImplementedError("Port Type: %s not supported" %(c_point.port_type))

        port_id = self.do_create_port(account, port, no_rwstatus=True)
        return port_id

    def _allocate_floating_ip(self, drv, pool_name):
        """
        Allocate a floating_ip. If unused floating_ip exists then its reused.
        Arguments:
          drv:       OpenstackDriver instance
          pool_name: Floating IP pool name

        Returns:
          An object of floating IP nova class (novaclient.v2.floating_ips.FloatingIP)
        """

        # available_ip = [ ip for ip in drv.nova_floating_ip_list() if ip.instance_id == None ]

        # if pool_name is not None:
        #     ### Filter further based on IP address
        #     available_ip = [ ip for ip in available_ip if ip.pool == pool_name ]

        # if not available_ip:
        #     floating_ip = drv.nova_floating_ip_create(pool_name)
        # else:
        #     floating_ip = available_ip[0]

        floating_ip = drv.nova_floating_ip_create(pool_name)
        return floating_ip


    @rwstatus(ret_on_failure=[""])
    def do_create_vdu(self, account, vdu_init):
        """Create a new virtual deployment unit

        Arguments:
            account     - a cloud account
            vdu_init  - information about VDU to create (RwcalYang.VDUInitParams)

        Returns:
            The vdu_id
        """
        ### First create required number of ports aka connection points
        with self._use_driver(account) as drv:
            ### If floating_ip is required and we don't have one, better fail before any further allocation
            if vdu_init.has_field('allocate_public_address') and vdu_init.allocate_public_address:
                if account.openstack.has_field('floating_ip_pool'):
                    pool_name = account.openstack.floating_ip_pool
                else:
                    pool_name = None
                floating_ip = self._allocate_floating_ip(drv, pool_name)
            else:
                floating_ip = None

        port_list = []
        network_list = []
        for c_point in vdu_init.connection_points:
            if c_point.virtual_link_id in network_list:
                assert False, "Only one port per network supported. Refer: http://specs.openstack.org/openstack/nova-specs/specs/juno/implemented/nfv-multiple-if-1-net.html"
            else:
                network_list.append(c_point.virtual_link_id)
            port_id = self._create_connection_point(account, c_point)
            port_list.append(port_id)

        with self._use_driver(account) as drv:
            ### Now Create VM
            vm           = RwcalYang.VMInfoItem()
            vm.vm_name   = vdu_init.name
            vm.flavor_id = vdu_init.flavor_id
            vm.image_id  = vdu_init.image_id
            vm_network   = vm.network_list.add()
            vm_network.network_id = drv._mgmt_network_id
            if vdu_init.has_field('vdu_init') and vdu_init.vdu_init.has_field('userdata'):
                vm.cloud_init.userdata = vdu_init.vdu_init.userdata

            if vdu_init.has_field('node_id'):
                vm.user_tags.node_id   = vdu_init.node_id;

            if vdu_init.has_field('availability_zone') and vdu_init.availability_zone.has_field('name'):
                vm.availability_zone = vdu_init.availability_zone.name

            if vdu_init.has_field('server_group'):
                ### Get list of server group in openstack for name->id mapping
                openstack_group_list = drv.nova_server_group_list()
                group_id = [ i['id'] for i in openstack_group_list if i['name'] == vdu_init.server_group.name]
                if len(group_id) != 1:
                    raise OpenstackServerGroupError("VM placement failed. Server Group %s not found in openstack. Available groups" %(vdu_init.server_group.name, [i['name'] for i in openstack_group_list]))
                vm.server_group = group_id[0]

            for port_id in port_list:
                port = vm.port_list.add()
                port.port_id = port_id

            pci_assignement = self.prepare_vpci_metadata(drv, vdu_init)
            if pci_assignement != '':
                vm.user_tags.pci_assignement = pci_assignement

            vm_id = self.do_create_vm(account, vm, no_rwstatus=True)
            self.prepare_vdu_on_boot(account, vm_id, floating_ip)
            return vm_id

    def prepare_vpci_metadata(self, drv, vdu_init):
        pci_assignement = ''
        ### TEF specific metadata creation for
        virtio_vpci = []
        sriov_vpci = []
        virtio_meta = ''
        sriov_meta = ''
        ### For MGMT interface
        if vdu_init.has_field('mgmt_vpci'):
            xx = 'u\''+ drv._mgmt_network_id + '\' :[[u\'' + vdu_init.mgmt_vpci + '\', ' + '\'\']]'
            virtio_vpci.append(xx)

        for c_point in vdu_init.connection_points:
            if c_point.has_field('vpci'):
                if c_point.has_field('vpci') and c_point.type_yang == 'VIRTIO':
                    xx = 'u\''+c_point.virtual_link_id + '\' :[[u\'' + c_point.vpci + '\', ' + '\'\']]'
                    virtio_vpci.append(xx)
                elif c_point.has_field('vpci') and c_point.type_yang == 'SR_IOV':
                    xx = '[u\'' + c_point.vpci + '\', ' + '\'\']'
                    sriov_vpci.append(xx)

        if virtio_vpci:
            virtio_meta += ','.join(virtio_vpci)

        if sriov_vpci:
            sriov_meta = 'u\'VF\': ['
            sriov_meta += ','.join(sriov_vpci)
            sriov_meta += ']'

        if virtio_meta != '':
            pci_assignement +=  virtio_meta
            pci_assignement += ','

        if sriov_meta != '':
            pci_assignement +=  sriov_meta

        if pci_assignement != '':
            pci_assignement = '{' + pci_assignement + '}'

        return pci_assignement



    def prepare_vdu_on_boot(self, account, server_id, floating_ip):
        cmd = PREPARE_VM_CMD.format(auth_url     = account.openstack.auth_url,
                                    username     = account.openstack.key,
                                    password     = account.openstack.secret,
                                    tenant_name  = account.openstack.tenant,
                                    mgmt_network = account.openstack.mgmt_network,
                                    server_id    = server_id)

        if floating_ip is not None:
            cmd += (" --floating_ip "+ floating_ip.ip)

        exec_path = 'python3 ' + os.path.dirname(openstack_drv.__file__)
        exec_cmd = exec_path+'/'+cmd
        self.log.info("Running command: %s" %(exec_cmd))
        subprocess.call(exec_cmd, shell=True)

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
        for c_point in vdu_modify.connection_points_add:
            if c_point.virtual_link_id in network_list:
                assert False, "Only one port per network supported. Refer: http://specs.openstack.org/openstack/nova-specs/specs/juno/implemented/nfv-multiple-if-1-net.html"
            else:
                network_list.append(c_point.virtual_link_id)
            port_id = self._create_connection_point(account, c_point)
            port_list.append(port_id)

        ### Now add the ports to VM
        for port_id in port_list:
            with self._use_driver(account) as drv:
                drv.nova_server_add_port(vdu_modify.vdu_id, port_id)

        ### Delete the requested connection_points
        for c_point in vdu_modify.connection_points_remove:
            self.do_delete_port(account, c_point.connection_point_id, no_rwstatus=True)

        if vdu_modify.has_field('image_id'):
            with self._use_driver(account) as drv:
                drv.nova_server_rebuild(vdu_modify.vdu_id, vdu_modify.image_id)


    @rwstatus
    def do_delete_vdu(self, account, vdu_id):
        """Delete a virtual deployment unit

        Arguments:
            account - a cloud account
            vdu_id  - id for the vdu to be deleted

        Returns:
            None
        """
        if not vdu_id:
            self.log.error("empty vdu_id during the vdu deletion")
            return

        with self._use_driver(account) as drv:
            ### Get list of floating_ips associated with this instance and delete them
            floating_ips = [ f for f in drv.nova_floating_ip_list() if f.instance_id == vdu_id ]
            for f in floating_ips:
                drv.nova_drv.floating_ip_delete(f)

            ### Get list of port on VM and delete them.
            port_list = drv.neutron_port_list(**{'device_id': vdu_id})

        for port in port_list:
            if ((port['device_owner'] == 'compute:None') or (port['device_owner'] == '')):
                self.do_delete_port(account, port['id'], no_rwstatus=True)

        self.do_delete_vm(account, vdu_id, no_rwstatus=True)


    @rwstatus(ret_on_failure=[None])
    def do_get_vdu(self, account, vdu_id):
        """Get information about a virtual deployment unit.

        Arguments:
            account - a cloud account
            vdu_id  - id for the vdu

        Returns:
            Object of type RwcalYang.VDUInfoParams
        """
        with self._use_driver(account) as drv:

            ### Get list of ports excluding the one for management network
            port_list = [p for p in drv.neutron_port_list(**{'device_id': vdu_id}) if p['network_id'] != drv.get_mgmt_network_id()]

            vm = drv.nova_server_get(vdu_id)

            flavor_info = None
            if ('flavor' in vm) and ('id' in vm['flavor']):
                try:
                    flavor_info = drv.nova_flavor_get(vm['flavor']['id'])
                except Exception as e:
                    self.log.critical("Exception encountered while attempting to get flavor info for flavor_id: %s. Exception: %s" %(vm['flavor']['id'], str(e)))

            openstack_group_list = drv.nova_server_group_list()
            server_group = [ i['name'] for i in openstack_group_list if vm['id'] in i['members']]
            return RwcalOpenstackPlugin._fill_vdu_info(vm,
                                                       flavor_info,
                                                       account.openstack.mgmt_network,
                                                       port_list,
                                                       server_group)


    @rwstatus(ret_on_failure=[None])
    def do_get_vdu_list(self, account):
        """Get information about all the virtual deployment units

        Arguments:
            account     - a cloud account

        Returns:
            A list of objects of type RwcalYang.VDUInfoParams
        """
        vnf_resources = RwcalYang.VNFResources()
        with self._use_driver(account) as drv:
            vms = drv.nova_server_list()
            for vm in vms:
                ### Get list of ports excluding one for management network
                port_list = [p for p in drv.neutron_port_list(**{'device_id': vm['id']}) if p['network_id'] != drv.get_mgmt_network_id()]

                flavor_info = None

                if ('flavor' in vm) and ('id' in vm['flavor']):
                    try:
                        flavor_info = drv.nova_flavor_get(vm['flavor']['id'])
                    except Exception as e:
                        self.log.critical("Exception encountered while attempting to get flavor info for flavor_id: %s. Exception: %s" %(vm['flavor']['id'], str(e)))

                else:
                    flavor_info = None

                openstack_group_list = drv.nova_server_group_list()
                server_group = [ i['name'] for i in openstack_group_list if vm['id'] in i['members']]

                vdu = RwcalOpenstackPlugin._fill_vdu_info(vm,
                                                          flavor_info,
                                                          account.openstack.mgmt_network,
                                                          port_list,
                                                          server_group)
                vnf_resources.vdu_info_list.append(vdu)
            return vnf_resources


