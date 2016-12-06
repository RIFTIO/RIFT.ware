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
# Author(s): Max Beckett
# Creation Date: 8/29/2015
# 

import asyncio
import logging
import sys

import tornado

from gi.repository import (
    RwDts,
    RwReststreamYang,
    RwYang,
)

from rift.restconf import (
    WebSocketHandler,
    ClientRegistry,
    SubscriptionParser,
)

import rift.tasklets


if sys.version_info < (3,4,4):
    asyncio.ensure_future = asyncio.async

class RestStreamTasklet(rift.tasklets.Tasklet):

    def start(self):
        """Tasklet entry point"""
        super(RestStreamTasklet, self).start()

        manifest = self.tasklet_info.get_pb_manifest()
        yang_model = RwYang.Model.create_libncx()

        self._schema_handle = RwYang.Model.load_and_get_schema("rw-reststream")
        self._schema = yang_model.get_root_node()

        self._dts = rift.tasklets.DTS(
            self.tasklet_info,
            self._schema_handle,
            self.loop,
            self.on_dts_state_change)

    def stop(self):
      try:
         self._dts.deinit()
      except Exception:
         print("Caught Exception in LP stop:", sys.exc_info()[0])
         raise

    @asyncio.coroutine
    def on_dts_state_change(self, state):
        """Take action according to current dts state to transition
        application in to the corresponding application state
        """
        switch = {
            RwDts.State.INIT: RwDts.State.RUN,
        }

        handlers = {
            RwDts.State.INIT: self.init,
            RwDts.State.RUN: self.run,
        }

        # Transition application to next state
        handler = handlers[state]
        yield from handler()

        try:
            # Transition dts to next state
            next_state = switch[state]
        except KeyError:
            # we don't handle all state changes
            return

        self._dts.handle.set_state(next_state)

    @asyncio.coroutine
    def init(self):
        self._mq = asyncio.Queue()
        self._cr = ClientRegistry(self.log, self._mq)
        self._mp = SubscriptionParser(self.log, self._schema)

        handler_arguments = {
            "asyncio_loop" : self.loop,
            "logger" : self.log,
            "client_registry" : self._cr,
            "message_parser" : self._mp,
            "dts" : self._dts
        }
        
        try:
            tornado_loop = tornado.platform.asyncio.BaseAsyncIOLoop(asyncio_loop=self.loop)
            tornado_loop.install()
        except:
            pass
        
        self._application = tornado.web.Application([
            (r"/ws", WebSocketHandler, handler_arguments),
        ], compress_response=True)
    
    @asyncio.coroutine
    def run(self):
        self._application.listen(8889)



