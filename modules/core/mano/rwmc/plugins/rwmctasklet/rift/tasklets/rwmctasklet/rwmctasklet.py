
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import logging
import os
import time
from datetime import timedelta

import gi
gi.require_version('RwDts', '1.0')
gi.require_version('RwMcYang', '1.0')
gi.require_version('RwMcYang', '1.0')
gi.require_version('RwcalYang', '1.0')
gi.require_version('RwTypes', '1.0')
gi.require_version('RwCal', '1.0')


from gi.repository import (
    RwDts as rwdts,
    RwMcYang,
    RwcalYang,
    RwsdnYang,
    RwTypes,
    ProtobufC,
)

import rift.rwcal.cloudsim
import rift.tasklets
import rw_peas

from . import launchpad


class LaunchpadStartError(Exception):
    pass


class LaunchpadStopError(Exception):
    pass


class UnknownCloudTypeError(Exception):
    pass


class AddResourceError(Exception):
    pass


class AssignResourceError(Exception):
    pass


class RemoveResourceError(Exception):
    pass


class PoolError(Exception):
    pass


class AddPoolError(PoolError):
    pass


class DeletePoolError(PoolError):
    pass


class PoolNotFoundError(PoolError):
    pass


class ResourceNotFoundError(Exception):
    pass


class DeleteMgmtDomainError(Exception):
    pass


class MgmtDomainNotFound(Exception):
    pass


class CloudAccountNotFound(Exception):
    pass


class CloudAccountError(Exception):
    pass


class AllocateError(Exception):
    pass


class DeallocateError(Exception):
    pass


class CalCallFailure(Exception):
    pass

class SdnCallFailure(Exception):
    pass

class SDNAccountError(Exception):
    pass


class MgmtDomain(object):
    def __init__(self, loop, log, name, vm_pool_mgr, network_pool_mgr, lp_minions):
        self._loop = loop
        self._log = log
        self._name = name
        self._vm_pool_mgr = vm_pool_mgr
        self._network_pool_mgr = network_pool_mgr
        self._lp_minions = lp_minions

        self._launchpad = launchpad.Launchpad(name)
        self._lifetime_manager = launchpad.LaunchpadSaltLifetimeManager(
                loop=self._loop,
                log=self._log,
                launchpad=self._launchpad,
                )

        self._lp_vm_allocator = PoolResourceAllocator(
                self._log,
                self._loop,
                self,
                self._vm_pool_mgr,
                1
                )

        self._lp_configurer = launchpad.LaunchpadConfigurer(
                loop=self._loop,
                log=self._log,
                launchpad=self._launchpad,
                vm_pool_mgr=self._vm_pool_mgr,
                network_pool_mgr=self._network_pool_mgr,
                )

        self._cloud_account = None
        self._launchpad_vm_info = None
        self._start_launchpad_task = None

    @property
    def name(self):
        return self._name

    @property
    def launchpad_vm_info(self):
        return self._launchpad_vm_info

    @launchpad_vm_info.setter
    def launchpad_vm_info(self, vm_info):
        self._launchpad_vm_info = vm_info

    @property
    def launchpad_state(self):
        return self._lifetime_manager.state

    @property 
    def launchpad_state_details(self):
        return self._lifetime_manager.state_details

    @property
    def launchpad_state_details(self):
        return self._lifetime_manager.state_details

    @property
    def launchpad_uptime(self):
        return self._lifetime_manager.uptime()

    @property
    def launchpad_create_time(self):
        return self._lifetime_manager.start_time

    @asyncio.coroutine
    def _allocate_launchpad(self):
        self._log.info("Starting to allocate launchpad: %s", self)

        # If there are no static resources allocated to the mgmt domain
        # then use the fallback resources.  This is an extreme hack and must
        # be removed at the first opportunity (12-2-2015).
        if (fallback_launchpad_resources is not None and
                not self._lp_vm_allocator.has_static_resources()):
            self._log.debug("Allocating launchpad from command line node ids")

            try:
                self._cloud_account = self._lp_vm_allocator.get_cloud_account()
            except Exception as e:
                self._log.warning(str(e))
                return False

            try:
                node_ip, node_id = fallback_launchpad_resources.pop()
            except IndexError:
                raise LaunchpadStartError("No more launchpad resources to allocate")

            self._launchpad.ip_address = node_ip
            self._launchpad.node_id = node_id

            # Fill in launchpad_vm_info so UI can query management_ip address
            self._launchpad_vm_info = RwcalYang.VMInfoItem(management_ip=node_ip, public_ip=fallback_launchpad_public_ip)
            return True

        self._log.info("Allocating launchpad from available resources")
        lp_resources = yield from self._lp_vm_allocator.allocate()

        if lp_resources is None:
            self._log.info("Launchpad resources unavailable, Mgmt-domain possibly deleted before resources were allocated.")
            return False

        pool = self._lp_vm_allocator.pool
        vm_id = self._lp_vm_allocator.resources[0]

        self._cloud_account = pool.cloud_account
        self._launchpad_vm_info = self._cloud_account.get_vm_info(vm_id)
        if self._launchpad_vm_info is None:
            raise LaunchpadStartError("Could not get vm info from cloud account")

        self._launchpad.ip_address = self._launchpad_vm_info.management_ip
        self._launchpad.node_id = self._launchpad_vm_info.user_tags.node_id

        self._log.info("Got allocated launchpad vm_info: %s",
                       self._launchpad_vm_info,
                       )
        return True

    @asyncio.coroutine
    def _configure_launchpad(self):
        self._log.info("Starting to configure launchpad: %s", self)
        self._lifetime_manager._set_state("configuring", "Configuring Launchpad resources")
        yield from self._lp_configurer.configure(cloud_account=self._cloud_account)

    def allocate_start_configure_launchpad_task(self):
        @asyncio.coroutine
        def allocate_and_start_launchpad():
            self._log.info("Running allocate and start launchpad")

            try:
                if self._launchpad_vm_info is None:
                    self._log.info("Launchpad VM Not allocated, allocating now.")
                    is_lp_allocated = yield from self._allocate_launchpad()
                    if not is_lp_allocated:
                        self._log.info("Mgmt-domain was deleted before resources were allocated for Launchpad. Returning")
                        return
                    self._log.info("Launchpad VM allocated.")
                else:
                    self._log.info("Launchpad VM already allocated.")

                # Check if LP VM is already up & running in case of a MC reboot
                # We need to use Salt to query its minions,
                # if any minions are returned, need to extract the node_id
                # and match it to the VM info returned by CAL. In case of a match
                # use that launchpad node and assign it to MD instead of creating
                # a new LP.

                previous_lp = False
                for lp_minion in self._lp_minions:
                    self._log.info("Found minion running LP VM (node_id, job_id): %s", lp_minion)
                    if lp_minion == self._launchpad.node_id:
                        previous_lp = True
                        lp_job_id = self._lp_minions[lp_minion]
                        self._log.info("Launchpad VM already running %s",self._launchpad.node_id)

                if not previous_lp:
                    # If no running LP minion found, start a new launchpad
                    # and configure it
                    yield from self.start_launchpad(None)
                    yield from self._configure_launchpad()
                else:
                    # If a matching LP minion was found, use its job_id
                    # to associate it to the launchpad
                    yield from self.start_launchpad(lp_job_id)

                self._log.info('Launchpad Started: %s', self._launchpad)
                self._lifetime_manager._set_state("started", "Launchpad has started successfully")

            finally:
                # Clear out the handle for the launchpad task
                self._start_launchpad_task = None

        self._lifetime_manager._set_state("pending", "Waiting for Pool Resources")
        if self._start_launchpad_task is None:
            self._log.info("Creating future to allocate and start launchpad: %s", self)
            self._start_launchpad_task = asyncio.ensure_future(
                    allocate_and_start_launchpad(),
                    loop=self._loop,
                    )
            self._log.info("Created allocated and start future: %s", self._start_launchpad_task)

    @asyncio.coroutine
    def stop_launchpad(self):
        if self._start_launchpad_task is not None:
            self._log.debug("Cancelling task to start launchpad")
            self._start_launchpad_task.cancel()
            try:
                yield from self._start_launchpad_task
            except asyncio.CancelledError:
                pass

        self._log.info('Attempting to stop launchpad: %s', self._launchpad)
        yield from self._lifetime_manager.stop()
        self._log.info('Launchpad stopped: %s', self._launchpad)
        self._lp_minions = {}

    def release_launchpad(self):
        if self._launchpad_vm_info is None:
            self._log.debug("No launchpad to deallocate: %s")
            return

        # TODO: Ensure the launchpad is stopped
        node_ip_id = []
        if fallback_launchpad_resources is not None:
            node_ip_id.append(self._launchpad.ip_address)
            node_ip_id.append(self._launchpad.node_id)
            fallback_launchpad_resources.append(node_ip_id)

        self._lp_vm_allocator.deallocate()

        # Since Launchpad has been deallocated & deleted,
        # Remove the confd persist dir as well
        self._log.info("Launchpad resources deallocated, Removing confd reload dir")
        self._lifetime_manager.delete_confd_reload_dir()

        self._log.info("Deallocated launchpad vm_info: %s",
                       self._launchpad_vm_info,
                       )

        self._launchpad_vm_info = None

    @asyncio.coroutine
    def start_launchpad(self, lp_job_id):
        self._log.info('Attempting to start launchpad: %s', self._launchpad)
        yield from self._lifetime_manager.start(lp_job_id)


class CloudAccount(object):
    log_hdl = None

    def __init__(self, log, name, account):
        self.log = log
        self._name = name
        self._account = account

        self.cal_plugin = None
        self.engine = None

        self._cal = self.plugin.get_interface("Cloud")
        self._cal.init(CloudAccount.log_hdl)

        self._credential_status = None
        self._credential_status_details = None

    @property
    def plugin(self):
        raise NotImplementedError()

    def _wrap_status_fn(self, fn, *args, **kwargs):
        ret = fn(*args, **kwargs)
        rw_status = ret[0]
        if type(rw_status) != RwTypes.RwStatus:
            self.log.warning("First return value of %s function was not a RwStatus",
                             fn.__name__)
            return ret

        if rw_status != RwTypes.RwStatus.SUCCESS:
            msg = "%s returned %s" % (fn.__name__, str(rw_status))
            self.log.error(msg)
            raise CalCallFailure(msg)

        # If there was only one other return value besides rw_status, then just
        # return that element.  Otherwise return the rest of the return values
        # as a list.
        return ret[1] if len(ret) == 2 else ret[1:]

    @property
    def cal(self):
        return self._cal

    @property
    def name(self):
        return self._name

    @property
    def account(self):
        return self._account

    @property
    def credential_status(self):
        return self._credential_status

    @property
    def credential_status_details(self):
        return self._credential_status_details

    @classmethod
    def from_cfg(cls, log, cfg):
        raise NotImplementedError("Subclass needs to implement from_cfg")

    def get_vm_info(self, vm_id):
        rc, vm_info = self._cal.get_vm(
                self._account,
                vm_id,
                )

        return vm_info

    def list_vms(self):
        vms = self._wrap_status_fn(self._cal.get_vm_list, self._account)
        self.log.debug("Got get_vm_list response: %s", vms)
        return vms

    def list_networks(self):
        networks = self._wrap_status_fn(self._cal.get_network_list, self._account)
        self.log.debug("Got get_network_list response: %s", networks)
        return networks

    def update_from_cfg(self, cfg):
        self.log.debug("Updating parent CloudAccount to %s", cfg)
        self._account = cfg

    @asyncio.coroutine
    def validate_cloud_account_credentials(self, loop):
        self.log.debug("Validating Cloud Account credentials %s", self._account)
        rwstatus, status = yield from loop.run_in_executor(
                None,
                self._cal.validate_cloud_creds,
                self._account
                )
        self._credential_status = "Validated"
        self._credential_status_details = status.details
        self.log.info("Got cloud account validation response: %s", status)

class OpenstackCloudAccount(CloudAccount):
    def __init__(self, log, name, openstack_cfg):
        self._admin_account = RwcalYang.CloudAccount.from_dict({
                    "name": name,
                    "account_type": "openstack",
                    "openstack": openstack_cfg.as_dict()
                    })

        super().__init__(
                log,
                name,
                self._admin_account,
                )

    @property
    def plugin(self):
        if self.cal_plugin is None:
            self.cal_plugin = rw_peas.PeasPlugin('rwcal_openstack', 'RwCal-1.0')
            self.engine, _, _ = self.cal_plugin()

        return self.cal_plugin

    @classmethod
    def from_cfg(cls, log, cfg):
        return cls(log, cfg.name, cfg.openstack)

    def update_from_cfg(self, cfg):
        self.log.info("Updating Openstack Cloud Account: (current: %s) (requested: %s)", self._admin_account.openstack, cfg)

        account = self._admin_account.openstack.as_dict()
        for key, value in cfg.openstack:
            self.log.debug("Updating field: %s with value: %s (current value: %s)",
                    key, value, account[key] if key in account else None)
            account[key] = value

        self._admin_account = RwcalYang.CloudAccount.from_dict({
                    "name": cfg.name,
                    "account_type": "openstack",
                    "openstack": account
                    })

        self.log.debug("After local update, Opensatck Cloud Account is %s", self._admin_account)
        super().update_from_cfg(self._admin_account)


class AwsCloudAccount(CloudAccount):
    def __init__(self, log, name, aws_cfg):
        self._admin_account = RwcalYang.CloudAccount.from_dict({
                    "name": name,
                    "account_type": "aws",
                    "aws": aws_cfg.as_dict()
                    })

        super().__init__(
                log,
                name,
                self._admin_account,
                )

    @property
    def plugin(self):
        if self.cal_plugin is None:
            self.cal_plugin = rw_peas.PeasPlugin('rwcal_aws', 'RwCal-1.0')
            self.engine, _, _ = self.cal_plugin()

        return self.cal_plugin

    @classmethod
    def from_cfg(cls, log, cfg):
        return cls(log, cfg.name, cfg.aws)

    def update_from_cfg(self, cfg):
        self.log.info("Updating AWS Cloud Account: (current: %s) (requested: %s)", self._admin_account.aws, cfg)

        account = self._admin_account.aws.as_dict()
        for key, value in cfg.aws:
            self.log.debug("Updating field: %s with value: %s (current value: %s)",
                           key, value, account[key] if key in account else None)
            account[key] = value

        self._admin_account = RwcalYang.CloudAccount.from_dict({
                    "name": cfg.name,
                    "account_type": "aws",
                    "aws": account
                    })

        self.log.debug("After local update, AWS Cloud Account is %s", self._admin_account)
        super().update_from_cfg(self._admin_account)


class SimCloudAccount(CloudAccount):
    def __init__(self, log, name):
        super().__init__(
                log,
                name,
                RwcalYang.CloudAccount.from_dict({
                    "account_type": "cloudsim_proxy",
                    "name": name,
                    "cloudsim_proxy": {
                        "host": "192.168.122.1",
                        }
                    })
                )

    @property
    def plugin(self):
        if self.cal_plugin is None:
            self.cal_plugin = rw_peas.PeasPlugin('rwcal_cloudsim', 'RwCal-1.0')
            self.engine, _, _ = self.cal_plugin()

        return self.cal_plugin

    @staticmethod
    def enable_debug_logging(handlers):
        cal_logger = logging.getLogger("rift.rwcal.cloudsim")
        cal_logger.setLevel(logging.DEBUG)
        for hdl in handlers:
            cal_logger.addHandler(hdl)

    @classmethod
    def from_cfg(cls, log, cfg):
        return cls(log, cfg.name)

    def update_from_cfg(self, cfg):
        self.log.info("Updating Cloudsim Cloud Account - nothing to update! ")

class OpenmanoCloudAccount(CloudAccount):
    def __init__(self, log, name, openmano_cfg):
        self._openmano_account = RwcalYang.CloudAccount.from_dict({
                    "name": name,
                    "account_type": "openmano",
                    "openmano": openmano_cfg.as_dict()
                    })

        super().__init__(
                log,
                name,
                self._openmano_account,
                )

    @property
    def plugin(self):
        if self.cal_plugin is None:
            self.cal_plugin = rw_peas.PeasPlugin('rwcal_openmano', 'RwCal-1.0')
            self.engine, _, _ = self.cal_plugin()

        return self.cal_plugin

    @classmethod
    def from_cfg(cls, log, cfg):
        return cls(log, cfg.name, cfg.openmano)


def get_cloud_account_cls_from_type(cloud_type=None):
    cloud_map = {
            "cloudsim": SimCloudAccount,
            "openstack": OpenstackCloudAccount,
            "aws": AwsCloudAccount,
            "openmano": OpenmanoCloudAccount,
            }

    if cloud_type not in cloud_map:
        raise UnknownCloudTypeError("Unknown cloud type: %s" % cloud_type)

    return cloud_map[cloud_type]


class SDNAccount(object):
    def __init__(self, log, name, account):
        self.log = log
        self._name = name
        self._account = account
        self.sdn_plugin = None
        self.engine = None

        self._sdn = self.plugin.get_interface("Topology")
        self._sdn.init(SDNAccount.log_hdl)

    @property
    def plugin(self):
        raise NotImplementedError()

    def _wrap_status_fn(self, fn, *args, **kwargs):
        ret = fn(*args, **kwargs)
        rw_status = ret[0]
        if type(rw_status) != RwTypes.RwStatus:
            self.log.warning("First return value of %s function was not a RwStatus",
                             fn.__name__)
            return ret

        if rw_status != RwTypes.RwStatus.SUCCESS:
            msg = "%s returned %s" % (fn.__name__, str(rw_status))
            self.log.error(msg)
            raise SdnCallFailure(msg)

        # If there was only one other return value besides rw_status, then just
        # return that element.  Otherwise return the rest of the return values
        # as a list.
        return ret[1] if len(ret) == 2 else ret[1:]

    @property
    def sdn(self):
        return self._sdn

    @property
    def name(self):
        return self._name

    @property
    def account(self):
        return self._account

    @classmethod
    def from_cfg(cls, log, cfg):
        raise NotImplementedError("Subclass needs to implement from_cfg")

    def update_from_cfg(self, cfg):
        raise NotImplementedError("SDN Account update not yet supported")

    def list_networks(self):
        networks = self._wrap_status_fn(self._sdn.get_network_list, self._account)
        self.log.debug("Got get_network_list response: %s", networks)
        return networks


class OdlSDNAccount(SDNAccount):
    def __init__(self, log, name, odl_cfg):
        self._admin_account = RwsdnYang.SDNAccount.from_dict({
                    "name": name,
                    "odl": odl_cfg.as_dict()
                    })

        super().__init__(
                log,
                name,
                self._admin_account,
                )

    @property
    def plugin(self):
        if self.sdn_plugin is None:
            self.sdn_plugin = rw_peas.PeasPlugin('rwsdn_odl', 'RwSdn-1.0')
            self.engine, _, _ = self.sdn_plugin()

        return self.sdn_plugin

    @classmethod
    def from_cfg(cls, log, cfg):
        return cls(log, cfg.name, cfg.odl)

    def update_from_cfg(self, cfg):
        self.log.debug("Updating SDN account details: (current: %s) (requested: %s)",  self._admin_account, cfg)


class ResourcePool(object):
    def __init__(self, log, name, cloud_account, is_dynamic_scaling, mgmt_domain=None):
        self._log = log
        self._name = name
        self._cloud_account = cloud_account
        self._dynamic_scaling = is_dynamic_scaling
        self._mgmt_domain = mgmt_domain

        self._resource_ids = set()
        self._unallocated_ids = set()

    def __repr__(self):
        return ("{class_name}(name={name}, cloud_account={cloud_account}, "
                "dynamic_scaling={dynamic_scaling}, mgmt_domain={mgmt_domain})").format(
                class_name=self.__class__.__name__,
                name=self.name,
                cloud_account=self.cloud_account.name,
                dynamic_scaling=self.is_dynamic_scaling,
                mgmt_domain=self.mgmt_domain.name if self.mgmt_domain else None,
                )

    @property
    def name(self):
        return self._name

    @property
    def cloud_account(self):
        return self._cloud_account

    @property
    def is_dynamic_scaling(self):
        return self._dynamic_scaling

    @property
    def mgmt_domain(self):
        return self._mgmt_domain

    @mgmt_domain.setter
    def mgmt_domain(self, mgmt_domain):
        if self._mgmt_domain is not None and mgmt_domain is not None:
            raise PoolError("Resource pool already has an assigned mgmt_domain: %s",
                            self._mgmt_domain)

        self._mgmt_domain = mgmt_domain

    @property
    def resource_ids(self):
        return list(self._resource_ids)

    @property
    def unallocated_ids(self):
        return list(self._unallocated_ids)

    def add_resource_id(self, resource_id):
        self._log.debug("Adding resource id %s to pool %s", resource_id, self)
        self._resource_ids.add(resource_id)
        self._unallocated_ids.add(resource_id)

    def remove_resource_id(self, resource_id):
        self._log.debug("Removing resource id %s from pool %s", resource_id, self)
        self._resource_ids.remove(resource_id)
        # TODO: What if the resource is is allocated?

    def set_dynamic_scaling(self, is_dynamic_scaling):
        self._dynamic_scaling = is_dynamic_scaling

    def allocate(self, num=1):
        if num > len(self._unallocated_ids):
            raise AllocateError("Not enough unallocated resources in pool %s. "
                                "(num_resources: %s, num_unallocated: %s)",
                                self, len(self._resource_ids), len(self._unallocated_ids))

        ids = []
        for i in range(num):
            ids.append(self._unallocated_ids.pop())

        return ids

    def deallocate(self, resource_ids):
        for id in resource_ids:
            if id not in self._resource_ids:
                raise DeallocateError("Unknown resource id: %s", id)

        for id in resource_ids:
            self._unallocated_ids.add(id)


class PoolResourceAllocator(object):
    def __init__(self, log, loop, mgmt_domain, pool_mgr, num_allocate):
        self._log = log
        self._loop = loop
        self._mgmt_domain = mgmt_domain
        self._pool_mgr = pool_mgr
        self._num_allocate = num_allocate

        self._pool = None
        self._resources = None

    def __del__(self):
        if self._resources is not None:
            self.deallocate()

    @property
    def pool(self):
        return self._pool

    @property
    def resources(self):
        return self._resources

    def has_static_resources(self):
        for pool in self._pool_mgr.list_mgmt_domain_pools(self._mgmt_domain.name):
            if pool.resource_ids:
                return True

        return False

    def get_cloud_account(self):
        for pool in self._pool_mgr.list_mgmt_domain_pools(self._mgmt_domain.name):
            return pool.cloud_account

        raise CloudAccountError("Could not find cloud account associated with mgmt_domain: %s",
                                self._mgmt_domain.name)

    @asyncio.coroutine
    def allocate(self):
        self._log.info("Entered Pool Resource allocate")
        if self.pool is not None or self.resources is not None:
            raise AllocateError("Resources already allocated")

        self._log.info("Starting %s pool allocation for %s resouces",
                        self._pool, self._num_allocate)
        while self._resources is None:
            self._log.info("Pool resources is None, waiting for resources to allocate")
            # Add a callback notification to the pool for when resources
            # are available.
            yield from asyncio.sleep(3, loop=self._loop)

            try:
                current_pools = self._pool_mgr.list_mgmt_domain_pools(self._mgmt_domain.name)
            except Exception as e:
                self._log.warning("Mgmt Domain lookup may have failed (possibly due to mgmt-domain being deleted) , current_pools: %s", current_pools)
                break

            for pool in current_pools:
                try:
                    self._resources = pool.allocate(self._num_allocate)
                    self._pool = pool
                except AllocateError as e:
                    self._log.debug("Could not allocate resources from pool %s: %s",
                                    pool, str(e))

        return self._resources

    def deallocate(self):
        if self._resources is None:
            self._log.warning("Nothing to deallocate")
            return

        self._pool.deallocate(self._resources)

        self._resources = None
        self._pool = None


class ResourcePoolManager(object):
    def __init__(self, log, mgmt_domains, cloud_accounts):
        self._log = log
        self._mgmt_domains = mgmt_domains
        self._cloud_accounts = cloud_accounts

        self._resource_pools = {}

    @property
    def id_field(self):
        raise NotImplementedError()

    def list_cloud_resources(self, cloud_account):
        raise NotImplementedError()

    def _find_resource_id_pool(self, resource_id, cloud_account):
        for pool in self._resource_pools.values():
            if resource_id in pool.resource_ids:
                return pool

        return None

    def _get_mgmt_domain(self, mgmt_domain_name):
        try:
            return self._mgmt_domains[mgmt_domain_name]
        except KeyError as e:
            raise MgmtDomainNotFound(e)

    def _get_cloud_account(self, cloud_account_name):
        if cloud_account_name not in self._cloud_accounts:
            raise CloudAccountNotFound("Cloud account name not found: %s", cloud_account_name)

        cloud_account = self._cloud_accounts[cloud_account_name]

        return cloud_account

    def _assign_resource_pool_mgmt_domain(self, pool, mgmt_domain):
        try:
            self._log.debug("Assigning pool (%s) to mgmt_domain (%s)", pool, mgmt_domain)
            pool.mgmt_domain = mgmt_domain
        except PoolError as e:
            raise AssignResourceError(e)

        self._log.info("Assigned pool (%s) to mgmt_domain (%s)", pool, mgmt_domain)

    def _unassign_resource_pool_mgmt_domain(self, pool):
        try:
            mgmt_domain = pool.mgmt_domain
            if mgmt_domain is None:
                self._log.warning("Pool does not have a mgmt_domain assigned.")
                return

            self._log.debug("Unassigning pool (%s) from mgmt_domain (%s)", pool, mgmt_domain)
            pool.mgmt_domain = None
        except PoolError as e:
            raise AssignResourceError(e)

        self._log.info("Unassigned mgmt_domain (%s) from pool: %s", mgmt_domain, pool)

    def _assign_mgmt_domain(self, mgmt_domain, pool):
        self._log.debug("Assigning pool %s to mgmt_domain %s", pool, mgmt_domain)
        pool.mgmt_domain = mgmt_domain

    def _unassign_mgmt_domain(self, mgmt_domain, pool):
        self._log.debug("Unassigning pool %s from mgmt_domain %s", pool, mgmt_domain)
        pool.mgmt_domain = None

    def list_cloud_pools(self, cloud_account_name):
        cloud_pools = []
        cloud_account = self._get_cloud_account(cloud_account_name)
        for pool in self._resource_pools.values():
            if pool.cloud_account == cloud_account:
                cloud_pools.append(pool)

        return cloud_pools

    def list_mgmt_domain_pools(self, mgmt_domain_name):
        mgmt_domain_pools = []
        mgmt_domain = self._get_mgmt_domain(mgmt_domain_name)
        for pool in self._resource_pools.values():
            if pool.mgmt_domain == mgmt_domain:
                mgmt_domain_pools.append(pool)

        return mgmt_domain_pools

    def list_available_cloud_resources(self, cloud_account_name, cloud_resources=None):
        cloud = self._get_cloud_account(cloud_account_name)
        resources = []

        # If cloud_resources wasn't passed in, then fetch the latest resources
        # from the import cloud.
        if cloud_resources is None:
            cloud_resources = self.list_cloud_resources(cloud_account_name)

        for resource in cloud_resources:
            if self._find_resource_id_pool(
                    getattr(resource, self.id_field),
                    cloud,
                    ) is None:
                resources.append(resource)

        return resources

    def list_available_resources(self, pool_name, cloud_resources=None):
        pool = self.get_pool(pool_name)
        cloud_account = pool.cloud_account

        return self.list_available_cloud_resources(cloud_account.name, cloud_resources)

    def get_pool(self, pool_name):
        try:
            return self._resource_pools[pool_name]
        except KeyError as e:
            raise PoolNotFoundError(e)

    def delete_mgmt_domain_pool(self, mgmt_domain_name, pool_name):
        mgmt_domain = self._get_mgmt_domain(mgmt_domain_name)
        pool = self.get_pool(pool_name)

        self._log.debug("Deleting mgmt_domain %s pool:  %s)",
                        mgmt_domain, pool)

        self._unassign_mgmt_domain(mgmt_domain, pool)

    def update_mgmt_domain_pools(self, mgmt_domain_name, pool_name):
        mgmt_domain = self._get_mgmt_domain(mgmt_domain_name)
        pool = self.get_pool(pool_name)

        self._log.debug("Updating mgmt_domain %s pools: %s",
                        mgmt_domain, pool)

        self._assign_mgmt_domain(mgmt_domain, pool)

    def add_id_to_pool(self, pool_name, resource_id):
        pool = self.get_pool(pool_name)
        resource_list = self.list_cloud_resources(pool.cloud_account.name)
        resource_ids = [getattr(r, self.id_field) for r in resource_list]
        if resource_id not in resource_ids:
            msg = ("Could not find resource_id %s in cloud account %s" %
                   (resource_id, pool.cloud_account.name))
            raise AddResourceError(msg)

        find_pool = self._find_resource_id_pool(pool.cloud_account, resource_id)
        if find_pool is not None:
            msg = ("resource_id %s in cloud account %s already added to pool %s" %
                   (resource_id, pool.cloud_account.name, find_pool.name))
            raise AddResourceError(msg)

        self._log.debug("Adding id %s to pool %s", resource_id, pool)
        pool.add_resource_id(resource_id)

    def remove_id_from_pool(self, pool_name, resource_id):
        pool = self.get_pool(pool_name)
        try:
            self._log.debug("Removing id %s from pool %s", resource_id, pool)
            pool.remove_resource_id(resource_id)
        except ValueError as e:
            self._log.error("Could not remove unknown resource_id(%s) from pool(%s)",
                            resource_id, pool_name)
            raise RemoveResourceError(e)

        self._log.info("Removed Resource (%s) from pool: %s", resource_id, pool)

    def update_dynamic_scaling(self, pool_name, dynamic_scaling):
        pool = self.get_pool(pool_name)
        pool.set_dynamic_scaling(dynamic_scaling)
        self._log.info("Updated Resource pool Dynamic Scaling to %s", dynamic_scaling)

    def add_resource_pool(self, pool_name, cloud_account_name, assigned_ids, is_dynamic_scaling):
        if pool_name in self._resource_pools:
            self._log.warning("Pool name already exists: %s" % pool_name)
            return

        avail_resources = self.list_available_cloud_resources(cloud_account_name)
        avail_ids = [getattr(a, self.id_field) for a in avail_resources]
        for assign_id in assigned_ids:
            if assign_id not in avail_ids:
                raise AddPoolError("Resource ID already assigned or not found: %s", assign_id)

        cloud_account = self._get_cloud_account(cloud_account_name)

        pool = ResourcePool(
                self._log,
                pool_name,
                cloud_account,
                is_dynamic_scaling,
                )

        self._resource_pools[pool_name] = pool

        self._log.info("Added Resource Pool: %s", pool)

    def update_resource_pool(self, pool_name, cloud_account_name, assigned_ids, is_dynamic_scaling):
        pool = self.get_pool(pool_name)
        cloud_account = self._get_cloud_account(cloud_account_name)

        if pool.cloud_account != cloud_account:
            raise PoolError("Cannnot modify a resource pool's cloud account")

        current_ids = pool.resource_ids

        added_ids = set(assigned_ids) - set(current_ids)
        for id in added_ids:
            pool.add_resource_id(id)

        removed_ids = set(current_ids) - set(assigned_ids)
        for id in removed_ids:
            pool.remove_resource_id(id)

        pool.set_dynamic_scaling(is_dynamic_scaling)

        self._log.info("Updated Resource Pool: %s", pool)

    def delete_resource_pool(self, pool_name):
        pool = self.get_pool(pool_name)
        if pool.resource_ids:
            self._log.warning("Resource pool still has Resources: %s. Disassociating them from the pool.", pool.resource_ids)
            for resourceid in pool.resource_ids:
                self.remove_id_from_pool(pool_name, resourceid)

        if pool.mgmt_domain:
            raise DeletePoolError("Management Domain %s still associated with Resource Pool: %s",
                                  pool.mgmt_domain.name, pool_name)

        del self._resource_pools[pool_name]

        self._log.info("Removed Resource Pool: %s", pool)


class VMPoolManager(ResourcePoolManager):
    @property
    def id_field(self):
        return "vm_id"

    def list_cloud_resources(self, cloud_account_name):
        cloud = self._get_cloud_account(cloud_account_name)
        resources = cloud.list_vms()
        return resources.vminfo_list


class NetworkPoolManager(ResourcePoolManager):
    @property
    def id_field(self):
        return "network_id"

    def list_cloud_resources(self, cloud_account_name):
        cloud = self._get_cloud_account(cloud_account_name)
        resources = cloud.list_networks()
        return resources.networkinfo_list


def get_add_delete_update_cfgs(dts_member_reg, xact, key_name):
    # Unforunately, it is currently difficult to figure out what has exactly
    # changed in this xact without Pbdelta support (RIFT-4916)
    # As a workaround, we can fetch the pre and post xact elements and
    # perform a comparison to figure out adds/deletes/updates
    xact_cfgs = list(dts_member_reg.get_xact_elements(xact))
    curr_cfgs = list(dts_member_reg.elements)

    xact_key_map = {getattr(cfg, key_name): cfg for cfg in xact_cfgs}
    curr_key_map = {getattr(cfg, key_name): cfg for cfg in curr_cfgs}

    # Find Adds
    added_keys = set(xact_key_map) - set(curr_key_map)
    added_cfgs = [xact_key_map[key] for key in added_keys]

    # Find Deletes
    deleted_keys = set(curr_key_map) - set(xact_key_map)
    deleted_cfgs = [curr_key_map[key] for key in deleted_keys]

    # Find Updates
    updated_keys = set(curr_key_map) & set(xact_key_map)
    updated_cfgs = [xact_key_map[key] for key in updated_keys if xact_key_map[key] != curr_key_map[key]]

    return added_cfgs, deleted_cfgs, updated_cfgs


class ResourcePoolDtsConfigHandler(object):
    def __init__(self, dts, log, pool_mgr, xpath):
        self._dts = dts
        self._log = log
        self._pool_mgr = pool_mgr
        self._xpath = xpath

        self._pool_reg = None

    def _delete_pool(self, pool_name):
        self._log.info("Deleting pool %s", pool_name)

        self._pool_mgr.delete_resource_pool(pool_name)

    def _add_pool(self, pool_cfg):
        self._log.info("Adding pool: %s", pool_cfg)

        self._pool_mgr.add_resource_pool(
                pool_name=pool_cfg.name,
                cloud_account_name=pool_cfg.cloud_account,
                assigned_ids=[a.id for a in pool_cfg.assigned],
                is_dynamic_scaling=pool_cfg.dynamic_scaling,
                )

    def _update_pool(self, pool_cfg):
        self._log.info("Updating pool: %s", pool_cfg)

        self._pool_mgr.update_resource_pool(
                pool_name=pool_cfg.name,
                cloud_account_name=pool_cfg.cloud_account,
                assigned_ids=[a.id for a in pool_cfg.assigned],
                is_dynamic_scaling=pool_cfg.dynamic_scaling,
                )

    def register(self):
        """ Register for Resource Pool create/update/delete/read requests from dts """

        def apply_config(dts, acg, xact, action, _):
            """Apply the pending pool configuration"""

            self._log.debug("Got pool apply config (xact: %s) (action: %s)",
                            xact, action)

            if xact.xact is None:
                # When RIFT first comes up, an INSTALL is called with the current config
                # Since confd doesn't actally persist data this never has any data so
                # skip this for now.
                self._log.debug("No xact handle.  Skipping apply config")
                return

            return RwTypes.RwStatus.SUCCESS

        @asyncio.coroutine
        def on_prepare(dts, acg, xact, xact_info, ks_path, msg, scratch):
            """ prepare callback from dts for resource pool """

            action = xact_info.handle.get_query_action()

            self._log.debug("Got resource pool prepare config (msg %s) (action %s)",
                           msg, action)

            fref = ProtobufC.FieldReference.alloc()
            pb_msg = msg.to_pbcm()
            fref.goto_whole_message(pb_msg)

            if action == rwdts.QueryAction.UPDATE:
                # Got UPDATE action in prepare callback. Check what got Created/Updated in a Resource Pool
                # It could either be a create of a new pool or updates for existing pool.
                # Separating the creation of Pool and adding resources to the pool.
                # In case of updates, we do not get the entire existing config, but only what changed

                # Create a new pool, return if it already exists
                fref.goto_proto_name(pb_msg,"name")
                if fref.is_field_present():
                    self._add_pool(msg)

                # Now either a resource ID is assigned to a newly created pool
                # or a pool is updated with a resource ID.
                fref.goto_proto_name(pb_msg,"assigned")
                if fref.is_field_present():
                    ids = msg.get_assigned()
                    for assign_id in ids:
                        assign_id_pb = assign_id.to_pbcm()
                        fref.goto_proto_name(assign_id_pb,"id")
                        if fref.is_field_present():
                            self._pool_mgr.add_id_to_pool(msg.get_name(), assign_id.get_id())

                # Dynamic scaling attribute was updated
                fref.goto_proto_name(pb_msg, "dynamic_scaling")
                if fref.is_field_present():
                    self._pool_mgr.update_dynamic_scaling(msg.get_name(), msg.get_dynamic_scaling())


            elif action == rwdts.QueryAction.DELETE:
                # Got DELETE action in prepare callback
                # Check what got deleted - it could be either
                # the pool itself, or its cloud account, or its assigned IDs.

                # Did the entire pool get deleted?
                # no [vm|network]-pool pool <name>
                if fref.is_field_deleted():
                    self._delete_pool(msg.name);

                # Did the assigned ID get deleted?
                # no [vm|network]-pool pool <name> assigned <IDs>
                fref.goto_proto_name(pb_msg,"assigned")
                if fref.is_field_deleted():
                    ids = msg.get_assigned()
                    for assign_id in ids:
                        assign_id_pb = assign_id.to_pbcm()
                        fref.goto_proto_name(assign_id_pb,"id")
                        if fref.is_field_present():
                            self._pool_mgr.remove_id_from_pool(msg.get_name(), assign_id.get_id())

            else:
                self._log.error("Action (%s) NOT SUPPORTED", action)
                xact_info.respond_xpath(rwdts.XactRspCode.NACK)

            xact_info.respond_xpath(rwdts.XactRspCode.ACK)

        self._log.debug("Registering for Resource Pool config using xpath: %s",
                        self._xpath,
                        )

        acg_handler = rift.tasklets.AppConfGroup.Handler(
                        on_apply=apply_config,
                        )
        with self._dts.appconf_group_create(handler=acg_handler) as acg:
            self._pool_reg = acg.register(
                    xpath="C," + self._xpath,
                    flags=rwdts.Flag.SUBSCRIBER | rwdts.Flag.DELTA_READY,
                    on_prepare=on_prepare
                    )


class PoolDtsOperdataHandler(object):
    def __init__(self, dts, log, pool_mgr):
        self._dts = dts
        self._log = log
        self._pool_mgr = pool_mgr

    @property
    def pool_gi_cls(self):
        raise NotImplementedError()

    @property
    def id_field(self):
        raise NotImplementedError()

    @property
    def name_field(self):
        raise NotImplementedError()

    def get_show_pool_xpath(self, pool_name=None):
        raise NotImplementedError()

    @asyncio.coroutine
    def register(self):
        @asyncio.coroutine
        def on_prepare_pool(xact_info, action, ks_path, msg):
            path_entry = self.pool_gi_cls.schema().keyspec_to_entry(ks_path)
            pool_name = path_entry.key00.name
            self._log.debug("Got show %s request: %s",
                            str(self.pool_gi_cls), ks_path.create_string())

            if not pool_name:
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            try:
                pool = self._pool_mgr.get_pool(pool_name)
                self._log.debug("Showing pool: %s", pool)
            except Exception as e:
                self._log.warning("Could not get pool: %s", e)
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            cloud_resources = self._pool_mgr.list_cloud_resources(pool.cloud_account.name)
            available_resources = self._pool_mgr.list_available_resources(pool_name, cloud_resources)
            unreserved_pool_resources = pool.unallocated_ids

            def find_cloud_resource(resource_id):
                for resource in cloud_resources:
                    if getattr(resource, self.id_field) == resource_id:
                        return resource

                raise ResourceNotFoundError(
                        "Could not find resource id %s in pool %s cloud account %s" %
                        (resource_id, pool, pool.cloud_account)
                        )

            msg = self.pool_gi_cls(name=pool_name)
            if pool.mgmt_domain is not None:
                msg.mgmt_domain = pool.mgmt_domain.name

            for avail in available_resources:
                new_avail = msg.available.add()
                new_avail.id = getattr(avail, self.id_field)
                new_avail.name = getattr(avail, self.name_field)

            for assigned_id in pool.resource_ids:
                cloud_resource = find_cloud_resource(assigned_id)
                self._log.debug("Found cloud resource: %s", cloud_resource)
                assigned = msg.assigned_detail.add()
                assigned.id = assigned_id
                assigned.is_reserved = assigned_id not in unreserved_pool_resources
                assigned.resource_info.from_dict(cloud_resource.as_dict())

            msg.dynamic_scaling = pool.is_dynamic_scaling

            self._log.debug("Responding to show pool: %s", msg)

            xact_info.respond_xpath(
                    rwdts.XactRspCode.ACK,
                    xpath=self.get_show_pool_xpath(pool_name),
                    msg=msg,
                    )

        yield from self._dts.register(
                xpath=self.get_show_pool_xpath(),
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare_pool),
                flags=rwdts.Flag.PUBLISHER,
                )



class NetworkPoolDtsOperdataHandler(PoolDtsOperdataHandler):
    def __init__(self, dts, log, network_pool_mgr):
        super().__init__(dts, log, network_pool_mgr)

    @property
    def pool_gi_cls(self):
        return RwMcYang.NetworkPool

    @property
    def id_field(self):
        return "network_id"

    @property
    def name_field(self):
        return "network_name"

    def get_show_pool_xpath(self, pool_name=None):
        path = "D,/rw-mc:network-pool/pool{}".format(
                "[rw-mc:name='%s']" % pool_name if pool_name is not None else ""
                )

        return path


class VMPoolDtsOperdataHandler(PoolDtsOperdataHandler):
    def __init__(self, dts, log, vm_pool_mgr):
        super().__init__(dts, log, vm_pool_mgr)

    @property
    def pool_gi_cls(self):
        return RwMcYang.VmPool

    @property
    def id_field(self):
        return "vm_id"

    @property
    def name_field(self):
        return "vm_name"

    def get_show_pool_xpath(self, pool_name=None):
        path = "D,/rw-mc:vm-pool/pool{}".format(
                "[rw-mc:name='%s']" % pool_name if pool_name is not None else ""
                )

        return path


class CloudAccountDtsConfigHandler(object):
    XPATH = "/rw-mc:cloud-account/account"

    def __init__(self, dts, loop, log, cloud_accounts):
        self._dts = dts
        self._loop = loop
        self._log = log
        self._cloud_accounts = cloud_accounts

        self._cloud_reg = None

    def _add_cloud(self, cfg):
        self._log.info("Adding cloud account: %s", cfg)
        # Check if cloud account already exists, if it does, its really
        # an update for the cloud account, and rest of the details are
        # handled in _update_cloud
        if cfg.name in self._cloud_accounts:
            self._log.warning("Cloud account name %s already exists!", cfg.name)
            if cfg.has_field('account_type'):
                raise CloudAccountError("Cannot change cloud's account-type")

            return False

        # If this is a new cloud account, then account-type field is mandatory
        # NOTE: Right now, account-type is not mandatory in yang due to a bug,
        # so we need to check for it and artifically enforce it to be mandatory
        if cfg.has_field('account_type'):
            cls = get_cloud_account_cls_from_type(cfg.account_type)
        else:
            raise CloudAccountError("Missing mandatory 'cloud-account' field")

        account = cls.from_cfg(self._log, cfg)

        self._cloud_accounts[account.name] = account
        return True

    def _delete_cloud(self, name):
        self._log.info("Deleting cloud account: %s", name)

        if name not in self._cloud_accounts:
            self._log.warning("Cloud name doesn't exist!")
            return

        del self._cloud_accounts[name]

    def _update_cloud(self, cfg):
        self._log.info("Updating cloud account: %s", cfg)

        if cfg.name not in self._cloud_accounts:
            self._log.warning("Cloud name doesn't exist!")
            return

        account = self._cloud_accounts[cfg.name]
        account.update_from_cfg(cfg)
        self._log.debug("After update, new account details: %s", account.account )

    def register(self):
        """ Register for Cloud Account create/update/delete/read requests from dts """

        def apply_config(dts, acg, xact, action, _):
            """Apply the pending cloud account configuration"""

            self._log.debug("Got cloud account apply config (xact: %s) (action: %s)",
                            xact, action)

            if xact.xact is None:
                # When RIFT first comes up, an INSTALL is called with the current config
                # Since confd doesn't actally persist data this never has any data so
                # skip this for now.
                self._log.debug("No xact handle.  Skipping apply config")
                return

            #return RwTypes.RwStatus.SUCCESS

        @asyncio.coroutine
        def on_prepare(dts, acg, xact, xact_info, ks_path, msg, scratch):
            """ Prepare callback from DTS for Cloud Account """

            action = xact_info.handle.get_query_action()
            self._log.debug("Got cloud account prepare config (msg %s) (action %s)",
                           msg, action)

            @asyncio.coroutine
            def start_cloud_account_validation(cloud_name):
                account = self._cloud_accounts[cloud_name]
                yield from account.validate_cloud_account_credentials(self._loop)

            fref = ProtobufC.FieldReference.alloc()
            pb_msg = msg.to_pbcm()
            fref.goto_whole_message(pb_msg)
            is_new_account = True

            if action == rwdts.QueryAction.UPDATE:
                # We get an UPDATE if either a new cloud-account is created or one
                # of its fields is updated.
                # Separating the creation of cloud-account from updating its fields
                fref.goto_proto_name(pb_msg,"name")
                if fref.is_field_present():
                    is_new_account = self._add_cloud(msg)

                    if not is_new_account:
                        # This was an Update of the fields of the cloud account
                        # Need to check which account-type's fields were updated
                        self._update_cloud(msg)

                    # Asynchronously check the cloud accounts credentials as soon as a
                    # new cloud account is created or an existing account is updated
                    self._loop.create_task(start_cloud_account_validation(msg.name))

            elif action == rwdts.QueryAction.DELETE:
                # Got DELETE action in prepare callback
                # We only allow the deletion of cloud account itself, not its fields.

                fref.goto_whole_message(pb_msg)
                if fref.is_field_deleted():
                    # Cloud account was deleted
                    self._delete_cloud(msg.name);

            else:
                self._log.error("Action (%s) NOT SUPPORTED", action)
                xact_info.respond_xpath(rwdts.XactRspCode.NACK)

            xact_info.respond_xpath(rwdts.XactRspCode.ACK)

        self._log.debug("Registering for Cloud Account config using xpath: %s",
                        CloudAccountDtsConfigHandler.XPATH)

        acg_handler = rift.tasklets.AppConfGroup.Handler(
                        on_apply=apply_config,
                        )
        with self._dts.appconf_group_create(handler=acg_handler) as acg:
            self._cloud_reg = acg.register(
                    xpath="C," + CloudAccountDtsConfigHandler.XPATH,
                    flags=rwdts.Flag.SUBSCRIBER | rwdts.Flag.DELTA_READY,
                    on_prepare=on_prepare
                    )


class CloudAccountDtsOperdataHandler(object):
    def __init__(self, dts, loop, log, cloud_accounts,
                 vm_pool_mgr, network_pool_mgr):
        self._dts = dts
        self._loop = loop
        self._log = log
        self._cloud_accounts = cloud_accounts
        self._vm_pool_mgr = vm_pool_mgr
        self._network_pool_mgr = network_pool_mgr

    def _register_show_pools(self):
        def get_xpath(cloud_name=None):
            return "D,/rw-mc:cloud-account/account{}/pools".format(
                    "[name='%s']" % cloud_name if cloud_name is not None else ''
                    )

        @asyncio.coroutine
        def on_prepare(xact_info, action, ks_path, msg):
            path_entry = RwMcYang.CloudAccount.schema().keyspec_to_entry(ks_path)
            cloud_account_name = path_entry.key00.name
            self._log.debug("Got show cloud pools request: %s", ks_path.create_string())

            if not cloud_account_name:
                self._log.warning("Cloud account name %s not found", cloud_account_name)
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            account = self._cloud_accounts[cloud_account_name]
            if not account:
                self._log.warning("Cloud account %s does not exist", cloud_account_name)
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            # If cloud account's credentials are not even valid, don't even try to fetch data using CAL APIs
            # as they will throw an exception & tracebacks.
            if account.credential_status != "Validated":
                self._log.warning("Cloud Account Credentials are not valid: %s", account.credential_status_details)
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            try:
                cloud_vm_pools = self._vm_pool_mgr.list_cloud_pools(cloud_account_name)
                cloud_network_pools = self._network_pool_mgr.list_cloud_pools(cloud_account_name)
            except Exception as e:
                self._log.warning("Could not get cloud pools: %s", e)
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            cloud_pools = RwMcYang.CloudPools()

            for vm in cloud_vm_pools:
                cloud_pools.vm.add().name = vm.name

            for network in cloud_network_pools:
                cloud_pools.network.add().name = network.name

            self._log.debug("Responding to cloud pools request: %s", cloud_pools)
            xact_info.respond_xpath(
                    rwdts.XactRspCode.MORE,
                    xpath=get_xpath(cloud_account_name),
                    msg=cloud_pools,
                    )

            xact_info.respond_xpath(rwdts.XactRspCode.ACK)

        yield from self._dts.register(
                xpath=get_xpath(),
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare),
                flags=rwdts.Flag.PUBLISHER,
                )

    def _register_show_resources(self):
        def get_xpath(cloud_name=None):
            return "D,/rw-mc:cloud-account/account{}/resources".format(
                    "[name='%s']" % cloud_name if cloud_name is not None else ''
                    )

        @asyncio.coroutine
        def on_prepare(xact_info, action, ks_path, msg):
            path_entry = RwMcYang.CloudAccount.schema().keyspec_to_entry(ks_path)
            cloud_account_name = path_entry.key00.name
            xpath = ks_path.create_string()
            self._log.debug("Got show cloud resources request: %s", xpath)

            if not cloud_account_name:
                self._log.warning("Cloud account name %s not found", cloud_account_name)
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            account = self._cloud_accounts[cloud_account_name]
            if not account:
                self._log.warning("Cloud account %s does not exist", cloud_account_name)
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            # If cloud account's credentials are not even valid, don't even try to fetch data using CAL APIs
            # as they will throw an exception & tracebacks.
            if account.credential_status != "Validated":
                self._log.warning("Cloud Account Credentials are not valid: %s", account.credential_status_details)
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            respond_types = ["vm", "network"]
            if "vm" in xpath:
                respond_types = ["vm"]

            if "network" in xpath:
                respond_types = ["network"]

            try:
                if "vm" in respond_types:
                    vms = self._vm_pool_mgr.list_cloud_resources(cloud_account_name)
                    avail_vms = self._vm_pool_mgr.list_available_cloud_resources(cloud_account_name, vms)
                    avail_vm_ids = [v.vm_id for v in avail_vms]

                if "network" in respond_types:
                    networks = self._network_pool_mgr.list_cloud_resources(cloud_account_name)
                    avail_networks = self._network_pool_mgr.list_available_cloud_resources(cloud_account_name, networks)
                    avail_network_ids = [n.network_id for n in avail_networks]

            except Exception as e:
                self._log.error("Could not get cloud resources: %s", e, exc_info=True)
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            avail = RwMcYang.CloudResources()

            if "vm" in respond_types:
                for vm in vms:
                    add_vm = avail.vm.add()
                    add_vm.id = vm.vm_id
                    add_vm.name = vm.vm_name
                    add_vm.available = add_vm.id in avail_vm_ids

            if "network" in respond_types:
                for network in networks:
                    add_network = avail.network.add()
                    add_network.id = network.network_id
                    add_network.name = network.network_name
                    add_network.available = add_network.id in avail_network_ids

            self._log.debug("Responding to cloud resources request: %s", avail)
            xact_info.respond_xpath(
                    rwdts.XactRspCode.MORE,
                    xpath=get_xpath(cloud_account_name),
                    msg=avail,
                    )

            xact_info.respond_xpath(rwdts.XactRspCode.ACK)

        yield from self._dts.register(
                xpath=get_xpath(),
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare),
                flags=rwdts.Flag.PUBLISHER,
                )

    def _register_show_status(self):
        def get_xpath(cloud_name=None):
            return "D,/rw-mc:cloud-account/account{}/connection".format(
                    "[name='%s']" % cloud_name if cloud_name is not None else ''
                    )

        @asyncio.coroutine
        def on_prepare(xact_info, action, ks_path, msg):
            path_entry = RwMcYang.CloudAccount.schema().keyspec_to_entry(ks_path)
            cloud_account_name = path_entry.key00.name
            self._log.debug("Got show cloud connection status request: %s", ks_path.create_string())

            if not cloud_account_name:
                self._log.warning("Cloud account name %s not found", cloud_account_name)
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            account = self._cloud_accounts[cloud_account_name]
            if not account:
                self._log.warning("Cloud account %s does not exist", cloud_account_name)
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            try:
                cred_status = account.credential_status
                cred_details = account.credential_status_details
            except Exception as e:
                self._log.error("Could not get cloud status: %s", e)
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            cloud_creds = RwMcYang.CloudStatus()
            if cred_status is not None:
                cloud_creds.status = cred_status
                cloud_creds.details = cred_details
            else:
                cloud_creds.status = "Validating..."
                cloud_creds.details = "Connection status is being validated, please wait..."

            self._log.debug("Responding to cloud connection status request: %s", cloud_creds)
            xact_info.respond_xpath(
                    rwdts.XactRspCode.MORE,
                    xpath=get_xpath(cloud_account_name),
                    msg=cloud_creds,
                    )

            xact_info.respond_xpath(rwdts.XactRspCode.ACK)

        yield from self._dts.register(
                xpath=get_xpath(),
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare),
                flags=rwdts.Flag.PUBLISHER,
                )

    @asyncio.coroutine
    def register(self):
        yield from self._register_show_pools()
        yield from self._register_show_resources()
        yield from self._register_show_status()

class SDNAccountDtsConfigHandler(object):
    XPATH="/rw-mc:sdn/account"

    def __init__(self, dts, log, sdn_accounts):
        self._dts = dts
        self._log = log
        self._sdn_accounts = sdn_accounts

        self._sdn_reg = None

    def _add_sdn(self, cfg):
        self._log.info("Adding sdn account: %s", cfg)
        if cfg.name in self._sdn_accounts:
            self._log.warning("SDN name already exists!")
            return

        # Right now we only have one SDN Account of type ODL;
        # when we support more SDN account types, we should
        # create a similar funtion to get sdn account class from type,
        # like 'get_cloud_account_cls_from_type'
        cls = OdlSDNAccount
        account = cls.from_cfg(self._log, cfg)
        self._sdn_accounts[account.name] = account

    def _delete_sdn(self, name):
        self._log.info("Deleting sdn account: %s", name)

        if name not in self._sdn_accounts:
            self._log.warning("SDN name doesn't exist!")
            return

        del self._sdn_accounts[name]

    def _update_sdn(self, cfg):
        self._log.info("Updating sdn account: %s", cfg)

        if cfg.name not in self._sdn_accounts:
            self._log.warning("SDN name doesn't exist!")
            return

        account = self._sdn_accounts[cfg.name]
        account.update_from_cfg(cfg)

    def register(self):
        def apply_config(dts, acg, xact, action, _):
            """Apply the pending sdn account configuration"""

            self._log.debug("Got sdn account apply config (xact: %s) (action: %s)",
                            xact, action)

            try:
                if xact.xact is None:
                    # When RIFT first comes up, an INSTALL is called with the current config
                    # Since confd doesn't actally persist data this never has any data so
                    # skip this for now.
                    self._log.debug("No xact handle.  Skipping apply config")
                    return

                sdn_add_cfgs, sdn_delete_cfgs, sdn_update_cfgs = get_add_delete_update_cfgs(
                        dts_member_reg=self._sdn_reg,
                        xact=xact,
                        key_name="name",
                        )

                # Handle Deletes
                for cfg in sdn_delete_cfgs:
                    self._delete_sdn(cfg.name)

                # Handle Adds
                for cfg in sdn_add_cfgs:
                    self._add_sdn(cfg)

                # Handle Updates
                for cfg in sdn_update_cfgs:
                    self._update_sdn(cfg)

            except Exception as e:
                self._log.warning("Could not apply config for SDN account: %s", e)


        self._log.debug("Registering for SDN Account config using xpath: %s",
                        SDNAccountDtsConfigHandler.XPATH)

        acg_handler = rift.tasklets.AppConfGroup.Handler(
                        on_apply=apply_config,
                        )
        with self._dts.appconf_group_create(acg_handler) as acg:
            self._sdn_reg = acg.register(
                    xpath="C," + SDNAccountDtsConfigHandler.XPATH,
                    flags=rwdts.Flag.SUBSCRIBER,
                    )

class MgmtDomainDtsConfigHandler(object):
    XPATH = "C,/rw-mc:mgmt-domain/rw-mc:domain"

    def __init__(self, dts, loop, log, mgmt_domains,
                 vm_pool_mgr, network_pool_mgr, lp_minions):
        self._dts = dts
        self._loop = loop
        self._log = log
        self._mgmt_domains = mgmt_domains
        self._vm_pool_mgr = vm_pool_mgr
        self._network_pool_mgr = network_pool_mgr
        self._lp_minions = lp_minions

        self._fed_reg = None
        self._vm_pool_configured = False
        self._net_pool_configured = False

    def _delete_mgmt_domain_vm_pool(self, mgmt_domain_name, vm_pool_name):
        self._log.debug("Deleting vm pool %s from mgmt_domain %s", vm_pool_name, mgmt_domain_name)
        self._vm_pool_mgr.delete_mgmt_domain_pool(mgmt_domain_name, vm_pool_name)
        self._vm_pool_configured = False

    def _delete_mgmt_domain_net_pool(self, mgmt_domain_name, net_pool_name):
        self._log.debug("Deleting network pool %s from mgmt_domain %s", net_pool_name, mgmt_domain_name)
        self._network_pool_mgr.delete_mgmt_domain_pool(mgmt_domain_name, net_pool_name)
        self._net_pool_configured = False

    def _delete_mgmt_domain(self, fed_cfg):
        self._log.debug("Deleting mgmt_domain: %s", fed_cfg.name)

        if self._mgmt_domains[fed_cfg.name].launchpad_state is "started":
            # Launchpad is running, can not delete Mgmt-domin. Abort
            raise DeleteMgmtDomainError("Cannot delete Mgmt-domain - Laucnhpad is still running!")

        for vm_pool in self._vm_pool_mgr.list_mgmt_domain_pools(fed_cfg.name):
            self._delete_mgmt_domain_vm_pool(fed_cfg.name, vm_pool.name)

        for net_pool in self._network_pool_mgr.list_mgmt_domain_pools(fed_cfg.name):
            self._delete_mgmt_domain_net_pool(fed_cfg.name, net_pool.name)

        # We need to free up LP resources when a MD is deleted
        mgmt_domain = self._mgmt_domains[fed_cfg.name]
        if self._mgmt_domains[fed_cfg.name].launchpad_state in ["pending", "configuring"]:
            # Mgmt-domain was deleted while launchpad was in pending/configuring state
            mgmt_domain.stop_launchpad()

        mgmt_domain.release_launchpad()

        del self._mgmt_domains[fed_cfg.name]

    def _update_mgmt_domain_pools(self, name, fed_cfg):
        self._log.debug("Updating mgmt_domain pools %s", name)

        for vm_pool in fed_cfg.pools.vm:
            self._vm_pool_mgr.update_mgmt_domain_pools(fed_cfg.name, vm_pool.name)
            self._vm_pool_configured = True

        for network_pool in fed_cfg.pools.network:
            self._network_pool_mgr.update_mgmt_domain_pools(fed_cfg.name, network_pool.name)
            self._net_pool_configured = True

    def _add_mgmt_domain(self, fed_cfg):
        self._log.debug("Creating new mgmt_domain: %s", fed_cfg.name)
        if fed_cfg.name in self._mgmt_domains:
            self._log.warning("Mgmt Domain name %s already exists!", fed_cfg.name)
            return

        mgmt_domain = MgmtDomain(
                self._loop,
                self._log,
                fed_cfg.name,
                self._vm_pool_mgr,
                self._network_pool_mgr,
                self._lp_minions,
                )

        self._mgmt_domains[fed_cfg.name] = mgmt_domain

    def _update_mgmt_domain(self, fed_cfg):
        self._log.debug("Updating mgmt_domain: %s", fed_cfg)

        self._update_mgmt_domain_pools(fed_cfg.name, fed_cfg)

        # Start launchpad ONLY IF both VM & NET pool have been configured
        if self._vm_pool_configured and self._net_pool_configured:
            mgmt_domain = self._mgmt_domains[fed_cfg.name]
            mgmt_domain.allocate_start_configure_launchpad_task()


    @asyncio.coroutine
    def register(self):
        """ Register for Mgmt Domain create/update/delete/read requests from dts """

        def apply_config(dts, acg, xact, action, _):
            """Apply the pending mgmt_domain configuration"""

            self._log.debug("Got mgmt_domain apply config (xact: %s) (action: %s)",
                            xact, action)

            if xact.xact is None:
                # When RIFT first comes up, an INSTALL is called with the current config
                # Since confd doesn't actally persist data this never has any data so
                # skip this for now.
                self._log.debug("No xact handle.  Skipping apply config")
                return

        @asyncio.coroutine
        def on_prepare(dts, acg, xact, xact_info, ks_path, msg, scratch):
            """ prepare callback from dts for mgmt domain """

            action = xact_info.handle.get_query_action()

            self._log.debug("Got mgmt domain prepare config (msg %s) (action %s)",
                           msg, action)

            fref = ProtobufC.FieldReference.alloc()
            pb_msg = msg.to_pbcm()
            fref.goto_whole_message(pb_msg)

            if action == rwdts.QueryAction.UPDATE:
                # We get an UPDATE if either a new mgmt-domain is created or a pool is added/updated.
                # Separating the creation of mgmt-domain from adding its pools
                fref.goto_proto_name(pb_msg,"name")
                if fref.is_field_present():
                    self._add_mgmt_domain(msg)

                fref.goto_proto_name(pb_msg,"pools")
                if fref.is_field_present():
                    self._update_mgmt_domain(msg)

            elif action == rwdts.QueryAction.DELETE:
                # Got DELETE action in prepare callback
                # Check what got deleted - it could be either
                # the mgmt_domain itself, or its network pool or its vm pool

                # Did the entire mgmt_domain get deleted?
                # no mgmt-domain domain <name>
                fref.goto_whole_message(pb_msg)
                if fref.is_field_deleted():
                    self._delete_mgmt_domain(msg)

                # Did the assigned pools get deleted?
                # no mgm-domain domain <name> pools
                # or
                # Did a specific pool get deleted?
                # no mgmt-domain domain <name> pools [vm|network] <name>
                # in either case, we get a DELETE call for each pool separately
                fref.goto_proto_name(pb_msg,"pools")
                if fref.is_field_deleted():
                    self._log.info("Removing pool: %s from mgmt-domain: %s", msg.pools, msg.get_name())

                pools = msg.get_pools()

                pools_pb = pools.to_pbcm()
                fref.goto_proto_name(pools_pb, "vm")
                vmpool = pools.vm
                if fref.is_field_deleted():
                    self._delete_mgmt_domain_vm_pool(msg.get_name(), vmpool[0].name)

                fref.goto_proto_name(pools_pb, "network")
                netpool = pools.network
                if fref.is_field_deleted():
                    self._delete_mgmt_domain_net_pool(msg.get_name(), netpool[0].name)

            else:
                self._log.error("Action (%s) NOT SUPPORTED", action)
                xact_info.respond_xpath(rwdts.XactRspCode.NACK)

            xact_info.respond_xpath(rwdts.XactRspCode.ACK)

        self._log.debug("Registering for mgmt_domain config using xpath: %s",
                        MgmtDomainDtsConfigHandler.XPATH)

        acg_handler = rift.tasklets.AppConfGroup.Handler(on_apply=apply_config)
        with self._dts.appconf_group_create(handler=acg_handler) as acg:
            self._fed_reg = acg.register(
                    xpath=MgmtDomainDtsConfigHandler.XPATH,
                    flags=rwdts.Flag.SUBSCRIBER | rwdts.Flag.DELTA_READY,
                    on_prepare=on_prepare
                    )


class MgmtDomainDtsRpcHandler(object):
    START_LAUNCHPAD_XPATH= "/rw-mc:start-launchpad"
    STOP_LAUNCHPAD_XPATH= "/rw-mc:stop-launchpad"

    def __init__(self, dts, log, mgmt_domains):
        self._dts = dts
        self._log = log
        self._mgmt_domains = mgmt_domains

        self.pending_msgs = []

    @asyncio.coroutine
    def register(self):
        @asyncio.coroutine
        def on_prepare_start(xact_info, action, ks_path, msg):
            self._log.debug("Got launchpad start request: %s", msg)

            name = msg.mgmt_domain
            if name not in self._mgmt_domains:
                msg = "Launchpad name %s not found" % name
                self._log.error(msg)
                xact_info.respond_xpath(rwdts.XactRspCode.NACK)
                return

            mgmt_domain = self._mgmt_domains[name]

            try:
                mgmt_domain.allocate_start_configure_launchpad_task()
            except Exception as e:
                self._log.error("Failed to start launchpad: %s", str(e))
                xact_info.respond_xpath(rwdts.XactRspCode.NACK)
                return

            xact_info.respond_xpath(
                    rwdts.XactRspCode.ACK,
                    xpath="O," + MgmtDomainDtsRpcHandler.START_LAUNCHPAD_XPATH,
                    )

        @asyncio.coroutine
        def on_prepare_stop(xact_info, action, ks_path, msg):
            self._log.debug("Got launchpad stop request: %s", msg)

            name = msg.mgmt_domain
            if name not in self._mgmt_domains:
                msg = "Launchpad name %s not found", name
                self._log.error(msg)
                xact_info.respond_xpath(rwdts.XactRspCode.NACK)
                return

            mgmt_domain = self._mgmt_domains[name]
            try:
                yield from mgmt_domain.stop_launchpad()
            except Exception as e:
                self._log.exception("Failed to stop launchpad: %s", str(e))
                xact_info.respond_xpath(rwdts.XactRspCode.NACK)
                return

            xact_info.respond_xpath(
                    rwdts.XactRspCode.ACK,
                    xpath="O," + MgmtDomainDtsRpcHandler.STOP_LAUNCHPAD_XPATH
                    )

        yield from self._dts.register(
                xpath="I," + MgmtDomainDtsRpcHandler.START_LAUNCHPAD_XPATH,
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare_start),
                flags=rwdts.Flag.PUBLISHER
                )

        yield from self._dts.register(
                xpath="I," + MgmtDomainDtsRpcHandler.STOP_LAUNCHPAD_XPATH,
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare_stop),
                flags=rwdts.Flag.PUBLISHER
                )


class MgmtDomainDtsOperdataHandler(object):
    def __init__(self, dts, log, mgmt_domains):
        self._dts = dts
        self._log = log
        self._mgmt_domains = mgmt_domains

    def _get_respond_xpath(self, mgmt_domain_name=None):
        return "D,/rw-mc:mgmt-domain/domain{}/launchpad".format(
                "[name='%s']" % mgmt_domain_name if mgmt_domain_name is not None else ""
                )

    @asyncio.coroutine
    def register(self):
        @asyncio.coroutine
        def on_prepare(xact_info, action, ks_path, msg):
            path_entry = RwMcYang.MgmtDomain.schema().keyspec_to_entry(ks_path)
            mgmt_domain_name = path_entry.key00.name
            self._log.debug("Got show mgmt_domain launchpad request: %s", ks_path.create_string())

            if not mgmt_domain_name:
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            mgmt_domain = self._mgmt_domains.get(mgmt_domain_name, None)
            if mgmt_domain is None:
                self._log.warning("Could not find management domain: %s", mgmt_domain)
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            try:
                lp_state = mgmt_domain.launchpad_state
                lp_state_details= mgmt_domain.launchpad_state_details
                lp_uptime = mgmt_domain.launchpad_uptime
                lp_create_time = mgmt_domain.launchpad_create_time

                lp_ip = None
                if mgmt_domain.launchpad_vm_info is not None:
                    if mgmt_domain.launchpad_vm_info.public_ip:
                        lp_ip = mgmt_domain.launchpad_vm_info.public_ip
                    else:
                        lp_ip = mgmt_domain.launchpad_vm_info.management_ip

            except Exception as e:
                self._log.warning("Could not get mgmt-domain launchpad info: %s", e)
                xact_info.respond_xpath(rwdts.XactRspCode.NA)
                return

            msg = RwMcYang.MgmtDomainLaunchpad()
            msg.state = lp_state
            msg.state_details = lp_state_details
            msg.uptime = lp_uptime
            if lp_create_time is not None:
                msg.create_time = lp_create_time
            if lp_ip is not None:
                msg.ip_address = lp_ip

            self._log.debug("Responding to mgmt_domain pools request: %s", msg)
            xact_info.respond_xpath(
                    rwdts.XactRspCode.ACK,
                    xpath=self._get_respond_xpath(mgmt_domain_name),
                    msg=msg,
                    )

        yield from self._dts.register(
                xpath=self._get_respond_xpath(),
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare),
                flags=rwdts.Flag.PUBLISHER,
                )


class MCUptimeDtsOperdataHandler(object):
    def __init__(self, dts, log, start_time):
        self._dts = dts
        self._log = log
        self._mc_start_time = start_time


    def get_start_time(self):
        return self._mc_start_time

    def _get_uptime_xpath(self):
        return "D,/rw-mc:mission-control"

    @asyncio.coroutine
    def register(self):
        @asyncio.coroutine
        def on_prepare(xact_info, action, ks_path, msg):
            self._log.debug("Got show MC uptime request: %s", ks_path.create_string())

            msg = RwMcYang.Uptime()
            uptime_secs = float(time.time() - self.get_start_time())
            uptime_str = str(timedelta(seconds = uptime_secs))
            msg.uptime = uptime_str
            msg.create_time = self.get_start_time()

            self._log.debug("Responding to MC Uptime request: %s", msg)
            xact_info.respond_xpath(
                    rwdts.XactRspCode.ACK,
                    xpath=self._get_uptime_xpath(),
                    msg=msg,
                    )

        yield from self._dts.register(
                xpath=self._get_uptime_xpath(),
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=on_prepare),
                flags=rwdts.Flag.PUBLISHER,
                )


fallback_launchpad_resources = None
fallback_launchpad_public_ip = None

def construct_fallback_launchpad_vm_pool():
    global fallback_launchpad_resources
    global fallback_launchpad_public_ip

    if "RIFT_LP_NODES" not in os.environ:
        return

    fallback_launchpad_resources = []
    for node in os.environ["RIFT_LP_NODES"].split(":"):
        node_ip_id = node.split("|")
        assert len(node_ip_id) == 2
        fallback_launchpad_resources.append(node_ip_id)

    if "RIFT_LP_PUBLIC_IP" not in os.environ:
        fallback_launchpad_public_ip = None
        return

    fallback_launchpad_public_ip = os.environ["RIFT_LP_PUBLIC_IP"]



class MissionControlTasklet(rift.tasklets.Tasklet):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.rwlog.set_category('rw-mc-log')

        self._dts = None
        self._mgmt_domains = {}
        self._domain_config_hdl = None
        self._domain_rpc_hdl = None
        self._pool_config_hdl = None
        self._cloud_account_config_hdl = None
        self._sdn_account_config_hdl = None
        self._error_test_rpc_hdl = None
        self._start_time = time.time()

        self._cloud_accounts = {}
        self._sdn_accounts = {}

        self._lp_minions = {}

        self._vm_pool_mgr = VMPoolManager(
                self.log,
                self._mgmt_domains,
                self._cloud_accounts,
                )
        self._network_pool_mgr = NetworkPoolManager(
                self.log,
                self._mgmt_domains,
                self._cloud_accounts,
                )

    def initialize_lxc(self):
        self.log.info("Enabling Container Cal Debug Logging")
        SimCloudAccount.enable_debug_logging(self.log.handlers)

    def start(self):
        super().start()
        self.log.info("Starting Mission Control Tasklet")

        CloudAccount.log_hdl = self.log_hdl
        SDNAccount.log_hdl = self.log_hdl

        # Initialize LXC to the extent possible until RIFT-8483, RIFT-8485 are completed
        self.initialize_lxc()

        # Use a fallback set of launchpad VM's when provided and no static
        # resources are selected
        construct_fallback_launchpad_vm_pool()

        self.log.debug("Registering with dts")
        self._dts = rift.tasklets.DTS(
                self.tasklet_info,
                RwMcYang.get_schema(),
                self.loop,
                self.on_dts_state_change
                )

        self.log.debug("Created DTS Api GI Object: %s", self._dts)

    @asyncio.coroutine
    def init(self):
        """Initialize application. During this state transition all DTS
        registrations and subscriptions required by application should be started
        """

        self._lp_minions = yield from launchpad.get_previous_lp(
                self.log, self.loop)

        self._uptime_operdata_hdl = MCUptimeDtsOperdataHandler(
                self._dts,
                self._log,
                self._start_time,
                )
        yield from self._uptime_operdata_hdl.register()

        self._domain_config_hdl = MgmtDomainDtsConfigHandler(
                self._dts,
                self.loop,
                self.log,
                self._mgmt_domains,
                self._vm_pool_mgr,
                self._network_pool_mgr,
                self._lp_minions,
                )
        yield from self._domain_config_hdl.register()

        self._domain_rpc_hdl = MgmtDomainDtsRpcHandler(
                self._dts,
                self.log,
                self._mgmt_domains,
                )
        yield from self._domain_rpc_hdl.register()

        self._domain_operdata_hdl = MgmtDomainDtsOperdataHandler(
                self._dts,
                self.log,
                self._mgmt_domains,
                )
        yield from self._domain_operdata_hdl.register()

        self._vm_pool_config_hdl = ResourcePoolDtsConfigHandler(
                self._dts,
                self.log,
                self._vm_pool_mgr,
                "/vm-pool/pool",
                )
        self._vm_pool_config_hdl.register()

        self._network_pool_config_hdl = ResourcePoolDtsConfigHandler(
                self._dts,
                self.log,
                self._network_pool_mgr,
                "/network-pool/pool",
                )
        self._network_pool_config_hdl.register()

        self._vm_pool_operdata_hdl = VMPoolDtsOperdataHandler(
                self._dts,
                self.log,
                self._vm_pool_mgr,
                )
        yield from self._vm_pool_operdata_hdl.register()

        self._network_pool_operdata_hdl = NetworkPoolDtsOperdataHandler(
                self._dts,
                self.log,
                self._network_pool_mgr,
                )
        yield from self._network_pool_operdata_hdl.register()

        self._cloud_account_config_hdl = CloudAccountDtsConfigHandler(
                self._dts,
                self.loop,
                self.log,
                self._cloud_accounts,
                )
        self._cloud_account_config_hdl.register()

        self._cloud_account_operdata_hdl = CloudAccountDtsOperdataHandler(
                self._dts,
                self.loop,
                self.log,
                self._cloud_accounts,
                self._vm_pool_mgr,
                self._network_pool_mgr,
                )
        yield from self._cloud_account_operdata_hdl.register()

        self._sdn_account_config_hdl = SDNAccountDtsConfigHandler(
                self._dts,
                self.log,
                self._sdn_accounts,
                )
        self._sdn_account_config_hdl.register()

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
