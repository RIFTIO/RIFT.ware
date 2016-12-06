#!/usr/bin/env python3

import asyncio
import gi
import logging

gi.require_version("RwDts", "1.0")
gi.require_version("RwDtsWrapperTestYang", "1.0")
from gi.repository import (
    RwDts,
    RwDtsWrapperTestYang as testing_yang,
    RwDtsYang,
)
import rift.tasklets

from rift.rwpb import (
    KeyCategory,
    Schema,
)

from rift.tasklets import (
    Registry,
    subscribe,
    publish,
    pre_commit,
    abort,
    commit,
    cached_member_data,
)

schema = Schema(["rw-dts-wrapper-test"])

import shared

class Sender(Registry):
    def __init__(self, *args, **kwargs):
        kwargs["schema"] = schema
        super(Sender, self).__init__(*args, **kwargs)

        self._dts = None
        self.rwlog.set_category("rw-dts-wrapper-test-log")

        shared.sender = self

        self.results = dict()

    def start(self):
        """ The task start callback """
        super(Sender, self).start()

        self._log = logging.getLogger("sender")
        self._dts = rift.tasklets.DTS(self.tasklet_info,
                                      testing_yang.get_schema(),
                                      self.loop,
                                      self.on_dts_state_change)

    def mark_visited(self, function):
        if function._path not in self.results:
            self.results[function._path] = 1
        else :
            self.results[function._path] += 1

    @staticmethod
    @cached_member_data
    def cached_member_data():
        return [
            schema.key(KeyCategory.Data).rw_dts_wrapper_test.simple_op_member_data,
            schema.key(KeyCategory.Data).rw_dts_wrapper_test.complex_op_data.mirror
        ]

    @pre_commit
    def handler_pre_commit(self, *args):
        return RwDts.MemberRspCode.ACTION_OK

    @abort
    def handler_abort(self, *args):
        return RwDts.MemberRspCode.ACTION_OK

    @commit
    def handler_commit(self, *args):
        return RwDts.MemberRspCode.ACTION_OK

    @asyncio.coroutine
    @subscribe(schema.key(KeyCategory.Config).rw_dts_wrapper_test.simple_config_data)
    def simple_config_data_handler(self, xact_info, action, keyspec, msg):
        self.mark_visited(self.simple_config_data_handler)

        xact_info.respond_xpath(RwDts.XactRspCode.ACK)

    @asyncio.coroutine
    @subscribe(schema.key(KeyCategory.Config).rw_dts_wrapper_test.complex_config_data.actual)
    def complex_config_data_handler(self, xact_info, action, keyspec, msg):
        self.mark_visited(self.complex_config_data_handler)

        xact_info.respond_xpath(RwDts.XactRspCode.ACK)

    @asyncio.coroutine
    @publish(schema.key(KeyCategory.Data).rw_dts_wrapper_test.simple_op_data)
    def simple_op_data_handler(self, xact_info, action, keyspec, msg):
        self.mark_visited(self.simple_op_data_handler)

        xact_info.respond_xpath(RwDts.XactRspCode.ACK)

    @asyncio.coroutine
    def init(self):
        self.register()

    @asyncio.coroutine
    def run(self):
        while True:
            yield from asyncio.sleep(1, loop=self.loop)

    @asyncio.coroutine
    def on_dts_state_change(self, state):
        switch = {
            RwDts.State.INIT: RwDts.State.REGN_COMPLETE,
            RwDts.State.CONFIG: RwDts.State.RUN,
        }

        handlers = {
            RwDts.State.INIT: self.init,
            RwDts.State.RUN: self.run,
        }

        # Transition application to next state
        handler = handlers.get(state, None)
        if handler is not None:
            yield from handler()

        # Transition dts to next state
        next_state = switch.get(state, None)
        if next_state is not None:
            self.log.debug("Sender changing state to %s", next_state)
            self._dts.handle.set_state(next_state)
