import asyncio
import os
import sys
import time
import yaml

import rift.openmano.rift2openmano as rift2openmano
import rift.openmano.openmano_client as openmano_client
from . import rwnsmplugin

if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async


DUMP_OPENMANO_DIR = os.path.join(
        os.environ["RIFT_ARTIFACTS"],
        "openmano_descriptors"
        )


def dump_openmano_descriptor(name, descriptor_str):
    filename = "{}_{}.yaml".format(
        time.strftime("%Y%m%d-%H%M%S"),
        name
        )

    filepath = os.path.join(
            DUMP_OPENMANO_DIR,
            filename
            )

    try:
        if not os.path.exists(DUMP_OPENMANO_DIR):
            os.makedirs(DUMP_OPENMANO_DIR)

        with open(filepath, 'w') as hdl:
            hdl.write(descriptor_str)

    except OSError as e:
        print("Failed to dump openmano descriptor: %s" % str(e))

    return filepath


class OpenmanoVnfr(object):
    def __init__(self, log, loop, cli_api, vnfr):
        self._log = log
        self._loop = loop
        self._cli_api = cli_api
        self._vnfr = vnfr
        self._vnfd_id = vnfr.vnfd.id

        self._vnf_id = None

        self._created = False

    @property
    def vnfd(self):
        return rift2openmano.RiftVNFD(self._vnfr.vnfd)

    @property
    def vnfr(self):
        return self._vnfr

    @property
    def rift_vnfd_id(self):
        return self._vnfd_id

    @property
    def openmano_vnfd_id(self):
        return self._vnf_id

    @property
    def openmano_vnfd(self):
        self._log.debug("Converting vnfd %s from rift to openmano", self.vnfd.id)
        openmano_vnfd = rift2openmano.rift2openmano_vnfd(self.vnfd)
        return openmano_vnfd

    @property
    def openmano_vnfd_yaml(self):
        return yaml.safe_dump(self.openmano_vnfd, default_flow_style=False)

    @asyncio.coroutine
    def create(self):
        self._log.debug("Creating openmano vnfd")
        openmano_vnfd = self.openmano_vnfd
        name = openmano_vnfd["vnf"]["name"]

        # If the name already exists, get the openmano vnfd id
        name_uuid_map = yield from self._loop.run_in_executor(
                    None,
                    self._cli_api.vnf_list,
                    )

        if name in name_uuid_map:
            vnf_id = name_uuid_map[name]
            self._log.debug("Vnf already created.  Got existing openmano vnfd id: %s", vnf_id)
            self._vnf_id = vnf_id
            return

        self._vnf_id, _ = yield from self._loop.run_in_executor(
                None,
                self._cli_api.vnf_create,
                self.openmano_vnfd_yaml,
                )

        fpath = dump_openmano_descriptor(
           "{}_vnf".format(name),
           self.openmano_vnfd_yaml
           )

        self._log.debug("Dumped Openmano VNF descriptor to: %s", fpath)

        self._created = True

    @asyncio.coroutine
    def delete(self):
        if not self._created:
            return

        self._log.debug("Deleting openmano vnfd")
        if self._vnf_id is None:
            self._log.warning("Openmano vnf id not set.  Cannot delete.")
            return

        yield from self._loop.run_in_executor(
                None,
                self._cli_api.vnf_delete,
                self._vnf_id,
                )


class OpenmanoNsr(object):
    TIMEOUT_SECS = 120

    def __init__(self, log, loop, publisher, cli_api, http_api, nsd_msg, nsr_config_msg):
        self._log = log
        self._loop = loop
        self._publisher = publisher
        self._cli_api = cli_api
        self._http_api = http_api

        self._nsd_msg = nsd_msg
        self._nsr_config_msg = nsr_config_msg

        self._vnfrs = []

        self._nsd_uuid = None
        self._nsr_uuid = None

        self._created = False

        self._monitor_task = None

    @property
    def nsd(self):
        return rift2openmano.RiftNSD(self._nsd_msg)

    @property
    def vnfds(self):
        return {v.rift_vnfd_id: v.vnfd for v in self._vnfrs}

    @property
    def vnfrs(self):
        return self._vnfrs

    @property
    def openmano_nsd_yaml(self):
        self._log.debug("Converting nsd %s from rift to openmano", self.nsd.id)
        openmano_nsd = rift2openmano.rift2openmano_nsd(self.nsd, self.vnfds)
        return yaml.safe_dump(openmano_nsd, default_flow_style=False)

    @asyncio.coroutine
    def add_vnfr(self, vnfr):
        vnfr = OpenmanoVnfr(self._log, self._loop, self._cli_api, vnfr)
        yield from vnfr.create()
        self._vnfrs.append(vnfr)

    @asyncio.coroutine
    def delete(self):
        if not self._created:
            self._log.debug("NSD wasn't created.  Skipping delete.")
            return

        self._log.debug("Deleting openmano nsr")

        yield from self._loop.run_in_executor(
               None,
               self._cli_api.ns_delete,
               self._nsd_uuid,
               )

        self._log.debug("Deleting openmano vnfrs")
        for vnfr in self._vnfrs:
            yield from vnfr.delete()

    @asyncio.coroutine
    def create(self):
        self._log.debug("Creating openmano scenario")
        name_uuid_map = yield from self._loop.run_in_executor(
                None,
                self._cli_api.ns_list,
                )

        if self._nsd_msg.name in name_uuid_map:
            self._log.debug("Found existing openmano scenario")
            self._nsd_uuid = name_uuid_map[self._nsd_msg.name]
            return


        # Use the nsd uuid as the scenario name to rebind to existing
        # scenario on reload or to support muliple instances of the name
        # nsd
        self._nsd_uuid, _ = yield from self._loop.run_in_executor(
                None,
                self._cli_api.ns_create,
                self.openmano_nsd_yaml,
                self._nsd_msg.name
                )
        fpath = dump_openmano_descriptor(
           "{}_nsd".format(self._nsd_msg.name),
           self.openmano_nsd_yaml,
           )

        self._log.debug("Dumped Openmano NS descriptor to: %s", fpath)

        self._created = True

    @asyncio.coroutine
    def instance_monitor_task(self):
        self._log.debug("Starting Instance monitoring task")

        vnfr_uuid_map = {}
        start_time = time.time()
        active_vnfs = []

        while True:
            yield from asyncio.sleep(1, loop=self._loop)

            try:
                instance_resp_json = yield from self._loop.run_in_executor(
                        None,
                        self._http_api.get_instance,
                        self._nsr_uuid,
                        )

                self._log.debug("Got instance response: %s for NSR ID %s",
                        instance_resp_json,
                        self._nsr_uuid)

            except openmano_client.InstanceStatusError as e:
                self._log.error("Could not get NS instance status: %s", str(e))
                continue

            def all_vms_active(vnf):
                for vm in vnf["vms"]:
                    vm_status = vm["status"]
                    vm_uuid = vm["uuid"]
                    if vm_status != "ACTIVE":
                        self._log.debug("VM is not yet active: %s (status: %s)", vm_uuid, vm_status)
                        return False

                return True

            def any_vm_active_nomgmtip(vnf):
                for vm in vnf["vms"]:
                    vm_status = vm["status"]
                    vm_uuid = vm["uuid"]
                    if vm_status != "ACTIVE":
                        self._log.debug("VM is not yet active: %s (status: %s)", vm_uuid, vm_status)
                        return False

                return True

            def any_vms_error(vnf):
                for vm in vnf["vms"]:
                    vm_status = vm["status"]
                    vm_vim_info = vm["vim_info"]
                    vm_uuid = vm["uuid"]
                    if vm_status == "ERROR":
                        self._log.error("VM Error: %s (vim_info: %s)", vm_uuid, vm_vim_info)
                        return True

                return False

            def get_vnf_ip_address(vnf):
                if "ip_address" in vnf:
                    return vnf["ip_address"].strip()
                return None

            def get_ext_cp_info(vnf):
                cp_info_list = []
                for vm in vnf["vms"]:
                    if "interfaces" not in vm:
                        continue

                    for intf in vm["interfaces"]:
                        if "external_name" not in intf:
                            continue

                        if not intf["external_name"]:
                            continue

                        ip_address = intf["ip_address"]
                        if ip_address is None:
                            ip_address = "0.0.0.0"

                        cp_info_list.append((intf["external_name"], ip_address))

                return cp_info_list

            def get_vnf_status(vnfr):
                # When we create an openmano descriptor we use <name>__<idx>
                # to come up with openmano constituent VNF name.  Use this
                # knowledge to map the vnfr back.
                openmano_vnfr_suffix = "__{}".format(
                        vnfr.vnfr.vnfr_msg.member_vnf_index_ref
                        )

                for vnf in instance_resp_json["vnfs"]:
                    if vnf["vnf_name"].endswith(openmano_vnfr_suffix):
                        return vnf

                self._log.warning("Could not find vnf status with name that ends with: %s",
                                  openmano_vnfr_suffix)
                return None

            for vnfr in self._vnfrs:
                if vnfr in active_vnfs:
                    # Skipping, so we don't re-publish the same VNF message.
                    continue

                vnfr_msg = vnfr.vnfr.vnfr_msg.deep_copy()
                vnfr_msg.operational_status = "init"

                try:
                    vnf_status = get_vnf_status(vnfr)
                    self._log.debug("Found VNF status: %s", vnf_status)
                    if vnf_status is None:
                        self._log.error("Could not find VNF status from openmano")
                        vnfr_msg.operational_status = "failed"
                        yield from self._publisher.publish_vnfr(None, vnfr_msg)
                        return

                    # If there was a VNF that has a errored VM, then just fail the VNF and stop monitoring.
                    if any_vms_error(vnf_status):
                        self._log.debug("VM was found to be in error state.  Marking as failed.")
                        vnfr_msg.operational_status = "failed"
                        yield from self._publisher.publish_vnfr(None, vnfr_msg)
                        return

                    if all_vms_active(vnf_status):
                        vnf_ip_address = get_vnf_ip_address(vnf_status)

                        if vnf_ip_address is None:
                            self._log.warning("No IP address obtained "
                                    "for VNF: {}, will retry.".format(
                                        vnf_status['vnf_name']))
                            continue

                        self._log.debug("All VMs in VNF are active.  Marking as running.")
                        vnfr_msg.operational_status = "running"

                        self._log.debug("Got VNF ip address: %s", vnf_ip_address)
                        vnfr_msg.mgmt_interface.ip_address = vnf_ip_address
                        vnfr_msg.vnf_configuration.config_access.mgmt_ip_address = vnf_ip_address

                        # Add connection point information for the config manager
                        cp_info_list = get_ext_cp_info(vnf_status)
                        for (cp_name, cp_ip) in cp_info_list:
                            cp = vnfr_msg.connection_point.add()
                            cp.name = cp_name
                            cp.short_name = cp_name
                            cp.ip_address = cp_ip

                        yield from self._publisher.publish_vnfr(None, vnfr_msg)
                        active_vnfs.append(vnfr)

                    if (time.time() - start_time) > OpenmanoNsr.TIMEOUT_SECS:
                        self._log.error("NSR timed out before reaching running state")
                        vnfr_msg.operational_status = "failed"
                        yield from self._publisher.publish_vnfr(None, vnfr_msg)
                        return

                except Exception as e:
                    vnfr_msg.operational_status = "failed"
                    yield from self._publisher.publish_vnfr(None, vnfr_msg)
                    self._log.exception("Caught exception publishing vnfr info: %s", str(e))
                    return

            if len(active_vnfs) == len(self._vnfrs):
                self._log.info("All VNF's are active.  Exiting NSR monitoring task")
                return

    @asyncio.coroutine
    def deploy(self):
        if self._nsd_uuid is None:
            raise ValueError("Cannot deploy an uncreated nsd")

        self._log.debug("Deploying openmano scenario")

        name_uuid_map = yield from self._loop.run_in_executor(
                None,
                self._cli_api.ns_instance_list,
                )


        openmano_datacenter = None
        if self._nsr_config_msg.has_field("om_datacenter"):
            openmano_datacenter = self._nsr_config_msg.om_datacenter

        if self._nsr_config_msg.name in name_uuid_map:
            self._log.debug("Found existing instance with nsr name: %s", self._nsr_config_msg.name)
            self._nsr_uuid = name_uuid_map[self._nsr_config_msg.name]

        else:
            self._nsr_uuid = yield from self._loop.run_in_executor(
                    None,
                    self._cli_api.ns_instantiate,
                    self._nsd_uuid,
                    self._nsr_config_msg.name,
                    openmano_datacenter
                    )

        self._monitor_task = asyncio.ensure_future(
                self.instance_monitor_task(), loop=self._loop
                )

    @asyncio.coroutine
    def terminate(self):
        if self._nsr_uuid is None:
            self._log.warning("Cannot terminate an un-instantiated nsr")
            return

        if self._monitor_task is not None:
            self._monitor_task.cancel()
            self._monitor_task = None

        self._log.debug("Terminating openmano nsr")
        yield from self._loop.run_in_executor(
               None,
               self._cli_api.ns_terminate,
               self._nsr_uuid,
               )


class OpenmanoNsPlugin(rwnsmplugin.NsmPluginBase):
    """
        RW Implentation of the NsmPluginBase
    """
    def __init__(self, dts, log, loop, publisher, cloud_account):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._publisher = publisher

        self._cli_api = None
        self._http_api = None
        self._openmano_nsrs = {}

        self._set_cloud_account(cloud_account)

    def _set_cloud_account(self, cloud_account):
        self._log.debug("Setting openmano plugin cloud account: %s", cloud_account)
        self._cli_api = openmano_client.OpenmanoCliAPI(
                self.log,
                cloud_account.openmano.host,
                cloud_account.openmano.port,
                cloud_account.openmano.tenant_id,
                )

        self._http_api = openmano_client.OpenmanoHttpAPI(
                self.log,
                cloud_account.openmano.host,
                cloud_account.openmano.port,
                cloud_account.openmano.tenant_id,
                )

    def create_nsr(self, nsr_config_msg, nsd_msg):
        """
        Create Network service record
        """
        openmano_nsr = OpenmanoNsr(
                self._log,
                self._loop,
                self._publisher,
                self._cli_api,
                self._http_api,
                nsd_msg,
                nsr_config_msg
                )
        self._openmano_nsrs[nsr_config_msg.id] = openmano_nsr

    @asyncio.coroutine
    def deploy(self, nsr_msg):
        openmano_nsr = self._openmano_nsrs[nsr_msg.ns_instance_config_ref]
        yield from openmano_nsr.create()
        yield from openmano_nsr.deploy()

    @asyncio.coroutine
    def instantiate_ns(self, nsr, xact):
        """
        Instantiate NSR with the passed nsr id
        """
        yield from nsr.instantiate(xact)

    @asyncio.coroutine
    def instantiate_vnf(self, nsr, vnfr):
        """
        Instantiate NSR with the passed nsr id
        """
        openmano_nsr = self._openmano_nsrs[nsr.id]
        yield from openmano_nsr.add_vnfr(vnfr)

        # Mark the VNFR as running
        # TODO: Create a task to monitor nsr/vnfr status
        vnfr_msg = vnfr.vnfr_msg.deep_copy()
        vnfr_msg.operational_status = "init"

        self._log.debug("Attempting to publish openmano vnf: %s", vnfr_msg)
        with self._dts.transaction() as xact:
            yield from self._publisher.publish_vnfr(xact, vnfr_msg)

    @asyncio.coroutine
    def instantiate_vl(self, nsr, vlr):
        """
        Instantiate NSR with the passed nsr id
        """
        pass

    @asyncio.coroutine
    def terminate_ns(self, nsr):
        """
        Terminate the network service
        """
        nsr_id = nsr.id
        openmano_nsr = self._openmano_nsrs[nsr_id]
        yield from openmano_nsr.terminate()
        yield from openmano_nsr.delete()

        with self._dts.transaction() as xact:
            for vnfr in openmano_nsr.vnfrs:
                self._log.debug("Unpublishing VNFR: %s", vnfr.vnfr.vnfr_msg)
                yield from self._publisher.unpublish_vnfr(xact, vnfr.vnfr.vnfr_msg)

        del self._openmano_nsrs[nsr_id]

    @asyncio.coroutine
    def terminate_vnf(self, vnfr):
        """
        Terminate the network service
        """
        pass

    @asyncio.coroutine
    def terminate_vl(self, vlr):
        """
        Terminate the virtual link
        """
        pass

