"""
# STANDARD_RIFT_IO_COPYRIGHT #

@file rwdtsperftasklet.py
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
gi.require_version('RwTypes', '1.0')
gi.require_version('RwDts', '1.0')
gi.require_version('RwDtsperfYang', '1.0')
gi.require_version('RwDtsperfmgrYang', '1.0')
gi.require_version('RwManifestYang', '1.0')
gi.require_version('RwVcsYang', '1.0')

import gi.repository.RwTypes as rwtypes
import rift.tasklets
import rift.tasklets.dts

from gi.repository import (
    RwDts as rwdts,
    RwDtsperfYang as rwdtsperf,
    RwDtsperfmgrYang as rwdtsperfmgr,
    RwManifestYang as rwmanifest,
    RwVcsYang as rwvcs
)


if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async


class TaskletState(object):
    """
    Different states of the tasklet.
    """
    STARTING, RUNNING, FINISHED = ("starting", "running", "finished")


class ExtendGi(object):
    """
    why?
    - Sub-Classing a GiObject is not possible, so we expose all the attributes
      of GiObject as attributes of the python class.
    - This gives us more flexibility and also enables us to add data related
      logic in a single place.
    - Also prevents the abuse of ".", by providing shorter properties.
    """
    def __init__(self, gi_object):
        self._gi_object = gi_object

    def __getattr__(self, name):
        if name in self._gi_object.fields:
            return getattr(self._gi_object, name)
        raise AttributeError("%r object has no attribute %r" %
                (self.__class__, name))

    def __setattr__(self, name, value):
        if name != "_gi_object" and name in self._gi_object.fields:
            setattr(self._gi_object, name, value)

        super().__setattr__(name, value)


class TestConfig(ExtendGi):
    """Extends the data class and adds more functionality such as
    rotating xact_details and facilitates easier retrieval of xact_detail
    based on the flavor name.

    Currently supports only outstanding mode. More modes to be added!!
    """
    def __init__(self, xact_config):
        """
        Args:
            xact_config (PerfdtsConfig): Config instance from the Manager
                    tasklet.
        """
        super().__init__(xact_config)

        self.mode = self._get_mode(self.xact_repeat)

        self._xacts = { xact.xact_detail.xact_rsp_flavor: XactDetail(xact)
                for xact in self.xact}

        self.xacts = itertools.cycle(list(self._xacts.values()))
        self.rsp_flavors = list(self._xacts.keys())

    @property
    def parallel_xacts(self):
        """
        Total number of parallel xacts
        """
        return self.xact_repeat.outstanding_and_repeat.num_xact_outstanding

    @property
    def total_xacts(self):
        """
        Total number of transactions to be triggered.
        """
        return self.xact_repeat.outstanding_and_repeat.xact_max_with_outstanding

    @total_xacts.setter
    def total_xacts(self, value):
        self.xact_repeat.outstanding_and_repeat.xact_max_with_outstanding = value

    def _get_mode(self, mode_config):
        """Mode of the config

        Args:
            mode_config (XactRepeat): Gi object for XactRepeat.

        Returns:
            str : The equivalent string representation of the Repeat mode.
        """
        if "outstanding_and_repeat" in mode_config:
            return "OUTSTANDING"

    def get_xact_detail(self, rsp_flavor):
        """XactDetail for rsp_flavor

        Args:
            rsp_flavor (str): rsp_flavor (eg: rsp-init-0)

        Returns:
           XactDetail : Returns the XactDetail config data for rsp_flavor.
        """
        return self._xacts[rsp_flavor]


class XactDetail(ExtendGi):
    """Extends the data class for Xact, contains the xact_name and xact_detail.
    """
    def __init__(self, xact):
        """
        Args:
            xact (Xact): Gi object for Xact in PerfdtsConfig list.
        """
        super().__init__(xact.xact_detail)
        self.xact_name = xact.xact_name

        self.action = self._get_operation(self.xact_operation)

    @property
    def xpath(self):
        """
        Generate the xpath for XactDetail. This is needed for on_prepare callback
        Although xpath can be generated from keyspec, we don't want to take that
        approach as it has a performance overhead.
        """
        return "D,/rwdtsperf:xact-msg/rwdtsperf:content" + \
                "[rwdtsperf:xact-detail='{}']".format(self.xact_rsp_flavor)

    def _get_operation(self, operation):
        """Get the Dts api to be triggered.
        
        Args:
            operation (str): create/update/read
        
        Returns:
            str: The callable in DTS class in str format.
        """
        if operation == "create":
            return "query_create"
        if operation == "update":
            return "query_update"
        if operation == "read":
            return "query_read"

        raise ValueError("Incorrect operation: {}. Valid choices are "
                "['create', 'update', 'read']".format(operation))

    def generate_content(self):
        """Generates the input payload based on the configuration provided.

        Returns:
            XactMsg_Content: Gi object
        """
        msg = rwdtsperf.XactMsg_Content()
        msg.xact_detail = self.xact_rsp_flavor

        content = self.payload_pattern
        times = 1
        if len(content) > 0:
            times = self.payload_len / len(content)

        content *= math.ceil(times)
        content = content[:self.payload_len]

        msg.input_payload = content

        return msg

    @asyncio.coroutine
    def query(self, dts):
        """A convenience method that abstracts the construction of payload and
        the api for performance test.

        Args:
            dts (rift.tasklets.dts.DTS): Dts handle.

        Yields:
            asyncio.Future: Return a future with transaction state.
        """
        msg = self.generate_content()
        query = getattr(dts, self.action)

        args = (self.xpath, rwdts.XactFlag.ADVISE, msg)
        if self.xact_operation == 'read':
            args = (self.xpath, rwdts.XactFlag.ADVISE)

        yield from query(*args)


class Statistics(ExtendGi):
    """
    Extends PerfStatistics and provides convenience operations over it.
    """
    def __init__(self, tasklet):
        """
        Args:
            tasklet (RwDtsPerfTasklet): tasklet instance.
        """
        super().__init__(rwdtsperf.PerfStatistics())
        self.tasklet = tasklet
        self.instance_id = int(tasklet.instance_id)

        self.rtt_index = [2 ** i for i in range(16)]
        self._rtt_buckets = [0] * 16

        self.start_time = time.time()
        start_str = str(self.start_time).split(".")
        self.start_tv_sec = int(start_str[0])
        self.start_tv_usec = int(start_str[1])

    def update_rtt(self, duration):
        """Compute and bucket the duration.

        Args:
            duration (float): Total time taken to complete the transaction.
        """
        # Scale it up to milliseconds
        duration *= 1000
        index = bisect.bisect(self.rtt_index, duration)
        if index > 15:
            index = 15

        self._rtt_buckets[index] += 1

    def update_end_time(self):
        """
        Sets the end time for the tasklet.
        """
        end_time = time.time()
        end_str = str(end_time).split(".")
        self.end_tv_sec = int(end_str[0])
        self.end_tv_usec = int(end_str[1])

    def get_gi_object(self):
        """Refreshes the GiObject stored internally and returns the data.
        Args:
            instance_id (id): Instance id of the RwDtsPerfTasklet.

        Returns:
            rwdtsperf.PerfStatistics: Gi object.
        """
        if self.tasklet.state != TaskletState.FINISHED:
            self.update_end_time()

        for index, value in enumerate(self._rtt_buckets):
            attr_name = "rtt_less_2_exp_{}".format(index)
            setattr(self, attr_name, value)

        return self._gi_object


class RwDtsPerfTasklet(rift.tasklets.Tasklet):
    def start(self):
        """Entry point for tasklet
        """
        self.log.setLevel(logging.INFO)
        super().start()

        self._dts = rift.tasklets.DTS(
                self.tasklet_info,
                rwdtsperf.get_schema(),
                self._loop,
                self.on_dts_state_change)

        # Set the instance id
        self.instance_name = self.tasklet_info.instance_name
        self.instance_id = int(self.instance_name.rsplit('-', 1)[1])
        self.log.debug("Starting RwDtsPerfTasklet Name: {}, Id: {}".format(
                self.instance_name,
                self.instance_id))

        self.state = TaskletState.STARTING
        self.xact_config = None  # Will contain the TestConfig data
        self.statistics = Statistics(self)
        # A container to hold the current outstanding transactions
        self.curr_xacts = {}

    def stop(self):
        """Exit point for the tasklet
        """
        # This ensures that no more transactions are triggered.
        self.xact_config.total_xacts = 0

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
        self.log.debug("DTS transition from {} -> {}".format(state, next_state))

        if next_state is not None:
            self._dts.handle.set_state(next_state)

    # Callbacks

    @asyncio.coroutine
    def init(self):
        """Initialize application. During this state transition all DTS
        registrations and subscriptions required by application
        should be started.
        """

        # 1. Register for statistics:
        self.perf_statistics = yield from self._dts.register(
                xpath="D,/rwdtsperf:perf-statistics",
                flags=rwdts.Flag.PUBLISHER|rwdts.Flag.SHARED,
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=self.handle_perf_statistics))

        # Register for RPC
        self.start_xact = yield from self._dts.register(
                xpath="I,/rwdtsperf:start-xact",
                flags=rwdts.Flag.PUBLISHER|rwdts.Flag.SHARED,
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=self.handle_start_xact))

        @asyncio.coroutine
        def prepare_acg(dts, acg, xact, xact_info, ksp, msg, scratch):
            """Prepare for application configuration. Stash the pending
            configuration object for subsequent transaction phases"""
            acg.scratch[xact.id] = msg
            acg.handle.prepare_complete_ok(xact_info.handle)

        def apply_config(dts, acg, xact, action, scratch):
            """On apply callback for AppConf registration"""
            if action == rwdts.AppconfAction.INSTALL and xact.id is None:
                return

            if not scratch:
                raise ValueError("No configuration found with "
                        "transaction id [{}]".format(xact.id))
            config = scratch

            instance = config.config_task_instance.instance_id[0]
            instance_id = instance.task_instance_id

            if instance_id != self.instance_id:
                return rwtypes.RwStatus.SUCCESS

            xact_cfg = instance.instance_config

            if "xact_config" in xact_cfg:
                self.xact_config = TestConfig(xact_cfg.xact_config)

            if "subscriber_config" in xact_cfg:
                subscriber_config = xact_cfg.subscriber_config
                for rsp_flavor in subscriber_config.rsp_flavor:
                    xpath = "D,/rwdtsperf:xact-msg/rwdtsperf:content" \
                            "[rwdtsperf:xact-detail='{}']".format(
                                    rsp_flavor.rsp_flavor_name)

                    handler = rift.tasklets.DTS.RegistrationHandler(
                            on_prepare=self.prepare_xact)

                    with dts.group_create() as group:
                        group.register(
                                xpath,
                                handler=handler,
                                flags=rwdts.Flag.SUBSCRIBER)

                        group.register(
                                xpath,
                                handler=handler,
                                flags=rwdts.Flag.PUBLISHER)

            return rwtypes.RwStatus.SUCCESS

        # Apply config
        with self._dts.appconf_group_create(
                handler=rift.tasklets.AppConfGroup.Handler(
                        on_apply=apply_config)) as acg:
            acg.register(
                    xpath="D,/perfdts-config",
                    flags=rwdts.Flag.SUBSCRIBER|rwdts.Flag.CACHE|0,
                    on_prepare=prepare_acg)

    @asyncio.coroutine
    def prepare_xact(self, xact_info, action, ks_path, msg):
        """Prepare callback for xacts
        """
        rsp_flavor = msg.xact_detail

        if rsp_flavor in self.xact_config.rsp_flavors:
            self.statistics.send_rsps += 1
            xact_detail = self.xact_config.get_xact_detail(rsp_flavor)
            msg = xact_detail.generate_content()
            xact_info.respond_xpath(
                rwdts.XactRspCode.ACK,
                xact_detail.xpath,
                msg)
            return

        # If not a valid rsp_flavor send NA.
        xpath = ks_path.to_xpath(rwdtsperf.get_schema())
        xact_info.respond_xpath(rwdts.XactRspCode.NA, xpath)

    @asyncio.coroutine
    def run(self):
        """Initialize application. During this state transition all DTS
        registrations and subscriptions required by application should be started
        """
        yield from self._dts.ready.wait()

    @asyncio.coroutine
    def handle_perf_statistics(self, xact_info, action, ks_path, msg):
        """
        On prepare callback for /perf-statistics
        """
        # Wildcard
        if msg.instance_id != 0 and msg.instance_id != self.instance_id:
            self.log.debug("Instance {}: Got perf-statistics msg for instance_id {}."
                    "Sending NA".format(self.instance_id, msg.instance_id))
            xpath = ks_path.to_xpath(rwdtsperf.get_schema())
            xact_info.respond_xpath(rwdts.XactRspCode.NA, xpath)
            return

        # once the DTS issue has been fixed, change msg.instance_id to self.
        xpath = "/rwdtsperf:perf-statistics[rwdtsperf:instance-id='{}']".format(
                self.instance_id)
        stats = self.statistics.get_gi_object()
        xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, stats)

    @asyncio.coroutine
    def handle_start_xact(self, xact_info, action, ks_path, msg):
        """On prepare callback for /start-xact rpc
        """
        xpath = ks_path.to_xpath(rwdtsperf.get_schema())
        # Check if the instance can respond to the xpath
        if msg.instance_id != self.instance_id:
            xact_info.respond_xpath(rwdts.XactRspCode.NA, xpath)
            return

        if self.state == TaskletState.RUNNING:
            xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath)
            return

        xact_detail = next(self.xact_config.xacts)
        self.state = TaskletState.RUNNING
        xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath)

        self.log.info("Starting RwDtsPerfTasklet id: {}".format(self.instance_id))
        self.send_xact(xact_detail)

    def handle_response(self, start_time, future):
        """
        Done callback for all xacts.

        Args:
            start_time (float): Time at which the transaction was started.
            future (asyncio.Future): Future that contains the xact result.
        """
        end_time = time.time()

        # Do not consider any more xacts after the state is done.
        if self.state == TaskletState.FINISHED:
            return

        del self.curr_xacts[future]

        self.statistics.update_rtt(end_time - start_time)
        self.statistics.curr_outstanding -= 1

        self.statistics.tot_rsps += 1
        try:
            result = future.result()
            self.statistics.succ_count += 1
        except rift.tasklets.dts.TransactionFailed:
            self.statistics.fail_count += 1
        except rift.tasklets.dts.TransactionAborted:
            self.statistics.abrt_count += 1

        self.send_next()

    # End of Callbacks

    def send_xact(self, xact_detail):
        """Triggers the transaction.

        Args:
            xact_detail (XactDetail): XactDetail instance
        """
        self.statistics.tot_xacts += 1
        self.statistics.curr_outstanding += 1

        # Dts python APIs doesn't provide a "on_response" callback, instead
        # it takes care of on_response internally and provides the results.
        # But we need the on_reponse to compute our perf statistics. So, here
        # we explicitly add a done callback.
        future = asyncio.ensure_future(xact_detail.query(self._dts), loop=self.loop)
        self.curr_xacts[future] = True
        future.add_done_callback(functools.partial(
                                        self.handle_response,
                                        time.time()))

        self.send_next()

    def send_next(self):
        """Check if next transaction can be started and triggers the next
        transaction.
        """
        while self.statistics.tot_xacts < self.xact_config.total_xacts and \
                self.statistics.curr_outstanding < self.xact_config.parallel_xacts:
            # Cycle thro' the list and get the next xact_detail.
            xact_detail = next(self.xact_config.xacts)
            self.send_xact(xact_detail)

        # Check and update the status of the tasklet.
        if self.statistics.tot_xacts >= self.xact_config.total_xacts:
            self.statistics.update_end_time()
            self.log.info("Completed all transactions")
            self.state = TaskletState.FINISHED
