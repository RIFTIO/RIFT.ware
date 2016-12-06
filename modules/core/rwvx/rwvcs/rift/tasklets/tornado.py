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

@file tornado.py
@author Austin Cormier (austin.cormier@riftio.com)
@date 11/04/2015
@description Tasklet IOLoop classes to play nicely with Tornado
"""

import tornado.platform.asyncio


class TaskletAsyncIOLoop(tornado.platform.asyncio.BaseAsyncIOLoop):
    """  Bridge class between our Tasklet Event Loop and Tornado

    This class adds functionality to the default BaseAyncIOLoop to correctly
    bridge between Tornado and the Tasklet's StackedEventLoop.
    """

    def initialize(self, asyncio_loop, **kwargs):
        """ Initialize the IOLoop

        This initializes the IOLoop and ensures that this IOloop is made the
        current Tornado IOLoop on every scheduler tick.
        """
        super().initialize(asyncio_loop=asyncio_loop)
        asyncio_loop.register_tick_enter_hook(self.make_current)
        asyncio_loop.register_tick_exit_hook(self.clear_current)

    def close(self, all_fds=False):
        """ Close the Tornado IOLoop """
        self.asyncio_loop.unregister_tick_enter_hook(self.make_current)
        self.asyncio_loop.unregister_tick_exit_hook(self.clear_current)
        self.asyncio_loop=None
        super().close(all_fds)
