"""
# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

@file testtasklet.py
@author Varun Prasad
@date 2015-09-28
"""

import abc
import asyncio
import bisect
import collections
import functools
import functools
import itertools
import logging
import math
import sys
import time

import gi
import gi.repository.RwTypes as rwtypes
import rift.tasklets
import rift.tasklets.dts

gi.require_version('InterfacesYang', '1.0')
gi.require_version('DnsYang', '1.0')
gi.require_version('RoutesYang', '1.0')
gi.require_version('NotifYang', '1.0')
gi.require_version('RwMgmtagtYang', '1.0')
gi.require_version('RwVcsYang', '1.0')
gi.require_version('UtCompositeYang', '1.0')

from gi.repository import (
    RwDts as rwdts,
    InterfacesYang as interfaces,
    DnsYang as dns,
    RoutesYang as routes,
    NotifYang,
    UtCompositeYang as composite,
    RwVcsYang as rwvcs
)

from gi.repository import RwMgmtagtYang

if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async


class TaskletState(object):
    """
    Different states of the tasklet.
    """
    STARTING, RUNNING, FINISHED = ("starting", "running", "finished")


class TestTasklet(rift.tasklets.Tasklet):
    def start(self):
        """Entry point for tasklet
        """
        self.log.setLevel(logging.INFO)
        super().start()

        self._dts = rift.tasklets.DTS(
                self.tasklet_info,
                composite.get_schema(),
                self._loop,
                self.on_dts_state_change)

        # Set the instance id
        self.instance_name = self.tasklet_info.instance_name
        self.instance_id = int(self.instance_name.rsplit('-', 1)[1])
        self.interfaces = {}
        self.log.debug("Starting TestTasklet Name: {}, Id: {}".format(
                self.instance_name,
                self.instance_id))

        self.state = TaskletState.STARTING

    def stop(self):
        """Exit point for the tasklet
        """
        # All done!
        super().stop()

    @asyncio.coroutine
    def on_dts_state_change(self, state):
        """Take action according to current dts state to transition
        application into the corresponding application state

        Args:
            state (rwdts.State): Description
        """

        switch = {
            rwdts.State.CONFIG: rwdts.State.INIT,
            rwdts.State.INIT: rwdts.State.REGN_COMPLETE,
            rwdts.State.REGN_COMPLETE: rwdts.State.RUN,
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
        self.log.info("DTS transition from {} -> {}".format(state, next_state))

        if next_state is not None:
            self._dts.handle.set_state(next_state)

    # Callbacks
    @asyncio.coroutine
    def init(self):
        """Initialize application. During this state transition all DTS
        registrations and subscriptions required by application
        should be started.
        """

        @asyncio.coroutine
        def interface_prepare_config(dts, acg, xact, xact_info, ksp, msg, scratch):
            """Prepare for application configuration.
            """
            self.log.debug("Prepare Callback")
            # Store the interfaces
            self.interfaces[msg.name] = msg
            acg.handle.prepare_complete_ok(xact_info.handle)

        @asyncio.coroutine
        def routes_prepare_config(dts, acg, xact, xact_info, ksp, msg, scratch):
            """Prepare for application configuration.
            """
            self.log.debug("Prepare Callback")
            acg.handle.prepare_complete_ok(xact_info.handle)

        @asyncio.coroutine
        def dns_prepare_config(dts, acg, xact, xact_info, ksp, msg, scratch):
            """Prepare for application configuration.
            """
            self.log.debug("Prepare Callback")
            acg.handle.prepare_complete_ok(xact_info.handle)

        def apply_config(dts, acg, xact, action, scratch):
            """On apply callback for AppConf registration"""
            self.log.debug("Apply Config")
            return rwtypes.RwStatus.SUCCESS

        @asyncio.coroutine
        def interface_status(xact_info, action, ks_path, msg):
            xpath = "D,/interfaces:interfaces/interfaces:interface[interfaces:name='eth0']"
            interf = interfaces.Interface()
            interf.name = "eth0"
            interf.status.link = "up"
            interf.status.speed = "hundred"
            interf.status.duplex = "full"
            interf.status.mtu = 1500
            interf.status.receive.bytes = 1234567
            interf.status.receive.packets = 1234
            interf.status.receive.errors = 0
            interf.status.receive.dropped = 100
            xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, interf)

        @asyncio.coroutine
        def clear_interface(xact_info, action, ks_path, msg):
            xpath = "O,/interfaces:clear-interface"
            op=interfaces.ClearInterfaceOp()
            op.status="Success"
            xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, op)

        #Operational data
        yield from self._dts.register(
              flags=rwdts.Flag.PUBLISHER,
              xpath="D,/interfaces:interfaces/interfaces:interface",
              handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=interface_status))

        #RPC
        yield from self._dts.register(
              xpath="I,/interfaces:clear-interface",
              flags=rwdts.Flag.PUBLISHER,
              handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=clear_interface))

        with self._dts.appconf_group_create(
                handler=rift.tasklets.AppConfGroup.Handler(
                        on_apply=apply_config)) as acg:
            acg.register(
                    xpath="C,/interfaces:interfaces/interfaces:interface",
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE|0,
                    on_prepare=interface_prepare_config)

            acg.register(
                    xpath="C,/routes:routes",
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE|0,
                    on_prepare=routes_prepare_config)

            acg.register(
                    xpath="C,/dns:dns",
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE|0,
                    on_prepare=dns_prepare_config)

    @asyncio.coroutine
    def run(self):
        """Initialize application. During this state transition all DTS
        registrations and subscriptions required by application should be started
        """
        yield from self._dts.ready.wait()
