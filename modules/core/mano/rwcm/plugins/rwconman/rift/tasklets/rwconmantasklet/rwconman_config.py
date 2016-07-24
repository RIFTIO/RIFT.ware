
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import os
import stat
import subprocess
import sys
import tempfile
import yaml

from gi.repository import (
    RwDts as rwdts,
    RwConmanYang as conmanY,
    ProtobufC,
)

import rift.tasklets

from . import rwconman_conagent as conagent
from . import RiftCM_rpc
from . import riftcm_config_plugin

if sys.version_info < (3, 4, 4):
    asyncio.ensure_future = asyncio.async

def get_vnf_unique_name(nsr_name, vnfr_short_name, member_vnf_index):
    return "{}.{}.{}".format(nsr_name, vnfr_short_name, member_vnf_index)

class ConmanConfigError(Exception):
    pass


class InitialConfigError(ConmanConfigError):
    pass


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

class PretendNsm(object):
    def __init__(self, dts, log, loop, parent):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._parent = parent
        self._nsrs = {}
        self._nsr_dict = parent._nsr_dict
        self._config_agent_plugins = []
        self._nsd_msg = {}

    @property
    def nsrs(self):
        # Expensive, instead use get_nsr, if you know id.
        self._nsrs = {}
        # Update the list of nsrs (agent nsr)
        for id, nsr_obj in self._nsr_dict.items():
            self._nsrs[id] = nsr_obj.agent_nsr
        return self._nsrs

    def get_nsr(self, nsr_id):
        if nsr_id in self._nsr_dict:
            nsr_obj = self._nsr_dict[nsr_id]
            return nsr_obj._nsr
        return None

    def get_vnfr_msg(self, vnfr_id, nsr_id=None):
        self._log.debug("get_vnfr_msg(vnfr=%s, nsr=%s)",
                        vnfr_id, nsr_id)
        found = False
        if nsr_id:
            if nsr_id in self._nsr_dict:
                nsr_obj = self._nsr_dict[nsr_id]
                if vnfr_id in nsr_obj._vnfr_dict:
                    found = True
        else:
            for nsr_obj in self._nsr_dict.values():
                if vnfr_id in nsr_obj._vnfr_dict:
                    # Found it
                    found = True
                    break
        if found:
            vnf_cfg = nsr_obj._vnfr_dict[vnfr_id]['vnf_cfg']
            return vnf_cfg['agent_vnfr'].vnfr_msg
        else:
            return None

    @asyncio.coroutine
    def get_nsd(self, nsr_id):
        if nsr_id not in self._nsd_msg:
            nsr_config = yield from self._parent.cmdts_obj.get_nsr_config(nsr_id)
            self._nsd_msg[nsr_id] = yield from self._parent.cmdts_obj.get_nsd_msg(nsr_config.nsd_ref)
        return self._nsd_msg[nsr_id]

    @property
    def config_agent_plugins(self):
        self._config_agent_plugins = []
        for agent in self._parent._config_agent_mgr._plugin_instances.values():
            self._config_agent_plugins.append(agent)
        return self._config_agent_plugins

class ConfigManagerConfig(object):
    def __init__(self, dts, log, loop, parent):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._parent = parent
        self._nsr_dict = {}
        self.pending_cfg = {}
        self.terminate_cfg = {}
        self.pending_tasks = [] # User for NSRid get retry
                                # (mainly excercised at restart case)
        self._config_xpath = "C,/cm-config"
        self._opdata_xpath = "D,/rw-conman:cm-state"

        self.cm_config = conmanY.SoConfig()
        # RO specific configuration
        self.ro_config = {}
        for key in self.cm_config.ro_endpoint.fields:
            self.ro_config[key] = None

        # Initialize cm-state
        self.cm_state = {}
        self.cm_state['cm_nsr'] = []
        self.cm_state['states'] = "Initialized"

        # Initialize objects to register
        self.cmdts_obj = ConfigManagerDTS(self._log, self._loop, self, self._dts)
        self._config_agent_mgr = conagent.RiftCMConfigAgent(
            self._dts,
            self._log,
            self._loop,
            self,
        )
        self.reg_handles = [
            self.cmdts_obj,
            self._config_agent_mgr,
            RiftCM_rpc.RiftCMRPCHandler(self._dts, self._log, self._loop,
                                        PretendNsm(
                                            self._dts, self._log, self._loop, self)),
        ]

    def is_nsr_valid(self, nsr_id):
        if nsr_id in self._nsr_dict:
            return True
        return False

    def add_to_pending_tasks(self, task):
        if self.pending_tasks:
            for p_task in self.pending_tasks:
                if p_task['nsrid'] == task['nsrid']:
                    # Already queued
                    return
        try:
            self.pending_tasks.append(task)
            self._log.debug("add_to_pending_tasks (nsrid:%s)",
                            task['nsrid'])
            if len(self.pending_tasks) == 1:
                self._loop.create_task(self.ConfigManagerConfig_pending_loop())
                # TBD - change to info level
                self._log.debug("Started pending_loop!")
        except Exception as e:
            self._log.error("Failed adding to pending tasks (%s)", str(e))

    def del_from_pending_tasks(self, task):
        try:
            self.pending_tasks.remove(task)
        except Exception as e:
            self._log.error("Failed removing from pending tasks (%s)", str(e))

    @asyncio.coroutine
    def ConfigManagerConfig_pending_loop(self):
        loop_sleep = 2
        while True:
            yield from asyncio.sleep(loop_sleep, loop=self._loop)
            """
            This pending task queue is ordred by events,
            must finish previous task successfully to be able to go on to the next task
            """
            if self.pending_tasks:
                self._log.debug("self.pending_tasks len=%s", len(self.pending_tasks))
                task = self.pending_tasks[0]
                done = False
                if 'nsrid' in task:
                    nsrid = task['nsrid']
                    self._log.debug("Will execute pending task for NSR id(%s)", nsrid)
                    try:
                        # Try to configure this NSR
                        task['retries'] -= 1
                        done = yield from self.config_NSR(nsrid)
                        self._log.info("self.config_NSR status=%s", done)

                    except Exception as e:
                        self._log.error("Failed(%s) configuring NSR(%s)," \
                                        "retries remained:%d!",
                                        str(e), nsrid, task['retries'])
                    finally:
                        self.pending_tasks.remove(task)

                    if done:
                        self._log.debug("Finished pending task NSR id(%s):", nsrid)
                    else:
                        self._log.error("Failed configuring NSR(%s), retries remained:%d!",
                                        nsrid, task['retries'])

                        # Failed, re-insert (append at the end)
                        # this failed task to be retried later
                        # If any retries remained.
                        if task['retries']:
                            self.pending_tasks.append(task)
            else:
                self._log.debug("Stopped pending_loop!")
                break

    @asyncio.coroutine
    def register(self):
        yield from self.register_cm_state_opdata()

        # Initialize all handles that needs to be registered
        for reg in self.reg_handles:
            yield from reg.register()
        
    @asyncio.coroutine
    def register_cm_state_opdata(self):

        def state_to_string(state):
            state_dict = {
                conmanY.RecordState.INIT : "init",
                conmanY.RecordState.RECEIVED : "received",
                conmanY.RecordState.CFG_PROCESS : "cfg_process",
                conmanY.RecordState.CFG_PROCESS_FAILED : "cfg_process_failed",
                conmanY.RecordState.CFG_SCHED : "cfg_sched",
                conmanY.RecordState.CFG_DELAY : "cfg_delay",
                conmanY.RecordState.CONNECTING : "connecting",
                conmanY.RecordState.FAILED_CONNECTION : "failed_connection",
                conmanY.RecordState.NETCONF_CONNECTED : "netconf_connected",
                conmanY.RecordState.NETCONF_SSH_CONNECTED : "netconf_ssh_connected",
                conmanY.RecordState.RESTCONF_CONNECTED : "restconf_connected",
                conmanY.RecordState.CFG_SEND : "cfg_send",
                conmanY.RecordState.CFG_FAILED : "cfg_failed",
                conmanY.RecordState.READY_NO_CFG : "ready_no_cfg",
                conmanY.RecordState.READY : "ready",
                }
            return state_dict[state]

        @asyncio.coroutine
        def on_prepare(xact_info, action, ks_path, msg):

            self._log.debug("Received cm-state: msg=%s, action=%s", msg, action)

            if action == rwdts.QueryAction.READ:
                show_output = conmanY.CmOpdata()
                show_output.from_dict(self.cm_state)
                self._log.debug("Responding to SHOW cm-state: %s", self.cm_state)
                xact_info.respond_xpath(rwdts.XactRspCode.ACK,
                                        xpath=self._opdata_xpath,
                                        msg=show_output)
            else:
                xact_info.respond_xpath(rwdts.XactRspCode.ACK)

        self._log.info("Registering for cm-opdata xpath: %s",
                        self._opdata_xpath)

        try:
            handler=rift.tasklets.DTS.RegistrationHandler(on_prepare=on_prepare)
            yield from self._dts.register(xpath=self._opdata_xpath,
                                          handler=handler,
                                          flags=rwdts.Flag.PUBLISHER)
            self._log.info("Successfully registered for opdata(%s)", self._opdata_xpath)
        except Exception as e:
            self._log.error("Failed to register for opdata as (%s)", e)

    @asyncio.coroutine
    def process_nsd_vnf_configuration(self, nsr_obj, vnfr):

        def get_config_method(vnf_config):
            cfg_types = ['netconf', 'juju', 'script']
            for method in cfg_types:
                if method in vnf_config:
                    return method
            return None
            
        def get_cfg_file_extension(method,  configuration_options):
            ext_dict = {
                "netconf" : "xml",
                "script" : {
                    "bash" : "sh",
                    "expect" : "exp",
                },
                "juju" : "yml"
            }

            if method == "netconf":
                return ext_dict[method]
            elif method == "script":
                return ext_dict[method][configuration_options['script_type']]
            elif method == "juju":
                return ext_dict[method]
            else:
                return "cfg"

        # This is how the YAML file should look like,
        # This routine will be called for each VNF, so keep appending the file.
        # priority order is determined by the number,
        # hence no need to generate the file in that order. A dictionary will be
        # used that will take care of the order by number.
        '''
        1 : <== This is priority
          name : trafsink_vnfd
          member_vnf_index : 2
          configuration_delay : 120
          configuration_type : netconf
          configuration_options :
            username : admin
            password : admin
            port : 2022
            target : running
        2 :
          name : trafgen_vnfd
          member_vnf_index : 1
          configuration_delay : 0
          configuration_type : netconf
          configuration_options :
            username : admin
            password : admin
            port : 2022
            target : running
        '''

        # Save some parameters needed as short cuts in flat structure (Also generated)
        vnf_cfg = vnfr['vnf_cfg']
        # Prepare unique name for this VNF
        vnf_cfg['vnf_unique_name'] = get_vnf_unique_name(
            vnf_cfg['nsr_name'], vnfr['short_name'], vnfr['member_vnf_index_ref'])

        nsr_obj.cfg_path_prefix = '{}/{}_{}'.format(
            nsr_obj.this_nsr_dir, vnfr['short_name'], vnfr['member_vnf_index_ref'])
        nsr_vnfr = '{}/{}_{}'.format(
            vnf_cfg['nsr_name'], vnfr['short_name'], vnfr['member_vnf_index_ref'])

        # Get vnf_configuration from vnfr
        vnf_config = vnfr['vnf_configuration']

        self._log.debug("vnf_configuration = %s", vnf_config)

        # Create priority dictionary
        cfg_priority_order = 0
        if ('config_attributes' in vnf_config and
            'config_priority' in vnf_config['config_attributes']):
            cfg_priority_order = vnf_config['config_attributes']['config_priority']

        if cfg_priority_order not in nsr_obj.nsr_cfg_config_attributes_dict:
            # No VNFR with this priority yet, initialize the list
            nsr_obj.nsr_cfg_config_attributes_dict[cfg_priority_order] = []

        method = get_config_method(vnf_config)
        if method is not None:
            # Create all sub dictionaries first
            config_priority = {
                'id' : vnfr['id'],
                'name' : vnfr['short_name'],
                'member_vnf_index' : vnfr['member_vnf_index_ref'],
            }

            if 'config_delay' in vnf_config['config_attributes']:
                config_priority['configuration_delay'] = vnf_config['config_attributes']['config_delay']
                vnf_cfg['config_delay'] = config_priority['configuration_delay']

            configuration_options = {}
            self._log.debug("config method=%s", method)
            config_priority['configuration_type'] = method
            vnf_cfg['config_method'] = method

            # Set config agent based on method
            self._config_agent_mgr.set_config_agent(
                nsr_obj.agent_nsr, vnf_cfg['agent_vnfr'], method)

            cfg_opt_list = [
                'port', 'target', 'script_type', 'ip_address', 'user', 'secret',
            ]
            for cfg_opt in cfg_opt_list:
                if cfg_opt in vnf_config[method]:
                    configuration_options[cfg_opt] = vnf_config[method][cfg_opt]
                    vnf_cfg[cfg_opt] = configuration_options[cfg_opt]

            cfg_opt_list = ['mgmt_ip_address', 'username', 'password']
            for cfg_opt in cfg_opt_list:
                if cfg_opt in vnf_config['config_access']:
                    configuration_options[cfg_opt] = vnf_config['config_access'][cfg_opt]
                    vnf_cfg[cfg_opt] = configuration_options[cfg_opt]

            # Add to the cp_dict
            vnf_cp_dict = nsr_obj._cp_dict[vnfr['member_vnf_index_ref']]
            vnf_cp_dict['rw_mgmt_ip'] = vnf_cfg['mgmt_ip_address']
            vnf_cp_dict['rw_username'] = vnf_cfg['username']
            vnf_cp_dict['rw_password'] = vnf_cfg['password']
            

            # TBD - see if we can neatly include the config in "config_attributes" file, no need though
            #config_priority['config_template'] = vnf_config['config_template']
            # Create config file
            vnf_cfg['juju_script'] = os.path.join(self._parent.cfg_dir, 'juju_if.py')

            if 'config_template' in vnf_config:
                vnf_cfg['cfg_template'] = '{}_{}_template.cfg'.format(nsr_obj.cfg_path_prefix, config_priority['configuration_type'])
                vnf_cfg['cfg_file'] = '{}.{}'.format(nsr_obj.cfg_path_prefix, get_cfg_file_extension(method, configuration_options))
                vnf_cfg['xlate_script'] = os.path.join(self._parent.cfg_dir, 'xlate_cfg.py')
                try:
                    # Now write this template into file
                    with open(vnf_cfg['cfg_template'], "w") as cf:
                        cf.write(vnf_config['config_template'])
                except Exception as e:
                    self._log.error("Processing NSD, failed to generate configuration template : %s (Error : %s)",
                                    vnf_config['config_template'], str(e))
                    raise

            self._log.debug("VNF endpoint so far: %s", vnf_cfg)

            # Populate filled up dictionary
            config_priority['configuration_options'] = configuration_options
            nsr_obj.nsr_cfg_config_attributes_dict[cfg_priority_order].append(config_priority)
            nsr_obj.num_vnfs_to_cfg += 1
            nsr_obj._vnfr_dict[vnf_cfg['vnf_unique_name']] = vnfr
            nsr_obj._vnfr_dict[vnfr['id']] = vnfr

            self._log.debug("VNF:(%s) config_attributes = %s",
                            log_this_vnf(vnfr['vnf_cfg']),
                            nsr_obj.nsr_cfg_config_attributes_dict)
        else:
            self._log.info("VNF:(%s) is not to be configured by Configuration Manager!",
                           log_this_vnf(vnfr['vnf_cfg']))
            yield from nsr_obj.update_vnf_cm_state(vnfr, conmanY.RecordState.READY_NO_CFG)

        # Update the cm-state
        nsr_obj.populate_vm_state_from_vnf_cfg()

    @asyncio.coroutine
    def config_NSR(self, id):

        def my_yaml_dump(config_attributes_dict, yf):

            yaml_dict = dict(sorted(config_attributes_dict.items()))
            yf.write(yaml.dump(yaml_dict, default_flow_style=False))
        
        nsr_dict = self._nsr_dict
        self._log.info("Configure NSR, id = %s", id)

        #####################TBD###########################
        # yield from self._config_agent_mgr.invoke_config_agent_plugins('notify_create_nsr', self.id, self._nsd)
        # yield from self._config_agent_mgr.invoke_config_agent_plugins('notify_nsr_active', self.id, self._vnfrs)
        
        try:
            if id not in nsr_dict:
                nsr_obj = ConfigManagerNSR(self._log, self._loop, self, id)
                nsr_dict[id] = nsr_obj
            else:
                self._log.info("NSR(%s) is already initialized!", id)
                nsr_obj = nsr_dict[id]
        except Exception as e:
            self._log.error("Failed creating NSR object for (%s) as (%s)", id, str(e))
            raise

        # Try to configure this NSR only if not already processed
        if nsr_obj.cm_nsr['state'] != nsr_obj.state_to_string(conmanY.RecordState.INIT):
            self._log.debug("NSR(%s) is already processed, state=%s",
                            nsr_obj.nsr_name, nsr_obj.cm_nsr['state'])
            yield from nsr_obj.publish_cm_state()
            return True
            
        cmdts_obj = self.cmdts_obj
        try:
            # Fetch NSR
            nsr = yield from cmdts_obj.get_nsr(id)
            self._log.debug("Full NSR : %s", nsr)
            if nsr['operational_status'] != "running":
                self._log.info("NSR(%s) is not ready yet!", nsr['nsd_name_ref'])
                return False
            self._nsr = nsr

            # Create Agent NSR class
            nsr_obj.agent_nsr = riftcm_config_plugin.RiftCMnsr(nsr)

            try:
                yield from nsr_obj.update_ns_cm_state(conmanY.RecordState.RECEIVED)

                # Parse NSR
                if nsr is not None:
                    nsr_obj.set_nsr_name(nsr['nsd_name_ref'])
                    nsr_dir = os.path.join(self._parent.cfg_dir, nsr_obj.nsr_name)
                    self._log.info("Checking NS config directory: %s", nsr_dir)
                    if not os.path.isdir(nsr_dir):
                        os.makedirs(nsr_dir)
                        # self._log.critical("NS %s is not to be configured by Service Orchestrator!", nsr_obj.nsr_name)
                        # yield from nsr_obj.update_ns_cm_state(conmanY.RecordState.READY_NO_CFG)
                        # return

                    nsr_obj.set_config_dir(self)
                    
                    for const_vnfr in nsr['constituent_vnfr_ref']:
                        self._log.debug("Fetching VNFR (%s)", const_vnfr['vnfr_id'])
                        vnfr_msg = yield from cmdts_obj.get_vnfr(const_vnfr['vnfr_id'])
                        if vnfr_msg:
                            vnfr = vnfr_msg.as_dict()
                            self._log.info("create VNF:{}/{}".format(nsr_obj.nsr_name, vnfr['short_name']))
                            agent_vnfr = yield from nsr_obj.add_vnfr(vnfr, vnfr_msg)

                            # Preserve order, self.process_nsd_vnf_configuration()
                            # sets up the config agent based on the method
                            yield from self.process_nsd_vnf_configuration(nsr_obj, vnfr)
                            yield from self._config_agent_mgr.invoke_config_agent_plugins(
                                'notify_create_vnfr',
                                nsr_obj.agent_nsr,
                                agent_vnfr)

                        #####################TBD###########################
                        # self._log.debug("VNF active. Apply initial config for vnfr {}".format(vnfr.name))
                        # yield from self._config_agent_mgr.invoke_config_agent_plugins('apply_initial_config',
                        #                                             vnfr.id, vnfr)
                        # yield from self._config_agent_mgr.invoke_config_agent_plugins('notify_terminate_vnf', self.id, vnfr)

            except Exception as e:
                self._log.exception("Failed processing NSR (%s) as (%s)", nsr_obj.nsr_name, str(e))
                self._log.error("Failed processing NSR (%s) as (%s)", nsr_obj.nsr_name, str(e))
                yield from nsr_obj.update_ns_cm_state(conmanY.RecordState.CFG_PROCESS_FAILED)
                raise

            try:
                # Generate config_config_attributes.yaml (For debug reference)
                with open(nsr_obj.config_attributes_file, "w") as yf:
                    my_yaml_dump(nsr_obj.nsr_cfg_config_attributes_dict, yf)
            except Exception as e:
                self._log.error("NS:(%s) failed to write config attributes file as (%s)", nsr_obj.nsr_name, str(e))

            try:
                # Generate nsr_xlate_dict.yaml (For debug reference)
                with open(nsr_obj.xlate_dict_file, "w") as yf:
                    yf.write(yaml.dump(nsr_obj._cp_dict, default_flow_style=False))
            except Exception as e:
                self._log.error("NS:(%s) failed to write nsr xlate tags file as (%s)", nsr_obj.nsr_name, str(e))

            self._log.debug("Starting to configure each VNF")

            # Check if this NS has input parametrs
            self._log.info("Checking NS configuration order: %s", nsr_obj.config_attributes_file)

            if os.path.exists(nsr_obj.config_attributes_file):
                # Apply configuration is specified order
                try:
                    # Go in loop to configure by specified order
                    self._log.info("Using Dynamic configuration input parametrs for NS: %s", nsr_obj.nsr_name)

                    # cfg_delay = nsr_obj.nsr_cfg_config_attributes_dict['configuration_delay']
                    # if cfg_delay:
                    #     self._log.info("Applying configuration delay for NS (%s) ; %d seconds",
                    #                    nsr_obj.nsr_name, cfg_delay)
                    #     yield from asyncio.sleep(cfg_delay, loop=self._loop)

                    for config_attributes_dict in nsr_obj.nsr_cfg_config_attributes_dict.values():
                        # Iterate through each priority level
                        for vnf_config_attributes_dict in config_attributes_dict:
                            # Iterate through each vnfr at this priority level
                                
                            # Make up vnf_unique_name with vnfd name and member index
                            #vnfr_name = "{}.{}".format(nsr_obj.nsr_name, vnf_config_attributes_dict['name'])
                            vnf_unique_name = get_vnf_unique_name(
                                nsr_obj.nsr_name,
                                vnf_config_attributes_dict['name'],
                                str(vnf_config_attributes_dict['member_vnf_index']),
                            )
                            self._log.info("NS (%s) : VNF (%s) - Processing configuration attributes",
                                           nsr_obj.nsr_name, vnf_unique_name)

                            # Find vnfr for this vnf_unique_name
                            if vnf_unique_name not in nsr_obj._vnfr_dict:
                                self._log.error("NS (%s) - Can not find VNF to be configured: %s", nsr_obj.nsr_name, vnf_unique_name)
                            else:
                                # Save this unique VNF's config input parameters
                                nsr_obj.vnf_config_attributes_dict[vnf_unique_name] = vnf_config_attributes_dict
                                nsr_obj.ConfigVNF(nsr_obj._vnfr_dict[vnf_unique_name])

                    # Now add the entire NS to the pending config list.
                    self._log.info("Scheduling NSR:{} configuration".format(nsr_obj.nsr_name))
                    self._parent.add_to_pending(nsr_obj)

                except Exception as e:
                    self._log.error("Failed processing input parameters for NS (%s) as %s", nsr_obj.nsr_name, str(e))
                    raise
            else:
                self._log.error("No configuration input parameters for NSR (%s)", nsr_obj.nsr_name)

        except Exception as e:
            self._log.error("Failed to configure NS (%s) as (%s)", nsr_obj.nsr_name, str(e))
            yield from nsr_obj.update_ns_cm_state(conmanY.RecordState.CFG_PROCESS_FAILED)
            raise

        return True

    @asyncio.coroutine
    def terminate_NSR(self, id):
        nsr_dict = self._nsr_dict
        if id not in nsr_dict:
            self._log.error("NSR(%s) does not exist!", id)
            return
        else:
            # Remove this NSR if we have it on pending task list
            for task in self.pending_tasks:
                if task['nsrid'] == id:
                    self.del_from_pending_tasks(task)

            # Remove this object from global list
            nsr_obj = nsr_dict.pop(id, None)

            # Remove this NS cm-state from global status list
            self.cm_state['cm_nsr'].remove(nsr_obj.cm_nsr)

            # Also remove any scheduled configuration event
            for nsr_obj_p in self._parent.pending_cfg:
                if nsr_obj_p == nsr_obj:
                    assert id == nsr_obj_p._nsr_id
                    #self._parent.pending_cfg.remove(nsr_obj_p)
                    # Mark this as being deleted so we do not try to configure it if we are in cfg_delay (will wake up and continue to process otherwise)
                    nsr_obj_p.being_deleted = True
                    self._log.info("Removed scheduled configuration for NSR(%s)", nsr_obj.nsr_name)

            # Call Config Agent to clean up for each VNF
            for agent_vnfr in nsr_obj.agent_nsr.vnfrs:
                yield from self._config_agent_mgr.invoke_config_agent_plugins(
                    'notify_terminate_vnfr',
                    nsr_obj.agent_nsr,
                    agent_vnfr)

            # publish delete cm-state (cm-nsr)
            yield from nsr_obj.delete_cm_nsr()

            #####################TBD###########################
            # yield from self._config_agent_mgr.invoke_config_agent_plugins('notify_terminate_ns', self.id)

            self._log.info("NSR(%s/%s) is deleted", nsr_obj.nsr_name, id)

    @asyncio.coroutine
    def process_ns_initial_config(self, nsr_obj):
        '''Apply the initial-config-primitives specified in NSD'''

        def get_input_file(parameters):
            inp = {}

            # Add NSR name to file
            inp['nsr_name'] = nsr_obj.nsr_name

            # TODO (pjoseph): Add config agents, we need to identify which all
            # config agents are required from this NS and provide only those
            inp['config-agent'] = {}

            # Add parameters for initial config
            inp['parameter'] = {}
            for parameter in parameters:
                try:
                    inp['parameter'][parameter['name']] = parameter['value']
                except KeyError as e:
                    self._log.info("NSR {} initial config parameter {} with no value: {}".
                                    format(nsr_obj.nsr_name, parameter, e))

            # Add vnfrs specific data
            inp['vnfr'] = {}
            for vnfr in nsr_obj.vnfrs:
                v = {}

                v['name'] = vnfr['name']
                v['mgmt_ip_address'] = vnfr['vnf_cfg']['mgmt_ip_address']
                v['mgmt_port'] = vnfr['vnf_cfg']['port']

                if 'dashboard_url' in vnfr:
                    v['dashboard_url'] = vnfr['dashboard_url']

                if 'connection_point' in vnfr:
                    v['connection_point'] = []
                    for cp in vnfr['connection_point']:
                        v['connection_point'].append(
                            {
                                'name': cp['name'],
                                'ip_address': cp['ip_address'],
                            }
                        )

                inp['vnfr'][vnfr['member_vnf_index_ref']] = v

            self._log.debug("Input data for NSR {}: {}".
                            format(nsr_obj.nsr_name, inp))

            # Convert to YAML string
            yaml_string = yaml.dump(inp, default_flow_style=False)

            # Write the inputs as yaml file
            tmp_file = None
            with tempfile.NamedTemporaryFile(delete=False) as tmp_file:
                tmp_file.write(yaml_string.encode("UTF-8"))
            self._log.debug("Input file created for NSR {}: {}".
                            format(nsr_obj.nsr_name, tmp_file.name))

            return tmp_file.name

        def get_script_file(script_name, nsd_name, nsd_id):
            # Get the full path to the script
            script = ''
            # If script name starts with /, assume it is full path
            if script_name[0] == '/':
                # The script has full path, use as is
                script = script_name
            else:
                script = os.path.join(os.environ['RIFT_ARTIFACTS'],
                                      'launchpad/packages/nsd',
                                      nsd_id,
                                      nsd_name,
                                      'scripts',
                                      script_name)
                self._log.debug("Checking for script at %s", script)
                if not os.path.exists(script):
                    self._log.debug("Did not find script %s", script)
                    script = os.path.join(os.environ['RIFT_INSTALL'],
                                          'usr/bin',
                                          script_name)

                # Seen cases in jenkins, where the script execution fails
                # with permission denied. Setting the permission on script
                # to make sure it has execute permission
                perm = os.stat(script).st_mode
                if not (perm  &  stat.S_IXUSR):
                    self._log.warn("NSR {} initial config script {} " \
                                  "without execute permission: {}".
                                  format(nsr_id, script, perm))
                    os.chmod(script, perm | stat.S_IXUSR)
                return script

        nsr_id = nsr_obj.nsr_id
        nsr_name = nsr_obj.nsr_name
        self._log.debug("Apply initial config for NSR {}({})".
                        format(nsr_name, nsr_id))

        # Fetch NSR
        nsr = yield from self.cmdts_obj.get_nsr(nsr_id)
        if nsr is not None:
            nsd = yield from self.cmdts_obj.get_nsd(nsr_id)

            try:
                # Check if initial config is present
                # TODO (pjoseph): Sort based on seq
                for conf in nsr['initial_config_primitive']:
                    self._log.debug("Parameter conf: {}".
                                    format(conf))

                    parameters = []
                    try:
                        parameters = conf['parameter']
                    except Exception as e:
                        self._log.debug("Parameter conf: {}, e: {}".
                                        format(conf, e))
                        pass

                    inp_file = get_input_file(parameters)

                    script = get_script_file(conf['user_defined_script'],
                                             nsd.name,
                                             nsd.id)

                    cmd = "{0} {1}".format(script, inp_file)
                    self._log.debug("Running the CMD: {}".format(cmd))

                    process = yield from asyncio. \
                              create_subprocess_shell(cmd, loop=self._loop)
                    yield from process.wait()
                    if process.returncode:
                        msg = "NSR {} initial config using {} failed with {}". \
                              format(nsr_name, script, process.returncode)
                        self._log.error(msg)
                        raise InitialConfigError(msg)
                    else:
                        os.remove(inp_file)

            except KeyError as e:
                self._log.debug("Did not find initial config {}".
                                format(e))


class ConfigManagerNSR(object):
    def __init__(self, log, loop, parent, id):
        self._log = log
        self._loop = loop
        self._rwcal = None
        self._vnfr_dict = {}
        self._cp_dict = {}
        self._nsr_id = id
        self._parent = parent
        self._log.info("Instantiated NSR entry for id=%s", id)
        self.nsr_cfg_config_attributes_dict = {}
        self.vnf_config_attributes_dict = {}
        self.num_vnfs_to_cfg = 0
        self._vnfr_list = []
        self.vnf_cfg_list = []
        self.this_nsr_dir = None
        self.being_deleted = False
        self.dts_obj = self._parent.cmdts_obj

        # Initialize cm-state for this NS
        self.cm_nsr = {}
        self.cm_nsr['cm_vnfr'] = []
        self.cm_nsr['id'] = id
        self.cm_nsr['state'] = self.state_to_string(conmanY.RecordState.INIT)

        self.set_nsr_name('Not Set')

        # Add this NSR cm-state object to global cm-state
        parent.cm_state['cm_nsr'].append(self.cm_nsr)

        # Place holders for NSR & VNFR classes
        self.agent_nsr = None

    @property
    def nsr_opdata_xpath(self):
        ''' Returns full xpath for this NSR cm-state opdata '''
        return(
            "D,/rw-conman:cm-state" +
            "/rw-conman:cm-nsr[rw-conman:id='{}']"
        ).format(self._nsr_id)

    @property
    def vnfrs(self):
        return self._vnfr_list

    @property
    def parent(self):
        return self._parent

    @property
    def nsr_id(self):
        return self._nsr_id

    @asyncio.coroutine
    def publish_cm_state(self):
        ''' This function publishes cm_state for this NSR '''

        cm_state = conmanY.CmOpdata()
        cm_state_nsr = cm_state.cm_nsr.add()
        cm_state_nsr.from_dict(self.cm_nsr)
        #with self._dts.transaction() as xact:
        yield from self.dts_obj.update(self.nsr_opdata_xpath, cm_state_nsr)
        self._log.info("Published cm-state with xpath %s and nsr %s",
                       self.nsr_opdata_xpath,
                       cm_state_nsr)

    @asyncio.coroutine
    def delete_cm_nsr(self):
        ''' This function publishes cm_state for this NSR '''

        yield from self.dts_obj.delete(self.nsr_opdata_xpath)
        self._log.info("Deleted cm-nsr with xpath %s",
                       self.nsr_opdata_xpath)

    def set_nsr_name(self, name):
        self.nsr_name = name
        self.cm_nsr['name'] = name

    def set_config_dir(self, caller):
        self.this_nsr_dir = os.path.join(
            caller._parent.cfg_dir, self.nsr_name, caller._nsr['name_ref'])
        if not os.path.exists(self.this_nsr_dir):
            os.makedirs(self.this_nsr_dir)
            self._log.debug("NSR:(%s), Created configuration directory(%s)",
                            caller._nsr['name_ref'], self.this_nsr_dir)
        self.config_attributes_file = os.path.join(self.this_nsr_dir, "configuration_config_attributes.yml")
        self.xlate_dict_file = os.path.join(self.this_nsr_dir, "nsr_xlate_dict.yml")
        
    def xlate_conf(self, vnfr, vnf_cfg):

        # If configuration type is not already set, try to read from attributes
        if vnf_cfg['interface_type'] is None:
            # Prepare unique name for this VNF
            vnf_unique_name = get_vnf_unique_name(
                    vnf_cfg['nsr_name'],
                    vnfr['short_name'],
                    vnfr['member_vnf_index_ref'],
                    )

            # Find this particular (unique) VNF's config attributes
            if (vnf_unique_name in self.vnf_config_attributes_dict):
                vnf_cfg_config_attributes_dict = self.vnf_config_attributes_dict[vnf_unique_name]
                vnf_cfg['interface_type'] = vnf_cfg_config_attributes_dict['configuration_type']
                if 'configuration_options' in vnf_cfg_config_attributes_dict:
                    cfg_opts = vnf_cfg_config_attributes_dict['configuration_options']
                    for key, value in cfg_opts.items():
                        vnf_cfg[key] = value

        cfg_path_prefix = '{}/{}/{}_{}'.format(
                self._parent._parent.cfg_dir,
                vnf_cfg['nsr_name'],
                vnfr['short_name'],
                vnfr['member_vnf_index_ref'],
                )

        vnf_cfg['cfg_template'] = '{}_{}_template.cfg'.format(cfg_path_prefix, vnf_cfg['interface_type'])
        vnf_cfg['cfg_file'] = '{}.cfg'.format(cfg_path_prefix)
        vnf_cfg['xlate_script'] = self._parent._parent.cfg_dir + '/xlate_cfg.py'

        self._log.debug("VNF endpoint so far: %s", vnf_cfg)

        self._log.info("Checking cfg_template %s", vnf_cfg['cfg_template'])
        if os.path.exists(vnf_cfg['cfg_template']):
            return True
        return False

    def ConfigVNF(self, vnfr):

        vnf_cfg = vnfr['vnf_cfg']
        vnf_cm_state = self.find_or_create_vnfr_cm_state(vnf_cfg)

        if (vnf_cm_state['state'] == self.state_to_string(conmanY.RecordState.READY_NO_CFG)
            or
            vnf_cm_state['state'] == self.state_to_string(conmanY.RecordState.READY)):
            self._log.warning("NS/VNF (%s/%s) is already configured! Skipped.", self.nsr_name, vnfr['short_name'])
            return

        #UPdate VNF state
        vnf_cm_state['state'] = self.state_to_string(conmanY.RecordState.CFG_PROCESS)

        # Now translate the configuration for iP addresses
        try:
            # Add cp_dict members (TAGS) for this VNF
            self._cp_dict['rw_mgmt_ip'] = vnf_cfg['mgmt_ip_address']
            self._cp_dict['rw_username'] = vnf_cfg['username']
            self._cp_dict['rw_password'] = vnf_cfg['password']
            ############################################################
            # TBD - Need to lookup above 3 for a given VNF, not global #
            # Once we do that no need to dump below file again before  #
            # each VNF configuration translation.                      #
            # This will require all existing config templates to be    #
            # changed for above three tags to include member index     #
            ############################################################
            try:
                nsr_obj = vnf_cfg['nsr_obj']
                # Generate config_config_attributes.yaml (For debug reference)
                with open(nsr_obj.xlate_dict_file, "w") as yf:
                    yf.write(yaml.dump(nsr_obj._cp_dict, default_flow_style=False))
            except Exception as e:
                self._log.error("NS:(%s) failed to write nsr xlate tags file as (%s)", nsr_obj.nsr_name, str(e))
            
            if 'cfg_template' in vnf_cfg:
                script_cmd = 'python3 {} -i {} -o {} -x "{}"'.format(vnf_cfg['xlate_script'], vnf_cfg['cfg_template'], vnf_cfg['cfg_file'], self.xlate_dict_file)
                self._log.debug("xlate script command (%s)", script_cmd)
                #xlate_msg = subprocess.check_output(script_cmd).decode('utf-8')
                xlate_msg = subprocess.check_output(script_cmd, shell=True).decode('utf-8')
                self._log.info("xlate script output (%s)", xlate_msg)
        except Exception as e:
            vnf_cm_state['state'] = self.state_to_string(conmanY.RecordState.CFG_PROCESS_FAILED)
            self._log.error("Failed to execute translation script for VNF: %s with (%s)", log_this_vnf(vnf_cfg), str(e))
            return

        self._log.info("Applying config to VNF: %s = %s!", log_this_vnf(vnf_cfg), vnf_cfg)
        try:
            #self.vnf_cfg_list.append(vnf_cfg)
            self._log.debug("Scheduled configuration!")
            vnf_cm_state['state'] = self.state_to_string(conmanY.RecordState.CFG_SCHED)
        except Exception as e:
            self._log.error("Failed apply_vnf_config to VNF: %s as (%s)", log_this_vnf(vnf_cfg), str(e))
            vnf_cm_state['state'] = self.state_to_string(conmanY.RecordState.CFG_PROCESS_FAILED)
            raise

    def add(self, nsr):
        self._log.info("Adding NS Record for id=%s", id)
        self._nsr = nsr

    def sample_cm_state(self):
        return (
            {
                'cm_nsr': [
                    {
                        'cm_vnfr': [
                            {
                                'cfg_location': 'location1',
                                'cfg_type': 'script',
                                'connection_point': [
                                    {'ip_address': '1.1.1.1', 'name': 'vnf1cp1'},
                                    {'ip_address': '1.1.1.2', 'name': 'vnf1cp2'}
                                ],
                                'id': 'vnfrid1',
                                'mgmt_interface': {'ip_address': '7.1.1.1',
                                                   'port': 1001},
                                'name': 'vnfrname1',
                                'state': 'init'
                            },
                            {
                                'cfg_location': 'location2',
                                'cfg_type': 'netconf',
                                'connection_point': [{'ip_address': '2.1.1.1', 'name': 'vnf2cp1'},
                                                     {'ip_address': '2.1.1.2', 'name': 'vnf2cp2'}],
                                'id': 'vnfrid2',
                                'mgmt_interface': {'ip_address': '7.1.1.2',
                                                   'port': 1001},
                                'name': 'vnfrname2',
                                'state': 'init'}
                        ],
                        'id': 'nsrid1',
                        'name': 'nsrname1',
                        'state': 'init'}
                ],
                'states': 'Initialized, '
            })

    def populate_vm_state_from_vnf_cfg(self):
        # Fill in each VNFR from this nsr object
        vnfr_list = self._vnfr_list
        for vnfr in vnfr_list:
            vnf_cfg = vnfr['vnf_cfg']
            vnf_cm_state = self.find_vnfr_cm_state(vnfr['id'])

            if vnf_cm_state:
                # Fill in VNF management interface
                vnf_cm_state['mgmt_interface']['ip_address'] = vnf_cfg['mgmt_ip_address']
                vnf_cm_state['mgmt_interface']['port'] = vnf_cfg['port']

                # Fill in VNF configuration details
                vnf_cm_state['cfg_type'] = vnf_cfg['config_method']
                vnf_cm_state['cfg_location'] = vnf_cfg['cfg_file']

                # Fill in each connection-point for this VNF
                if "connection_point" in vnfr:
                    cp_list = vnfr['connection_point']
                    for cp_item_dict in cp_list:
                        vnf_cm_state['connection_point'].append(
                            {
                                'name' : cp_item_dict['name'],
                                'ip_address' : cp_item_dict['ip_address'],
                            }
                        )

    def state_to_string(self, state):
        state_dict = {
            conmanY.RecordState.INIT : "init",
            conmanY.RecordState.RECEIVED : "received",
            conmanY.RecordState.CFG_PROCESS : "cfg_process",
            conmanY.RecordState.CFG_PROCESS_FAILED : "cfg_process_failed",
            conmanY.RecordState.CFG_SCHED : "cfg_sched",
            conmanY.RecordState.CFG_DELAY : "cfg_delay",
            conmanY.RecordState.CONNECTING : "connecting",
            conmanY.RecordState.FAILED_CONNECTION : "failed_connection",
            conmanY.RecordState.NETCONF_CONNECTED : "netconf_connected",
            conmanY.RecordState.NETCONF_SSH_CONNECTED : "netconf_ssh_connected",
            conmanY.RecordState.RESTCONF_CONNECTED : "restconf_connected",
            conmanY.RecordState.CFG_SEND : "cfg_send",
            conmanY.RecordState.CFG_FAILED : "cfg_failed",
            conmanY.RecordState.READY_NO_CFG : "ready_no_cfg",
            conmanY.RecordState.READY : "ready",
        }
        return state_dict[state]

    def find_vnfr_cm_state(self, id):
        if self.cm_nsr['cm_vnfr']:
            for vnf_cm_state in self.cm_nsr['cm_vnfr']:
                if vnf_cm_state['id'] == id:
                    return vnf_cm_state
        return None

    def find_or_create_vnfr_cm_state(self, vnf_cfg):
        vnfr = vnf_cfg['vnfr']
        vnf_cm_state = self.find_vnfr_cm_state(vnfr['id'])

        if vnf_cm_state is None:
            # Not found, Create and Initialize this VNF cm-state
            vnf_cm_state = {
                'id' : vnfr['id'],
                'name' : vnfr['short_name'],
                'state' : self.state_to_string(conmanY.RecordState.RECEIVED),
                'mgmt_interface' :
                {
                    'ip_address' : vnf_cfg['mgmt_ip_address'],
                    'port' : vnf_cfg['port'],
                },
                'cfg_type' : vnf_cfg['config_method'],
                'cfg_location' : vnf_cfg['cfg_file'],
                'connection_point' : [],
            }
            self.cm_nsr['cm_vnfr'].append(vnf_cm_state)

            # Publish newly created cm-state


        return vnf_cm_state

    @asyncio.coroutine
    def get_vnf_cm_state(self, vnfr):
        if vnfr:
            vnf_cm_state = self.find_vnfr_cm_state(vnfr['id'])
            if vnf_cm_state:
                return vnf_cm_state['state']
        return False

    @asyncio.coroutine
    def update_vnf_cm_state(self, vnfr, state):
        if vnfr:
            vnf_cm_state = self.find_vnfr_cm_state(vnfr['id'])
            if vnf_cm_state is None:
                self._log.error("No opdata found for NS/VNF:%s/%s!",
                                self.nsr_name, vnfr['short_name'])
                return

            if vnf_cm_state['state'] != self.state_to_string(state):
                old_state = vnf_cm_state['state']
                vnf_cm_state['state'] = self.state_to_string(state)
                # Publish new state
                yield from self.publish_cm_state()
                self._log.info("VNF ({}/{}/{}) state change: {} -> {}"
                               .format(self.nsr_name,
                                       vnfr['short_name'],
                                       vnfr['member_vnf_index_ref'],
                                       old_state,
                                       vnf_cm_state['state']))

        else:
            self._log.error("No VNFR supplied for state update (NS=%s)!",
                            self.nsr_name)

    @property
    def get_ns_cm_state(self):
        return self.cm_nsr['state']

    @asyncio.coroutine
    def update_ns_cm_state(self, state):
        if self.cm_nsr['state'] != self.state_to_string(state):
            old_state = self.cm_nsr['state']
            self.cm_nsr['state'] = self.state_to_string(state)
            self._log.info("NS ({}) state change: {} -> {}"
                           .format(self.nsr_name,
                                   old_state,
                                   self.cm_nsr['state']))
            # Publish new state
            yield from self.publish_cm_state()

    @asyncio.coroutine
    def add_vnfr(self, vnfr, vnfr_msg):

        @asyncio.coroutine
        def populate_subnets_from_vlr(id):
            try:
                # Populate cp_dict with VLR subnet info
                vlr = yield from self.dts_obj.get_vlr(id)
                if vlr is not None and 'assigned_subnet' in vlr:
                    subnet = {vlr.name:vlr.assigned_subnet}
                    self._cp_dict[vnfr['member_vnf_index_ref']].update(subnet)
                    self._cp_dict.update(subnet)
                    self._log.debug("VNF:(%s) Updated assigned subnet = %s",
                                    vnfr['short_name'], subnet)
            except Exception as e:
                self._log.error("VNF:(%s) VLR Error = %s",
                                vnfr['short_name'], e)
            
        if vnfr['id'] not in self._vnfr_dict:
            self._log.info("NSR(%s) : Adding VNF Record for name=%s, id=%s", self._nsr_id, vnfr['short_name'], vnfr['id'])
            # Add this vnfr to the list for show, or single traversal
            self._vnfr_list.append(vnfr)
        else:
            self._log.warning("NSR(%s) : VNF Record for name=%s, id=%s already exists, overwriting", self._nsr_id, vnfr['short_name'], vnfr['id'])

        # Make vnfr available by id as well as by name
        unique_name = get_vnf_unique_name(self.nsr_name, vnfr['short_name'], vnfr['member_vnf_index_ref'])
        self._vnfr_dict[unique_name] = vnfr
        self._vnfr_dict[vnfr['id']] = vnfr

        # Create vnf_cfg dictionary with default values
        vnf_cfg = {
            'nsr_obj' : self,
            'vnfr' : vnfr,
            'agent_vnfr' : self.agent_nsr.add_vnfr(vnfr, vnfr_msg),
            'nsr_name' : self.nsr_name,
            'nsr_id' : self._nsr_id,
            'vnfr_name' : vnfr['short_name'],
            'member_vnf_index' : vnfr['member_vnf_index_ref'],
            'port' : 0,
            'username' : 'admin',
            'password' : 'admin',
            'config_method' : 'None',
            'protocol' : 'None',
            'mgmt_ip_address' : '0.0.0.0',
            'cfg_file' : 'None',
            'cfg_retries' : 0,
            'script_type' : 'bash',
        }

        # Update the mgmt ip address
        # In case the config method is none, this is not
        # updated later
        try:
            vnf_cfg['mgmt_ip_address'] = vnfr_msg.mgmt_interface.ip_address
            vnf_cfg['port'] = vnfr_msg.mgmt_interface.port
        except Exception as e:
            self._log.warn(
                "VNFR {}({}), unable to retrieve mgmt ip address: {}".
                format(vnfr['short_name'], vnfr['id'], e))

        vnfr['vnf_cfg'] = vnf_cfg
        self.find_or_create_vnfr_cm_state(vnf_cfg)

        '''
        Build the connection-points list for this VNF (self._cp_dict)
        '''
        # Populate global CP list self._cp_dict from VNFR
        cp_list = []
        if 'connection_point' in vnfr:
            cp_list = vnfr['connection_point']

        self._cp_dict[vnfr['member_vnf_index_ref']] = {}
        if 'vdur' in vnfr:
            for vdur in vnfr['vdur']:
                if 'internal_connection_point' in vdur:
                    cp_list += vdur['internal_connection_point']

                for cp_item_dict in cp_list:
                    # Populate global dictionary
                    self._cp_dict[
                        cp_item_dict['name']
                    ] = cp_item_dict['ip_address']

                    # Populate unique member specific dictionary
                    self._cp_dict[
                        vnfr['member_vnf_index_ref']
                    ][
                        cp_item_dict['name']
                    ] = cp_item_dict['ip_address']

                    # Fill in the subnets from vlr
                    if 'vlr_ref' in cp_item_dict:
                        ### HACK: Internal connection_point do not have VLR reference
                        yield from populate_subnets_from_vlr(cp_item_dict['vlr_ref'])

        if 'internal_vlr' in vnfr:
            for ivlr in vnfr['internal_vlr']:
                yield from populate_subnets_from_vlr(ivlr['vlr_ref'])
                
        # Update vnfr
        vnf_cfg['agent_vnfr']._vnfr = vnfr
        return vnf_cfg['agent_vnfr']


class XPaths(object):
    @staticmethod
    def nsr_opdata(k=None):
        return ("D,/nsr:ns-instance-opdata/nsr:nsr" +
                ("[nsr:ns-instance-config-ref='{}']".format(k) if k is not None else ""))

    @staticmethod
    def nsd_msg(k=None):
        return ("C,/nsd:nsd-catalog/nsd:nsd" +
                "[nsd:id = '{}']".format(k) if k is not None else "")

    @staticmethod
    def vnfr_opdata(k=None):
        return ("D,/vnfr:vnfr-catalog/vnfr:vnfr" +
                ("[vnfr:id='{}']".format(k) if k is not None else ""))

    @staticmethod
    def config_agent(k=None):
        return ("D,/rw-config-agent:config-agent/rw-config-agent:account" +
                ("[rw-config-agent:name='{}']".format(k) if k is not None else ""))

    @staticmethod
    def nsr_config(k=None):
        return ("C,/nsr:ns-instance-config/nsr:nsr[nsr:id='{}']".format(k) if k is not None else "")

    @staticmethod
    def vlr(k=None):
        return ("D,/vlr:vlr-catalog/vlr:vlr[vlr:id='{}']".format(k) if k is not None else "")

class ConfigManagerDTS(object):
    ''' This class either reads from DTS or publishes to DTS '''

    def __init__(self, log, loop, parent, dts):
        self._log = log
        self._loop = loop
        self._parent = parent
        self._dts = dts

    @asyncio.coroutine
    def _read_dts(self, xpath, do_trace=False):
        self._log.debug("_read_dts path = %s", xpath)
        flags = rwdts.XactFlag.MERGE
        flags += rwdts.XactFlag.TRACE if do_trace else 0
        res_iter = yield from self._dts.query_read(
                xpath, flags=flags
                )

        results = []
        for i in res_iter:
            result = yield from i
            if result is not None:
                results.append(result.result)

        return results


    @asyncio.coroutine
    def get_nsr(self, id):
        self._log.debug("Attempting to get NSR: %s", id)
        nsrl = yield from self._read_dts(XPaths.nsr_opdata(id), False)
        nsr = None
        if len(nsrl) > 0:
            nsr =  nsrl[0].as_dict()
        return nsr

    @asyncio.coroutine
    def get_nsr_config(self, id):
        self._log.debug("Attempting to get config NSR: %s", id)
        nsrl = yield from self._read_dts(XPaths.nsr_config(id), False)
        nsr = None
        if len(nsrl) > 0:
            nsr =  nsrl[0]
        return nsr

    @asyncio.coroutine
    def get_nsd_msg(self, id):
        self._log.debug("Attempting to get NSD: %s", id)
        nsdl = yield from self._read_dts(XPaths.nsd_msg(id), False)
        nsd_msg = None
        if len(nsdl) > 0:
            nsd_msg =  nsdl[0]
        return nsd_msg

    @asyncio.coroutine
    def get_nsd(self, nsr_id):
        self._log.debug("Attempting to get NSD for NSR: %s", id)
        nsr_config = yield from self.get_nsr_config(nsr_id)
        nsd_msg = yield from self.get_nsd_msg(nsr_config.nsd_ref)
        return nsd_msg

    @asyncio.coroutine
    def get_vnfr(self, id):
        self._log.debug("Attempting to get VNFR: %s", id)
        vnfrl = yield from self._read_dts(XPaths.vnfr_opdata(id), do_trace=False)
        vnfr_msg = None
        if len(vnfrl) > 0:
            vnfr_msg = vnfrl[0]
        return vnfr_msg

    @asyncio.coroutine
    def get_vlr(self, id):
        self._log.debug("Attempting to get VLR subnet: %s", id)
        vlrl = yield from self._read_dts(XPaths.vlr(id), do_trace=True)
        vlr_msg = None
        if len(vlrl) > 0:
            vlr_msg = vlrl[0]
        return vlr_msg

    @asyncio.coroutine
    def get_config_agents(self, name):
        self._log.debug("Attempting to get config_agents: %s", name)
        cfgagentl = yield from self._read_dts(XPaths.config_agent(name), False)
        return cfgagentl

    @asyncio.coroutine
    def update(self, path, msg, flags=rwdts.XactFlag.REPLACE):
        """
        Update a cm-state (cm-nsr) record in DTS with the path and message
        """
        self._log.debug("Updating cm-state %s:%s dts_pub_hdl = %s", path, msg, self.dts_pub_hdl)
        self.dts_pub_hdl.update_element(path, msg, flags)
        self._log.debug("Updated cm-state, %s:%s", path, msg)

    @asyncio.coroutine
    def delete(self, path):
        """
        Delete cm-nsr record in DTS with the path only
        """
        self._log.debug("Deleting cm-nsr %s dts_pub_hdl = %s", path, self.dts_pub_hdl)
        self.dts_pub_hdl.delete_element(path)
        self._log.debug("Deleted cm-nsr, %s", path)

    @asyncio.coroutine
    def register(self):
        yield from self.register_to_publish()
        yield from self.register_for_nsr()
        
    @asyncio.coroutine
    def register_to_publish(self):
        ''' Register to DTS for publishing cm-state opdata '''

        xpath = "D,/rw-conman:cm-state/rw-conman:cm-nsr"
        self._log.debug("Registering to publish cm-state @ %s", xpath)
        hdl = rift.tasklets.DTS.RegistrationHandler()
        with self._dts.group_create() as group:
            self.dts_pub_hdl = group.register(xpath=xpath,
                                              handler=hdl,
                                              flags=rwdts.Flag.PUBLISHER | rwdts.Flag.NO_PREP_READ)

    @property
    def nsr_xpath(self):
        return "D,/nsr:ns-instance-opdata/nsr:nsr"

    @asyncio.coroutine
    def register_for_nsr(self):
        """ Register for NSR changes """

        @asyncio.coroutine
        def on_prepare(xact_info, query_action, ks_path, msg):
            """ This NSR is created """
            self._log.debug("Received NSR instantiate on_prepare (%s:%s:%s)",
                            query_action,
                            ks_path,
                            msg)

            if (query_action == rwdts.QueryAction.UPDATE or
                query_action == rwdts.QueryAction.CREATE):
                msg_dict = msg.as_dict()
                # Update Each NSR/VNFR state)
                if ('operational_status' in msg_dict and
                    msg_dict['operational_status'] == 'running'):
                    # Add to the task list
                    self._parent.add_to_pending_tasks({'nsrid' : msg_dict['ns_instance_config_ref'], 'retries' : 5})
            elif query_action == rwdts.QueryAction.DELETE:
                nsr_id = msg.ns_instance_config_ref
                asyncio.ensure_future(self._parent.terminate_NSR(nsr_id), loop=self._loop)
            else:
                raise NotImplementedError(
                    "%s action on cm-state not supported",
                    query_action)

            xact_info.respond_xpath(rwdts.XactRspCode.ACK)

        try:
            handler = rift.tasklets.DTS.RegistrationHandler(on_prepare=on_prepare)
            self.dts_reg_hdl = yield from self._dts.register(self.nsr_xpath,
                                                             flags=rwdts.Flag.SUBSCRIBER | rwdts.Flag.DELTA_READY,
                                                             handler=handler)
        except Exception as e:
            self._log.error("Failed to register for NSR changes as %s", str(e))


