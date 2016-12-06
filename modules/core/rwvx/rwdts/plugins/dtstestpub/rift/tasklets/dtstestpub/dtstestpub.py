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

@file dtstestpub.py
@author (info@riftio.com)
@date 02/04/2016
"""
import asyncio
import collections
import logging
import sys

import gi
gi.require_version('RwBaseYang', '1.0')
gi.require_version('RwDts', '1.0')
gi.require_version('RwDtsToyTaskletYang', '1.0')

import gi.repository.RwBaseYang as rwbase
import gi.repository.RwDts as rwdts
import gi.repository.RwDtsToyTaskletYang as toyyang

import rift.tasklets


if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async


class DtsPubTestTasklet(rift.tasklets.Tasklet):


    @asyncio.coroutine
    def main(self):
        self.log.debug('tasklet main function running')
        xpath = '/rw-dts-toy-tasklet:a-container'
        ret = toyyang.AContainer()
        ret.a_string = 'pub' 
        events = [asyncio.Event(loop=self.loop) for _ in range(2)]
        @asyncio.coroutine
        def on_ready(*args):
            yield from asyncio.sleep(1, loop=self.loop)
            events[0].set()

        @asyncio.coroutine
        def on_prepare(xact_info, *args):
            xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret)


        handler = rift.tasklets.DTS.RegistrationHandler(
                on_ready=on_ready,
                on_prepare=on_prepare)
        self.reg = yield from self._dts.register(
                    xpath,
                    flags=rwdts.Flag.PUBLISHER,
                    handler=handler)
        yield from events[1].wait()

        # Normally this wouldn't be done, but all the printing of ticks is annoying
        self.log.debug('tasklet main finished')

    @asyncio.coroutine
    def on_dts_state_change(self, state):
        # Handle dts state changes.  This demo doesn't have any initialization
        # or recovery to preform.  If it did, they would be scheduled from here.
        # Once DTS marks this tasklet as ready to run, we add the main() function
        # to the asyncio event loop
        switch = {
            rwdts.State.CONFIG: rwdts.State.INIT,
            rwdts.State.INIT: rwdts.State.REGN_COMPLETE,
            rwdts.State.REGN_COMPLETE: rwdts.State.RUN,
        }

        next_state = switch.get(state, None)
        if next_state is not None:
            self.log.debug('dts state change %s -> %s', state, next_state)
            self._dts.handle.set_state(next_state)

        if state == rwdts.State.RUN:
            # Schedule main() to run within the ioloop
            asyncio.ensure_future(self.main(), loop=self._loop)

    def start(self):
        self.log.setLevel(logging.DEBUG)
        super(DtsPubTestTasklet, self).start()
        self.schema = toyyang.get_schema()
        self._dts = rift.tasklets.DTS(
                self.tasklet_info,
                self.schema,
                self.loop,
                self.on_dts_state_change)

