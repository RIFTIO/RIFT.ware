
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import logging
import time
import uuid
from enum import Enum

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

from gi.repository.RwTypes import RwStatus
import rift.tasklets
import rift.mano.cloud


class ResourceMgrConfig(object):
    XPATH_POOL_OPER_DATA = "D,/rw-resource-mgr:resource-pool-records"
    def __init__(self, dts, log, rwlog_hdl, loop, parent):
        self._dts = dts
        self._log = log
        self._rwlog_hdl = rwlog_hdl
        self._loop = loop
        self._parent = parent

        self._cloud_sub = None

    @asyncio.coroutine
    def register(self):
        yield from self.register_resource_pool_operational_data()
        self.register_cloud_account_config()

    def register_cloud_account_config(self):
        def on_add_cloud_account_apply(account):
            self._log.debug("Received on_add_cloud_account: %s", account)
            self._parent.add_cloud_account_config(account)

        def on_delete_cloud_account_apply(account_name):
            self._log.debug("Received on_delete_cloud_account_apply: %s", account_name)
            self._parent.delete_cloud_account_config(account_name)

        @asyncio.coroutine
        def on_delete_cloud_account_prepare(account_name):
            self._log.debug("Received on_delete_cloud_account_prepare: %s", account_name)
            self._parent.delete_cloud_account_config(account_name, dry_run=True)

        cloud_callbacks = rift.mano.cloud.CloudAccountConfigCallbacks(
                on_add_apply=on_add_cloud_account_apply,
                on_delete_apply=on_delete_cloud_account_apply,
                on_delete_prepare=on_delete_cloud_account_prepare,
                )

        self._cloud_sub = rift.mano.cloud.CloudAccountConfigSubscriber(
                self._dts, self._log, self._rwlog_hdl, cloud_callbacks
                )
        self._cloud_sub.register()

    @asyncio.coroutine
    def register_resource_pool_operational_data(self):
        @asyncio.coroutine
        def on_prepare(xact_info, action, ks_path, msg):
            self._log.debug("ResourceMgr providing resource-pool information")
            msg = RwResourceMgrYang.ResourcePoolRecords()

            cloud_accounts = self._parent.get_cloud_account_names()
            for cloud_account_name in cloud_accounts:
                pools = self._parent.get_pool_list(cloud_account_name)
                self._log.debug("Publishing information about cloud account %s %d resource pools",
                                cloud_account_name, len(pools))

                cloud_account_msg = msg.cloud_account.add()
                cloud_account_msg.name = cloud_account_name
                for pool in pools:
                    pool_info = self._parent.get_pool_info(cloud_account_name, pool.name)
                    cloud_account_msg.records.append(pool_info)

            xact_info.respond_xpath(rwdts.XactRspCode.ACK,
                                    ResourceMgrConfig.XPATH_POOL_OPER_DATA,
                                    msg=msg,)

        self._log.debug("Registering for Resource Mgr resource-pool-record using xpath: %s",
                        ResourceMgrConfig.XPATH_POOL_OPER_DATA)

        handler=rift.tasklets.DTS.RegistrationHandler(on_prepare=on_prepare)
        response = yield from self._dts.register(xpath=ResourceMgrConfig.XPATH_POOL_OPER_DATA,
                                                 handler=handler,
                                                 flags=rwdts.Flag.PUBLISHER)

