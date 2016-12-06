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

@file rwdtsmockservertasklet.py
@author Austin Cormier(austin.cormier@riftio.com)
@date 2015-09-08
"""

import asyncio
import functools
import importlib
import logging
import os
import sys

import gi
gi.require_version('RwDts', '1.0')

from gi.repository import (
    RwDts as rwdts,
)

import rift.tasklets

import txaio
txaio.use_asyncio()

from autobahn.wamp.types import ComponentConfig, PublishOptions
from autobahn.websocket.protocol import parseWsUrl
from autobahn.asyncio.websocket import WampWebSocketClientFactory
from autobahn.asyncio.wamp import ApplicationSession

if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async


class UnknownWampUriError(Exception):
    pass


class UnknownXpathError(Exception):
    pass


def patch_txaio(loop):
    """ Patch instances in txaio which loop config parameter isn't being used """
    def inject_loop_kwarg_wrapper(f):
        @functools.wraps(f)
        def wrapper(*args, **kwargs):
            if "loop" not in kwargs:
                kwargs["loop"] = loop
            return f(*args, **kwargs)

        return wrapper

    future_init = asyncio.Future.__init__
    new_future_init = inject_loop_kwarg_wrapper(future_init)
    setattr(asyncio.Future, "__init__", new_future_init)


def wamp_uri_to_xpath(wamp_path):
    if not wamp_path.startswith("dts."):
        raise UnknownWampUriError(wamp_path)

    wamp_path = wamp_path.replace("dts.", "", 1)

    if wamp_path.startswith("config."):
        xpath="C,/"
        wamp_path = wamp_path.replace("config.", "", 1)
    elif wamp_path.startswith("rpc."):
        xpath="I,/"
        wamp_path = wamp_path.replace("rpc.", "", 1)
    elif wamp_path.startswith("data."):
        xpath="D,/"
        wamp_path = wamp_path.replace("data.", "", 1)
    else:
        raise UnknownWampUriError(wamp_path)

    return xpath + wamp_path


def xpath_to_wamp_uri(xpath):
    if xpath.startswith("C,/"):
        wamp_path = "dts.config."
        xpath = xpath.replace("C,/", "")
    elif xpath.startswith("I,/"):
        wamp_path = "dts.rpc."
        xpath = xpath.replace("I,/", "")
    elif xpath.startswith("D,/"):
        wamp_path = "dts.data."
        xpath = xpath.replace("D,/", "")
    else:
        raise UnknownXpathError(xpath)

    return wamp_path + xpath


class WampDtsMockApplication(ApplicationSession):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._dts = self.config.extra["dts"]
        self._loop = self.config.extra["loop"]
        self._log = self.config.extra["log"]

    @asyncio.coroutine
    def _subscribe_xpath(self, xpath):
        @asyncio.coroutine
        def prepare(xact_info, action, ks_path, msg):
            try:
                prepare_xpath = ks_path.create_string()
                self._log.debug("Got prepare for xpath: (xpath: %s)(msg: %s)",
                                prepare_xpath, msg)

                wamp_uri = xpath_to_wamp_uri(xpath)

                publication = yield from self.publish(
                        wamp_uri,
                        prepare_xpath,
                        msg.as_dict(),
                        options=PublishOptions(acknowledge=True, disclose_me=True),
                        )
                self._log.debug("Published: %s", publication)

                xact_info.respond_xpath(rwdts.XactRspCode.ACK)

            except Exception as e:
                self._log.exception("Failed to publish config: %s %s", e, e.args)
                xact_info.respond_xpath(rwdts.XactRspCode.NACK)


        yield from self._dts.register(
                xpath=xpath,
                handler=rift.tasklets.DTS.RegistrationHandler(
                        on_prepare=prepare,
                        ),
                flags=rwdts.Flag.SUBSCRIBER,
                )

    @asyncio.coroutine
    def _register_xpath(self, xpath):
        @asyncio.coroutine
        def prepare(xact_info, action, ks_path, msg):
            self._log.debug("Got prepare for xpath: (xpath: %s)(msg: %s)", xpath, msg)
            try:
                wamp_uri = xpath_to_wamp_uri(xpath)
                call_result = yield from self.call(
                        wamp_uri,
                        ks_path.create_string(),
                        msg.as_dict() if msg is not None else None,
                        )

                gi_type, response_msg = call_result.results
                respond_xpath = ks_path.create_string()
                if "xpath" in call_result.kwresults:
                    respond_xpath = call_result.kwresults["xpath"]

                self._log.debug("Got reponse gi_type (%s) and msg (%s)", gi_type, msg)

                module_name, type_name = gi_type.split(".")
                mod = importlib.import_module('gi.repository.%s' % module_name)
                gi_type = getattr(mod, type_name)

                self._log.debug("Attempting to deserialize %s into type %s",
                                response_msg, gi_type)
                try:
                    msg = gi_type.from_dict(response_msg)
                except Exception as e:
                    self._log.exception("Deserialization failed")
                    xact_info.respond_xpath(rwdts.XactRspCode.NACK)

                self._log.debug("Responding with ACK to xpath: %s",
                                ks_path.create_string(),
                                )
                xact_info.respond_xpath(
                        rwdts.XactRspCode.ACK,
                        xpath=respond_xpath,
                        msg=gi_type.from_dict(response_msg),
                        )

            except Exception as e:
                self._log.exception("Failed to publish config: %s", e)
                xact_info.respond_xpath(rwdts.XactRspCode.NACK)
                return


        yield from self._dts.register(
                xpath=xpath,
                handler=rift.tasklets.DTS.RegistrationHandler(
                        on_prepare=prepare,
                        ),
                flags=rwdts.Flag.PUBLISHER,
                )

    @asyncio.coroutine
    def onJoin(self, details):
        self._log.debug("%s.on_connect() called: (details: %s)",
                        self.__class__.__name__, details)

        def on_create_subscribe(id, event):
            self._log.debug("Got sub_on_create() (id: %s, event: %s)", id, event)
            xpath = wamp_uri_to_xpath(event["uri"])
            asyncio.ensure_future(
                    self._subscribe_xpath(xpath),
                    loop=self._loop,
                    )

        def on_create_register(id, event):
            self._log.debug("Got reg_on_create() (id: %s, event: %s)", id, event)
            xpath = wamp_uri_to_xpath(event["uri"])
            asyncio.ensure_future(
                    self._register_xpath(xpath),
                    loop=self._loop,
                    )

        yield from self.subscribe(on_create_subscribe, u'wamp.subscription.on_create')
        yield from self.subscribe(on_create_register, u'wamp.registration.on_create')


class WampTaskletRunner(object):
    def __init__(self, dts, loop, log):
        self._dts = dts
        self._loop = loop
        self._log = log
        self._url = "ws://127.0.0.1:8090/ws"
        self._realm = "dts_mock"

        self._app = None

    @asyncio.coroutine
    def register(self):
        def create():
            cfg = ComponentConfig(
                    self._realm,
                    extra={"dts": self._dts, "loop": self._loop, "log": self._log}
                    )

            # Set the asyncio loop for txaio
            txaio.config.loop = self._loop
            self._app = WampDtsMockApplication(cfg)
            self._app.debug_app = True

            return self._app

        isSecure, host, port, resource, path, params = parseWsUrl(self._url)
        transport_factory = WampWebSocketClientFactory(
                create, url=self._url,
                serializers=None, debug=True, debug_wamp=True,
                )

        while True:
            try:
                create_conn = self._loop.create_connection(transport_factory, host, port)
                (transport, protocol) = yield from create_conn
                self._log.debug("Connected to crossbar.io")
                break
            except ConnectionRefusedError:
                self._log.error("ConnectionRefusedError: sleeping for a second and retrying")
                yield from asyncio.sleep(1, self._loop)


class RwDtsMockServerTasklet(rift.tasklets.Tasklet):
    """RW DTS jock Server Tasklet"""
    def start(self):
        """Tasklet entry point"""
        self.log.setLevel(logging.DEBUG)
        self.loop.set_debug(True)
        os.environ["PYTHONASYNCIODEBUG"] = "1"
        asyncio_logger = logging.getLogger("asyncio")
        asyncio_logger.setLevel(logging.DEBUG)

        # txaio has instances where they are using asyncio classes without
        # passing in loop kwarg
        patch_txaio(self.loop)

        super().start()

        self._dts = rift.tasklets.DTS(
                self.tasklet_info,
                RwDtsYang.get_schema(),
                self._loop,
                self.on_dts_state_change,
                )

        self._wamp_runner = WampTaskletRunner(self._dts, self._loop, self._log)

    @asyncio.coroutine
    def on_dts_state_change(self, state):
        """Take action according to current dts state to transition
        application into the corresponding application state

        Arguments
            state - current dts state
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
        if next_state is not None:
            self._dts.handle.set_state(next_state)

    @asyncio.coroutine
    def init(self):
        """Initialize application. During this state transition all DTS
        registrations and subscriptions required by application should be started
        """

        yield from self._wamp_runner.register()

    @asyncio.coroutine
    def run(self):
        """Run application. Wait for dts to be ready before continuing"""
        yield from self._dts.ready.wait()
