#!/usr/bin/env python3

import asyncio
import gi
import logging
import time

gi.require_version("RwDts", "1.0")
from gi.repository import (
    RwDts as rwdts,
    RwDtsYang,
)
import rift.tasklets
import rift.test.dts
import rwlogger


@asyncio.coroutine
def fake_library_log_call(loop):
    logger = logging.getLogger("library")
    logger.setLevel(logging.DEBUG)
    for _ in range(5):
        # Use two seperate lines to bypass duplicate detection in rwlog
        logger.debug("library_debug")
        logger.debug("library_debug")


@asyncio.coroutine
def fake_threaded_library_log_calls(rwlog, loop):
    """ Simulate a library logging messages while running in a seperate thread """
    def thread_logs():
        with rwlogger.rwlog_root_handler(rwlog):
            logger = logging.getLogger("library")
            for _ in range(4):
                # Use two seperate lines to bypass duplicate detection in rwlog
                logger.debug("threaded_library")
                logger.debug("threaded_library")

                with rwlogger.rwlog_root_handler(rwlog) as rwlog2:
                    rwlog2.set_category("rw-generic-log")
                    logger.debug("threaded_nested_library")

                # Give the thread a chance to swap out and potentially
                # conflict with the test's rwmain logger
                time.sleep(.05)

            logger.debug("threaded_library")
            logger.debug("threaded_library")


    yield from loop.run_in_executor(None, thread_logs)


class RwLogTestTasklet(rift.tasklets.Tasklet):
    """ A tasklet to test Python rwlog interactions  """
    def __init__(self, *args, **kwargs):
        super(RwLogTestTasklet, self).__init__(*args, **kwargs)
        self._dts = None
        self.rwlog.set_category("rw-logtest-log")

    def start(self):
        """ The task start callback """
        super(RwLogTestTasklet, self).start()

        self._dts = rift.tasklets.DTS(self.tasklet_info,
                                      RwDtsYang.get_schema(),
                                      self.loop,
                                      self.on_dts_state_change)

    @asyncio.coroutine
    def init(self):
        self.log.debug("tasklet_debug")
        yield from fake_library_log_call(self.loop)
        yield from fake_threaded_library_log_calls(self.rwlog, self.loop)

    @asyncio.coroutine
    def run(self):
        pass

    @asyncio.coroutine
    def on_dts_state_change(self, state):
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
            self.log.debug("Changing state to %s", next_state)
            self._dts.handle.set_state(next_state)
