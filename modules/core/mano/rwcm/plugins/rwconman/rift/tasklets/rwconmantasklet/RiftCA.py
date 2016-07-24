# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import concurrent.futures
import re
import tempfile
import yaml
import os

from gi.repository import (
    RwDts as rwdts,
)

from . import riftcm_config_plugin
from . import rwconman_events as Events

class RiftCAConfigPlugin(riftcm_config_plugin.RiftCMConfigPluginBase):
    """
        Implementation of the riftcm_config_plugin.RiftCMConfigPluginBase
    """
    def __init__(self, dts, log, loop, account):
        riftcm_config_plugin.RiftCMConfigPluginBase.__init__(self, dts, log, loop, account)
        self._name = account.name
        self._rift_install_dir = os.environ['RIFT_INSTALL']
        self._rift_artif_dir = os.environ['RIFT_ARTIFACTS']
        self._rift_vnfs = {}
        self._tasks = {}

        # Instantiate events that will handle RiftCA configuration requests
        self._events = Events.ConfigManagerEvents(dts, log, loop, self)

    @property
    def name(self):
        return self._name
        
    @asyncio.coroutine
    def notify_create_vlr(self, agent_nsr, agent_vnfr, vld, vlr):
        """
        Notification of create VL record
        """
        pass

    @asyncio.coroutine
    def is_vnf_configurable(self, agent_vnfr):
        '''
        This needs to be part of abstract class
        '''
        loop_count = 10
        while loop_count:
            loop_count -= 1
            # Set this VNF's configurability status (need method to check)
            yield from asyncio.sleep(2, loop=self._loop)

    def riftca_log(self, name, level, log_str, *args):
        getattr(self._log, level)('RiftCA:({}) {}'.format(name, log_str), *args)
        
    @asyncio.coroutine
    def notify_create_vnfr(self, agent_nsr, agent_vnfr):
        """
        Notification of create Network VNF record
        """
        # Deploy the charm if specified for the vnf
        self._log.debug("Rift config agent: create vnfr nsr={}  vnfr={}"
                        .format(agent_nsr.name, agent_vnfr.name))
        try:
            self._loop.create_task(self.is_vnf_configurable(agent_vnfr))
        except Exception as e:
            self._log.debug("Rift config agent: vnf_configuration error for VNF:%s/%s: %s",
                            agent_nsr.name, agent_vnfr.name, str(e))
            return False

        return True

    @asyncio.coroutine
    def notify_instantiate_vnfr(self, agent_nsr, agent_vnfr):
        """
        Notification of Instantiate NSR with the passed nsr id
        """
        pass

    @asyncio.coroutine
    def notify_instantiate_vlr(self, agent_nsr, agent_vnfr, vlr):
        """
        Notification of Instantiate NSR with the passed nsr id
        """
        pass

    @asyncio.coroutine
    def notify_terminate_vnfr(self, agent_nsr, agent_vnfr):
        """
        Notification of Terminate the network service
        """

    @asyncio.coroutine
    def notify_terminate_vlr(self, agent_nsr, agent_vnfr, vlr):
        """
        Notification of Terminate the virtual link
        """
        pass

    @asyncio.coroutine
    def vnf_config_primitive(self, agent_nsr, agent_vnfr, primitive, output):
        '''
        primitives support by RiftCA
        '''
        pass
        
    @asyncio.coroutine
    def apply_config(self, config, nsr, vnfr, rpc_ip):
        """ Notification on configuration of an NSR """
        pass

    @asyncio.coroutine
    def apply_ns_config(self, agent_nsr, agent_vnfrs, rpc_ip):
        """Hook: Runs the user defined script. Feeds all the necessary data
        for the script thro' yaml file.

        Args:
            rpc_ip (YangInput_Nsr_ExecNsConfigPrimitive): The input data.
            nsr (NetworkServiceRecord): Description
            vnfrs (dict): VNFR ID => VirtualNetworkFunctionRecord
        """

        def xlate(tag, tags):
            # TBD
            if tag is None or tags is None:
                return tag
            val = tag
            if re.search('<.*>', tag):
                try:
                    if tag == '<rw_mgmt_ip>':
                        val = tags['rw_mgmt_ip']
                except KeyError as e:
                    self._log.info("RiftCA: Did not get a value for tag %s, e=%s",
                                   tag, e)
            return val

        def get_meta(agent_nsr, agent_vnfrs):
            unit_names, initial_params, vnfr_index_map, vnfr_data_map = {}, {}, {}, {}

            for vnfr_id in agent_nsr.vnfr_ids:
                vnfr = agent_vnfrs[vnfr_id]
                
                # index->vnfr ref
                vnfr_index_map[vnfr.member_vnf_index] = vnfr_id
                vnfr_data_dict = dict()
                if 'mgmt_interface' in vnfr.vnfr:
                    vnfr_data_dict['mgmt_interface'] = vnfr.vnfr['mgmt_interface']
                vnfr_data_dict['connection_point'] = []
                if 'connection_point' in vnfr.vnfr:
                    for cp in vnfr.vnfr['connection_point']:
                        cp_dict = dict()
                        cp_dict['name'] = cp['name']
                        cp_dict['ip_address'] = cp['ip_address']
                        vnfr_data_dict['connection_point'].append(cp_dict)
                
                vnfr_data_map[vnfr.member_vnf_index] = vnfr_data_dict
                # Unit name
                unit_names[vnfr_id] = vnfr.name
                # Flatten the data for simplicity
                param_data = {}
                if 'initial_config_primitive' in vnfr.vnf_configuration:
                    for primitive in vnfr.vnf_configuration['initial_config_primitive']:
                        for parameter in primitive.parameter:
                            value = xlate(parameter.value, vnfr.tags)
                            param_data[parameter.name] = value

                initial_params[vnfr_id] = param_data


            return unit_names, initial_params, vnfr_index_map, vnfr_data_map

        unit_names, init_data, vnfr_index_map, vnfr_data_map = get_meta(agent_nsr, agent_vnfrs)
        # The data consists of 4 sections
        # 1. Account data
        # 2. The input passed.
        # 3. Unit names (keyed by vnfr ID).
        # 4. Initial config data (keyed by vnfr ID).
        data = dict()
        data['config_agent'] = dict(
                name=self._name,
                )
        data["rpc_ip"] = rpc_ip.as_dict()
        data["unit_names"] = unit_names
        data["init_config"] = init_data
        data["vnfr_index_map"] = vnfr_index_map
        data["vnfr_data_map"] = vnfr_data_map

        tmp_file = None
        with tempfile.NamedTemporaryFile(delete=False) as tmp_file:
            tmp_file.write(yaml.dump(data, default_flow_style=True)
                    .encode("UTF-8"))

        # Get the full path to the script
        script = ''
        if rpc_ip.user_defined_script[0] == '/':
            # The script has full path, use as is
            script = rpc_ip.user_defined_script
        else:
            script = os.path.join(self._rift_artif_dir, 'launchpad/libs', agent_nsr.nsd_id, 'scripts',
                                  rpc_ip.user_defined_script)
            self._log.debug("Rift config agent: Checking for script in %s", script)
            if not os.path.exists(script):
                self._log.debug("Rift config agent: Did not find scipt %s", script)
                script = os.path.join(self._rift_install_dir, 'usr/bin', rpc_ip.user_defined_script)

        cmd = "{} {}".format(script, tmp_file.name)
        self._log.debug("Rift config agent: Running the CMD: {}".format(cmd))

        coro = asyncio.create_subprocess_shell(cmd, loop=self._loop)
        process = yield from coro
        task = self._loop.create_task(process.wait())

        return task

    @asyncio.coroutine
    def apply_initial_config(self, agent_nsr, agent_vnfr):
        """
        Apply the initial configuration
        """
        rc = False
        self._log.debug("Rift config agent: Apply initial config to VNF:%s/%s",
                        agent_nsr.name, agent_vnfr.name)
        try:
            if agent_vnfr.id in self._rift_vnfs.keys():
                # Check if VNF instance is configurable (TBD - future)
                ### Remove this once is_vnf_configurable() is implemented
                agent_vnfr.set_to_configurable()
                if agent_vnfr.is_configurable:
                    # apply initial config for the vnfr
                    rc = yield from self._events.apply_vnf_config(agent_vnfr.vnf_cfg)
                else:
                    self._log.info("Rift config agent: VNF:%s/%s is not configurable yet!",
                                   agent_nsr.name, agent_vnfr.name)
        except Exception as e:
            self._log.error("Rift config agent: Error on initial configuration to VNF:{}/{}, e {}"
                            .format(agent_nsr.name, agent_vnfr.name, str(e)))
            
            self._log.exception(e)
            return rc

        return rc

    def is_vnfr_managed(self, vnfr_id):
        try:
            if vnfr_id in self._rift_vnfs:
                return True
        except Exception as e:
            self._log.debug("Rift config agent: Is VNFR {} managed: {}".
                            format(vnfr_id, e))
        return False

    def add_vnfr_managed(self, agent_vnfr):
        if agent_vnfr.id not in self._rift_vnfs.keys():
            self._log.info("Rift config agent: add vnfr={}/{}".format(agent_vnfr.name, agent_vnfr.id))
            self._rift_vnfs[agent_vnfr.id] = agent_vnfr

    @asyncio.coroutine
    def get_config_status(self, agent_nsr, agent_vnfr):
            if agent_vnfr.id in self._rift_vnfs.keys():
                return 'configured'
            return 'unknown'


    def get_action_status(self, execution_id):
        ''' Get the action status for an execution ID
            *** Make sure this is NOT a asyncio coroutine function ***
        '''
        return None
