# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import asyncio
import concurrent.futures
import re
import tempfile
import yaml
import os

from . import juju_intf
from . import riftcm_config_plugin


# Charm service name accepts only a to z and -.
def get_vnf_unique_name(nsr_name, vnfr_short_name, member_vnf_index):
    name = "{}-{}-{}".format(nsr_name, vnfr_short_name, member_vnf_index)
    new_name = ''
    for c in name:
        if c.isdigit():
            c = chr(97 + int(c))
        elif not c.isalpha():
            c = "-"
        new_name += c
    return new_name.lower()

class JujuExecuteHelper(object):
    ''' Run Juju API calls that dwe do not need to wait for response '''
    def __init__(self, log, loop):
        self._log = log
        self._loop = loop
        self._executor = concurrent.futures.ThreadPoolExecutor(max_workers=1)

    @property
    def loop(self):
        return self._loop

    @property
    def log(self):
        return self._log

    @property
    def executor(self):
        return self._executor

    @asyncio.coroutine
    def deploy_service(self, api, charm, service, path):
        self.log.debug("Deploying service using {} as {} from {}".
                       format(charm, service, path))
        try:
            rc = yield from self.loop.run_in_executor(
                self.executor,
                api._deploy_service,
                charm,
                service,
                path,
            )
            self.log.info("Deploy service {} returned {}".format(service, rc))
        except Exception as e:
            self.log.error("Error deploying service {}, e={}".
                           format(service, e))
        self.log.debug("Deployed service using %s as %s " % (charm, service))

    @asyncio.coroutine
    def destroy_service(self, api, service):
        self.log.debug("Destroying service %s" % (service))
        rc = yield from self.loop.run_in_executor(
            self.executor,
            api._destroy_service,
            service,
        )
        self.log.debug("Destroyed service {} ({})".format(service, rc))


class JujuConfigPlugin(riftcm_config_plugin.RiftCMConfigPluginBase):
    """
        Juju implementation of the riftcm_config_plugin.RiftCMConfigPluginBase
    """
    def __init__(self, dts, log, loop, account):
        riftcm_config_plugin.RiftCMConfigPluginBase.__init__(self, dts, log, loop, account)
        self._name = account.name
        self._ip_address = account.juju.ip_address
        self._port = account.juju.port
        self._user = account.juju.user
        self._secret = account.juju.secret
        self._rift_install_dir = os.environ['RIFT_INSTALL']
        self._rift_artif_dir = os.environ['RIFT_ARTIFACTS']
        
        ############################################################
        # This is wrongfully overloaded with 'juju' private data.  #
        # Really need to separate agent_vnfr from juju vnfr data.  #
        # Currently, this holds agent_vnfr, which has actual vnfr, #
        # then this juju overloads actual vnfr with its own        #
        # dictionary elemetns (WRONG!!!)                           #
        self._juju_vnfs = {}
        ############################################################
        
        self._helper = JujuExecuteHelper(log, loop)
        self._tasks = {}

    @property
    def name(self):
        return self._name

    @property
    def vnfr(self, vnfr_id):
        try:
            vnfr = self._juju_vnfs[vnfr_id].vnfr
        except KeyError:
            self._log.error("jujuCA: Did not find VNFR %s in juju plugin", vnfr_id)
            return None
            
        return vnfr

    def juju_log(self, level, name, log_str, *args):
        if name is not None:
            g_log_str = 'jujuCA:({}) {}'.format(name, log_str)
        else:
            g_log_str = 'jujuCA: {}'.format(log_str)
        getattr(self._log, level)(g_log_str, *args)
        
    @asyncio.coroutine
    def _get_api(self):
        # Create an juju api instance
        try:
            self._log.debug("jujuCA: Create API for {}:{}".
                            format(self._ip_address, self._port))
            api = yield from self._loop.run_in_executor(
                None,
                juju_intf.JujuApi,
                self._log, self._ip_address,
                self._port, self._user, self._secret,
                self.loop
            )
            if not isinstance(api, juju_intf.JujuApi):
                self._log.error("jujuCA: Did not get JujuApi instance: {}".
                                format(api))
                api = None
        except Exception as e:
            self._log.critical("jujuCA: Instantiate API exception: {}".
                               format(e))
            self._log.exception(e)
            api = None

        return api

    def _get_api_blocking(self):
        # Create an juju api instance
        try:
            self._log.debug("jujuCA: Blocking create API for {}:{}".
                            format(self._ip_address, self._port))
            api = juju_intf.JujuApi(self._log, self._ip_address,
                                    self._port, self._user, self._secret,
                                    self.loop)
            if not isinstance(api, juju_intf.JujuApi):
                self._log.error("jujuCA: Did not get JujuApi instance blocking: {}".
                                format(api))
                api = None
        except Exception as e:
            self._log.critical("jujuCA: Instantiate API exception blocking: {}".
                               format(e))
            self._log.exception(e)
            api = None

        return api

    # TBD: Do a better, similar to config manager
    def xlate(self, tag, tags):
        # TBD
        if tag is None:
            return tag
        val = tag
        if re.search('<.*>', tag):
            self._log.debug("jujuCA: Xlate value %s", tag)
            try:
                if tag == '<rw_mgmt_ip>':
                    val = tags['rw_mgmt_ip']
            except KeyError as e:
                self._log.info("jujuCA: Did not get a value for tag %s, e=%s",
                               tag, e)
        return val

    @asyncio.coroutine
    def notify_create_vlr(self, agent_nsr, agent_vnfr, vld, vlr):
        """
        Notification of create VL record
        """
        return True

    @asyncio.coroutine
    def notify_create_vnfr(self, agent_nsr, agent_vnfr):
        """
        Notification of create Network VNF record
        Returns True if configured using config_agent
        """
        # Deploy the charm if specified for the vnf
        self._log.debug("jujuCA: create vnfr nsr=%s  vnfr=%s",
                        agent_nsr.name, agent_vnfr.name)
        self._log.debug("jujuCA: Config = %s",
                        agent_vnfr.vnf_configuration)
        try:
            vnf_config = agent_vnfr.vnfr_msg.vnf_configuration
            self._log.debug("jujuCA: vnf_configuration = %s", vnf_config)
            if not vnf_config.has_field('juju'):
                return False
            charm = vnf_config.juju.charm
            self._log.debug("jujuCA: charm = %s", charm)
        except Exception as e:
            self._log.Error("jujuCA: vnf_configuration error for vnfr {}: {}".
                            format(agent_vnfr.name, e))
            return False

        # Prepare unique name for this VNF
        vnf_unique_name = get_vnf_unique_name(agent_nsr.name, agent_vnfr.name, agent_vnfr.member_vnf_index)
        if vnf_unique_name in self._tasks:
            self._log.warn("jujuCA: Service %s already deployed",
                           vnf_unique_name)

        vnfr_dict = agent_vnfr.vnfr
        vnfr_dict.update({'vnf_juju_name': vnf_unique_name,
                          'charm': charm,
                          'nsr_id': agent_nsr.id,
                          'member_vnf_index': agent_vnfr.member_vnf_index,
                          'tags': {},
                          'active': False,
                          'config': vnf_config,
                          'vnfr_name' : agent_vnfr.name})
        self._log.debug("jujuCA: Charm %s for vnf %s to be deployed as %s",
                        charm, agent_vnfr.name, vnf_unique_name)

        # Find the charm directory
        try:
            path = os.path.join(self._rift_artif_dir, 'launchpad/libs', agent_vnfr.vnfr_msg.vnfd_ref, 'charms/trusty', charm)
            self._log.debug("jujuCA: Charm dir is {}".format(path))
            if not os.path.isdir(path):
                self._log.error("jujuCA: Did not find the charm directory at {}".
                                format(path))
                path = None
        except Exception as e:
            self.log.exception(e)
            return False

        try:
            if vnf_unique_name not in self._tasks:
                self._tasks[vnf_unique_name] = {}
            api = yield from self._get_api()
            if api:
                self._tasks[vnf_unique_name]['deploy'] = self.loop.create_task(
                    self._helper.deploy_service(api, charm, vnf_unique_name, path)
                )
                self._log.debug("jujuCA: Deploying service %s",
                                vnf_unique_name)
            else:
                self._log.error("jujuCA: Unable to get API for deploy")
        except Exception as e:
            self._log.error("jujuCA: Unable to deploy service {} for charm {}: {}".
                               format(vnf_unique_name, charm, e))
            self.log.exception(e)
            return False

        return True

    @asyncio.coroutine
    def notify_instantiate_vnfr(self, agent_nsr, agent_vnfr):
        """
        Notification of Instantiate NSR with the passed nsr id
        """
        return True

    @asyncio.coroutine
    def notify_instantiate_vlr(self, agent_nsr, agent_vnfr, vlr):
        """
        Notification of Instantiate NSR with the passed nsr id
        """
        return True

    @asyncio.coroutine
    def notify_terminate_nsr(self, agent_nsr, agent_vnfr):
        """
        Notification of Terminate the network service
        """
        return True

    @asyncio.coroutine
    def notify_terminate_vnfr(self, agent_nsr, agent_vnfr):
        """
        Notification of Terminate the network service
        """
        self._log.debug("jujuCA: Terminate VNFr {}, current vnfrs={}".
                        format(agent_vnfr.name, self._juju_vnfs))
        try:
            api = yield from self._get_api()
            vnfr = agent_vnfr.vnfr
            service = vnfr['vnf_juju_name']
            if api:
                self._log.debug ("jujuCA: Terminating VNFr %s, %s",
                                 agent_vnfr.name, service)
                self._tasks[service]['destroy'] = self.loop.create_task(
                    self._helper.destroy_service(api, service)
                )
            else:
                self._log.error("Juju: Unable to get API for terminate")
            del self._juju_vnfs[agent_vnfr.id]
            self._log.debug ("jujuCA: current vnfrs={}".
                             format(self._juju_vnfs))
            if service in self._tasks:
                tasks = []
                for action in self._tasks[service].keys():
                    #if self.check_task_status(service, action):
                    tasks.append(action)
                del tasks
        except KeyError as e:
            self._log.debug ("jujuCA: Termiating charm service for VNFr {}, e={}".
                             format(agent_vnfr.name, e))
        except Exception as e:
            self._log.error("jujuCA: Exception terminating charm service for VNFR {}: {}".
                            format(agent_vnfr.name, e))

        return True

    @asyncio.coroutine
    def notify_terminate_vlr(self, agent_nsr, agent_vnfr, vlr):
        """
        Notification of Terminate the virtual link
        """
        return True

    def check_task_status(self, service, action):
        #self.log.debug("jujuCA: check task status for %s, %s" % (service, action))
        try:
            task = self._tasks[service][action]
            if task.done():
                self.log.debug("jujuCA: Task for %s, %s done" % (service, action))
                e = task.exception()
                if e:
                    self.log.error("jujuCA: Error in task for {} and {} : {}".
                                   format(service, action, e))
                r= task.result()
                if r:
                    self.log.debug("jujuCA: Task for {} and {}, returned {}".
                                   format(service, action,r))
                return True
            else:
                self.log.debug("jujuCA: task {}, {} not done".
                               format(service, action))
                return False
        except KeyError as e:
            self.log.error("jujuCA: KeyError for task for {} and {}: {}".
                           format(service, action, e))
        except Exception as e:
            self.log.error("jujuCA: Error for task for {} and {}: {}".
                           format(service, action, e))
        return True

    @asyncio.coroutine
    def vnf_config_primitive(self, nsr_id, vnfr_id, primitive, output):
        self._log.debug("jujuCA: VNF config primititve {} for nsr {}, vnfr_id {}".
                        format(primitive, nsr_id, vnfr_id))
        output.execution_status = "failed"
        output.execution_id = ''
        api = None
        try:
            vnfr = self._juju_vnfs[vnfr_id].vnfr
        except KeyError:
            self._log.error("jujuCA: Did not find VNFR %s in juju plugin",
                            vnfr_id)
            return

        try:
            service = vnfr['vnf_juju_name']
            vnf_config = vnfr['config']
            self._log.debug("VNF config %s", vnf_config)
            configs = vnf_config.config_primitive
            for config in configs:
                if config.name == primitive.name:
                    self._log.debug("jujuCA: Found the config primitive %s",
                                    config.name)
                    params = {}
                    for parameter in primitive.parameter:
                        if parameter.value:
                            val = self.xlate(parameter.value, vnfr['tags'])
                            # TBD do validation of the parameters
                            data_type = 'string'
                            found = False
                            for ca_param in config.parameter:
                                if ca_param.name == parameter.name:
                                    data_type = ca_param.data_type
                                    found = True
                                    break
                                if data_type == 'integer':
                                    val = int(parameter.value)
                            if not found:
                                self._log.warn("jujuCA: Did not find parameter {} for {}".
                                               format(parameter, config.name))
                            params.update({parameter.name: val})
                    if config.name == 'config':
                        if len(params):
                            self._log.debug("jujuCA: applying config with params {} for service {}".
                                            format(params, service))
                            if api is None:
                                api = yield from self._get_api()
                                if api is None:
                                    self._log.error("jujuCA: No API handle present for {}".
                                                    format(vnfr['vnf_juju_name']))
                                    return

                            rc = yield from self._loop.run_in_executor(
                                None,
                                api._apply_config,
                                service,
                                params
                            )
                            if rc:
                                output.execution_status = "completed"
                                self._log.debug("jujuCA: applied config {} on {}".
                                                format(params, service))
                                # Removing this as clearwater has fixed its config hook
                                # Sleep for sometime for the config to take effect
                                # self._log.debug("jujuCA: Wait sometime for config to take effect")
                                # yield from self._loop.run_in_executor(
                                #     None,
                                #     time.sleep,
                                #     30
                                # )
                                # self._log.debug("jujuCA: Wait over for config to take effect")
                            else:
                                output.execution_status = 'failed'
                                self._log.error("jujuCA: Error applying config {} on service {}".
                                                format(params, service))
                        else:
                            self._log.warn("jujuCA: Did not find valid paramaters for config : {}".
                                           format(primitive.parameter))
                    else:
                        self._log.debug("jujuCA: Execute action {} on service {} with params {}".
                                        format(config.name, service, params))
                        if api is None:
                            api = yield from self._get_api()
                            if api is None:
                                self._log.error("jujuCA: No API handle present for {}".
                                                format(vnfr['vnf_juju_name']))
                                return
                        tags = yield from self._loop.run_in_executor(
                            None,
                            api._execute_actions,
                            service, config.name, params
                        )
                        if len(tags):
                            output.execution_id = tags[0]
                            output.execution_status = api.get_action_status(tags[0])
                            self._log.debug("jujuCA: excute action {} on service {} returned {}".
                                            format(config.name, service, output.execution_status))
                        else:
                            self._log.error("jujuCA: error executing action {} for {} with {}".
                                            format(config.name, service, params))
                            output.execution_id = ''
                            output.execution_status = 'failed'
                    break
        except KeyError as e:
            self._log.info("VNF %s does not have config primititves, e=%s", vnfr_id, e)

    @asyncio.coroutine
    def apply_config(self, agent_nsr, agent_vnfr, config, rpc_ip):
        """ Notification on configuration of an NSR """
        pass

    @asyncio.coroutine
    def apply_ns_config(self, agent_nsr, agent_vnfrs, rpc_ip):
        """

        ###### TBD - This really does not belong here. Looks more like NS level script ####
        ###### apply_config should be called for a particular VNF only here ###############

        Hook: Runs the user defined script. Feeds all the necessary data
        for the script thro' yaml file.

        Args:
            rpc_ip (YangInput_Nsr_ExecNsConfigPrimitive): The input data.
            nsr (NetworkServiceRecord): Description
            vnfrs (dict): VNFR ID => VirtualNetworkFunctionRecord

        """
        def get_meta(agent_nsr):
            unit_names, initial_params, vnfr_index_map = {}, {}, {}

            for vnfr_id in agent_nsr.vnfr_ids:
                juju_vnf = self._juju_vnfs[vnfr_id].vnfr
                
                # Vnfr -> index ref
                vnfr_index_map[vnfr_id] = juju_vnf['member_vnf_index']

                # Unit name
                unit_names[vnfr_id] = juju_vnf['vnf_juju_name']

                # Flatten the data for simplicity
                param_data = {}
                self._log.debug("Juju Config:%s", juju_vnf['config'])
                for primitive in juju_vnf['config'].initial_config_primitive:
                    for parameter in primitive.parameter:
                        value = self.xlate(parameter.value, juju_vnf['tags'])
                        param_data[parameter.name] = value

                initial_params[vnfr_id] = param_data


            return unit_names, initial_params, vnfr_index_map

        unit_names, init_data, vnfr_index_map = get_meta(agent_nsr)

        # The data consists of 4 sections
        # 1. Account data
        # 2. The input passed.
        # 3. Juju unit names (keyed by vnfr ID).
        # 4. Initial config data (keyed by vnfr ID).
        data = dict()
        data['config_agent'] = dict(
                name=self._name,
                host=self._ip_address,
                port=self._port,
                user=self._user,
                secret=self._secret
                )
        data["rpc_ip"] = rpc_ip.as_dict()
        data["unit_names"] = unit_names
        data["init_config"] = init_data
        data["vnfr_index_map"] = vnfr_index_map

        tmp_file = None
        with tempfile.NamedTemporaryFile(delete=False) as tmp_file:
            tmp_file.write(yaml.dump(data, default_flow_style=True)
                    .encode("UTF-8"))

        self._log.debug("jujuCA: Creating a temp file: {} with input data".format(
                tmp_file.name))

        # Get the full path to the script
        script = ''
        if rpc_ip.user_defined_script[0] == '/':
            # The script has full path, use as is
            script = rpc_ip.user_defined_script
        else:
            script = os.path.join(self._rift_artif_dir, 'launchpad/libs', agent_nsr.id, 'scripts',
                                  rpc_ip.user_defined_script)
            self.log.debug("jujuCA: Checking for script in %s", script)
            if not os.path.exists(script):
                script = os.path.join(self._rift_install_dir, 'usr/bin', rpc_ip.user_defined_script)

        cmd = "{} {}".format(rpc_ip.user_defined_script, tmp_file.name)
        self._log.debug("jujuCA: Running the CMD: {}".format(cmd))

        coro = asyncio.create_subprocess_shell(cmd, loop=self._loop)
        process = yield from coro
        task = self._loop.create_task(process.wait())

        return task

    @asyncio.coroutine
    def apply_initial_config(self, agent_nsr, agent_vnfr):
        """
        Apply the initial configuration
        Expect config directives mostly, not actions
        Actions in initial config may not work based on charm design
        """

        vnfr = agent_vnfr.vnfr
        service = vnfr['vnf_juju_name']

        rc = yield from self.is_service_up(service)
        if not rc:
            return False

        try:
            tags = []
            api = None

            vnf_cat = agent_vnfr.vnfr_msg
            if vnf_cat and vnf_cat.mgmt_interface.ip_address:
                vnfr['tags'].update({'rw_mgmt_ip': vnf_cat.mgmt_interface.ip_address})
                self._log.debug("jujuCA:(%s) tags: %s", vnfr['vnf_juju_name'], vnfr['tags'])
            config = {}
            try:
                for primitive in vnfr['config'].initial_config_primitive:
                    self._log.debug("jujuCA:(%s) Initial config primitive %s", vnfr['vnf_juju_name'], primitive)
                    if primitive.name == 'config':
                        for param in primitive.parameter:
                            if vnfr['tags']:
                                val = self.xlate(param.value, vnfr['tags'])
                                config.update({param.name: val})
            except KeyError as e:
                self._log.exception("jujuCA:(%s) Initial config error(%s): config=%s",
                                    vnfr['vnf_juju_name'], str(e), config)
                config = None
                return False

            if config:
                self.juju_log('info', vnfr['vnf_juju_name'],
                              "Applying Initial config:%s",
                              config)
                if api is None:
                    api = yield from self._get_api()
                    if api is None:
                        self._log.error("jujuCA:(%s) No API available to apply initial config:%s",
                                        vnfr['vnf_juju_name'], config)
                        return False
                yield from self._loop.run_in_executor(
                    None,
                    api._apply_config,
                    service,
                    config
                )

            # Apply any actions specified as part of initial config
            for primitive in vnfr['config'].initial_config_primitive:
                if primitive.name != 'config':
                    self._log.debug("jujuCA:(%s) Initial config action primitive %s",
                                    vnfr['vnf_juju_name'], primitive)
                    action = primitive.name
                    params = {}
                    for param in primitive.parameter:
                        val = self.xlate(param.value, vnfr['tags'])
                        params.update({param.name: val})

                    self._log.info("jujuCA:(%s) Action %s with params %s",
                                   vnfr['vnf_juju_name'], action, params)
                    if api is None:
                        api = yield from self._get_api()
                        if api is None:
                            self._log.error("jujuCA:(%s) No API available for apply initial config actions",
                                            vnfr['vnf_juju_name'])
                            return False
                    tag = yield from self._loop.run_in_executor(
                        None,
                        api._execute_actions,
                        service,
                        action, params
                    )
                    tags.append(tag)

        except KeyError as e:
            self._log.info("Juju config agent(%s): VNFR %s not managed by Juju",
                           vnfr['vnf_juju_name'], agent_vnfr.id)
            return False
        except Exception as e:
            self._log.exception("jujuCA:(%s) Exception juju apply_initial_config for VNFR {}: {}".
                                format(vnfr['vnf_juju_name'], agent_vnfr.id, e))
            return False
        return True

    def add_vnfr_managed(self, agent_vnfr):
        if agent_vnfr.id not in self._juju_vnfs.keys():
            self._log.info("juju config agent: add vnfr={}/{}".format(agent_vnfr.name, agent_vnfr.id))
            self._juju_vnfs[agent_vnfr.id] = agent_vnfr

    def is_vnfr_managed(self, vnfr_id):
        try:
            if vnfr_id in self._juju_vnfs:
                return True
        except Exception as e:
            self._log.debug("jujuCA: Is VNFR {} managed: {}".
                            format(vnfr_id, e))
        return False

    def is_service_active(self, service):
        """ Is the juju service active  """
        resp = False
        try:
            api = yield from self._get_api()
            if api is None:
                self._log.error("jujuCA: Unable to get API for checking service is active")
                return resp

            # Check if deploy is over
            if self.check_task_status(service, 'deploy'):
                resp = yield from self._loop.run_in_executor(
                    None,
                    api.is_service_active,
                    service
                )
                self._log.debug("jujuCA: Is the service %s active? %s", service, resp)
                return resp
        except KeyError:
            self._log.error("jujuCA: Check active unknown service ", service)
        except Exception as e:
            self._log.error("jujuCA: Caught exception when checking for service is active: ", e)
            self._log.exception(e)
        return resp

    def is_service_up(self, service):
        """ Is the juju service up (active or blocked)  """
        resp = False
        try:
            api = yield from self._get_api()
            if api is None:
                self._log.error("jujuCA: Unable to get API for checking service is active")
                return resp

            # Check if deploy is over
            if self.check_task_status(service, 'deploy'):
                resp = yield from self._loop.run_in_executor(
                    None,
                    api.is_service_up,
                    service
                )
            self._log.debug("jujuCA: Is the service %s up? %s", service, resp)
            return resp
        except KeyError:
            self._log.error("jujuCA: Check unknown service %s is up", service)
        except Exception as e:
            self._log.error("jujuCA: Caught exception when checking for service is active: %s", e)
            self._log.exception(e)
        return resp

    @asyncio.coroutine
    def is_configured(self, vnfr_id):
        try:
            agent_vnfr = self._juju_vnfs[vnfr_id]
            vnfr = agent_vnfr.vnfr
            if vnfr['active']:
                return True

            vnfr = self._juju_vnfs[vnfr_id].vnfr
            service = vnfr['vnf_juju_name']
            resp = self.is_service_active(service)
            self._juju_vnfs[vnfr_id]['active'] = resp
            self._log.debug("jujuCA: Service state for {} is {}".
                            format(service, resp))

            ### This should check the response and return True or False
            return resp
        except KeyError:
            self._log.debug("jujuCA: VNFR id {} not found in config agent".
                            format(vnfr_id))
            return False
        except Exception as e:
            self._log.error("jujuCA: VNFR id {} is_configured: {}".
                            format(vnfr_id, e))
        return False

    @asyncio.coroutine
    def get_config_status(self, agent_nsr, agent_vnfr):
        """Get the configuration status for the VNF"""
        rc = 'unknown'

        try:
            vnfr = agent_vnfr.vnfr
            service = vnfr['vnf_juju_name']
        except KeyError:
            # This VNF is not managed by Juju
            return rc

        rc = 'configuring'

        if not self.check_task_status(service, 'deploy'):
            return rc

        try:
            api = yield from self._get_api()
            if api is None:
                self._log.error("jujuCA: Unable to get API for checking service is active")
                return rc

            resp = yield from self._loop.run_in_executor(
                None,
                api.get_service_status,
                service
            )
            self._log.debug("jujuCA: Get service %s status? %s", service, resp)

            if resp == 'error':
                return 'error'
            if resp == 'active':
                return 'configured'
        except KeyError:
            self._log.error("jujuCA: Check unknown service %s status", service)
        except Exception as e:
            self._log.error("jujuCA: Caught exception when checking for service is active: %s", e)
            self._log.exception(e)

        return rc

    def get_action_status(self, execution_id):
        ''' Get the action status for an execution ID
            *** Make sure this is NOT a asyncio coroutine function ***
        '''
        api = self._get_api_blocking()
        if api is None:
            self._log.error("jujuCA: Unable to get API in get_action_status")
            return None
        try:
            return api.get_action_status(execution_id)
        except Exception as e:
            self._log.exception("jujuCA: Error fetching execution status for %s",
                                execution_id)
            self._log.exception(e)
            return None
