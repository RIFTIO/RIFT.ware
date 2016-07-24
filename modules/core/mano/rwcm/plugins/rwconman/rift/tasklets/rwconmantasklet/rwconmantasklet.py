
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

'''
This file - ConfigManagerTasklet()
|
+--|--> ConfigurationManager()
        |
        +--> rwconman_config.py - ConfigManagerConfig()
        |    |
        |    +--> ConfigManagerNSR()
        |
        +--> rwconman_events.py - ConfigManagerEvents()
             |
             +--> ConfigManagerROif()

'''

import asyncio
import logging
import os

import gi
gi.require_version('RwDts', '1.0')
gi.require_version('RwConmanYang', '1.0')

from gi.repository import (
    RwDts as rwdts,
    RwConmanYang as conmanY,
)

import rift.tasklets

from . import rwconman_config as Config
from . import rwconman_events as Event

def log_this_vnf(vnf_cfg):
    log_vnf = ""
    used_item_list = ['nsr_name', 'vnfr_name', 'member_vnf_index', 'mgmt_ip_address']
    for item in used_item_list:
        if item in vnf_cfg:
            if item == 'mgmt_ip_address':
                log_vnf += "({})".format(vnf_cfg[item])
            else:
                log_vnf += "{}/".format(vnf_cfg[item])
    return log_vnf

class ConfigurationManager(object):
    def __init__(self, log, loop, dts):
        self._log            = log
        self._loop           = loop
        self._dts            = dts
        self.cfg_sleep       = True
        self.cfg_dir         = os.path.join(os.environ["RIFT_INSTALL"], "etc/conman")
        self._config         = Config.ConfigManagerConfig(self._dts, self._log, self._loop, self)
        self._event          = Event.ConfigManagerEvents(self._dts, self._log, self._loop, self)
        self.pending_cfg     = []
        self.pending_tasks   = {}

        self._handlers = [
            self._config,
            self._event,
        ]


    @asyncio.coroutine
    def update_vnf_state(self, vnf_cfg, state):
        nsr_obj = vnf_cfg['nsr_obj']
        self._log.info("Updating cm-state for VNF(%s/%s) to:%s", nsr_obj.nsr_name, vnf_cfg['vnfr_name'], state)
        yield from nsr_obj.update_vnf_cm_state(vnf_cfg['vnfr'], state)

    @asyncio.coroutine
    def update_ns_state(self, nsr_obj, state):
        self._log.info("Updating cm-state for NS(%s) to:%s", nsr_obj.nsr_name, state)
        yield from nsr_obj.update_ns_cm_state(state)

    def add_to_pending(self, nsr_obj):
        
        if (nsr_obj not in self.pending_cfg and
            nsr_obj.cm_nsr['state'] == nsr_obj.state_to_string(conmanY.RecordState.RECEIVED)):
            
            self._log.info("Adding NS={} to pending config list"
                           .format(nsr_obj.nsr_name))
            
            # Build the list
            nsr_obj.vnf_cfg_list = []
            # Sort all the VNF by their configuration attribute priority
            sorted_dict = dict(sorted(nsr_obj.nsr_cfg_config_attributes_dict.items()))
            for config_attributes_dict in sorted_dict.values():
                # Iterate through each priority level
                for config_priority in config_attributes_dict:
                    # Iterate through each vnfr at this priority level
                    vnfr = nsr_obj._vnfr_dict[config_priority['id']]
                    self._log.debug("Adding VNF:(%s) to pending cfg list", log_this_vnf(vnfr['vnf_cfg']))
                    nsr_obj.vnf_cfg_list.append(vnfr['vnf_cfg'])
            self.pending_cfg.append(nsr_obj)

    @asyncio.coroutine
    def configuration_handler(self):
        @asyncio.coroutine
        def process_vnf_cfg(agent_vnfr, nsr_obj):
            vnf_cfg = agent_vnfr.vnf_cfg
            done = False

            if vnf_cfg['cfg_retries']:
                # This failed previously, lets give it some time
                yield from asyncio.sleep(5, loop=self._loop)

            vnf_cfg['cfg_retries'] += 1

            # Check to see if this vnfr is managed
            done = yield from self._config._config_agent_mgr.invoke_config_agent_plugins(
                'apply_initial_config',
                nsr_obj.agent_nsr,
                agent_vnfr)
            self._log.debug("Apply configuration for VNF={} on attempt {} " \
                            "returned {}".format(log_this_vnf(vnf_cfg),
                                                 vnf_cfg['cfg_retries'],
                                                 done))

            if done:
                yield from self.update_vnf_state(vnf_cfg, conmanY.RecordState.READY)

            else:
                # Check to see if the VNF configure failed
                status = yield from self._config._config_agent_mgr.invoke_config_agent_plugins(
                    'get_config_status',
                    nsr_obj.agent_nsr,
                    agent_vnfr)

                if status and status == 'error':
                    # Failed configuration
                    nsr_obj.vnf_failed = True
                    done = True
                    yield from self.update_vnf_state(vnf_cfg, conmanY.RecordState.CFG_FAILED)
                    self._log.error("Failed to apply configuration for VNF = {}"
                                    .format(log_this_vnf(vnf_cfg)))

            return done

        @asyncio.coroutine
        def process_nsr_obj(nsr_obj):
            # Return status, this will be set to False is if we fail to configure any VNF
            ret_status = True

            # Reset VNF failed flag
            nsr_obj.vnf_failed = False
            vnf_cfg_list = nsr_obj.vnf_cfg_list
            while vnf_cfg_list:
                # Check to make sure the NSR is still valid
                if nsr_obj.parent.is_nsr_valid(nsr_obj.nsr_id) is False:
                    self._log.info("NSR {} not found, could be terminated".
                                    format(nsr_obj.nsr_id))
                    return

                # Need while loop here, since we will be removing list item
                vnf_cfg = vnf_cfg_list.pop(0)
                self._log.info("Applying Pending Configuration for VNF = %s / %s",
                               log_this_vnf(vnf_cfg), vnf_cfg['agent_vnfr'])
                vnf_done = yield from process_vnf_cfg(vnf_cfg['agent_vnfr'], nsr_obj)
                self._log.debug("Applied Pending Configuration for VNF = {}, status={}"
                                .format(log_this_vnf(vnf_cfg), vnf_done))

                if not vnf_done:
                    # We will retry, but we will give other VNF chance first since this one failed.
                    vnf_cfg_list.append(vnf_cfg)

            if nsr_obj.vnf_failed:
                # Atleast one VNF config failed
                ret_status = False

            if ret_status:
                # Apply NS initial config if present
                nsr_obj.nsr_failed = False
                self._log.debug("Apply initial config on NSR {}".format(nsr_obj.nsr_name))
                try:
                    yield from nsr_obj.parent.process_ns_initial_config(nsr_obj)
                except Exception as e:
                    nsr_obj.nsr_failed = True
                    self._log.exception(e)
                    ret_status = False

            # Set the config status for the NSR
            if ret_status:
                yield from nsr_obj.update_ns_cm_state(conmanY.RecordState.READY)
            elif nsr_obj.vnf_failed or nsr_obj.nsr_failed:
                yield from nsr_obj.update_ns_cm_state(conmanY.RecordState.CFG_FAILED)
            return ret_status

        # Basically, this loop will never end.
        while True:
            # Check the pending tasks are complete
            # Store a list of tasks that are completed and
            # remove from the pending_tasks list outside loop
            ids = []
            for nsr_id, task in self.pending_tasks.items():
                if task.done():
                    ids.append(nsr_id)
                    e = task.exception()
                    if e:
                        self._log.error("Exception in configuring nsr {}: {}".
                                        format(nsr_id, e))
                        self._log.exception(e)

                    else:
                        rc = task.result()
                        self._log.debug("NSR {} configured: {}".format(nsr_id, rc))
                else:
                    self._log.debug("NSR {} still configuring".format(nsr_id))

            # Remove the completed tasks
            for nsr_id in ids:
                self.pending_tasks.pop(nsr_id)

            # TODO (pjoseph): Fix this
            # Sleep before processing any NS (Why are we getting multiple NSR running DTS updates?)
            # If the sleep is not 10 seconds it does not quite work, NSM is marking it 'running'
            # wrongfully 10 seconds in advance?
            yield from asyncio.sleep(10, loop=self._loop)

            if self.pending_cfg:
                # get first NS, pending_cfg is nsr_obj list
                nsr_obj = self.pending_cfg[0]
                nsr_done = False
                if nsr_obj.being_deleted is False:
                    # Process this NS, returns back same obj is successfull or exceeded retries
                    try:
                        self._log.info("Processing NSR:{}".format(nsr_obj.nsr_name))

                        # Check if we already have a task running for this NSR
                        # Case where we are still configuring and terminate is called
                        if nsr_obj.nsr_id in self.pending_tasks:
                            self._log.error("NSR {} in state {} has a configure task running.".
                                            format(nsr_obj.nsr_name, nsr_obj.get_ns_cm_state()))
                            # Terminate the task for this NSR
                            self.pending_tasks[nsr_obj.nsr_id].cancel()

                        yield from self.update_ns_state(nsr_obj, conmanY.RecordState.CFG_PROCESS)

                        # Call in a separate thread
                        self.pending_tasks[nsr_obj.nsr_id] = \
                            self._loop.create_task(
                                    process_nsr_obj(nsr_obj)
                            )

                        # Remove this nsr_obj
                        self.pending_cfg.remove(nsr_obj)

                    except Exception as e:
                        self._log.error("Failed to process NSR as %s", str(e))
                        self._log.exception(e)


    @asyncio.coroutine
    def register(self):
        # Perform register() for all handlers
        for reg in self._handlers:
            yield from reg.register()

        asyncio.ensure_future(self.configuration_handler(), loop=self._loop)

class ConfigManagerTasklet(rift.tasklets.Tasklet):
    def __init__(self, *args, **kwargs):
        super(ConfigManagerTasklet, self).__init__(*args, **kwargs)
        self.rwlog.set_category("rw-conman-log")

        self._dts = None
        self._con_man = None

    def start(self):
        super(ConfigManagerTasklet, self).start()

        self.log.debug("Registering with dts")

        self._dts = rift.tasklets.DTS(self.tasklet_info,
                                      conmanY.get_schema(),
                                      self.loop,
                                      self.on_dts_state_change)

        self.log.debug("Created DTS Api GI Object: %s", self._dts)

    def on_instance_started(self):
        self.log.debug("Got instance started callback")

    @asyncio.coroutine
    def init(self):
        self._log.info("Initializing the Configuration-Manager tasklet")
        self._con_man = ConfigurationManager(self.log,
                                             self.loop,
                                             self._dts)
        yield from self._con_man.register()

    @asyncio.coroutine
    def run(self):
        pass

    @asyncio.coroutine
    def on_dts_state_change(self, state):
        """Take action according to current dts state to transition
        application into the corresponding application state

        Arguments
            state - current dts state
        """
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
            self._dts.handle.set_state(next_state)
