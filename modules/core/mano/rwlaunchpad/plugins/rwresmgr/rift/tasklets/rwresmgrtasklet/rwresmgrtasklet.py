
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import logging
import sys

import gi
gi.require_version('RwDts', '1.0')
gi.require_version('RwYang', '1.0')
gi.require_version('RwResourceMgrYang', '1.0')
gi.require_version('RwLaunchpadYang', '1.0')
gi.require_version('RwcalYang', '1.0')
from gi.repository import (
    RwDts as rwdts,
    RwYang,
    RwResourceMgrYang,
    RwLaunchpadYang,
    RwcalYang,
)

import rift.tasklets

from . import rwresmgr_core as Core
from . import rwresmgr_config as Config
from . import rwresmgr_events as Event


class ResourceManager(object):
    def __init__(self, log, log_hdl, loop, dts):
        self._log            = log
        self._log_hdl        = log_hdl
        self._loop           = loop
        self._dts            = dts
        self.config_handler  = Config.ResourceMgrConfig(self._dts, self._log, self._log_hdl, self._loop, self)
        self.event_handler   = Event.ResourceMgrEvent(self._dts, self._log, self._loop, self)
        self.core            = Core.ResourceMgrCore(self._dts, self._log, self._log_hdl, self._loop, self)

    @asyncio.coroutine
    def register(self):
        yield from self.config_handler.register()
        yield from self.event_handler.register()

    def add_cloud_account_config(self, account):
        self._log.debug("Received Cloud-Account add config event for account: %s", account.name)
        self.core.add_cloud_account(account)

    def update_cloud_account_config(self, account):
        self._log.debug("Received Cloud-Account update config event for account: %s", account.name)
        self.core.update_cloud_account(account)

    def delete_cloud_account_config(self, account_name, dry_run=False):
        self._log.debug("Received Cloud-Account delete event for account (dry_run: %s): %s",
                        dry_run, account_name)
        self.core.delete_cloud_account(account_name, dry_run)

    def get_cloud_account_names(self):
        cloud_account_names = self.core.get_cloud_account_names()
        return cloud_account_names

    def pool_add(self, cloud_account_name, pool):
        self._log.debug("Received Pool add event for cloud account %s pool: %s",
                        cloud_account_name, pool.name)
        self.core.add_resource_pool(cloud_account_name, pool)

    def pool_modify(self, cloud_account_name, pool):
        self._log.debug("Received Pool modify event for cloud account %s pool: %s",
                        cloud_account_name, pool.name)
        self.core.modify_resource_pool(cloud_account_name, pool)

    def pool_delete(self, cloud_account_name, pool_name):
        self._log.debug("Received Pool delete event for cloud account %s pool: %s",
                        cloud_account_name, pool_name)
        self.core.delete_resource_pool(cloud_account_name, pool_name)

    def get_pool_list(self, cloud_account_name):
        return self.core.get_resource_pool_list(cloud_account_name)

    def get_pool_info(self, cloud_account_name, pool_name):
        self._log.debug("Received get-pool-info event for cloud account %s pool: %s",
                        cloud_account_name, pool_name)
        return self.core.get_resource_pool_info(cloud_account_name, pool_name)

    def lock_pool(self, cloud_account_name, pool_name):
        self._log.debug("Received pool unlock event for pool: %s",
                        cloud_account_name, pool_name)
        self.core.lock_resource_pool(cloud_account_name, pool_name)

    def unlock_pool(self, cloud_account_name, pool_name):
        self._log.debug("Received pool unlock event for pool: %s",
                        cloud_account_name, pool_name)
        self.core.unlock_resource_pool(cloud_account_name, pool_name)

    @asyncio.coroutine
    def allocate_virtual_network(self, event_id, cloud_account_name, request):
        self._log.info("Received network resource allocation request with event-id: %s", event_id)
        resource = yield from self.core.allocate_virtual_resource(event_id, cloud_account_name, request, 'network')
        return resource

    @asyncio.coroutine
    def reallocate_virtual_network(self, event_id, cloud_account_name, request, resource):
        self._log.info("Received network resource allocation request with event-id: %s", event_id)
        resource = yield from self.core.reallocate_virtual_resource(event_id, cloud_account_name, request, 'network', resource)
        return resource

    @asyncio.coroutine
    def release_virtual_network(self, event_id):
        self._log.info("Received network resource release request with event-id: %s", event_id)
        yield from self.core.release_virtual_resource(event_id, 'network')

    @asyncio.coroutine
    def read_virtual_network_info(self, event_id):
        self._log.info("Received network resource read request with event-id: %s", event_id)
        info = yield from self.core.read_virtual_resource(event_id, 'network')
        return info

    @asyncio.coroutine
    def allocate_virtual_compute(self, event_id, cloud_account_name, request):
        self._log.info("Received compute resource allocation request "
                       "(cloud account: %s) with event-id: %s",
                       cloud_account_name, event_id)
        resource = yield from self.core.allocate_virtual_resource(
                event_id, cloud_account_name, request, 'compute',
                )
        return resource

    @asyncio.coroutine
    def reallocate_virtual_compute(self, event_id, cloud_account_name, request, resource):
        self._log.info("Received compute resource allocation request "
                       "(cloud account: %s) with event-id: %s",
                       cloud_account_name, event_id)
        resource = yield from self.core.reallocate_virtual_resource(
                event_id, cloud_account_name, request, 'compute', resource, 
                )
        return resource

    @asyncio.coroutine
    def release_virtual_compute(self, event_id):
        self._log.info("Received compute resource release request with event-id: %s", event_id)
        yield from self.core.release_virtual_resource(event_id, 'compute')

    @asyncio.coroutine
    def read_virtual_compute_info(self, event_id):
        self._log.info("Received compute resource read request with event-id: %s", event_id)
        info = yield from self.core.read_virtual_resource(event_id, 'compute')
        return info


class ResMgrTasklet(rift.tasklets.Tasklet):
    def __init__(self, *args, **kwargs):
        super(ResMgrTasklet, self).__init__(*args, **kwargs)
        self.rwlog.set_category("rw-resource-mgr-log")
        self._dts = None
        self._resource_manager = None

    def start(self):
        super(ResMgrTasklet, self).start()
        self.log.info("Starting ResMgrTasklet")

        self.log.debug("Registering with dts")

        self._dts = rift.tasklets.DTS(self.tasklet_info,
                                      RwResourceMgrYang.get_schema(),
                                      self.loop,
                                      self.on_dts_state_change)

        self.log.debug("Created DTS Api GI Object: %s", self._dts)

    def stop(self):
      try:
         self._dts.deinit()
      except Exception:
         print("Caught Exception in RESMGR stop:", sys.exc_info()[0])
         raise

    def on_instance_started(self):
        self.log.debug("Got instance started callback")

    @asyncio.coroutine
    def init(self):
        self._log.info("Initializing the Resource Manager tasklet")
        self._resource_manager = ResourceManager(self.log,
                                                 self.log_hdl,
                                                 self.loop,
                                                 self._dts)
        yield from self._resource_manager.register()

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
