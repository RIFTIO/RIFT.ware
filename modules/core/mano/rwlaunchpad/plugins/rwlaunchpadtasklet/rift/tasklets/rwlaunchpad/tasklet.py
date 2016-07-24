
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import logging
import math
import mmap
import os
import re
import tarfile
import tempfile
import sys

import tornado
import tornado.httputil
import tornado.httpserver
import tornado.platform.asyncio

import gi
gi.require_version('RwDts', '1.0')
gi.require_version('RwcalYang', '1.0')
gi.require_version('RwTypes', '1.0')
gi.require_version('RwLaunchpadYang', '1.0')

from gi.repository import (
    RwDts as rwdts,
    RwLaunchpadYang as rwlaunchpad,
    RwcalYang as rwcal,
    RwTypes,
)

import rift.tasklets
import rift.mano.cloud
import rift.mano.config_agent

from . import uploader
from . import datacenters


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


class CatalogDtsHandler(object):
    def __init__(self, tasklet, app):
        self.app = app
        self.reg = None
        self.tasklet = tasklet

    @property
    def log(self):
        return self.tasklet.log

    @property
    def dts(self):
        return self.tasklet.dts


class VldCatalogDtsHandler(CatalogDtsHandler):
    XPATH = "C,/vld:vld-catalog/vld:vld"

    def add_vld(self, vld):
        self.log.debug('vld-catalog-handler:add:{}'.format(vld.id))
        if vld.id not in self.tasklet.vld_catalog:
            self.tasklet.vld_catalog[vld.id] = vld
        else:
            self.log.error("vld already in catalog: {}".format(vld.id))

    def update_vld(self, vld):
        self.log.debug('vld-catalog-handler:update:{}'.format(vld.id))
        if vld.id in self.tasklet.vld_catalog:
            self.tasklet.vld_catalog[vld.id] = vld
        else:
            self.log.error("unrecognized VLD: {}".format(vld.id))

    def delete_vld(self, vld_id):
        self.log.debug('vld-catalog-handler:delete:{}'.format(vld_id))
        if vld_id in self.tasklet.vld_catalog:
            del self.tasklet.vld_catalog[vld_id]
        else:
            self.log.error("unrecognized VLD: {}".format(vld_id))

    @asyncio.coroutine
    def register(self):
        def apply_config(dts, acg, xact, action, _):
            if xact.xact is None:
                # When RIFT first comes up, an INSTALL is called with the current config
                # Since confd doesn't actally persist data this never has any data so
                # skip this for now.
                self.log.debug("No xact handle.  Skipping apply config")
                return

            add_cfgs, delete_cfgs, update_cfgs = get_add_delete_update_cfgs(
                    dts_member_reg=self.reg,
                    xact=xact,
                    key_name="id",
                    )

            # Handle Deletes
            for cfg in delete_cfgs:
                self.delete_vld(cfg.id)

            # Handle Adds
            for cfg in add_cfgs:
                self.add_vld(cfg)

            # Handle Updates
            for cfg in update_cfgs:
                self.update_vld(cfg)

        self.log.debug("Registering for VLD catalog")

        acg_handler = rift.tasklets.AppConfGroup.Handler(
                        on_apply=apply_config,
                        )

        with self.dts.appconf_group_create(acg_handler) as acg:
            self.reg = acg.register(
                    xpath=VldCatalogDtsHandler.XPATH,
                    flags=rwdts.Flag.SUBSCRIBER,
                    )


class NsdCatalogDtsHandler(CatalogDtsHandler):
    XPATH = "C,/nsd:nsd-catalog/nsd:nsd"

    def add_nsd(self, nsd):
        self.log.debug('nsd-catalog-handler:add:{}'.format(nsd.id))
        if nsd.id not in self.tasklet.nsd_catalog:
            self.tasklet.nsd_catalog[nsd.id] = nsd
        else:
            self.log.error("nsd already in catalog: {}".format(nsd.id))

    def update_nsd(self, nsd):
        self.log.debug('nsd-catalog-handler:update:{}'.format(nsd.id))
        if nsd.id in self.tasklet.nsd_catalog:
            self.tasklet.nsd_catalog[nsd.id] = nsd
        else:
            self.log.error("unrecognized NSD: {}".format(nsd.id))

    def delete_nsd(self, nsd_id):
        self.log.debug('nsd-catalog-handler:delete:{}'.format(nsd_id))
        if nsd_id in self.tasklet.nsd_catalog:
            del self.tasklet.nsd_catalog[nsd_id]
        else:
            self.log.error("unrecognized NSD: {}".format(nsd_id))

    @asyncio.coroutine
    def register(self):
        def apply_config(dts, acg, xact, action, _):
            if xact.xact is None:
                # When RIFT first comes up, an INSTALL is called with the current config
                # Since confd doesn't actally persist data this never has any data so
                # skip this for now.
                self.log.debug("No xact handle.  Skipping apply config")
                return

            add_cfgs, delete_cfgs, update_cfgs = get_add_delete_update_cfgs(
                    dts_member_reg=self.reg,
                    xact=xact,
                    key_name="id",
                    )

            # Handle Deletes
            for cfg in delete_cfgs:
                self.delete_nsd(cfg.id)

            # Handle Adds
            for cfg in add_cfgs:
                self.add_nsd(cfg)

            # Handle Updates
            for cfg in update_cfgs:
                self.update_nsd(cfg)

        self.log.debug("Registering for NSD catalog")

        acg_handler = rift.tasklets.AppConfGroup.Handler(
                        on_apply=apply_config,
                        )

        with self.dts.appconf_group_create(acg_handler) as acg:
            self.reg = acg.register(
                    xpath=NsdCatalogDtsHandler.XPATH,
                    flags=rwdts.Flag.SUBSCRIBER,
                    )


class VnfdCatalogDtsHandler(CatalogDtsHandler):
    XPATH = "C,/vnfd:vnfd-catalog/vnfd:vnfd"

    def add_vnfd(self, vnfd):
        self.log.debug('vnfd-catalog-handler:add:{}'.format(vnfd.id))
        if vnfd.id not in self.tasklet.vnfd_catalog:
            self.tasklet.vnfd_catalog[vnfd.id] = vnfd

        else:
            self.log.error("VNFD already in catalog: {}".format(vnfd.id))

    def update_vnfd(self, vnfd):
        self.log.debug('vnfd-catalog-handler:update:{}'.format(vnfd.id))
        if vnfd.id in self.tasklet.vnfd_catalog:
            self.tasklet.vnfd_catalog[vnfd.id] = vnfd

        else:
            self.log.error("unrecognized VNFD: {}".format(vnfd.id))

    def delete_vnfd(self, vnfd_id):
        self.log.debug('vnfd-catalog-handler:delete:{}'.format(vnfd_id))
        if vnfd_id in self.tasklet.vnfd_catalog:
            del self.tasklet.vnfd_catalog[vnfd_id]

        else:
            self.log.error("unrecognized VNFD: {}".format(vnfd_id))

    @asyncio.coroutine
    def register(self):
        def apply_config(dts, acg, xact, action, _):
            if xact.xact is None:
                # When RIFT first comes up, an INSTALL is called with the current config
                # Since confd doesn't actally persist data this never has any data so
                # skip this for now.
                self.log.debug("No xact handle.  Skipping apply config")
                return

            add_cfgs, delete_cfgs, update_cfgs = get_add_delete_update_cfgs(
                    dts_member_reg=self.reg,
                    xact=xact,
                    key_name="id",
                    )

            # Handle Deletes
            for cfg in delete_cfgs:
                self.delete_vnfd(cfg.id)

            # Handle Adds
            for cfg in add_cfgs:
                self.add_vnfd(cfg)

            # Handle Updates
            for cfg in update_cfgs:
                self.update_vnfd(cfg)

        self.log.debug("Registering for VNFD catalog")

        acg_handler = rift.tasklets.AppConfGroup.Handler(
                        on_apply=apply_config,
                        )

        with self.dts.appconf_group_create(acg_handler) as acg:
            self.reg = acg.register(
                    xpath=VnfdCatalogDtsHandler.XPATH,
                    flags=rwdts.Flag.SUBSCRIBER,
                    )


class LaunchpadConfigDtsHandler(object):
    XPATH = "C,/rw-launchpad:launchpad-config"

    def __init__(self, dts, log, launchpad):
        self.dts = dts
        self.log = log
        self.task = launchpad
        self.reg = None

    @asyncio.coroutine
    def register(self):
        def apply_config(dts, acg, xact, action, _):
            if xact.xact is None:
                # When RIFT first comes up, an INSTALL is called with the current config
                # Since confd doesn't actally persist data this never has any data so
                # skip this for now.
                self.log.debug("No xact handle.  Skipping apply config")
                return

            cfg = list(self.reg.get_xact_elements(xact))[0]
            self.task.set_mode(cfg.operational_mode)

        self.log.debug("Registering for Launchpad Config")

        acg_handler = rift.tasklets.AppConfGroup.Handler(
                        on_apply=apply_config,
                        )

        with self.dts.appconf_group_create(acg_handler) as acg:
            self.reg = acg.register(
                    xpath=LaunchpadConfigDtsHandler.XPATH,
                    flags=rwdts.Flag.SUBSCRIBER,
                    )

class CfgAgentAccountHandlers(object):
    def __init__(self, dts, log, log_hdl, loop):
        self._dts = dts
        self._log = log
        self._log_hdl = log_hdl
        self._loop = loop

        self._log.debug("creating config agent account config handler")
        self.cfg_agent_cfg_handler = rift.mano.config_agent.ConfigAgentSubscriber(
            self._dts, self._log,
            rift.mano.config_agent.ConfigAgentCallbacks(
                on_add_apply=self.on_cfg_agent_account_added,
                on_delete_apply=self.on_cfg_agent_account_deleted,
            )
        )

        self._log.debug("creating config agent account opdata handler")
        self.cfg_agent_operdata_handler = rift.mano.config_agent.CfgAgentDtsOperdataHandler(
            self._dts, self._log, self._loop,
        )

    def on_cfg_agent_account_deleted(self, account):
        self._log.debug("config agent account deleted")
        self.cfg_agent_operdata_handler.delete_cfg_agent_account(account.name)

    def on_cfg_agent_account_added(self, account):
        self._log.debug("config agent account added")
        self.cfg_agent_operdata_handler.add_cfg_agent_account(account)

    @asyncio.coroutine
    def register(self):
        self.cfg_agent_cfg_handler.register()
        yield from self.cfg_agent_operdata_handler.register()

class CloudAccountHandlers(object):
    def __init__(self, dts, log, log_hdl, loop, app):
        self._log = log
        self._log_hdl = log_hdl
        self._dts = dts
        self._loop = loop
        self._app = app

        self._log.debug("creating cloud account config handler")
        self.cloud_cfg_handler = rift.mano.cloud.CloudAccountConfigSubscriber(
            self._dts, self._log, self._log_hdl,
            rift.mano.cloud.CloudAccountConfigCallbacks(
                on_add_apply=self.on_cloud_account_added,
                on_delete_apply=self.on_cloud_account_deleted,
            )
        )

        self._log.debug("creating cloud account opdata handler")
        self.cloud_operdata_handler = rift.mano.cloud.CloudAccountDtsOperdataHandler(
            self._dts, self._log, self._loop,
        )

    def on_cloud_account_deleted(self, account_name):
        self._log.debug("cloud account deleted")
        self._app.accounts.clear()
        self._app.accounts.extend(list(self.cloud_cfg_handler.accounts.values()))
        self.cloud_operdata_handler.delete_cloud_account(account_name)

    def on_cloud_account_added(self, account):
        self._log.debug("cloud account added")
        self._app.accounts.clear()
        self._app.accounts.extend(list(self.cloud_cfg_handler.accounts.values()))
        self._log.debug("accounts: %s", self._app.accounts)
        self.cloud_operdata_handler.add_cloud_account(account)

    @asyncio.coroutine
    def register(self):
        self.cloud_cfg_handler.register()
        yield from self.cloud_operdata_handler.register()


class LaunchpadTasklet(rift.tasklets.Tasklet):
    UPLOAD_MAX_BODY_SIZE = 1e10
    UPLOAD_PORT = "4567"

    def __init__(self, *args, **kwargs):
        super(LaunchpadTasklet, self).__init__(*args, **kwargs)
        self.rwlog.set_category("rw-mano-log")
        self.rwlog.set_subcategory("launchpad")

        self.app = None
        self.server = None

        self.account_handler = None
        self.config_handler = None
        self.nsd_catalog_handler = None
        self.vld_catalog_handler = None
        self.vnfd_catalog_handler = None
        self.cloud_handler = None
        self.datacenter_handler = None
        self.lp_config_handler = None

        self.nsd_catalog = dict()
        self.vld_catalog = dict()
        self.vnfd_catalog = dict()

        self.mode = rwlaunchpad.OperationalMode.STANDALONE

    @property
    def cloud_accounts(self):
        if self.cloud_handler is None:
            return list()

        return list(self.cloud_handler.cloud_cfg_handler.accounts.values())

    def start(self):
        super(LaunchpadTasklet, self).start()
        self.log.info("Starting LaunchpadTasklet")

        self.log.debug("Registering with dts")
        self.dts = rift.tasklets.DTS(
                self.tasklet_info,
                rwlaunchpad.get_schema(),
                self.loop,
                self.on_dts_state_change
                )

        self.log.debug("Created DTS Api GI Object: %s", self.dts)

    def stop(self):
        try:
            self.server.stop()
            self.dts.deinit()
        except Exception:
            self.log.exception("Caught Exception in LP stop")
            raise

    def set_mode(self, mode):
        """ Sets the mode of this launchpad"""
        self.mode = mode

    @asyncio.coroutine
    def init(self):
        io_loop = rift.tasklets.tornado.TaskletAsyncIOLoop(asyncio_loop=self.loop)
        self.app = uploader.UploaderApplication(self)

        manifest = self.tasklet_info.get_pb_manifest()
        ssl_cert = manifest.bootstrap_phase.rwsecurity.cert
        ssl_key = manifest.bootstrap_phase.rwsecurity.key
        ssl_options = {
                "certfile": ssl_cert,
                "keyfile": ssl_key,
                }

        if manifest.bootstrap_phase.rwsecurity.use_ssl:
            self.server = tornado.httpserver.HTTPServer(
                self.app,
                max_body_size=LaunchpadTasklet.UPLOAD_MAX_BODY_SIZE,
                io_loop=io_loop,
                ssl_options=ssl_options,
            )

        else:
            self.server = tornado.httpserver.HTTPServer(
                self.app,
                max_body_size=LaunchpadTasklet.UPLOAD_MAX_BODY_SIZE,
                io_loop=io_loop,
            )

        self.log.debug("creating VLD catalog handler")
        self.vld_catalog_handler = VldCatalogDtsHandler(self, self.app)
        yield from self.vld_catalog_handler.register()

        self.log.debug("creating NSD catalog handler")
        self.nsd_catalog_handler = NsdCatalogDtsHandler(self, self.app)
        yield from self.nsd_catalog_handler.register()

        self.log.debug("creating VNFD catalog handler")
        self.vnfd_catalog_handler = VnfdCatalogDtsHandler(self, self.app)
        yield from self.vnfd_catalog_handler.register()

        self.log.debug("creating launchpad config handler")
        self.lp_config_handler = LaunchpadConfigDtsHandler(self.dts, self.log, self)
        yield from self.lp_config_handler.register()

        self.log.debug("creating datacenter handler")
        self.datacenter_handler = datacenters.DataCenterPublisher(self)
        yield from self.datacenter_handler.register()

        self.log.debug("creating cloud account handler")
        self.cloud_handler = CloudAccountHandlers(
                self.dts, self.log, self.log_hdl, self.loop, self.app
                )
        yield from self.cloud_handler.register()

        self.log.debug("creating config agent handler")
        self.config_handler = CfgAgentAccountHandlers(self.dts, self.log, self.log_hdl, self.loop)
        yield from self.config_handler.register()

    @asyncio.coroutine
    def run(self):
        self.server.listen(LaunchpadTasklet.UPLOAD_PORT)

    def on_instance_started(self):
        self.log.debug("Got instance started callback")

    @asyncio.coroutine
    def on_dts_state_change(self, state):
        """Handle DTS state change

        Take action according to current DTS state to transition application
        into the corresponding application state

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
            self.dts.handle.set_state(next_state)
