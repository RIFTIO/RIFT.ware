"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
