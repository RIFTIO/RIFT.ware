"""
# STANDARD_RIFT_IO_COPYRIGHT #

@file rwdtsperfmgrtasklet.py
@author Paul Laidler (Paul.Laidler@riftio.com)
@date 2015-06-24
"""

import asyncio
import functools
import itertools
import logging
import math
import sys

import gi
gi.require_version('RwDts', '1.0')
gi.require_version('RwDtsperfYang', '1.0')
gi.require_version('RwDtsperfmgrYang', '1.0')
gi.require_version('RwManifestYang', '1.0')
gi.require_version('RwVcsYang', '1.0')



from gi.repository import (
    RwDts as rwdts,
    RwDtsperfYang as rwdtsperf,
    RwDtsperfmgrYang as rwdtsperfmgr,
    RwManifestYang as rwmanifest,
    RwVcsYang as rwvcs
)

import rift.tasklets

if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async

class DtsPerfTestHandler(object):
    """Performance test handler"""
    DEFAULT_TASKLET = "c_tasklet"
    def __init__(self, on_start, on_score, on_finish):
        """ Wrap callbacks required by a performance test

        Each callback receives an instance of RwDtsPerfMgrTasklet's context, and
        the DtsPerfTest_Test object representing the test on which the callback
        is being invoked as arguments.

        Arguments:
            on_start  - Callback called when RPC start-perf-test is invoked
                        responsible for initializing resources required for test
            on_score  - Callback to derive performance result from test results
            on_finish - Callback called when RPC stop-perf-test is invoked
        """
        self.on_start = on_start
        self.on_score = on_score
        self.on_finish = on_finish

class RwDtsPerfMgrTasklet(rift.tasklets.Tasklet):
    """RW DTS Performance Manager Tasklet"""
    def start(self):
        """Tasklet entry point"""
        self.log.setLevel(logging.INFO)
        super().start()

        self.test_results = rwdtsperfmgr.PerfTest()
        self.config = rwdtsperfmgr.PerfmgrConfig()

        self.test_handlers = {
            "test_basic" : DtsPerfTestHandler(
                    on_start=self.start_test_basic,
                    on_score=self.score_test_basic,
                    on_finish=self.finish_test_basic),
            "test_latency" : DtsPerfTestHandler(
                    on_start=self.start_test_latency,
                    on_score=self.score_test_latency,
                    on_finish=self.finish_test_latency)
        }

        self._dts = rift.tasklets.DTS(
                self.tasklet_info,
                rwdtsperfmgr.get_schema(),
                self._loop,
                self.on_dts_state_change)

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

        # Subscribe to config: /rw-dtsperfmgr:perfmgr-config
        self.config = rwdtsperfmgr.PerfmgrConfig()
        self.pending = {}

        @asyncio.coroutine
        def register_perf_tasklets():
            """Registers both c & py flavors of the perf tasklets.
            Default behavior is to run the C tasklet, but can be changed by
            providing the tasklet_type in perfmgr-config.
            """
            for tasklet_type in ["c_tasklet", "py_tasklet"]:
                tasklet_type = self._resolve_tasklet_name(tasklet_type)

                # Provide config: add perf tasklet to the manifest
                xpath = ("C,/rw-manifest:manifest/rw-manifest:inventory"
                         "/rw-manifest:component[rw-manifest:component-name='{}']"
                                .format(tasklet_type))

                vcs_component = rwmanifest.Inventory_Component()
                vcs_component.component_name = tasklet_type
                vcs_component.component_type = "RWTASKLET"
                vcs_component.rwtasklet.plugin_directory = tasklet_type
                vcs_component.rwtasklet.plugin_name = tasklet_type

                reg = yield from self._dts.register(
                        xpath=xpath,
                        flags=(rwdts.Flag.PUBLISHER|rwdts.Flag.NO_PREP_READ))

                reg.create_element(xpath, vcs_component)

                proc = tasklet_type + "-proc"
                # Provide config: add process to the manifest
                xpath = ("C,/rw-manifest:manifest/rw-manifest:inventory"
                         "/rw-manifest:component[rw-manifest:component-name='{}']"
                                .format(proc))

                vcs_component = rwmanifest.Inventory_Component.from_dict({
                  "component_name": proc,
                  'component_type': 'RWPROC',
                  'rwproc': {
                      'tasklet': [
                        {
                          'name': 'dts-perf-tasklet',
                          'component_name': tasklet_type
                        }
                      ]
                  }
                })

                reg = yield from self._dts.register(
                        xpath=xpath,
                        flags=(rwdts.Flag.PUBLISHER|rwdts.Flag.NO_PREP_READ))

                reg.create_element(xpath, vcs_component)

        @asyncio.coroutine
        def prepare_acg(dts, acg, xact, xact_info, ksp, msg):
            """Prepare for application configuration. Stash the pending
            configuration object for subsequent transaction phases"""
            self.pending[xact.id] = msg
            acg.handle.prepare_complete_ok(xact_info.handle)

        def apply_config(dts, acg, xact, action, scratch):
            """Apply the pending configuration object"""
            if action == rwdts.AppconfAction.INSTALL and xact.id is None:
                return

            if xact.id not in self.pending:
                raise KeyError("No stashed configuration found with transaction id [{}]".format(xact.id))

            config = self.pending[xact.id]

            for vm_config in config.monitor_vm:
                monitor_vm = rwdtsperfmgr.PerfmgrConfig_MonitorVm()
                monitor_vm.instance_name = vm_config.instance_name
                self.config.monitor_vm.append(monitor_vm)


        with self._dts.appconf_group_create(
                handler=rift.tasklets.AppConfGroup.Handler(
                        on_apply=apply_config)) as acg:
            acg.register(
                    xpath="C,/rw-dtsperfmgr:perfmgr-config",
                    flags=rwdts.Flag.SUBSCRIBER,
                    on_prepare=prepare_acg)

        yield from register_perf_tasklets()


        # Provide operational data: /rw-dtsperfmgr:perf-test
        self.reg_perf_test = yield from self._dts.register(
                xpath="D,/rw-dtsperfmgr:perf-test",
                flags=rwdts.Flag.PUBLISHER,
                handler=rift.tasklets.DTS.RegistrationHandler(
                    on_prepare=self.prepare_result))

        # Provide RPC: start-perf-test
        yield from self._dts.register(
                xpath="I,/rw-dtsperfmgr:start-perf-test",
                handler=rift.tasklets.DTS.RegistrationHandler(
                        on_prepare=self.prepare_start_perf_test),
                flags=rwdts.Flag.PUBLISHER)

        # Provide RPC: stop-perf-test
        yield from self._dts.register(
                xpath="I,/rw-dtsperfmgr:stop-perf-test",
                handler=rift.tasklets.DTS.RegistrationHandler(
                        on_prepare=self.prepare_stop_perf_test),
                flags=rwdts.Flag.PUBLISHER)

    @asyncio.coroutine
    def run(self):
        """Run application. Wait for dts to be ready before continuing"""
        yield from self._dts.ready.wait()

    def _resolve_tasklet_name(self, name):
        """
        Convert the yang enum to tasklet names.
        """
        if name == "c_tasklet":
            return "rwdtsperf-c"
        if name == "py_tasklet":
            return "rwdtsperftasklet"

        raise ValueError("Incorrect tasklet_type specified %s" % name)

    @asyncio.coroutine
    def prepare_start_perf_test(self, xact_info, action, ks_path, msg):
        """ Prepare callback for start-perf-test
        Allocates a new performance test result, and kicks off a test
        in order to populate the test result
        """
        test_id = len(self.test_results.test)
        xpath = "O,/rw-dtsperfmgr:start-perf-test"
        response = rwdtsperfmgr.StartPerfTestOutput(test_id=test_id)

        if msg.test_type not in self.test_handlers:
            xact_info.respond_xpath(rwdts.XactRspCode.NACK, xpath)
            log.error("No test handler for {}".format(msg.test_type))
            return

        # Fill in gaps in StartPerfTestInput
        if msg.publishers is None or msg.publishers < 1:
            msg.publishers = 1

        if msg.subscribers is None or msg.subscribers < 0:
            msg.subscribers = 0

        if msg.parallel_xact_limit is None or msg.parallel_xact_limit < 1:
            msg.parallel_xact_limit = 1

        if msg.xact_max_with_outstanding is None or msg.xact_max_with_outstanding < 1:
            msg.xact_max_with_outstanding = 50000

        if msg.xact_operation is None:
            msg.xact_operation = 'create'

        if msg.payload.length is None:
            msg.payload.length = 0

        if msg.payload.pattern is None:
            msg.payload.pattern = ""

        if msg.test_type == "test_latency":
            msg.publishers = 1
            msg.subscribers = 1
            msg.parallel_xact_limit = 1
            if msg.payload.length == 0:
                msg.payload.length = 1000
            if msg.payload.pattern == "":
                msg.payload.pattern = "read-write"

        if msg.tasklet_type is None:
            msg.tasklet_type = self.DEFAULT_TASKLET

        test = rwdtsperfmgr.PerfTest_Test.from_dict({
                'test_id':test_id,
                'test_type':msg.test_type,
                'publishers':msg.publishers,
                'subscribers':msg.subscribers,
                'parallel_xact_limit':msg.parallel_xact_limit,
                'xact_max_with_outstanding':msg.xact_max_with_outstanding,
                'test_state':"test_initializing",
                'xact_operation':msg.xact_operation,
                'tasklet_type':msg.tasklet_type,
                'payload':{
                  'length':msg.payload.length,
                  'pattern':msg.payload.pattern}})

        self.test_results.test.append(test)

        @asyncio.coroutine
        def run_test(test):
            """Invokes test handlers to initialize and kick off a test
            Populates the test result with the set of tasklets running the test
            to allow them to be queried later"""

            # Initialize test
            (publishers, subscribers) = yield from self.test_handlers[test.test_type].on_start(test)

            if None in publishers:
                test.test_state = "test_failed"
                self.log.error("Failed to initialize test, a publisher failed initialization")
                return

            if None in subscribers:
                test.test_state = "test_failed"
                self.log.error("Failed to initialize test, a subscriber failed initialization")
                return

            test.tasklet_info.publisher = [rwdtsperfmgr.PerfTest_Test_TaskletInfo_Publisher(instance_name=name) for name in publishers]
            test.tasklet_info.subscriber = [rwdtsperfmgr.PerfTest_Test_TaskletInfo_Subscriber(instance_name=name) for name in subscribers]

            # Start test
            start_xact = rwdtsperf.StartXact()
            initiators = test.tasklet_info.publisher
            if test.test_type == 'test_latency':
                initiators = test.tasklet_info.subscriber

            yield from asyncio.sleep(5.0, loop=self._loop)

            for initiator in initiators:
                tasklet_id = initiator.instance_name.rsplit('-', 1)[1]
                start_xact.instance_id = int(tasklet_id)

                try:
                    query_iter = yield from self._dts.query_rpc(
                            xpath="/rwdtsperf:start-xact",
                            flags=0,
                            msg=start_xact)
                    test.test_state = "test_running"
                except (TransactionFailed, TransactionAborted):
                    test.test_state = "test_failed"

        asyncio.ensure_future(run_test(test), loop=self._loop)
        xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, response)

    @asyncio.coroutine
    def prepare_stop_perf_test(self, xact_info, action, ks_path, msg):
        """ Prepare callback for RPC: stop-perf-test

        Finalizes a previously started test by invoking the tests on_finish handler
        """
        xpath = "O,/rw-dtsperfmgr:stop-perf-test"
        response = rwdtsperfmgr.StopPerfTestOutput(status=0)

        try:
            test = self.test_results.test[msg.test_id]
        except IndexError:
            xact_info.respond_xpath(rwdts.XactRspCode.NACK, xpath)
            return

        yield from self.test_handlers[test.test_type].on_finish(test)
        test.test_state = "test_stopped"

        xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, response)


    @asyncio.coroutine
    def prepare_result(self, xact_info, action, ks_path, msg):
        """Prepare callback for perf-test
        Updates cached test result information and responds with
        the set of results requested by the input message
        """
        try:
            if not msg.test:
                # No specific instance specified, return all results
                yield from self.update_result()
                xact_info.respond_xpath(
                    rwdts.XactRspCode.ACK,
                    "D,/rw-dtsperfmgr:perf-test",
                    self.test_results)
                return

            test_id = msg.test[0].test_id
            yield from self.update_result(test_id)
            xact_info.respond_xpath(
                rwdts.XactRspCode.ACK,
                ( "D,/rw-dtsperfmgr:perf-test"
                  "/rw-dtsperfmgr:test[rw-dtsperfmgr:test-id='{}']"
                ).format(test_id),
                self.test_results.test[test_id])
        except Exception:
            xact_info.respond_xpath(
                rwdts.XactRspCode.NACK,
                "D,/rw-dtsperfmgr:perf-test",
                msg)
            raise


    @asyncio.coroutine
    def update_result(self, test_id=None):
        """Populate result DTS Performance test result object
        with a mixture of information from the result cache and
        DTS queries.

        Arguments:
            test_id - test instance identifier
        """
        if test_id is None:
            test_instances = self.test_results.test
        else:
            test_instances = [self.test_results.test[test_id]]

        @asyncio.coroutine
        def update_stats(test):
            if test.test_state not in ["test_initializing", "test_stopped"]:
                test.performance_metric = yield from self.test_handlers[test.test_type].on_score(test)

        # Asynchronously update test results
        future_updates = []
        for test in test_instances:
            future_updates.append(asyncio.ensure_future(update_stats(test), loop=self._loop))

        if future_updates:
            yield from asyncio.wait(future_updates, loop=self._loop)

    @asyncio.coroutine
    def get_parent_instances(self):
        """Get the list of parent instances on which to launch rwdtsperf-c tasklets
        This list is either provided through the performance manager's configuration
        or if no configuration exists defaults to the list of RWVM instances.

        Returns:
            list of instances to serve as parents to rwdtsperf-c tasklet instances
        """
        vms = []
        if self.config.monitor_vm:
            vms = [vm.instance_name for vm in self.config.monitor_vm]
        else:
            # No configuration - Grab the component list and find all VMs
            iter_resp = yield from self._dts.query_read(
                    xpath="D,/rw-base:vcs/rw-base:info",
                    flags=rwdts.XactFlag.MERGE)
            for fut_resp in iter_resp:
                r = yield from fut_resp
                resp = r.result
                for info in resp.components.component_info:
                    # gi.repository.RwBaseYang.ComponentInfo
                    if info.component_type == "RWVM":
                        vms.append("{}-{}".format(info.component_name, info.instance_id))
        return vms

    @asyncio.coroutine
    def start_test_basic(self, test):
        """Callback to start a basic dts performance test

        Launches performance tasklets based on the performance manager's
        configuration or lack thereof...
        Configures a single rsp flavor and xact for each tasklet launched

        Arguments:
            test - test result object

        Returns:
            A tuple containing two elements:
              The list of publishers participating in the test
              The list of subscribers participating in the test
        """

        @asyncio.coroutine
        def get_component_info(instance_name):
            """Retrieve component information for the given component instance

            Arguments:
                instance_name - The name of a component instance

            Returns:
                A gi.repository.RwBaseYang.ComponentInfo object
            """
            component_info = None

            xpath = ("D,/rw-base:vcs"
                    "/info"
                    "/components"
                    "/component_info[instance_name='{}']".format(instance_name))

            iter_resp = yield from self._dts.query_read(
                    xpath=xpath,
                    flags=rwdts.XactFlag.MERGE)

            for fut_resp in iter_resp:
                r = yield from fut_resp
                resp = r.result

                # response should be a gi.repository.RwBaseYang.ComponentInfo
                if resp.__class__.__name__ != "ComponentInfo":
                    # RIFT-8612: Both TaskletInfo and ComponentInfo are returned from query
                    continue

                component_info = resp

            return component_info

        @asyncio.coroutine
        def start_component(component_name, parent_instance):
            """VStart a component instance using dts

            Arguments:
                component_name  - name of component to start
                parent_instance - parent of component being spawned

            Returns:
                instance_name of started component
            """
            vstart_input = rwvcs.VStartInput(
                    component_name=component_name,
                    parent_instance=parent_instance)
            iter_resp = yield from self._dts.query_rpc(
                    xpath="/rw-vcs:vstart",
                    flags=0,
                    msg=vstart_input)

            # VStart "should" only return one response, but we have an iterator.
            # So... lets iterate over the responses.
            instance = None
            for fut_resp in iter_resp:
                resp = yield from fut_resp
                instance = resp.result.instance_name

            return instance

        @asyncio.coroutine
        def wait_for_process_tasklet_running(process_name, max_retries=30):
            """Wait for the tasklet wrapped by process with the given name to reach RUNNNING state

            Arguments:
                process_name - instance name of the process wrapping the tasklet being waited on
                max_retries - Maximum allowed attempts to determine if tasklet is running
                              (one second elapses between attempts)

            Returns:
                instance_name of the running tasklet
            """

            tasklet_name = None
            process_component = None
            tasklet_component = None
            retry_attempts = 1
            while tasklet_name == None and retry_attempts <= max_retries:
                if process_component is None:
                    process_component = yield from get_component_info(process_name)
                    if not process_component or not process_component.rwcomponent_children:
                        process_component = None
                        yield from asyncio.sleep(1.0, loop=self._loop)
                        retry_attempts += 1
                        continue

                tasklet_name = process_component.rwcomponent_children[0]
                tasklet_component = yield from get_component_info(tasklet_name)
                if tasklet_component is None or tasklet_component.state not in ["RUNNING"]:
                    tasklet_name = None
                    yield from asyncio.sleep(1.0, loop=self._loop)
                    retry_attempts += 1
                    continue

            return tasklet_name

        # Start each performance tasklet on one component chosen sequentially from
        # the list of all VMs, or in the case of configuration, configured parents

        xact = 'xact-init-{}'.format(test.test_id)
        rsp = 'rsp-init-{}'.format(test.test_id)

        @asyncio.coroutine
        def start_publisher(test, parent, xact, rsp):
            """Initializes a publisher process

            arguments:
              test - test instance currently being started
              parent - process is to be created under this parent component
              xact - transaction identifier
              rsp - response flavor for xact's response

            returns:
              name of newly created publisher tasklet or None
            """

            @asyncio.coroutine
            def configure_tasklet(test, xact, rsp, tasklet_id):
                subscriber_config = rwdtsperf.PerfdtsConfig.from_dict({
                    'config_task_instance':{
                        'instance_id':[{
                            'task_instance_id':int(tasklet_id),
                            'instance':int(tasklet_id),
                            'instance_config':{
                                'subscriber_config':{
                                    'rsp_flavor':[{
                                        'rsp_flavor_name':rsp,
                                    }]
                                }
                            }
                        }]
                    }
                })

                xact_config = rwdtsperf.PerfdtsConfig.from_dict({
                    'config_task_instance':{
                        'instance_id':[{
                            'task_instance_id':int(tasklet_id),
                            'instance':int(tasklet_id),
                            'instance_config':{
                                'xact_config':{
                                    'xact_repeat':{
                                        'outstanding_and_repeat':{
                                            'num_xact_outstanding':test.parallel_xact_limit,
                                            'xact_max_with_outstanding':test.xact_max_with_outstanding,
                                        }
                                    },
                                    'xact':[{
                                        'xact_name':xact,
                                        'xact_detail':{
                                            'xact_operation':test.xact_operation,
                                            'xact_rsp_flavor':rsp,
                                            'payload_len':test.payload.length,
                                            'payload_pattern':test.payload.pattern,
                                        }
                                    }]
                                }
                            }
                        }
                    ]}
                })

                yield from self._dts.query_update(
                        "C,/rwdtsperf:perfdts-config",
                        rwdts.XactFlag.ADVISE,
                        subscriber_config)

                yield from self._dts.query_update(
                        "C,/rwdtsperf:perfdts-config",
                        rwdts.XactFlag.ADVISE,
                        xact_config)

            proc = self._resolve_tasklet_name(test.tasklet_type) + "-proc"
            process_name = yield from start_component(proc, parent)
            tasklet_name = yield from wait_for_process_tasklet_running(process_name, max_retries=30)

            if tasklet_name:
                (_, _id) = tasklet_name.rsplit('-', 1)
                yield from configure_tasklet(test, xact, rsp, _id)

            return tasklet_name


        @asyncio.coroutine
        def start_subscriber(test, parent, rsp):
            """Initializes a tasklet who will participate in each transaction

            arguments:
              test - test instance currently being started
              parent - tasklet is to be created under this parent component
              rsp - response flavor to subscribe for

            returns:
              name of newly created tasklet
            """


            def configure_tasklet(test, xact, rsp, tasklet_id):
                subscriber_config = rwdtsperf.PerfdtsConfig.from_dict({
                    'config_task_instance':{
                        'instance_id':[{
                            'task_instance_id':int(tasklet_id),
                            'instance':int(tasklet_id),
                            'instance_config':{
                                'subscriber_config':{
                                    'rsp_flavor':[{
                                        'rsp_flavor_name':rsp,
                                    }]
                                }
                            }
                        }]
                    }
                })

                yield from self._dts.query_update(
                        "C,/rwdtsperf:perfdts-config",
                        rwdts.XactFlag.ADVISE,
                        subscriber_config)

            proc = self._resolve_tasklet_name(test.tasklet_type) + "-proc"
            process_name = yield from start_component(proc, parent)
            tasklet_name = yield from wait_for_process_tasklet_running(process_name, max_retries=30)

            if tasklet_name:
                (_, _id) = tasklet_name.rsplit('-', 1)
                yield from configure_tasklet(test, xact, rsp, _id)

            return tasklet_name


        parents = yield from self.get_parent_instances()
        parent_cycle = itertools.cycle(parents)

        # Asynchronously start and configure tasklets

        # ~NOTE RIFT-8067 implies the publisher is also a subscriber...
        publisher_tasks = []
        for parent in itertools.islice(parent_cycle, 0, test.publishers):
            result = yield from start_publisher(test, parent, xact, rsp)
            publisher_tasks.append(result)

        subscriber_tasks = []
        if test.subscribers > 0:
            for parent in itertools.islice(parent_cycle, 0, test.subscribers):
                result = yield from start_subscriber(test, parent, rsp)
                subscriber_tasks.append(result)

        return (publisher_tasks, subscriber_tasks)


    @asyncio.coroutine
    def score_test_basic(self, test):
        """Provide a score for the basic performance test.
        The score for a basic performance test is composed of a number of performance metrics

        Latency Metrics:
            Average round-trip-time (rtt)

        Throughput Metrics:
            Transactions per second (tps)

        Arguments:
            test - test instance to provide score for

        Returns:
            A list of performance-metric objects
        """

        def min_latency_rtt(exp):
            '''Returns the minimum allowed latency for an rtt buckets exponent'''
            if exp == 0:
                return 0.0
            return math.pow(2,exp-1)

        rtt_fields = [
            'rtt_less_2_exp_0',
            'rtt_less_2_exp_1',
            'rtt_less_2_exp_2',
            'rtt_less_2_exp_3',
            'rtt_less_2_exp_4',
            'rtt_less_2_exp_5',
            'rtt_less_2_exp_6',
            'rtt_less_2_exp_7',
            'rtt_less_2_exp_8',
            'rtt_less_2_exp_9',
            'rtt_less_2_exp_10',
            'rtt_less_2_exp_11',
            'rtt_less_2_exp_12',
            'rtt_less_2_exp_13',
            'rtt_less_2_exp_14',
            'rtt_less_2_exp_15',
            'rtt_greater',
        ]

        # Collect statistics from publisher tasklets
        for tasklet in test.tasklet_info.publisher:
            tasklet_id = tasklet.instance_name.rsplit('-', 1)[1]
            query_iter = yield from self._dts.query_read(
                    xpath="/perf-statistics[instance-id='{}']".format(tasklet_id),
                    flags=rwdts.XactFlag.MERGE)

            # Derive performance metrics
            average_rtt = 0
            requests_per_second = 0
            for fut_resp in query_iter:
                query_resp = yield from fut_resp
                stats = query_resp.result

                xacts_completed = stats.tot_xacts - stats.curr_outstanding
                elapsed_secs = ((stats.end_tv_sec - stats.start_tv_sec) * 1000000.0
                               + (stats.end_tv_usec - stats.start_tv_usec)) / 1000000.0

                if xacts_completed <= 0 or elapsed_secs <= 0:
                    continue

                if average_rtt == 0:
                    # FIXME
                    for exp, field in enumerate(rtt_fields):
                        average_rtt += (min_latency_rtt(exp) * getattr(stats, field)) / xacts_completed

                requests_per_second += xacts_completed / elapsed_secs

        metrics = [
                rwdtsperfmgr.PerfTest_Test_PerformanceMetric(
                    name="Average round-trip-time",
                    value=average_rtt),
                rwdtsperfmgr.PerfTest_Test_PerformanceMetric(
                    name="Transactions per second",
                    value=requests_per_second)]

        return metrics

    @asyncio.coroutine
    def finish_test_basic(self, test):
        ''' Callback invoked when rwdtsperfmgr receieves a stop-perf-test
        for a test instance of type test_basic

        Update statistics one final time and stop the set of tasklets
        associated with the given test

        arguments
          test - gi-object representing the test being finished
        '''

        # Get the final performance numbers for the test
        yield from self.update_result(test.test_id)

        @asyncio.coroutine
        def vstop_tasklet(tasklet):
            vstop_input = rwvcs.VStopInput(instance_name=tasklet.instance_name)
            query_iter = yield from self._dts.query_rpc(
                    xpath="/rw-vcs:vstop",
                    flags=0,
                    msg=vstop_input)
            # VStop "should" only return one response, but we have an iterator.
            # So... lets iterate over the responses.
            for fut_resp in query_iter:
                query_resp = yield from fut_resp
                result = query_resp.result
                self.log.info("vstop returned status %d", result.rw_status)

        # Asynchronously stop all tasklets involved in the test
        future_vstop_tasklets = []
        for tasklet in test.tasklet_info.publisher:
            future_vstop_tasklets.append(asyncio.ensure_future(vstop_tasklet(tasklet), loop=self._loop))

        for tasklet in test.tasklet_info.subscriber:
            future_vstop_tasklets.append(asyncio.ensure_future(vstop_tasklet(tasklet), loop=self._loop))

        if future_vstop_tasklets:
            yield from asyncio.wait(future_vstop_tasklets, loop=self._loop)

    @asyncio.coroutine
    def start_test_latency(self, test):
        """Callback to start a basic dts performance test

        Launches performance tasklets based on the performance manager's
        configuration or lack thereof...
        Configures a single rsp flavor and xact for each tasklet launched

        Arguments:
            test - test result object

        Returns:
            A tuple containing two elements:
              The list of publishers participating in the test
              The list of subscribers participating in the test
        """

        @asyncio.coroutine
        def get_component_info(instance_name):
            """Retrieve component information for the given component instance

            Arguments:
                instance_name - The name of a component instance

            Returns:
                A gi.repository.RwBaseYang.ComponentInfo object
            """
            component_info = None

            xpath = ("D,/rw-base:vcs"
                    "/info"
                    "/components"
                    "/component_info[instance_name='{}']".format(instance_name))

            iter_resp = yield from self._dts.query_read(
                    xpath=xpath,
                    flags=rwdts.XactFlag.MERGE)

            for fut_resp in iter_resp:
                r = yield from fut_resp
                resp = r.result

                # response should be a gi.repository.RwBaseYang.ComponentInfo
                if resp.__class__.__name__ != "ComponentInfo":
                    # RIFT-8612: Both TaskletInfo and ComponentInfo are returned from query
                    continue

                component_info = resp

            return component_info

        @asyncio.coroutine
        def start_component(component_name, parent_instance):
            """VStart a component instance using dts

            Arguments:
                component_name  - name of component to start
                parent_instance - parent of component being spawned

            Returns:
                instance_name of started component
            """
            vstart_input = rwvcs.VStartInput(
                    component_name=component_name,
                    parent_instance=parent_instance)
            iter_resp = yield from self._dts.query_rpc(
                    xpath="/rw-vcs:vstart",
                    flags=0,
                    msg=vstart_input)

            # VStart "should" only return one response, but we have an iterator.
            # So... lets iterate over the responses.
            instance = None
            for fut_resp in iter_resp:
                resp = yield from fut_resp
                instance = resp.result.instance_name

            return instance

        @asyncio.coroutine
        def wait_for_process_tasklet_running(process_name, max_retries=30):
            """Wait for the tasklet wrapped by process with the given name to reach RUNNNING state

            Arguments:
                process_name - instance name of the process wrapping the tasklet being waited on
                max_retries - Maximum allowed attempts to determine if tasklet is running
                              (one second elapses between attempts)

            Returns:
                instance_name of the running tasklet
            """

            tasklet_name = None
            process_component = None
            tasklet_component = None
            retry_attempts = 1
            while tasklet_name == None and retry_attempts <= max_retries:
                if process_component is None:
                    process_component = yield from get_component_info(process_name)
                    if not process_component or not process_component.rwcomponent_children:
                        process_component = None
                        yield from asyncio.sleep(1.0, loop=self._loop)
                        retry_attempts += 1
                        continue

                tasklet_name = process_component.rwcomponent_children[0]
                tasklet_component = yield from get_component_info(tasklet_name)
                if tasklet_component is None or tasklet_component.state not in ["RUNNING"]:
                    tasklet_name = None
                    yield from asyncio.sleep(1.0, loop=self._loop)
                    retry_attempts += 1
                    continue

            return tasklet_name

        # Start each performance tasklet on one component chosen sequentially from
        # the list of all VMs, or in the case of configuration, configured parents

        xact_read = 'xact-read-init-{}'.format(test.test_id)
        xact_create = 'xact-create-init-{}'.format(test.test_id)
        rsp_read = 'rsp-read-init-{}'.format(test.test_id)
        rsp_create = 'rsp-create-init-{}'.format(test.test_id)

        @asyncio.coroutine
        def start_publisher(test, parent, rsp_read, rsp_create):
            """Initializes a publisher process

            arguments:
              test - test instance currently being started
              parent - process is to be created under this parent component
              rsp_read - response flavor for read query
              rsp_create - response flavor for create query

            returns:
              name of newly created publisher tasklet or None
            """

            @asyncio.coroutine
            def configure_tasklet(test, rsp_read, rsp_create, tasklet_id):
                subscriber_config = rwdtsperf.PerfdtsConfig.from_dict({
                    'config_task_instance':{
                        'instance_id':[{
                            'task_instance_id':int(tasklet_id),
                            'instance':int(tasklet_id),
                            'instance_config':{
                                'subscriber_config':{
                                    'rsp_flavor':[{
                                        'rsp_flavor_name':rsp_read,
                                        'payload_len':test.payload.length,
                                        'payload_pattern':test.payload.pattern,
                                    },
                                    {
                                        'rsp_flavor_name':rsp_create,
                                    }]
                                }
                            }
                        }]
                    }
                })

                yield from self._dts.query_update(
                        "C,/rwdtsperf:perfdts-config",
                        rwdts.XactFlag.ADVISE,
                        subscriber_config)


            process_name = yield from start_component("rwdtsperf-c-proc", parent)
            tasklet_name = yield from wait_for_process_tasklet_running(process_name, max_retries=30)

            if tasklet_name:
                (_, _id) = tasklet_name.rsplit('-', 1)
                yield from configure_tasklet(test, rsp_read, rsp_create, _id)

            return tasklet_name


        @asyncio.coroutine
        def start_subscriber(test, parent, xact_read, xact_create, rsp_read, rsp_create):
            """Initializes a tasklet who will participate in each transaction

            arguments:
              test - test instance currently being started
              parent - tasklet is to be created under this parent component
              xact_read - invoke non transaction read query latency test
              xact_create - invoke transactional create query latency test
              rsp_read - name of the response flavor for xact_read
              rsp_create - name of the response flavor for xact_create

            returns:
              name of newly created tasklet
            """


            def configure_tasklet(test, xact_read, xact_create, rsp_read, rsp_create, tasklet_id):
                xact_config = rwdtsperf.PerfdtsConfig.from_dict({
                    'config_task_instance':{
                        'instance_id':[{
                            'task_instance_id':int(tasklet_id),
                            'instance':int(tasklet_id),
                            'instance_config':{
                                'xact_config':{
                                    'xact':[{
                                        'xact_name':xact_read,
                                        'xact_detail':{
                                            'xact_operation':'read',
                                            'xact_rsp_flavor':rsp_read,
                                            'xact_repeat_count':1000,
                                        }
                                    },
                                    {
                                        'xact_name':xact_create,
                                        'xact_detail':{
                                            'xact_operation':'create',
                                            'xact_rsp_flavor':rsp_create,
                                            'payload_len':test.payload.length,
                                            'payload_pattern':test.payload.pattern,
                                            'xact_repeat_count':1000,
                                        }
                                    }]
                                }
                            }
                        }
                    ]}
                })

                yield from self._dts.query_update(
                        "C,/rwdtsperf:perfdts-config",
                        rwdts.XactFlag.ADVISE,
                        xact_config)

            process_name = yield from start_component("rwdtsperf-c-proc", parent)
            tasklet_name = yield from wait_for_process_tasklet_running(process_name, max_retries=30)

            if tasklet_name:
                (_, _id) = tasklet_name.rsplit('-', 1)
                yield from configure_tasklet(test, xact_read, xact_create, rsp_read, rsp_create, _id)

            return tasklet_name


        parents = yield from self.get_parent_instances()
        parent_cycle = itertools.cycle(parents)

        # ~NOTE RIFT-8067 implies the publisher is also a subscriber...
        publisher_tasks = []
        for parent in itertools.islice(parent_cycle, 0, test.publishers):
                result = yield from start_publisher(test, parent, rsp_read, rsp_create)
                publisher_tasks.append(result)

        subscriber_tasks = []
        if test.subscribers > 0:
            for parent in itertools.islice(parent_cycle, 0, test.subscribers):
                result = yield from start_subscriber(test, parent, xact_read, xact_create, rsp_read, rsp_create)
                subscriber_tasks.append(result)

        return (publisher_tasks, subscriber_tasks)

    @asyncio.coroutine
    def score_test_latency(self, test):
        """Provide a score for the basic performance test.
        The score for a basic performance test is composed of a number of performance metrics

        Latency Metrics:
            Average Latency

        Arguments:
            test - test instance to provide score for

        Returns:
            A list of performance-metric objects
        """
        # Collect statistics from publisher tasklets
        for tasklet in test.tasklet_info.subscriber:
            tasklet_id = tasklet.instance_name.rsplit('-', 1)[1]
            query_iter = yield from self._dts.query_read(
                    xpath="/perf-statistics[instance-id='{}']".format(tasklet_id),
                    flags=rwdts.XactFlag.MERGE)

            # Derive performance metrics
            for fut_resp in query_iter:
                query_resp = yield from fut_resp
                stats = query_resp.result
            if stats.curr_outstanding == 0:
                test.test_state = "test_finished"

        metrics = []
        for latency in stats.latency_averages:
            metrics.append(
                rwdtsperfmgr.PerfTest_Test_PerformanceMetric(
                    name="Average Latency of '{}' for {} xacts in (usec)".format(latency.xact_name, latency.num_xacts),
                    value=latency.ave_latency))
            metrics.append(
                rwdtsperfmgr.PerfTest_Test_PerformanceMetric(
                    name="Minimum Latency of '{}' for {} xacts in (usec)".format(latency.xact_name, latency.num_xacts),
                    value=latency.min_latency))
            metrics.append(
                rwdtsperfmgr.PerfTest_Test_PerformanceMetric(
                    name="Maximum Latency of '{}' for {} xacts in (usec)".format(latency.xact_name, latency.num_xacts),
                    value=latency.max_latency))

        return metrics

    @asyncio.coroutine
    def finish_test_latency(self, test):
        ''' Callback invoked when rwdtsperfmgr receieves a stop-perf-test
        for a test instance of type test_latency

        Update statistics one final time and stop the set of tasklets
        associated with the given test

        arguments
          test - gi-object representing the test being finished
        '''

        # Get the final performance numbers for the test
        yield from self.update_result(test.test_id)

        @asyncio.coroutine
        def vstop_tasklet(tasklet):
            vstop_input = rwvcs.VStopInput(instance_name=tasklet.instance_name)
            query_iter = yield from self._dts.query_rpc(
                    xpath="/rw-vcs:vstop",
                    flags=0,
                    msg=vstop_input)
            # VStop "should" only return one response, but we have an iterator.
            # So... lets iterate over the responses.
            for fut_resp in query_iter:
                query_resp = yield from fut_resp
                result = query_resp.result
                self.log.debug("vstop returned status %d", result.rw_status)

        # Asynchronously stop all tasklets involved in the test
        future_vstop_tasklets = []
        for tasklet in test.tasklet_info.publisher:
            future_vstop_tasklets.append(asyncio.ensure_future(vstop_tasklet(tasklet), loop=self._loop))

        for tasklet in test.tasklet_info.subscriber:
            future_vstop_tasklets.append(asyncio.ensure_future(vstop_tasklet(tasklet), loop=self._loop))

        if future_vstop_tasklets:
            yield from asyncio.wait(future_vstop_tasklets, loop=self._loop)
