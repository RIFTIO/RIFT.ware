# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

# Part of the code taken from
# https://github.com/chuckbutler/juju_action_api_class/blob/master/juju_actions.py

import asyncio
import jujuclient
import os
import ssl
import sys
import time
import uuid


class Action(object):
    def __init__(self, data):
        # I am undecided if we need this
        # model_id = ""
        self.uuid = data['action']['tag']
        self.data = data  # straight from juju api
        self.juju_status = data['status']

    @classmethod
    def from_data(cls, data):
        o = cls(data=data)
        return o


def get_service_units(status):
    results = {}
    services = status.get('Services', {})
    for svc_name, svc_data in services.items():
        units = svc_data['Units'] or {}
        sub_to = svc_data['SubordinateTo']
        if not units and sub_to:
            for sub in sub_to:
                for unit_name, unit_data in \
                        (services[sub].get('Units') or {}).items():
                    for sub_name, sub_data in \
                            (unit_data['Subordinates'] or {}).items():
                        if sub_name.startswith(svc_name):
                            units[sub_name] = sub_data
        results[svc_name] = units
    return results


class ApiEnvironmentMock(object):
    service = None
    actions = []
    config = None
    status_str = ''
    machine_spec = None

    def __init__(self, endpoint):
        self._endpoint = endpoint
        self._user = None
        self._secret = None
        self._url = None

    def __getattr__(self, name):
        print("***ERROR: Mock did not implement %s method***" % name)
        raise AttributeError(name)

    def _service_status(self):
        if ApiEnvironmentMock.service is None:
            return {}

        return {ApiEnvironmentMock.service: {
                    'MeterStatuses': None,
                    'Status': {
                        'Status': ApiEnvironmentMock.status_str,
                        'Version': '',
                        'Kind': '',
                        'Life': '',
                        'Info': '' if ApiEnvironmentMock.config is None else "configured",
                        'Since': '2016-03-30T19:43:39.899923542Z',
                        'Err': None,
                        'Data': {}
                    },
                    'Life': '',
                    'Charm': self._url,
                    'Exposed': False,
                    'Err': None,
                    'CanUpgradeTo': '',
                    'Units': {
                        '{}/0'.format(ApiEnvironmentMock.service): {
                            'AgentStateInfo': '',
                            'OpenedPorts': None,
                            'AgentVersion': '1.25.3.1',
                            'Charm': '',
                            'Machine': '0/lxc/17',
                            'UnitAgent': {
                                'Status': 'idle',
                                'Version': '1.25.3.1',
                                'Kind': '',
                                'Life': '',
                                'Info': '',
                                'Since': '2016-03-30T19:43:42.677814097Z',
                                'Err': None,
                                'Data': {}
                            },
                            'Workload': {
                                'Status': 'active',
                                'Version': '',
                                'Kind': '',
                                'Life': '',
                                'Info': 'configured',
                                'Since': '2016-03-30T19:43:39.899923542Z',
                                'Err': None,
                                'Data': {}
                            },
                            'Life': '',
                            'Err': None,
                            'Subordinates': None,
                            'PublicAddress': '10.1.3.109',
                            'AgentState': 'started'
                        }
                    },
                    'Networks': {
                        'Disabled': None,
                        'Enabled': None
                    },
                    'SubordinateTo': [],
                    'Relations': {}
                }
            }

    def close(self):
        pass

    def status(self):
        status = {
            'EnvironmentName': 'mwc16manual',
            'AvailableVersion': '',
            'Services': {
                'juju-gui': {
                    'MeterStatuses': None,
                    'Status': {
                        'Status': 'unknown',
                        'Version': '',
                        'Kind': '',
                        'Life': '',
                        'Info': '',
                        'Since': '2016-03-25T17:42:43.003899974Z',
                        'Err': None,
                        'Data': {}
                    },
                    'Life': '',
                    'Charm': 'cs:trusty/juju-gui-52',
                    'Exposed': False,
                    'Err': None,
                    'CanUpgradeTo': '',
                    'Units': {
                        'juju-gui/1': {
                            'AgentStateInfo': '',
                            'OpenedPorts': ['80/tcp', '443/tcp'],
                            'AgentVersion': '1.25.3.1',
                            'Charm': '',
                            'Machine': '0',
                            'UnitAgent': {
                                'Status': 'idle',
                                'Version': '1.25.3.1',
                                'Kind': '',
                                'Life': '',
                                'Info': '',
                                'Since': '2016-03-30T19:42:45.252871046Z',
                                'Err': None,
                                'Data': {}
                            },
                            'Workload': {
                                'Status': 'unknown',
                                'Version': '',
                                'Kind': '',
                                'Life': '',
                                'Info': '',
                                'Since': '2016-03-25T17:42:43.003899974Z',
                                'Err': None,
                                'Data': {}
                            },
                            'Life': '',
                            'Err': None,
                            'Subordinates': None,
                            'PublicAddress': '10.0.216.53',
                            'AgentState': 'started'
                        }
                    },
                    'Networks': {
                        'Disabled': None,
                        'Enabled': None
                    },
                    'SubordinateTo': [],
                    'Relations': {}
                }
            },
            'Relations': None,
            'Networks': {},
            'Machines': {
                '0': {
                    'AgentStateInfo': '',
                    'DNSName': '10.0.216.53',
                    'AgentVersion': '1.25.3.1',
                    'WantsVote': True,
                    'Hardware': 'arch=amd64 cpu-cores=2 mem=3953M',
                    'Id': '0',
                    'Containers': {
                        '0/lxc/17': {
                            'AgentStateInfo': '',
                            'DNSName': '10.1.3.109',
                            'AgentVersion': '1.25.3.1',
                            'WantsVote': False,
                            'Hardware': 'arch=amd64',
                            'Id': '0/lxc/17',
                            'Containers': {},
                            'InstanceState': '',
                            'HasVote': False,
                            'Life': '',
                            'Jobs': ['JobHostUnits'],
                            'Err': None,
                            'Series': 'trusty',
                            'InstanceId': 'juju-machine-0-lxc-17',
                            'AgentState': 'started',
                            'Agent': {
                                'Status': 'started',
                                'Version': '1.25.3.1',
                                'Kind': '',
                                'Life': '',
                                'Info': '',
                                'Since': '2016-03-30T19:43:28.665683958Z',
                                'Err': None,
                                'Data': {}
                            }
                        }
                    },
                    'InstanceState': '',
                    'HasVote': True,
                    'Life': '',
                    'Jobs': ['JobManageEnviron', 'JobHostUnits'],
                    'Err': None,
                    'Series': 'trusty',
                    'InstanceId': 'manual:',
                    'AgentState': 'started',
                    'Agent': {
                        'Status': 'started',
                        'Version': '1.25.3.1',
                        'Kind': '',
                        'Life': '',
                        'Info': '',
                        'Since': '2016-03-25T19:59:40.36127932Z',
                        'Err': None,
                        'Data': {}
                    }
                }
            }
        }

        for key, val in self._service_status().items():
            status["Services"][key] = val

        return status

    def deploy(self, service, url, machine_spec):
        #called with args (('cwims-cwims-vnfd-b', 'local:trusty/clearwater-aio-proxy-31')), kwargs ({'machine_spec': 'lxc:0'})
        ApiEnvironmentMock.service = service
        ApiEnvironmentMock.machine_spec = machine_spec
        ApiEnvironmentMock.status_str = "active"
        self._url = url

    def destroy_service(self, service_name):
        ApiEnvironmentMock.status_str = "terminated"

    def wait_for_units(self, timeout):
        return

    def add_local_charm_dir(self, *args, **kwargs):
        return {"CharmURL": 'local:trusty/clearwater-aio-proxy-32'}

    def login(self, secret, user):
        self._secret = secret
        self._user = user

        return {
            'Servers': [
                [{
                    'Value': '10.0.216.53',
                    'Port': 17070,
                    'Type': 'ipv4',
                    'Scope': 'public',
                    'NetworkName': ''
                }, {
                    'Value': '127.0.0.1',
                    'Port': 17070,
                    'Type': 'ipv4',
                    'Scope': 'local-machine',
                    'NetworkName': ''
                }, {
                    'Value': '::1',
                    'Port': 17070,
                    'Type': 'ipv6',
                    'Scope': 'local-machine',
                    'NetworkName': ''
                }]
            ],
            'EnvironTag': 'environment-51e330d4-de4c-4d18-82f6-651434324ff9',
            'Facades': [{
                'Versions': [0],
                'Name': 'Action'
            }, {
                'Versions': [1],
                'Name': 'Addresser'
            }, {
                'Versions': [0, 1],
                'Name': 'Agent'
            }, {
                'Versions': [1],
                'Name': 'AllEnvWatcher'
            }, {
                'Versions': [0],
                'Name': 'AllWatcher'
            }, {
                'Versions': [1],
                'Name': 'Annotations'
            }, {
                'Versions': [0],
                'Name': 'Backups'
            }, {
                'Versions': [1],
                'Name': 'Block'
            }, {
                'Versions': [0],
                'Name': 'CharmRevisionUpdater'
            }, {
                'Versions': [1],
                'Name': 'Charms'
            }, {
                'Versions': [1],
                'Name': 'Cleaner'
            }, {
                'Versions': [0],
                'Name': 'Client'
            }, {
                'Versions': [0],
                'Name': 'Deployer'
            }, {
                'Versions': [1],
                'Name': 'DiskManager'
            }, {
                'Versions': [1],
                'Name': 'EntityWatcher'
            }, {
                'Versions': [0],
                'Name': 'Environment'
            }, {
                'Versions': [1],
                'Name': 'FilesystemAttachmentsWatcher'
            }, {
                'Versions': [1],
                'Name': 'Firewaller'
            }, {
                'Versions': [1],
                'Name': 'HighAvailability'
            }, {
                'Versions': [1],
                'Name': 'ImageManager'
            }, {
                'Versions': [1],
                'Name': 'ImageMetadata'
            }, {
                'Versions': [1],
                'Name': 'InstancePoller'
            }, {
                'Versions': [0],
                'Name': 'KeyManager'
            }, {
                'Versions': [0],
                'Name': 'KeyUpdater'
            }, {
                'Versions': [1],
                'Name': 'LeadershipService'
            }, {
                'Versions': [0],
                'Name': 'Logger'
            }, {
                'Versions': [1],
                'Name': 'MachineManager'
            }, {
                'Versions': [0],
                'Name': 'Machiner'
            }, {
                'Versions': [0],
                'Name': 'MetricsManager'
            }, {
                'Versions': [0],
                'Name': 'Networker'
            }, {
                'Versions': [0],
                'Name': 'NotifyWatcher'
            }, {
                'Versions': [0],
                'Name': 'Pinger'
            }, {
                'Versions': [0, 1],
                'Name': 'Provisioner'
            }, {
                'Versions': [1],
                'Name': 'Reboot'
            }, {
                'Versions': [0],
                'Name': 'RelationUnitsWatcher'
            }, {
                'Versions': [1],
                'Name': 'Resumer'
            }, {
                'Versions': [0],
                'Name': 'Rsyslog'
            }, {
                'Versions': [1],
                'Name': 'Service'
            }, {
                'Versions': [1],
                'Name': 'Spaces'
            }, {
                'Versions': [1],
                'Name': 'Storage'
            }, {
                'Versions': [1],
                'Name': 'StorageProvisioner'
            }, {
                'Versions': [0],
                'Name': 'StringsWatcher'
            }, {
                'Versions': [1],
                'Name': 'Subnets'
            }, {
                'Versions': [0, 1, 2],
                'Name': 'Uniter'
            }, {
                'Versions': [0],
                'Name': 'Upgrader'
            }, {
                'Versions': [0],
                'Name': 'UserManager'
            }, {
                'Versions': [1],
                'Name': 'VolumeAttachmentsWatcher'
            }, {
                'Versions': [0],
                'Name': 'payloads'
            }, {
                'Versions': [0],
                'Name': 'payloads-hook-context'
            }],
            'LastConnection': '2016-03-30T19:14:47Z'
        }

    def set_config(self, service, config):
        ApiEnvironmentMock.config = config

    def actions_enqueue(self, action_name, unit_names, arguments):
        uuid_str = str(uuid.uuid4())

        ApiEnvironmentMock.actions.append({
                'message': 'exit status 0',
                'status': 'completed',
                'enqueued': '2016-03-30T19:18:53Z',
                'action': {
                    'receiver': 'unit-{}'.format(ApiEnvironmentMock.service),
                    'parameters': arguments,
                    'name': action_name,
                    'tag': 'action-{}'.format(uuid_str),
                },
                'completed': '2016-03-30T19:18:59Z',
                'started': '2016-03-30T19:18:54Z'
            })


        return {
            'results': [{
                'enqueued': '2016-03-30T19:18:53Z',
                'started': '0001-01-01T00:00:00Z',
                'status': 'pending',
                'completed': '0001-01-01T00:00:00Z',
                'action': {
                    'receiver': 'unit-{}'.format(ApiEnvironmentMock.service),
                    'parameters': arguments,
                    'name': action_name,
                    'tag': 'action-{}'.format(uuid_str),
                }
            }]
        }

    def actions_list_all(self, service=None):
        return {
            'actions': [{
                'receiver': 'unit-{}'.format(ApiEnvironmentMock.service),
                'actions': ApiEnvironmentMock.actions,
                }, {
                    'receiver': 'unit-juju-gui-1'
                }]
        }


class ApiEnvironment(jujuclient.Environment):
    def actions_available(self, service=None):
        args = {
            "Type": 'Action',
            "Request": 'ServicesCharmActions',
            "Params": {
                "Entities": []
            }
        }

        services = self.status().get('Services', {})
        service_names = [service] if service else services
        for name in service_names:
            args['Params']['Entities'].append(
                {
                    "Tag": 'service-' + name
                }
            )

        return self._rpc(args)

    def actions_list_all(self, service=None):
        args = {
            "Type": 'Action',
            "Request": 'ListAll',
            "Params": {
                "Entities": []
            }
        }

        service_units = get_service_units(self.status())
        service_names = [service] if service else service_units.keys()
        units = []

        for name in service_names:
            units += service_units[name].keys()

        for unit in set(units):
            args['Params']['Entities'].append(
                {
                    "Tag": "unit-%s" % unit.replace('/', '-'),
                }
            )

        return self._rpc(args)

    def actions_enqueue(self, action, receivers, params=None):
        args = {
            "Type": "Action",
            "Request": "Enqueue",
            "Params": {
                "Actions": []
            }
        }

        for receiver in receivers:
            args['Params']['Actions'].append({
                "Receiver": receiver,
                "Name": action,
                "Parameters": params or {},
            })

        return self._rpc(args)

    def actions_cancel(self, uuid):
        return self._rpc({
            'Type': 'Action',
            'Request': 'Cancel',
            "Params": {
                "Entities": [{'Tag': 'action-' + uuid}]
            }
        })


def _parse_action_specs(api_results):
    results = {}

    r = api_results['results']
    for service in r:
        servicetag = service['servicetag']
        service_name = servicetag[8:]  # remove 'service-' prefix
        specs = {}
        if service['actions']['ActionSpecs']:
            for spec_name, spec_def in \
                    service['actions']['ActionSpecs'].items():
                specs[spec_name] = ActionSpec(spec_name, spec_def)
        results[service_name] = specs
    return results


def _parse_action_properties(action_properties_dict):
    results = {}

    d = action_properties_dict
    for prop_name, prop_def in d.items():
        results[prop_name] = ActionProperty(prop_name, prop_def)
    return results


class Dict(dict):
    def __getattr__(self, name):
        return self[name]


class ActionSpec(Dict):
    def __init__(self, name, data_dict):
        params = data_dict['Params']
        super(ActionSpec, self).__init__(
            name=name,
            title=params['title'],
            description=params['description'],
            properties=_parse_action_properties(params['properties'])
        )


class ActionProperty(Dict):
    types = {
        'string': str,
        'integer': int,
        'boolean': bool,
        'number': float,
    }
    type_checks = {
        str: 'string',
        int: 'integer',
        bool: 'boolean',
        float: 'number',
    }

    def __init__(self, name, data_dict):
        super(ActionProperty, self).__init__(
            name=name,
            description=data_dict.get('description', ''),
            default=data_dict.get('default', ''),
            type=data_dict.get(
                'type', self._infer_type(data_dict.get('default'))),
        )

    def _infer_type(self, default):
        if default is None:
            return 'string'
        for _type in self.type_checks:
            if isinstance(default, _type):
                return self.type_checks[_type]
        return 'string'

    def to_python(self, value):
        f = self.types.get(self.type)
        return f(value) if f else value


class JujuApi(object):
    def __init__ (self, log, server, port, user, secret, loop):
        ''' Connect to the juju host '''
        self.log = log
        self.server = server
        self.user = user
        self.port = port
        self.secret = secret
        self.loop = loop
        endpoint = 'wss://%s:%d' % (server.split()[0], int(port))
        self.endpoint = endpoint
        self.env = ApiEnvironment(endpoint)

        self.env.login(secret, user=user)
        self.deploy_timeout = 600
        # Check python version and setup up SSL
        if sys.version_info >= (3,4):
            # This is needed for python 3.4 above as by default certificate
            # validation is enabled
            ssl._create_default_https_context = ssl._create_unverified_context

    def reconnect(self):
        ''' Reconnect on error cases'''
        self.log.info("Juju: try reconnect to endpoint {}".
                      format(self.endpoint))
        try:
            self.env.close()
            del self.env
        except Exception as e:
            self.log.debug("Juju: env close threw e {}".
                           format(e))
            self.log.exception(e)

        try:
            self.env = ApiEnvironment(self.endpoint)
            self.env.login(self.secret, user=self.user)
            self.log.info("Juju: reconnected to endpoint {}".
                          format(self.endpoint))
        except Exception as e:
            self.log.error("Juju: exception in reconnect e={}".format(e))
            self.log.exception(e)


    def get_status(self):
        try:
            status = self.env.status()
            return status
        except Exception as e:
            self.log.error("Juju: exception while getting status: {}".format(e))
            self.log.exception(e)
            self.reconnect()
        return None

    def get_annotations(self, services):
        '''
        Return dict of (servicename: annotations) for each servicename
        in `services`.
        '''
        if not services:
            return None

        d = {}
        for s in services:
            d[s] = self.env.get_annotation(s, 'service')['Annotations']
        return d

    def get_actions(self, service=None):
        return self.env.actions_list_all(service)

    def get_action_status(self, action_tag):
        '''
        responds with the action status, which is one of three values:

         - completed
         - pending
         - failed

         @param action_tag - the action UUID return from the enqueue method
         eg: action-3428e20d-fcd7-4911-803b-9b857a2e5ec9
        '''
        try:
            receiver = self.get_actions()
        except Exception as e:
            self.log.error("Juju: exception is get actions: {}".format(e))
            self.log.exception(e)

        try:
            for receiver in receiver['actions']:
                if 'actions' in receiver.keys():
                    for action_record in receiver['actions']:
                        if 'action' in action_record.keys():
                            if action_record['action']['tag'] == action_tag:
                                return action_record['status']
        except Exception as e:
            self.log.error("Juju: exception in get action status {}".format(e))
            self.log.exception(e)

    def cancel_action(self, uuid):
        return self.env.actions_cancel(uuid)

    def get_service_units(self):
        return get_service_units(self.get_status())

    def get_action_specs(self):
        results = self.env.actions_available()
        return _parse_action_specs(results)

    def enqueue_action(self, action, receivers, params):
        try:
            result = self.env.actions_enqueue(action, receivers, params)
            resp = Action.from_data(result['results'][0])
            return resp
        except Exception as e:
            self.log.error("Juju: Exception enqueing action {} on units {} with params {}: {}".
                           format(action, receivers, params, e))
            self.log.exception(e)
            return None

    @asyncio.coroutine
    def is_deployed(self, service):
        return self._is_deployed(service)

    def _is_deployed(self, service, status=None):
        status = self.get_service_status(service, status=status)
        if status not in ['terminated', 'NA']:
            return True

        return False

    def get_service_status(self, service, status=None):
        ''' Get service status:
            maintenance : The unit is not yet providing services, but is actively doing stuff.
            unknown : Service has finished an event but the charm has not called status-set yet.
            waiting : Service is unable to progress to an active state because of dependency.
            blocked : Service needs manual intervention to get back to the Running state.
            active  : Service correctly offering all the services.
            None    : Service is not deployed
            *** Make sure this is NOT a asyncio coroutine function ***
        '''
        try:
            #self.log.debug ("In get service status for service %s, %s" % (service, services))
            if status is None:
                status = self.get_status()
            if status:
                srv_status = status['Services'][service]['Status']['Status']
                return srv_status
        except KeyError as e:
            self.log.info("Juju: Did not find service {}, e={}".format(service, e))
        except Exception as e:
            self.log.error("Juju: exception checking service status for {}, e {}".
                           format(service, e))
            self.log.exception(e)

        return 'NA'

    def is_service_active(self, service):
        if self.get_service_status(service) == 'active':
            self.log.debug("Juju: service is active for %s " % service)
            return True

        return False

    def is_service_blocked(self, service):
        if self.get_service_status(service) == 'blocked':
            return True

        return False

    def is_service_up(self, service):
        if self.get_service_status(service) in ['active', 'blocked']:
            return True

        return False

    def is_service_in_error(self, service):
        if self.get_service_status(service) == 'error':
            self.log.debug("Juju: service is in error state for %s" % service)

    def wait_for_service(self, service):
        # Check if the agent for the unit is up, wait for units does not wait for service to be up
        waiting = 12
        delay = 5 # seconds
        print ("In wait for service %s" % service)
        while waiting:
            waiting -= 1
            if self.is_service_up(service):
                return True
            else:
                yield from asyncio.sleep(delay, loop=self.loop)
        return False

    @asyncio.coroutine
    def apply_config(self, service, config):
        return self._apply_config(service, config)

    def _apply_config(self, service,config):
        if config is None or len(config) == 0:
            self.log.warn("Juju: Empty config passed for service %s" % service)
            return False
        if not self._is_deployed(service):
            self.log.warn("Juju: Charm service %s not deployed" % (service))
            return False
        self.log.debug("Juju: Config for {} updated to: {}".format(service, config))
        try:
            # Try to fix error on service, most probably due to config issue
            if self.is_service_in_error:
                self.resolve_error(service)
            self.env.set_config(service, config)
            return True
        except Exception as e:
            self.log.error("Juju: exception setting config for {} with {}, e {}".
                           format(service, config, e))
            self.log.exception(e)
            self.reconnect()
        return False

    @asyncio.coroutine
    def set_parameter(self, service, parameter, value):
        return self.apply_config(service, {parameter : value})

    @asyncio.coroutine
    def deploy_service(self, charm, service, config=None, wait=False):
        self._deploy_service(charm, service, config=config, wait=wait)

    def _deploy_service(self, charm, service, path=None, config=None, wait=False):
        self.log.debug("Juju: Deploy service for charm {}({}) with service {}".
                       format(charm, path, service))
        if self._is_deployed(service):
            self.log.info("Juju: Charm service %s already deployed" % (service))
            if config:
                self._apply_config(service, config)
            return 'deployed'
        series = "trusty"
        deploy_to = "lxc:0"
        if path is None:
            prefix=os.getenv('RIFT_INSTALL', '/')
            path = os.path.join(prefix, 'usr/rift/charms', series, charm)

        try:
            self.log.debug("Juju: Local charm settings: dir=%s, series=%s" %
                           (path, series))
            result = self.env.add_local_charm_dir(path, series)
            url = result['CharmURL']

        except Exception as e:
            self.log.critical('Juju: Error setting local charm directory {} for {}: {}'.
                              format(path, service, e))
            self.log.exception(e)
            self.reconnect()
            return 'error'

        try:
            self.log.debug("Juju: Deploying using: service={}, url={}, to={}, config={}".
                           format(service, url, deploy_to, config))
            if config:
                self.env.deploy(service, url, machine_spec=deploy_to, config=config)
            else:
                self.env.deploy(service, url, machine_spec=deploy_to)
        except Exception as e:
            self.log.error('Juju: Error deploying {}: {}'.format(service, e))
            self.log.exception(e)
            if not self._is_deployed(service):
                self.log.critical ("Juju: Service {} is not deployed" % service)
                self.reconnect()
                return 'error'

        if wait:
            # Wait for the deployed units to start
            try:
                self.log.debug("Juju: Waiting for charm %s to come up" % service)
                self.env.wait_for_units(timeout=self.deploy_timeout)
            except Exception as e:
                self.log.critical('Juju: Error starting all units for {}: {}'.
                                  format(service, e))
                self.log.exception(e)
                self.reconnect()
                return 'error'

        rc = self.wait_for_service(service)
        if rc:
            return 'deployed'
        else:
            return 'deploying'

    @asyncio.coroutine
    def execute_actions(self, service, action, params, wait=False, bail=False):
        return self._execute_actions(service, action, params, wait=wait, bail=bail)

    def _execute_actions(self, service, action, params, wait=False, bail=False):
        tags = []
        try:
            services = get_service_units(self.env.status())
            depl_units = services[service]
        except KeyError as e:
            self.log.error("Juju: Unable to get service units for {}, e={}".
                           format(services, e))
            return tags
        except Exception as e:
            self.log.error("Juju: Error on getting service details for service {}, e={}".
                           format(service, e))
            self.log.exception(e)
            self.reconnect()
            return tags

        # Go through each unit deployed and apply the actions to the unit
        for unit, status in depl_units.items():
            self.log.debug("Juju: Execute on unit {} with {}".
                           format(unit, status))
            idx = int(unit[unit.index('/')+1:])
            self.log.debug("Juju: Unit index is %d" % idx)

            unit_name = "unit-%s-%d" % (service, idx)
            self.log.debug("Juju: Sending action: {}, {}, {}".
                           format(action, unit_name, params))
            try:
                result = self.enqueue_action(action, [unit_name], params)
                if result:
                    tags.append(result.uuid)
                else:
                    self.log.error("Juju: Error applying the action {} on {} with params {}".
                                  format(action, unit, params))
            except Exception as e:
                self.log.error("Juju: Error applying the action {} on {} with params {}, e={}" %
                               format(action, unit, params, e))
                self.log.exception(e)
                self.reconnect()

            # act_status = 'pending'
            # #self.log.debug("Juju: Action %s status is %s on %s" % (action, act_status, unit))
            # while wait and ((act_status == 'pending') or (act_status == 'running')):
            #     act_status = self.get_action_status(result.uuid)
            #     self.log.debug("Juju: Action %s status is %s on %s" % (action, act_status, unit))
            #     if bail and (act_status ==  'failed'):
            #         self.log.error("Juju: Error applying action %s on %s with %s" % (action, unit, params))
            #         raise RuntimeError("Juju: Error applying action %s on %s with %s" % (action, unit, params))
            #     yield from asyncio.sleep(1, loop=self.loop)

        return tags

    def get_service_units_status(self, service, status):
        units_status = {}
        if status is None:
            return units_status
        try:
            units = get_service_units(status)[service]
            for name, data in units.items():
                # Action rpc require unit name as unit-service-index
                # while resolved API require unit name as service/index
                #idx = int(name[name.index('/')+1:])
                #unit = "unit-%s-%d" % (service, idx)
                units_status.update({name : data['Workload']['Status']})
        except KeyError:
            pass
        except Exception as e:
            self.log.error("Juju: service unit status for service {}, e={}".
                           format(service, e))
            self.log.exception(e)
        self.log.debug("Juju: service unit status for service {}: {}".
                       format(service, units_status))
        return units_status

    def resolve_error(self, service, status=None):
        if status is None:
            status = self.get_status()

        if status is None:
            return

        srv_status = self.get_service_status(service, status)
        if srv_status and srv_status not in ['terminated', 'NA']:
            units = self.get_service_units_status(service, status)
            for unit, ustatus in units.items():
                if ustatus == 'error':
                    self.log.info("Juju: Found unit %s with status %s" %
                                  (unit, ustatus))
                    try:
                        # Takes the unit name as service_name/idx unlike action
                        self.env.resolved(unit)
                    except Exception as e:
                        self.log.debug("Juju: Exception when running resolve on unit {}: {}".
                                       format(unit, e))
                        self.log.exception(e)


    @asyncio.coroutine
    def destroy_service(self, service):
        self._destroy_service(service)

    def _destroy_service(self, service):
        ''' Destroy juju service
            *** Do NOT add aysncio yield on this function, run in separate thread ***
        '''
        self.log.debug("Juju: Destroy charm service: %s" % service)
        status = self.get_status()
        if status is None:
            return

        srv_status = self.get_service_status(service, status)
        count = 0
        while srv_status and srv_status not in ['terminated', 'NA']:
            count += 1
            self.log.debug("Juju: service %s is in %s state, count %d" %
                           (service, srv_status, count))
            if count > 25:
                self.log.error("Juju: Not able to destroy service %s, status %s after %d tries" %
                               (service, srv_status, count))
                break

            self.resolve_error(service, status)

            try:
                self.env.destroy_service(service)
            except Exception as e:
                self.log.debug("Juju: Exception when running destroy on service {}: {}".
                       format(service, e))
                self.log.exception(e)
                self.reconnect()

            time.sleep(3)
            status = self.get_status()
            if status is None:
                return
            srv_status = self.get_service_status(service, status)

        self.log.debug("Destroyed service %s (%s)" % (service, srv_status))
