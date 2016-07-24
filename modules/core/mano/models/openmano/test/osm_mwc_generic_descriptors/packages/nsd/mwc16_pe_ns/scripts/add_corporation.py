#!/usr/bin/env python3
import argparse
import hashlib
import ipaddress
import itertools
import jujuclient
import logging
import sys
import time
import yaml


logging.basicConfig(filename="/tmp/rift_ns_add_corp.log", level=logging.DEBUG)
logger = logging.getLogger()

ch = logging.StreamHandler()
ch.setLevel(logging.INFO)

# create formatter and add it to the handlers
formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
ch.setFormatter(formatter)
logger.addHandler(ch)


dry_run = False

class JujuActionError(Exception):
    pass


class JujuClient(object):
    """Class for executing Juju actions """
    def __init__(self, ip, port, user, passwd):
        self._ip = ip
        self._port = port
        self._user = user
        self._passwd = passwd

        endpoint = 'wss://%s:%d' % (ip, port)
        logger.debug("Using endpoint=%s", endpoint)
        if dry_run:
            return
        self.env = jujuclient.Environment(endpoint)
        self.env.login(passwd, user)

    def get_service(self, name):
        return self.env.get_service(name)

    def _get_units(self, name):
        """
        Get the units associated with service
        """
        units = self.env.status(name)['Services'][name]['Units']
        units = list(units.keys())

        # convert to a friendly format for juju-python-client
        units[:] = [('unit-%s' % u).replace('/', '-') for u in units]
        return units

    def exec_action(self, name, action_name, params, block=False):
        logger.debug("execute actiion %s using params %s", action_name, params)
        if dry_run:
            return

        actions = jujuclient.Actions(self.env)
        results = actions.enqueue_units(self._get_units(name),
                                        action_name,
                                        params)
        if not block:
            return results

        if 'error' in results['results'][0].keys():
            raise JujuActionError("Juju action error: %s" % results['results'][0])

        action = results['results'][0]['action']
        info = actions.info([action])
        i = 0
        logging.debug("Initial action results: %s", results['results'][0])
        while info['results'][0]['status'] not in ['completed', 'failed']:
            time.sleep(1)
            info = actions.info([action])

            # break out if the action doesn't complete in 10 secs
            i += 1
            if i == 10:
                raise JujuActionError("Juju action timed out after 30 seconds")

        if info['results'][0]['status'] != 'completed':
            raise JujuActionError("Action %s failure: %s" % (action_name, info['results'][0]))

        return info


class CharmAction(object):
    def __init__(self, deployed_name, action_name, action_params=None):
        self._deployed_name = deployed_name
        self._action_name = action_name
        self._params = action_params if action_params is not None else []

    def execute(self, juju_client):
        logger.info("Executing charm (%s) action (%s) with params (%s)",
                     self._deployed_name, self._action_name, self._params)
        try:
            info = juju_client.exec_action(
                    name=self._deployed_name,
                    action_name=self._action_name,
                    params=self._params,
                    block=True
                    )

        except JujuActionError as e:
            logger.error("Juju charm (%s) action (%s) failed: %s",
                         self._deployed_name, self._action_name, str(e))
            raise

        logger.debug("Juju charm (%s) action (%s) success.",
                     self._deployed_name, self._action_name)


class DeployedProxyCharm(object):
    def __init__(self, juju_client, service_name, mgmt_ip=None, charm_name=None):
        self._juju_client = juju_client
        self.service_name = service_name
        self.mgmt_ip = mgmt_ip
        self.charm_name = charm_name

    def do_action(self, action_name, action_params={}):
        action = CharmAction(self.service_name, action_name, action_params)
        action.execute(self._juju_client)


class SixWindPEProxyCharm(DeployedProxyCharm):
    USER = "root"
    PASSWD = "6windos"

    def configure_interface(self, iface_name, ipv4_interface_str=None):
        action = "configure-interface"
        params = {'iface-name', iface_name}

        if ipv4_interface_str is None:
            # Use ipaddress module to validate ipv4 interface string
            ip_intf = ipaddress.IPv4Interface(ipv4_interface_str)
            params["cidr"] = ip_intf.with_prefixlen

            self.do_action(action, params)
        else:
            self.do_action(action, params)


    def add_corporation(self, domain_name, user_iface_name, vlan_id, corp_gw,
                        corp_net, local_net="10.255.255.0/24", local_net_area="0"):
        logger.debug("Add corporation called with params: %s", locals())

        action = "add-corporation"
        params = {
                "domain-name": domain_name,
                "iface-name": user_iface_name,
                "vlan-id": int(vlan_id),
                "cidr": corp_net,
                "area": corp_gw,
                "subnet-cidr":local_net,
                "subnet-area":local_net_area,
                }

        self.do_action(action, params)

    def connect_domains(self, domain_name, core_iface_name, local_ip, remote_ip,
                        internal_local_ip, internal_remote_ip, tunnel_name,
                        tunnel_key, tunnel_type="gre"):

        logger.debug("Connect domains called with params: %s", locals())

        action = "connect-domains"
        params = {
                "domain-name": domain_name,
                "iface-name": core_iface_name,
                "tunnel-name": tunnel_name,
                "local-ip": local_ip,
                "remote-ip": remote_ip,
                "tunnel-key": tunnel_key,
                "internal-local-ip": internal_local_ip,
                "internal-remote-ip": internal_remote_ip,
                "tunnel-type":tunnel_type,
                }

        self.do_action(action, params)


class PEGroupConfig(object):
    def __init__(self, pe_group_cfg):
        self._pe_group_cfg = pe_group_cfg

    def _get_param_value(self, param_name):
        for param in self._pe_group_cfg["parameter"]:
            if param["name"] == param_name:
                return param["value"]

        raise ValueError("PE param not found: %s" % param_name)

    @property
    def vlan_id(self):
        return self._get_param_value("Vlan ID")

    @property
    def interface_name(self):
        return self._get_param_value("Interface Name")

    @property
    def corp_network(self):
        return self._get_param_value("Corp. Network")

    @property
    def corp_gateway(self):
        return self._get_param_value("Corp. Gateway")


class AddCorporationRequest(object):
    def __init__(self, add_corporation_rpc):
        self._add_corporation_rpc = add_corporation_rpc

    @property
    def name(self):
        return self._add_corporation_rpc["name"]

    @property
    def param_groups(self):
        return self._add_corporation_rpc["parameter_group"]

    @property
    def params(self):
        return self._add_corporation_rpc["parameter"]

    @property
    def corporation_name(self):
        for param in self.params:
            if param["name"] == "Corporation Name":
                return param["value"]

        raise ValueError("Could not find 'Corporation Name' field")

    @property
    def tunnel_key(self):
        for param in self.params:
            if param["name"] == "Tunnel Key":
                return param["value"]

        raise ValueError("Could not find 'Tunnel Key' field")

    def get_pe_parameter_group_map(self):
        group_name_map = {}
        for group in self.param_groups:
            group_name_map[group["name"]] = group

        return group_name_map

    def get_parameter_name_map(self):
        name_param_map = {}
        for param in self.params:
            name_param_map[param["name"]] = param

        return name_param_map

    @classmethod
    def from_yaml_cfg(cls, yaml_hdl):
        config = yaml.load(yaml_hdl)
        return cls(
                config["rpc_ip"],
                )


class JujuVNFConfig(object):
    def __init__(self, vnfr_index_map, vnf_name_map, vnf_init_config_map):
        self._vnfr_index_map = vnfr_index_map
        self._vnf_name_map = vnf_name_map
        self._vnf_init_config_map = vnf_name_map

    def get_service_name(self, vnf_index):
        for vnfr_id, index in self._vnfr_index_map.items():
            if index != vnf_index:
                continue

            return self._vnf_name_map[vnfr_id]

        raise ValueError("VNF Index not found: %s" % vnf_index)

    def get_vnfr_id(self, vnf_index):
        for vnfr_id, index in self._vnfr_index_map.items():
            if index != vnf_index:
                continue

            return vnfr_id

        raise ValueError("VNF Index not found: %s" % vnf_index)

    @classmethod
    def from_yaml_cfg(cls, yaml_hdl):
        config = yaml.load(yaml_hdl)
        return cls(
                config["vnfr_index_map"],
                config["unit_names"],
                config["init_config"],
                )


class JujuClientConfig(object):
    def __init__(self, juju_ctrl_cfg):
        self._juju_ctrl_cfg = juju_ctrl_cfg

    @property
    def name(self):
        return self._juju_ctrl_cfg["name"]

    @property
    def host(self):
        return self._juju_ctrl_cfg["host"]

    @property
    def port(self):
        return self._juju_ctrl_cfg["port"]

    @property
    def user(self):
        return self._juju_ctrl_cfg["user"]

    @property
    def secret(self):
        return self._juju_ctrl_cfg["secret"]

    @classmethod
    def from_yaml_cfg(cls, yaml_hdl):
        config = yaml.load(yaml_hdl)
        return cls(
                config["config_agent"],
                )


class OSM_MWC_Demo(object):
    VNF_INDEX_NAME_MAP = {
            "PE1": 1,
            "PE2": 2,
            "PE3": 3,
        }

    CORE_PE_CONN_MAP = {
            "PE1": {
                "PE2": {
                    "ifacename": "eth1",
                    "ip": "10.10.10.9",
                    "mask": "30",
                    "internal_local_ip": "10.255.255.1"
                },
                "PE3": {
                    "ifacename": "eth2",
                    "ip": "10.10.10.1",
                    "mask": "30",
                    "internal_local_ip": "10.255.255.1"
                },
            },
            "PE2": {
                "PE1": {
                    "ifacename": "eth1",
                    "ip": "10.10.10.10",
                    "mask": "30",
                    "internal_local_ip": "10.255.255.2"
                },
                "PE3": {
                    "ifacename": "eth2",
                    "ip": "10.10.10.6",
                    "mask": "30",
                    "internal_local_ip": "10.255.255.2"
                }
            },
            "PE3": {
                "PE1": {
                    "ifacename": "eth1",
                    "ip": "10.10.10.2",
                    "mask": "30",
                    "internal_local_ip": "10.255.255.3"
                },
                "PE2": {
                    "ifacename": "eth2",
                    "ip": "10.10.10.5",
                    "mask": "30",
                    "internal_local_ip": "10.255.255.3"
                }
            }
        }

    @staticmethod
    def get_pe_vnf_index(pe_name):
        if pe_name not in OSM_MWC_Demo.VNF_INDEX_NAME_MAP:
            raise ValueError("Could not find PE name: %s", pe_name)

        return OSM_MWC_Demo.VNF_INDEX_NAME_MAP[pe_name]

    @staticmethod
    def get_src_core_iface(src_pe_name, dest_pe_name):
        return OSM_MWC_Demo.CORE_PE_CONN_MAP[src_pe_name][dest_pe_name]["ifacename"]

    @staticmethod
    def get_local_ip(src_pe_name, dest_pe_name):
        return OSM_MWC_Demo.CORE_PE_CONN_MAP[src_pe_name][dest_pe_name]["ip"]

    @staticmethod
    def get_remote_ip(src_pe_name, dest_pe_name):
        return OSM_MWC_Demo.CORE_PE_CONN_MAP[dest_pe_name][src_pe_name]["ip"]

    @staticmethod
    def get_internal_local_ip(src_pe_name, dest_pe_name):
        return OSM_MWC_Demo.CORE_PE_CONN_MAP[src_pe_name][dest_pe_name]["internal_local_ip"]

    @staticmethod
    def get_internal_remote_ip(src_pe_name, dest_pe_name):
        return OSM_MWC_Demo.CORE_PE_CONN_MAP[dest_pe_name][src_pe_name]["internal_local_ip"]


def add_pe_corporation(src_pe_name, src_pe_charm, src_pe_group_cfg, corporation_name):
    domain_name = corporation_name
    vlan_id = src_pe_group_cfg.vlan_id
    corp_gw = src_pe_group_cfg.corp_gateway
    corp_net = src_pe_group_cfg.corp_network

    user_iface = src_pe_group_cfg.interface_name

    src_pe_charm.add_corporation(domain_name, user_iface, vlan_id, corp_gw, corp_net)


def connect_pe_domains(src_pe_name, src_pe_charm, dest_pe_name, corporation_name, tunnel_key):
    domain_name = corporation_name
    core_iface_name = OSM_MWC_Demo.get_src_core_iface(src_pe_name, dest_pe_name)
    local_ip = OSM_MWC_Demo.get_local_ip(src_pe_name, dest_pe_name)
    remote_ip = OSM_MWC_Demo.get_remote_ip(src_pe_name, dest_pe_name)
    internal_local_ip = OSM_MWC_Demo.get_internal_local_ip(src_pe_name, dest_pe_name)
    internal_remote_ip = OSM_MWC_Demo.get_internal_remote_ip(src_pe_name, dest_pe_name)


    src_pe_idx = OSM_MWC_Demo.get_pe_vnf_index(src_pe_name)
    dest_pe_idx = OSM_MWC_Demo.get_pe_vnf_index(dest_pe_name)

    # Create a 4 digit hash of the corporation name
    hash_object = hashlib.md5(corporation_name.encode())
    corp_hash = hash_object.hexdigest()[-4:]

    # Tunnel name is the 4 digit corporation name hash followed by
    # src index and dest index. When there are less than 10 PE's
    # this creates a 8 character tunnel name which is the limit.
    tunnel_name = "".join([corp_hash, "_", str(src_pe_idx), str(dest_pe_idx)])

    src_pe_charm.connect_domains(domain_name, core_iface_name, local_ip, remote_ip,
                                 internal_local_ip, internal_remote_ip, tunnel_name,
                                 tunnel_key)


def main(argv=sys.argv[1:]):
    parser = argparse.ArgumentParser()
    parser.add_argument("yaml_cfg_file", type=argparse.FileType('r'))
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--quiet", "-q", dest="verbose", action="store_false")
    args = parser.parse_args()
    if args.verbose:
        ch.setLevel(logging.DEBUG)

    global dry_run
    dry_run = args.dry_run

    yaml_str = args.yaml_cfg_file.read()

    juju_cfg = JujuClientConfig.from_yaml_cfg(yaml_str)
    juju_client = JujuClient(juju_cfg.host, juju_cfg.port, juju_cfg.user, juju_cfg.secret)

    juju_vnf_config = JujuVNFConfig.from_yaml_cfg(yaml_str)

    rpc_request = AddCorporationRequest.from_yaml_cfg(yaml_str)
    pe_param_group_map = rpc_request.get_pe_parameter_group_map()

    pe_name_charm_map = {}
    for pe_name, pe_group_cfg in pe_param_group_map.items():
        # The PE name (i.e. PE1) must be in the parameter group name so we can correlate
        # to an actual VNF in the descriptor.
        pe_vnf_index = OSM_MWC_Demo.get_pe_vnf_index(pe_name)

        # Get the deployed VNFR charm service name
        pe_charm_service_name = juju_vnf_config.get_service_name(pe_vnf_index)

        pe_name_charm_map[pe_name] = SixWindPEProxyCharm(juju_client, pe_charm_service_name)

    # At this point we have SixWindPEProxyCharm() instances for each PE and each
    # PE param group configuration.
    for src_pe_name in pe_param_group_map:
        add_pe_corporation(
                src_pe_name=src_pe_name,
                src_pe_charm=pe_name_charm_map[src_pe_name],
                src_pe_group_cfg=PEGroupConfig(pe_param_group_map[src_pe_name]),
                corporation_name=rpc_request.corporation_name
                )

    # Create a permutation of all PE's involved in this topology and connect
    # them together by creating tunnels with matching keys
    for src_pe_name, dest_pe_name in itertools.permutations(pe_name_charm_map, 2):
        connect_pe_domains(
                src_pe_name=src_pe_name,
                src_pe_charm=pe_name_charm_map[src_pe_name],
                dest_pe_name=dest_pe_name,
                corporation_name=rpc_request.corporation_name,
                tunnel_key=rpc_request.tunnel_key,
                )

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        logger.exception("Caught exception when executing add_corporation ns")
        raise
