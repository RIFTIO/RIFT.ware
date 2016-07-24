
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import logging
import os
import shlex
import subprocess
import time
import uuid

import gi
gi.require_version('RwDts', '1.0')
gi.require_version('RwcalYang', '1.0')
gi.require_version('RwCal', '1.0')


from gi.repository import (
    RwDts as rwdts,
    RwcalYang,
)

import rift.rwcal.cloudsim.lvm as lvm
import rift.rwcal.cloudsim.lxc as lxc
import rift.tasklets
import rw_peas


class SaltConnectionTimeoutError(Exception):
    pass


class ContainerManager(rift.tasklets.Tasklet):
    def __init__(self, *args, **kwargs):
        super(ContainerManager, self).__init__(*args, **kwargs)
        self.lvm = None
        self.resources = None
        self.dts_api = None

    def start(self):
        super(ContainerManager, self).start()
        self.log.info("Starting ContainerManager")
        self.log.setLevel(logging.DEBUG)
        ResourceProvisioning.log_hdl = self.log_hdl

        self.log.debug("Registering with dts")
        self._dts = rift.tasklets.DTS(
                self.tasklet_info,
                RwcalYang.get_schema(),
                self.loop,
                self.on_dts_state_change
                )

        self.log.debug("Created DTS Api GI Object: %s", self._dts)

    def on_instance_started(self):
        self.log.debug("Got instance started callback")

    def stop(self):
        super(ContainerManager, self).stop()
        self.resources.destroy()
        self.lvm.destroy()

    @asyncio.coroutine
    def init(self):
        # Create the LVM backing store with the 'rift' volume group
        self.lvm = LvmProvisioning()
        self.resources = ResourceProvisioning(self.loop, self.log)

        # Create lvm partition
        yield from self.loop.run_in_executor(
                None,
                self.resources.destroy,
                )

        if "REUSE_LXC" not in os.environ:
            # Create lvm partition
            yield from self.loop.run_in_executor(
                    None,
                    self.lvm.destroy,
                    )

            # Create lvm partition
            yield from self.loop.run_in_executor(
                    None,
                    self.lvm.create,
                    )

        # Create an initial set of VMs
        yield from self.loop.run_in_executor(
                None,
                self.resources.create,
                )

        yield from self.loop.run_in_executor(
                None,
                self.resources.wait_ready,
                )

    @asyncio.coroutine
    def run(self):
        pass

    @asyncio.coroutine
    def on_dts_state_change(self, state):
        """Take action according to current dts state to transition
        application into the corresponding application state

        Arguments
            state - current dts state
        """

        switch = {
            rwdts.State.INIT: rwdts.State.REGN_COMPLETE,
            rwdts.State.CONFIG: rwdts.State.RUN,
        }

        handlers = {
            rwdts.State.INIT: self.init,
            rwdts.State.RUN: self.run,
        }

        # Transition application to next state
        handler = handlers.get(state, None)
        if handler is not None:
            yield from handler()

        # Transition dts to next state
        next_state = switch.get(state, None)
        if next_state is not None:
            self._dts.handle.set_state(next_state)


class LvmProvisioning(object):
    """
    This class represents LVM provisioning.
    """

    def create(self):
        """Creates an LVM backing store"""
        lvm.create('rift')

    def destroy(self):
        """Destroys the existing LVM backing store"""
        lvm.destroy('rift')


class ResourceProvisioning(object):
    """
    This is a placeholder class that is used to represent the provisioning of
    container resources.
    """

    cal_interface = None
    log_hdl = None

    def __init__(self, loop, log):
        # Initialize the CAL interface if it has not already been initialized
        if ResourceProvisioning.cal_interface is None:
            plugin = rw_peas.PeasPlugin('rwcal_cloudsimproxy', 'RwCal-1.0')
            engine, info, extension = plugin()

            ResourceProvisioning.cal_interface = plugin.get_interface("Cloud")
            ResourceProvisioning.cal_interface.init(ResourceProvisioning.log_hdl)

        self.account = RwcalYang.CloudAccount()
        self.account.account_type = "cloudsim_proxy"
        self.account.cloudsim_proxy.host = "192.168.122.1"

        self.log = log
        self.loop = loop
        self.nvms = 1

        self._vms = []

    @property
    def cal(self):
        return ResourceProvisioning.cal_interface

    def create(self):
        """Create all of the necessary resources"""

        rift_root = os.environ['RIFT_ROOT']
        image = self.create_image("%s/images/rift-root-latest.qcow2" % (rift_root))

        # Create a VM
        for index in range(self.nvms):
            self._vms.append(self.create_vm(image, index))

        # Start the VMs
        for vm in self._vms:
            self.cal.start_vm(self.account, vm.vm_id)

    def destroy(self):
        """Destroy all of the provided resources"""

        for container in lxc.containers():
            lxc.stop(container)

        for container in lxc.containers():
            if not ("REUSE_LXC" in os.environ and container == "rwm0"):
                lxc.destroy(container)

    def create_image(self, location):
        """Creates and returns a CAL image"""

        image = RwcalYang.ImageInfoItem()
        image.name = "rift-lxc-image"
        image.location = location
        image.disk_format = "qcow2"
        rc, image.id = self.cal.create_image(self.account, image)
        return image

    def create_network(self, network_name, subnet):
        """Creates and returns a CAL network"""

        network = RwcalYang.NetworkInfoItem(
                network_name=network_name,
                subnet=subnet,
                )
        rc, network.network_id = self.cal.create_network(self.account, network)
        return network

    def create_vm(self, image, index):
        """Returns a VM

        Arguments:
            image - the image used to create the VM
            index - an index used to label the VM

        Returns:
            A VM object

        """
        vm = RwcalYang.VMInfoItem()
        vm.vm_name = 'rift-s{}'.format(index + 1)
        vm.image_id = image.id
        vm.user_tags.node_id = str(uuid.uuid4())

        user_data_template_str = open(
                    os.path.join(
                        os.environ['RIFT_INSTALL'],
                        'etc/userdata-template',
                        )
                    ).read()

        # Get the interface ip address of the mgmt network
        # This is where the salt master is accessible on
        mgmt_interface_ip = "192.168.122.1"

        # Create salt-stack userdata
        vm.cloud_init.userdata = user_data_template_str.format(
                master_ip=mgmt_interface_ip,
                lxcname=vm.user_tags.node_id,
                )

        rc, vm.vm_id = self.cal.create_vm(self.account, vm)

        return vm

    def wait_vm_salt_connection(self, vm, timeout_secs=600):
        """ Wait for vm salt minion to reach up state with master """

        vm_node_id = vm.user_tags.node_id
        start_time = time.time()
        self.log.debug("Waiting up to %s seconds for node id %s",
                       timeout_secs, vm_node_id)
        while (time.time() - start_time) < timeout_secs:
            try:
                stdout = subprocess.check_output(
                        shlex.split('salt %s test.ping' % vm_node_id),
                        universal_newlines=True,
                        )
            except subprocess.CalledProcessError:
                continue

            up_minions = stdout.splitlines()
            for line in up_minions:
                if "True" in line:
                    return

        raise SaltConnectionTimeoutError(
                "Salt id %s did not enter UP state in %s seconds" % (
                    vm_node_id, timeout_secs
                    )
                )

    def wait_ready(self):
        """ Wait for all resources to become ready """

        self.log.info("Waiting for all VM's to make a salt minion connection")
        for i, vm in enumerate(self._vms):
            self.wait_vm_salt_connection(vm)
            self.log.debug(
                "Node id %s came up (%s/%s)",
                vm.user_tags.node_id, i + 1, len(self._vms)
                )

    def create_port(self, network, vm, index):
        """Returns a port

        Arguments:
            network - a network object
            vm      - a VM object
            index   - an index to label the port

        Returns:
            Returns a port object

        """
        port = RwcalYang.PortInfoItem()
        port.port_name = "eth1"
        port.network_id = network.network_id
        port.vm_id = vm.vm_id

        rc, port.port_id = self.cal.create_port(self.account, port)
        return port
