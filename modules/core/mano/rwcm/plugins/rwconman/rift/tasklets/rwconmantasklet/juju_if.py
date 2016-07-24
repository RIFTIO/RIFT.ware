#!/usr/bin/env python3

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

# Part of the code taken from
# https://github.com/chuckbutler/juju_action_api_class/blob/master/juju_actions.py

"""
This script is used to control a bootstrapped Juju environment
This has been tested only with local environemnt and local charms
Provide a yaml file with the details to this script to execute
Sample yaml file to deploy a service

ims-a:
  deploy:
    store: local
    directory: /usr/rift/charms/trusty/clearwater-aio-proxy
    series: trusty
    to: "lxc:0"

  #destroy: true

  # Data under config passed as such during deployment
  config:
      proxied_ip: 10.0.202.39
      home_domain: "ims.riftio.local"
      base_number: "1234567000"
      number_count: 1000

  units:
    - unit:
        #id: 0
        # Wait for each command to complete
        wait: true
        # Bail on failure
        bail: true
        #destroy: true
        actions:
          - create-user: { number: "1234567001", password: "secret"}
          - create-user: { number: "1234567002", password: "secret"}

    - unit:
        wait: true
        destroy: true
        bail: true
        actions:
          - create-user: { number: "1234567010", password: "secret"}
          - create-user: { number: "1234567011", password: "secret"}

Sample yaml file to destroy a service
clearwater-aio-proxy:
  destroy: true

  units:
    - unit:
        actions:
          - delete-user: { number: "1234567001" }
          - delete-user: { number: "1234567002" }
    - unit:
        actions:
          - delete-user: { number: "1234567010" }
          - delete-user: { number: "1234567011" }
"""

import logging
import argparse
import yaml
import jujuclient
import sys
import time
import ssl
import os

ssl_ok = False
ssh_cmd = None
scp_cmd = None


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


class API(object):
    def __init__ (self, args, logger):
        logger.debug("Args: %s" % args)

        self.file = args.file
        stream = open(self.file)
        self.yaml = yaml.load(stream)
        stream.close()

        self.ca_cert = args.ca_cert
        if args.ca_cert is None:
            try:
                self.ca_cert = self.yaml['ca-cert']
            except KeyError:
                logger.warning("Did not get the CA certificate to use")

        endpoint = 'wss://%s:%d' % (args.server.split()[0], int(args.port))
        logger.info("Juju API endpoint %s" % endpoint)
        self.env = ApiEnvironment(endpoint, ca_cert=self.ca_cert)
        self.env.login(args.password, user=args.user)
        #self.actions=jujuclient.Actions(self.env)
        self.logger = logger
        if args.file:
            logger.debug("File %s" % args.file)

        self.server = args.server
        self.user = args.user
        self.port = args.port
        self.deploy_timeout = args.deploy_timeout
        self.password = args.password
        self.cur_units = 0
        self.req_units = 0
        self.charm = None

    def get_status(self):
        return self.env.status()

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
        receiver = self.get_actions()
        for receiver in receiver['actions']:
            if 'actions' in receiver.keys():
                for action_record in receiver['actions']:
                    if 'action' in action_record.keys():
                        if action_record['action']['tag'] == action_tag:
                            return action_record['status']

    def cancel_action(self, uuid):
        return self.env.actions_cancel(uuid)

    def get_service_units(self):
        return get_service_units(self.env.status())

    def get_action_specs(self):
        results = self.env.actions_available()
        return _parse_action_specs(results)

    def enqueue_action(self, action, receivers, params):
        result = self.env.actions_enqueue(action, receivers, params)
        return Action.from_data(result['results'][0])

    def apply_config(self, service, details):
        if self.cur_units == 0:
            # Nothing to do
            return
        if 'config' in details:
            self.logger.debug("Config for %s updated to: %s" % (service, details['config']))
            self.env.set_config(service, details['config'])
        else:
            self.logger.debug("No config section found for %s" % service)

    def deploy_service(self, service, details):
        if self.cur_units == 0:
            # No units of the service running
            if details['deploy'] is not None:
                deploy = details['deploy']
                self.logger.debug("Config used for deployment: %s" % details['config'])
                if self.req_units > 0:
                    # Deploy the service
                    series = 'trusty'
                    try:
                        series = deploy['series']
                    except KeyError:
                        self.logger.debug("Using default series %s" % series)

                    store_type = 'online'
                    try:
                        store_type = deploy['store']
                    except KeyError:
                        self.logger.debug("Using default store type %s" % store_type)

                    deploy_to = None
                    try:
                        deploy_to = deploy['to']
                    except KeyError:
                        self.logger.debug("No deploy machine specified")

                    config = None
                    if 'config' in details:
                        config = details['config']
                        self.logger.debug("Config for %s is %s" % (service, config))
                    else:
                        self.logger.debug("No config section found")

                    if store_type == 'local':
                        try:
                            directory = deploy['directory']
                            prefix=''
                            try:
                                prefix=os.environ.get('RIFT_INSTALL')
                            except KeyError:
                                self.logger.info("RIFT_INSTALL not set in environemnt")
                            directory = "%s/%s" % (prefix, deploy['directory'])
                            if ssl_ok:
                                self.logger.debug("Local charm settings: dir=%s, series=%s" % (directory, series))
                                result = self.env.add_local_charm_dir(directory, series)
                                url = result['CharmURL']
                            else:
                                os.system('%s mkdir -p /home/ubuntu/charms/trusty' % (ssh_cmd))
                                os.system('%s %s ubuntu@%s:/home/ubuntu/charms/trusty' % (scp_cmd, directory, self.server))

                        except:
                            self.logger.critical('Error deploying local charm %s: %s' % (service, sys.exc_info()[0]))
                            raise
                    else:
                        try:
                            self.logger.debug("Deploying from online")
                            url = deploy['url']
                        except KeyError:
                            self.logger.critical("Charm url not specified")
                            raise

                    try:
                        if ssl_ok:
                            self.logger.debug("Deploying using: service=%s, url=%s, num_units=%d, to=%s, config=%s" %(service, url, self.req_units, deploy_to, details['config']))
                            self.env.deploy(service, url, num_units=self.req_units, config=config, machine_spec=deploy_to)
                        else:
                            os.system('%s juju deploy --repository=/home/ubuntu/charms --to %s local:trusty/%s %s' % (ssh_cmd, deploy_to, os.path.basename(directory), service))
                            # Apply the config
                            self.apply_config(service, details)
                    except:
                        self.logger.critical('Error deploying %s: %s' % (service, sys.exc_info()[0]))
                        raise

        elif self.cur_units < self.req_units:
            try:
                self.env.add_units(service, (self.req_units - self.cur_units))
            except:
                self.logger.critical('Error adding units for %s: %s' % (self.name, sys.exc_info()[0]))
                raise

        # Wait for the deployed units to start
        try:
            self.logger.debug("Waiting for units to come up")
            self.env.wait_for_units(timeout=self.deploy_timeout)
        except:
            self.logger.critical('Error starting all units for %s: %s' % (service, sys.exc_info()[0]))
            raise

    def execute_on_units(self, service, details):
        units = None
        try:
            units = details['units']
        except KeyError:
            self.logger.info("No units for service %s defined" % service)
            return
        self.logger.debug("Service units def: %s" % units)

        try:
            services = get_service_units(self.env.status())
            depl_units = services[service]
        except KeyError:
            self.logger.error("Unable to get units %s" % services)
            raise
        except:
            self.logger.critical("Error on getting service details for service %s" % service)
            raise

        # Slightly complicated logic to support define actions for
        # specific units.
        # Get all the unit definitions specified
        units_ids = []
        units_no_ids = []
        for unit_conf in units:
            try:
                conf_id = unit_conf['id']
                self.logger.debug("Unit conf id %d" % conf_id)
                units_ids[conf_id] = unit_conf['unit']
            except KeyError:
                units_no_ids.append(unit_conf['unit'])
                continue

        # Go through each unit deployed and apply the actions to the unit
        # if the id is specified, else the first unit available
        no_id_idx = 0
        for unit, status in depl_units.items():
            self.logger.debug("Execute on unit %s with %s" % (unit, status))
            idx = int(unit[unit.index('/')+1:])
            self.logger.debug("Unit index is %d" % idx)
            try:
                unit_conf = units_ids[idx]
                self.logger.debug("Found unit config %s" % unit_conf)
            except IndexError:
                unit_conf = units_no_ids[no_id_idx]
                self.logger.debug("Applying on unit %s" % unit_conf)
                no_id_idx += 1

            bail = False
            try:
                bail = unit_conf['bail']
            except KeyError:
                pass
            wait = False
            try:
                wait = unit_conf['wait']
            except KeyError:
                pass
            self.logger.debug("Bail is %s, Wait is %s" % (bail, wait))

            unit_name = "unit-%s-%d" % (service, idx)
            for entry in unit_conf['actions']:
                for action, params in entry.items():
                    self.logger.debug("Sending action: %s, %s, %s" % (action, unit_name, params))
                    #result = self.actions.enqueue_units([unit], action, params)
                    try:
                        result = self.enqueue_action(action, [unit_name], params)
                        act_status = self.get_action_status(result.uuid)
                    except Exception as e:
                        self.logger.critical("Error applying the action %s on %s with params %s" % (action, unit, params))
                        raise e

                    self.logger.debug("Action %s status is %s on %s" % (action, act_status, unit))
                    while wait and ((act_status == 'pending') or (act_status == 'running')):
                        time.sleep(1)
                        act_status = self.get_action_status(result.uuid)
                        self.logger.debug("Action %s status is %s on %s" % (action, act_status, unit))
                    if bail and (act_status ==  'failed'):
                        self.logger.critical("Error applying action %s on %s with %s" % (action, unit, params))
                        raise RuntimeError("Error applying action %s on %s with %s" % (action, unit, params))

    def remove_units(self, service, details):
        if self.cur_units == 0:
            # Nothing to do
            return
        try:
            units = details['units']
        except KeyError:
            self.logger.debug("remove_units: No units specified")
            return

        for unit in units:
            self.logger.debug("Check destroy units for %s, %s" %(service, unit))
            try:
                if unit['destroy'] == False:
                    continue
            except KeyError:
                continue
            try:
                idx = unit['id']
            except KeyError:
                self.logger.error("Need to specify unit id to destroy")
                continue

            unit = '%s/%d' % (service, idx)
            self.logger.debug("Destroy unit %s" % unit)
            try:
                status = self.env.status()['Services'][service]['Units'][unit]
            except KeyError:
                status = None
            self.logger.debug("Status of unit %s" % status)
            if status is None:
                continue
            unit_name = "unit-%s-%d" %(service, idx)
            self.logger.debug("Destroying unit %s" % unit_name)
            self.env.remove_units([unit_name])

    def execute (self):
        for service, details in self.yaml.items():
            self.cur_units = 0
            self.req_units = 0
            self.charm = service
            try:
                self.charm = details['charm']
            except KeyError:
                pass

            self.logger.debug("Service: %s - %s" % (service, details))
            services = self.env.status()['Services']
            self.logger.debug("Services : %s" % services)
            cur_units = 0
            try:
                cur_units = len(services[service]['Units'])
            except KeyError:
                pass
            req_units = 0
            try:
                req_units = len(details['units'])
            except KeyError:
                # Deploy atleast one unit
                req_units = 1

            self.logger.debug("Units requested: %d, deployed: %d" % (req_units, cur_units))
            self.cur_units = cur_units
            self.req_units = req_units
            destroy = False
            try:
                destroy = details['destroy']
            except KeyError:
                pass
            if destroy:
                if cur_units == 0:
                    # Nothing to do
                    return
                # Execute any commands for units before destroy as this could have
                # side effects for something like proxy charms
                self.execute_on_units(service, details)
                self.logger.debug("Destroying service %s" % service)
                self.env.destroy_service(service)
                return
            # Apply config on already running units first
            self.apply_config(service, details)
            self.deploy_service(service, details)
            self.execute_on_units(service, details)
            # Removing units after execute to run any cleanup actions
            self.remove_units(service, details)

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




if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Perform actions on Juju')
    parser.add_argument("-s", "--server", required=True, help="Juju API server")
    parser.add_argument("-u", "--user", default='user-admin', help="User, default user-admin")
    parser.add_argument("-p", "--password", default='nfvjuju', help="Password for the user")
    parser.add_argument("-P", "--port", default="17070", help="Port number, default 17070")
    parser.add_argument("-c", "--ca-cert", default=None, help="CA certificate for the server");
    parser.add_argument("-T", "--deploy-timeout", default=600, help="Timeout when bringing up units, default 600")
    parser.add_argument("--debug", action="store_true")
    parser.add_argument("file", help="File with commands, config parameters and actions")
    args = parser.parse_args()

    if args.debug:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.ERROR)
    logger = logging.getLogger("juju-client")

    #Workaround for certificae failure, insecure. Does not work with ssl module in Python 3.3
    if sys.version_info >= (3,4):
        ssl._create_default_https_context = ssl._create_unverified_context
        ssl_ok=True
    else:
        ssh_cmd = 'ssh -i %s %s@%s' % ('~/.ssh/id_grunt', 'ubuntu', args.server)
        scp_cmd = 'scp -r -i %s %s@%s' % ('~/.ssh/id_grunt', 'ubuntu', args.server)

    ssl_ok=False
    api = API(args, logger)
    api.execute()
