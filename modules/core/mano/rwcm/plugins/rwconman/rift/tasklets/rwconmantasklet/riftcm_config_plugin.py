# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import abc

class RiftCMnsr(object):
    '''
    Agent class for NSR
    created for Agents to use objects from NSR
    '''
    def __init__(self, nsr_dict):
        self._nsr = nsr_dict
        self._vnfrs = []
        self._vnfrs_msg = []
        self._vnfr_ids = {}
        self._job_id = 0

    @property
    def name(self):
        return self._nsr['name_ref']

    @property
    def nsd_name(self):
        return self._nsr['nsd_name_ref']

    @property
    def nsd_id(self):
        return self._nsr['nsd_ref']

    @property
    def id(self):
        return self._nsr['ns_instance_config_ref']

    @property
    def nsr_dict(self):
        return self._nsr

    @property
    def job_id(self):
        ''' Get a new job id for config primitive'''
        self._job_id += 1
        return self._job_id

    @property
    def vnfrs(self):
        return self._vnfrs

    @property
    def member_vnf_index(self):
        return self._vnfr['member_vnf_index_ref']

    def add_vnfr(self, vnfr, vnfr_msg):
        if vnfr['id'] in self._vnfr_ids.keys():
            agent_vnfr = self._vnfr_ids[vnfr['id']]
        else:
            agent_vnfr = RiftCMvnfr(self.name, vnfr, vnfr_msg)
            self._vnfrs.append(agent_vnfr)
            self._vnfrs_msg.append(vnfr_msg)
            self._vnfr_ids[agent_vnfr.id] = agent_vnfr
        return agent_vnfr

    @property
    def vnfr_ids(self):
        return self._vnfr_ids

class RiftCMvnfr(object):
    '''
    Agent base class for VNFR processing
    '''
    def __init__(self, nsr_name, vnfr_dict, vnfr_msg):
        self._vnfr = vnfr_dict
        self._vnfr_msg = vnfr_msg
        self._nsr_name = nsr_name
        self._configurable = False

    @property
    def nsr_name(self):
        return self._nsr_name

    @property
    def vnfr(self):
        return self._vnfr

    @property
    def vnfr_msg(self):
        return self._vnfr_msg

    @property
    def name(self):
        return self._vnfr['short_name']

    @property
    def tags(self):
        try:
            return self._vnfr['tags']
        except KeyError:
            return None

    @property
    def id(self):
        return self._vnfr['id']

    @property
    def member_vnf_index(self):
        return self._vnfr['member_vnf_index_ref']

    @property
    def vnf_configuration(self):
        return self._vnfr['vnf_configuration']

    @property
    def xpath(self):
        """ VNFR xpath """
        return "D,/vnfr:vnfr-catalog/vnfr:vnfr[vnfr:id = '{}']".format(self.id)

    def set_to_configurable(self):
        self._configurable = True

    @property
    def is_configurable(self):
        return self._configurable

    @property
    def vnf_cfg(self):
        return self._vnfr['vnf_cfg']

class RiftCMConfigPluginBase(object):
    """
        Abstract base class for the NSM Configuration agent plugin.
        There will be single instance of this plugin for each plugin type.
    """

    def __init__(self, dts, log, loop, config_agent):
        self._dts = dts
        self._log = log
        self._loop = loop
        self._config_agent = config_agent

    @property
    def dts(self):
        return self._dts

    @property
    def log(self):
        return self._log

    @property
    def loop(self):
        return self._loop

    @property
    def nsm(self):
        return self._nsm


    @abc.abstractmethod
    @asyncio.coroutine
    def apply_config(self, agent_nsr, agent_vnfr, config, rpc_ip):
        """ Notification on configuration of an NSR """
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def apply_ns_config(self, agent_nsr, agent_vnfrs, config, rpc_ip):
        """ Notification on configuration of an NSR """
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def notify_create_vlr(self, agent_nsr, vld):
        """ Notification on creation of an VL """
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def notify_create_vnfr(self, agent_nsr, agent_vnfr):
        """ Notification on creation of an VNFR """
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def notify_instantiate_vnfr(self, agent_nsr, agent_vnfr):
        """ Notify instantiation of the virtual network function """
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def notify_instantiate_vlr(self, agent_nsr, vl):
        """ Notify instantiate of the virtual link"""
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def notify_terminate_vnfr(self, agent_nsr, agent_vnfr):
        """Notify termination of the VNF """
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def notify_terminate_vlr(self, agent_nsr, vlr):
        """Notify termination of the Virtual Link Record"""
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def apply_initial_config(self, vnfr_id, vnf):
        """Apply initial configuration"""
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def get_config_status(self, vnfr_id):
        """Get the status for the VNF"""
        pass

    @abc.abstractmethod
    def get_action_status(self, execution_id):
        """Get the action exection status"""
        pass

    @abc.abstractmethod
    @asyncio.coroutine
    def vnf_config_primitive(self, nsr_id, vnfr_id, primitive, output):
        """Apply config primitive on a VNF"""
        pass

    @abc.abstractmethod
    def is_vnfr_managed(self, vnfr_id):
        """ Check if VNR is managed by config agent """
        pass

    @abc.abstractmethod
    def add_vnfr_managed(self, agent_vnfr):
        """ Add VNR to be managed by this config agent """
        pass

    @asyncio.coroutine
    def invoke(self, method, *args):
        try:
            rc = None
            self._log.debug("Config agent plugin: method {} with args {}: {}".
                            format(method, args, self))

            # TBD - Do a better way than string compare to find invoke the method
            if method == 'notify_create_nsr':
                rc = yield from self.notify_create_nsr(args[0], args[1])
            elif method == 'notify_create_vlr':
                rc = yield from self.notify_create_vlr(args[0], args[1], args[2])
            elif method == 'notify_create_vnfr':
                rc = yield from self.notify_create_vnfr(args[0], args[1])
            elif method == 'notify_instantiate_nsr':
                rc = yield from self.notify_instantiate_nsr(args[0])
            elif method == 'notify_instantiate_vnfr':
                rc = yield from self.notify_instantiate_vnfr(args[0], args[1])
            elif method == 'notify_instantiate_vlr':
                rc = yield from self.notify_instantiate_vlr(args[0], args[1])
            elif method == 'notify_nsr_active':
                rc = yield from self.notify_nsr_active(args[0], args[1])
            elif method == 'notify_terminate_nsr':
                rc = yield from self.notify_terminate_nsr(args[0])
            elif method == 'notify_terminate_vnfr':
                rc = yield from self.notify_terminate_vnfr(args[0], args[1])
            elif method == 'notify_terminate_vlr':
                rc = yield from self.notify_terminate_vlr(args[0], args[1])
            elif method == 'apply_initial_config':
                rc = yield from self.apply_initial_config(args[0], args[1])
            elif method == 'apply_config':
                rc = yield from self.apply_config(args[0], args[1], args[2])
            elif method == 'get_config_status':
                rc = yield from self.get_config_status(args[0], args[1])
            else:
                self._log.error("Unknown method %s invoked on config agent plugin",
                                method)
        except Exception as e:
            self._log.exception(e)
        return rc
