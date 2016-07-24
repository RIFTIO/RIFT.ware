
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import logging

import requests

import json
import re
import socket
import time

import gi
gi.require_version('RwTypes', '1.0')
gi.require_version('RwsdnYang', '1.0')
gi.require_version('RwSdn', '1.0')
gi.require_version('RwTopologyYang','1.0')

from gi.repository import (
    GObject,
    RwSdn, # Vala package
    RwTypes,
    RwsdnYang, 
    RwTopologyYang as RwTl,
    )

import rw_status
import rwlogger


logger = logging.getLogger('rwsdn.sdnodl')
logger.setLevel(logging.DEBUG)


sff_rest_based = True

class UnknownAccountError(Exception):
    pass


class MissingFileError(Exception):
    pass


rwstatus = rw_status.rwstatus_from_exc_map({
    IndexError: RwTypes.RwStatus.NOTFOUND,
    KeyError: RwTypes.RwStatus.NOTFOUND,
    UnknownAccountError: RwTypes.RwStatus.NOTFOUND,
    MissingFileError: RwTypes.RwStatus.NOTFOUND,
    })


class SdnOdlPlugin(GObject.Object, RwSdn.Topology):

    def __init__(self):
        GObject.Object.__init__(self)
        self.sdnodl = SdnOdl()
        

    @rwstatus
    def do_init(self, rwlog_ctx):
        if not any(isinstance(h, rwlogger.RwLogger) for h in logger.handlers):
            logger.addHandler(
                rwlogger.RwLogger(
                    category="rw-cal-log",
                    subcategory="odl", 
                    log_hdl=rwlog_ctx,
                )
            )

    @rwstatus(ret_on_failure=[None])
    def do_validate_sdn_creds(self, account):
        """
        Validates the sdn account credentials for the specified account.
        Performs an access to the resources using Keystone API. If creds
        are not valid, returns an error code & reason string

        @param account - a SDN account

        Returns:
            Validation Code and Details String
        """
        #logger.debug('Received validate SDN creds')
        status = self.sdnodl.validate_account_creds(account)
        #logger.debug('Done with validate SDN creds: %s', type(status))
        return status

    @rwstatus(ret_on_failure=[None])
    def do_get_network_list(self, account):
        """
        Returns the list of discovered networks

        @param account - a SDN account

        """
        logger.debug('Received Get network list: ')
        nwtop = self.sdnodl.get_network_list( account)
        logger.debug('Done with get network list: %s', type(nwtop))
        return nwtop

    @rwstatus(ret_on_failure=[""])
    def do_create_vnffg_chain(self, account,vnffg_chain):
        """
        Creates Service Function chain in ODL

        @param account - a SDN account

        """
        logger.debug('Received Create VNFFG chain ')
        vnffg_id = self.sdnodl.create_sfc( account,vnffg_chain)
        logger.debug('Done with create VNFFG chain with name : %s', vnffg_id)
        return vnffg_id

    @rwstatus
    def do_terminate_vnffg_chain(self, account,vnffg_id):
        """
        Terminate Service Function chain in ODL

        @param account - a SDN account

        """
        logger.debug('Received terminate VNFFG chain for id %s ', vnffg_id)
        # TODO: Currently all the RSP, SFPs , SFFs and SFs are deleted
        # Need to handle deletion of specific RSP, SFFs, SFs etc
        self.sdnodl.terminate_all_sfc(account)
        logger.debug('Done with terminate VNFFG chain with name : %s', vnffg_id)

    @rwstatus(ret_on_failure=[None])
    def do_get_vnffg_rendered_paths(self, account):
        """
           Get ODL Rendered Service Path List (SFC)

           @param account - a SDN account
        """
        vnffg_list = self.sdnodl.get_rsp_list(account)
        return vnffg_list 

    @rwstatus(ret_on_failure=[None])
    def do_create_vnffg_classifier(self, account, vnffg_classifier):
        """
           Add VNFFG Classifier 

           @param account - a SDN account
        """
        classifier_name = self.sdnodl.create_sfc_classifier(account,vnffg_classifier)
        return classifier_name 

    @rwstatus(ret_on_failure=[None])
    def do_terminate_vnffg_classifier(self, account, vnffg_classifier_name):
        """
           Add VNFFG Classifier 

           @param account - a SDN account
        """
        self.sdnodl.terminate_sfc_classifier(account,vnffg_classifier_name)


class Sff(object):
    """
    Create SFF object to hold SFF related details
    """

    def __init__(self,sff_name, mgmt_address, mgmt_port, dp_address, dp_port,sff_dp_name, sff_br_name=''):
        self.name = sff_name
        self.ip = mgmt_address
        self.sff_rest_port = mgmt_port
        self.sff_port = dp_port
        self.dp_name = sff_dp_name
        self.dp_ip = dp_address
        self.br_name = sff_br_name
        self.sf_dp_list = list()
    
    def add_sf_dp_to_sff(self,sf_dp):
        self.sf_dp_list.append(sf_dp)

    def __repr__(self):
        return 'Name:{},Bridge Name:{}, IP: {}, SF List: {}'.format(self.dp_name,self.br_name, self.ip, self.sf_dp_list) 

class SfDpLocator(object):
    """
    Create Service Function Data Plane Locator related Object to hold details related to each DP Locator endpoint
    """
    def __init__(self,name,sfdp_id,vnfr_name,vm_id):
        self.name = name
        self.port_id = sfdp_id
        self.vnfr_name = vnfr_name
        self.vm_id = vm_id
        self.sff_name = None 
        self.ovsdb_tp_name = None

    def _update_sff_name(self,sff_name):
        self.sff_name = sff_name

    def _update_vnf_params(self,service_function_type,address, port,transport_type):
        self.service_function_type = service_function_type
        self.address = address
        self.port = port
        self.transport_type = "service-locator:{}".format(transport_type)

    def __repr__(self):
        return 'Name:{},Port id:{}, VNFR ID: {}, VM ID: {}, SFF Name: {}'.format(self.name,self.port_id, self.vnfr_name, self.vm_id,self.sff_name) 

class SdnOdl(object):
    """
    SDN ODL Class to support REST based API calls
    """

    @property
    def _network_topology_path(self):
        return 'restconf/operational/network-topology:network-topology'  

    @property
    def _node_inventory_path(self):
        return 'restconf/operational/opendaylight-inventory:nodes'  
     
    def _network_topology_rest_url(self,account):
        return '{}/{}'.format(account.odl.url,self._network_topology_path)

    def _node_inventory_rest_url(self,account):
        return '{}/{}'.format(account.odl.url,self._node_inventory_path)

    def _get_rest_url(self,account, rest_path):
        return '{}/{}'.format(account.odl.url,rest_path)


    def _get_peer_termination_point(self,node_inv,tp_id):
        for node in node_inv['nodes']['node']:
            if "node-connector" in node and len(node['node-connector']) > 0:
                for nodec in node['node-connector']:
                    if ("flow-node-inventory:name" in nodec and nodec["flow-node-inventory:name"] == tp_id):
                        return(node['id'], nodec['id'])
        return (None,None)

    def _get_termination_point_mac_address(self,node_inv,tp_id):
        for node in node_inv['nodes']['node']:
            if "node-connector" in node and len(node['node-connector']) > 0:
                for nodec in node['node-connector']:
                    if ("flow-node-inventory:name" in nodec and nodec["flow-node-inventory:name"] == tp_id):
                        return nodec.get("flow-node-inventory:hardware-address")

    def _add_host(self,ntwk,node,term_point,vmid,node_inv):
        for ntwk_node in ntwk.node:
            if ntwk_node.node_id ==  vmid:
                break
        else:
            ntwk_node = ntwk.node.add()
            if "ovsdb:bridge-name" in node:
                ntwk_node.rw_node_attributes.ovs_bridge_name = node["ovsdb:bridge-name"]
            ntwk_node.node_id = vmid
            intf_id = [res for res in term_point['ovsdb:interface-external-ids'] if res['external-id-key'] == 'iface-id']
            if intf_id:
                ntwk_node_tp = ntwk_node.termination_point.add()
                ntwk_node_tp.tp_id = intf_id[0]['external-id-value']
                att_mac = [res for res in term_point['ovsdb:interface-external-ids'] if res['external-id-key'] == 'attached-mac']
                if att_mac:
                    ntwk_node_tp.l2_termination_point_attributes.mac_address = att_mac[0]['external-id-value']
                peer_node,peer_node_tp = self._get_peer_termination_point(node_inv,term_point['tp-id'])
                if peer_node and peer_node_tp:
                    nw_lnk = ntwk.link.add()
                    nw_lnk.source.source_tp = ntwk_node_tp.tp_id
                    nw_lnk.source.source_node = ntwk_node.node_id
                    nw_lnk.destination.dest_tp = term_point['tp-id']
                    nw_lnk.destination.dest_node = node['node-id']
                    nw_lnk.link_id = peer_node_tp + '-' + 'source'

                    nw_lnk = ntwk.link.add()
                    nw_lnk.source.source_tp = term_point['tp-id']
                    nw_lnk.source.source_node = node['node-id']
                    nw_lnk.destination.dest_tp = ntwk_node_tp.tp_id
                    nw_lnk.destination.dest_node = ntwk_node.node_id
                    nw_lnk.link_id = peer_node_tp + '-' + 'dest'

    def _get_address_from_node_inventory(self,node_inv,node_id):
        for node in node_inv['nodes']['node']:
            if node['id'] == node_id:
                return node["flow-node-inventory:ip-address"]
        return None

    def _fill_network_list(self,nw_topo,node_inventory):
        """
        Fill Topology related information
        """
        nwtop = RwTl.YangData_IetfNetwork()

        for topo in nw_topo['network-topology']['topology']:
            if ('node' in topo and len(topo['node']) > 0):
                ntwk = nwtop.network.add()
                ntwk.network_id = topo['topology-id']
                ntwk.server_provided = True
                for node in topo['node']:
                    if ('termination-point' in node and len(node['termination-point']) > 0):
                        ntwk_node = ntwk.node.add()
                        ntwk_node.node_id = node['node-id']
                        addr = self._get_address_from_node_inventory(node_inventory,ntwk_node.node_id)
                        if addr:
                            ntwk_node.l2_node_attributes.management_address.append(addr)
                        for term_point in node['termination-point']:
                            ntwk_node_tp = ntwk_node.termination_point.add()
                            ntwk_node_tp.tp_id = term_point['tp-id']
                            mac_address = self._get_termination_point_mac_address(node_inventory,term_point['tp-id'])
                            if mac_address:
                                ntwk_node_tp.l2_termination_point_attributes.mac_address = mac_address
                            if 'ovsdb:interface-external-ids' in term_point:
                                vm_id = [res for res in term_point['ovsdb:interface-external-ids'] if res['external-id-key'] == 'vm-id']
                                if vm_id:
                                    vmid = vm_id[0]['external-id-value']
                                    self._add_host(ntwk,node,term_point,vmid,node_inventory)
                if ('link' in topo and len(topo['link']) > 0):
                    for link in topo['link']:
                        nw_link = ntwk.link.add()
                        if 'destination' in link:  
                            nw_link.destination.dest_tp = link['destination'].get('dest-tp')
                            nw_link.destination.dest_node = link['destination'].get('dest-node')
                        if 'source' in link:
                            nw_link.source.source_node = link['source'].get('source-node')
                            nw_link.source.source_tp = link['source'].get('source-tp')
                        nw_link.link_id = link.get('link-id')
        return nwtop


    def validate_account_creds(self, account):
        """
            Validate the SDN account credentials by accessing the rest API using the provided credentials
        """
        status = RwsdnYang.SdnConnectionStatus()
        url = '{}/{}'.format(account.odl.url,"restconf")
        try:
            r=requests.get(url,auth=(account.odl.username,account.odl.password))
            r.raise_for_status()
        except requests.exceptions.HTTPError as e:
            msg = "SdnOdlPlugin: SDN account credential validation failed. Exception: %s", str(e)
            #logger.error(msg)
            print(msg)
            status.status = "failure"
            status.details = "Invalid Credentials: %s" % str(e)
        except Exception as e:
            msg = "SdnPdlPlugin: SDN connection failed. Exception: %s", str(e)
            #logger.error(msg)
            print(msg)
            status.status = "failure"
            status.details = "Connection Failed (Invlaid URL): %s" % str(e)
        else:
            print("SDN Successfully connected")
            status.status = "success"
            status.details = "Connection was successful"

        return status

    def get_network_list(self, account):
        """
           Get the networks details from ODL
        """
        url = self._network_topology_rest_url(account)
        r=requests.get(url,auth=(account.odl.username,account.odl.password))
        r.raise_for_status()
        nw_topo = r.json()

        url = self._node_inventory_rest_url(account)
        r = requests.get(url,auth=(account.odl.username,account.odl.password)) 
        r.raise_for_status()
        node_inventory = r.json()
        return self._fill_network_list(nw_topo,node_inventory)

    @property
    def _service_functions_path(self):
        return 'restconf/config/service-function:service-functions'

    @property
    def _service_function_path(self):
        return 'restconf/config/service-function:service-functions/service-function/{}'

    @property
    def _service_function_forwarders_path(self):
        return 'restconf/config/service-function-forwarder:service-function-forwarders'

    @property
    def _service_function_forwarder_path(self):
        return 'restconf/config/service-function-forwarder:service-function-forwarders/service-function-forwarder/{}'

    @property
    def _service_function_chains_path(self):
        return 'restconf/config/service-function-chain:service-function-chains'

    @property
    def _service_function_chain_path(self):
        return 'restconf/config/service-function-chain:service-function-chains/service-function-chain/{}'
   
    @property
    def _sfp_metadata_path(self):
        return 'restconf/config/service-function-path-metadata:service-function-metadata/context-metadata/{}'
   
    @property
    def _sfps_metadata_path(self):
        return 'restconf/config/service-function-path-metadata:service-function-metadata'
   
    @property
    def _sfps_path(self):
        return 'restconf/config/service-function-path:service-function-paths'

    @property
    def _sfp_path(self):
        return 'restconf/config/service-function-path:service-function-paths/service-function-path/{}'


    @property
    def _create_rsp_path(self):
        return 'restconf/operations/rendered-service-path:create-rendered-path'

    @property
    def _delete_rsp_path(self):
        return 'restconf/operations/rendered-service-path:delete-rendered-path'


    @property
    def _get_rsp_paths(self):
        return 'restconf/operational/rendered-service-path:rendered-service-paths'

    @property
    def _get_rsp_path(self):
        return 'restconf/operational/rendered-service-path:rendered-service-paths/rendered-service-path/{}'

    @property
    def _access_list_path(self):
        return 'restconf/config/ietf-access-control-list:access-lists/acl/{}'

    @property
    def _service_function_classifier_path(self):
        return 'restconf/config/service-function-classifier:service-function-classifiers/service-function-classifier/{}'

    @property
    def _access_lists_path(self):
        return 'restconf/config/ietf-access-control-list:access-lists'

    @property
    def _service_function_classifiers_path(self):
        return 'restconf/config/service-function-classifier:service-function-classifiers'


    def _create_sf(self,account,vnffg_chain,sf_dp_list):
        "Create SF"
        sf_json = {}

        for vnf in vnffg_chain.vnf_chain_path:
            for vnfr in vnf.vnfr_ids:
                sf_url = self._get_rest_url(account,self._service_function_path.format(vnfr.vnfr_name))
                print(sf_url)
                r=requests.get(sf_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'})
                # If the SF is not found; create new SF
                if r.status_code == 200:
                    logger.info("SF with name %s is already present in ODL. Skipping update", vnfr.vnfr_name)
                    continue
                elif r.status_code != 404:
                    r.raise_for_status()

                sf_dict = {}
                sf_dict['name'] = vnfr.vnfr_name
                sf_dict['nsh-aware'] = vnf.nsh_aware
                sf_dict['type'] = vnf.service_function_type
                sf_dict['ip-mgmt-address'] = vnfr.mgmt_address
                sf_dict['rest-uri'] = 'http://{}:{}'.format(vnfr.mgmt_address, vnfr.mgmt_port)

                sf_dict['sf-data-plane-locator'] = list()
                for vdu in vnfr.vdu_list:
                    sf_dp = {}
                    if vdu.port_id in sf_dp_list.keys():
                        sf_dp_entry = sf_dp_list[vdu.port_id]
                        sf_dp['name'] = sf_dp_entry.name
                        sf_dp['ip'] = vdu.address
                        sf_dp['port'] = vdu.port
                        sf_dp['transport'] = "service-locator:{}".format(vnf.transport_type)
                        if vnfr.sff_name:
                            sf_dp['service-function-forwarder'] = vnfr.sff_name
                        else:
                            sff_name = sf_dp_entry.sff_name
                            if sff_name is None:
                                logger.error("SFF not found for port %s in SF %s", vdu.port_id, vnfr.vnfr_name)
                            sf_dp['service-function-forwarder'] = sff_name
                            sf_dp['service-function-ovs:ovs-port'] = dict()
                            if sf_dp_entry.ovsdb_tp_name is not None:
                                sf_dp['service-function-ovs:ovs-port']['port-id'] =  sf_dp_entry.ovsdb_tp_name
                        sf_dict['sf-data-plane-locator'].append(sf_dp)
                    else:
                        logger.error("Port %s not found in SF DP list",vdu.port_id)

                sf_json['service-function'] = sf_dict
                sf_data = json.dumps(sf_json)
                sf_url = self._get_rest_url(account,self._service_function_path.format(vnfr.vnfr_name))
                print(sf_url)
                print(sf_data)
                r=requests.put(sf_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'}, data=sf_data)
                r.raise_for_status()


    def _create_sff(self,account,vnffg_chain,sff):
        "Create SFF"
        sff_json = {}
        sff_dict = {}
        #sff_dp_name = "SFF1" + '-' + 'DP1'
        sff_dp_name = sff.dp_name 
                
        sff_url = self._get_rest_url(account,self._service_function_forwarder_path.format(sff.name))
        print(sff_url)
        r=requests.get(sff_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'})
        # If the SFF is not found; create new SF
        if r.status_code == 200:
            logger.info("SFF with name %s is already present in ODL. Skipping full update", sff.name)
            sff_dict = r.json()
            sff_updated = False
            for sf_dp in sff.sf_dp_list:
                for sff_sf in sff_dict['service-function-forwarder'][0]['service-function-dictionary']:
                    if sf_dp.vnfr_name == sff_sf['name']:
                        logger.info("SF with name %s is already found in SFF %s SF Dictionay. Skipping update",sf_dp.vnfr_name,sff.name) 
                        break
                else:
                    logger.info("SF with name %s is not found in SFF %s SF Dictionay",sf_dp.vnfr_name, sff.name)
                    sff_updated = True
                    sff_sf_dict = {}
                    sff_sf_dp_loc = {}
                    sff_sf_dict['name'] = sf_dp.vnfr_name

                    # Below two lines are enabled only for ODL Beryillium
                    sff_sf_dp_loc['sff-dpl-name'] = sff_dp_name
                    sff_sf_dp_loc['sf-dpl-name'] = sf_dp.name

                    sff_sf_dict['sff-sf-data-plane-locator'] = sff_sf_dp_loc
                    sff_dict['service-function-forwarder'][0]['service-function-dictionary'].append(sff_sf_dict)
            if sff_updated is True:
                sff_data = json.dumps(sff_dict)
                print(sff_data)
                r=requests.put(sff_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'}, data=sff_data)
                r.raise_for_status()
            return        
        elif r.status_code != 404:
            r.raise_for_status()
        
        sff_name = sff.name
        sff_ip = sff.ip
        sff_dp_ip = sff.dp_ip
        sff_port = sff.sff_port
        sff_bridge_name = ''
        sff_rest_port = sff.sff_rest_port
        sff_ovs_op = {}
        if sff_rest_based is False:
            sff_bridge_name = sff.br_name
            sff_ovs_op  = {"key": "flow",
                       "nshc1": "flow",
                       "nsp": "flow",
                       "remote-ip": "flow",
                       "dst-port": sff_port,
                       "nshc3": "flow",
                       "nshc2": "flow",
                       "nshc4": "flow",
                       "nsi": "flow"}


        sff_dict['name'] = sff_name
        sff_dict['service-node'] = ''
        sff_dict['ip-mgmt-address'] = sff_ip
        if sff_rest_based:
            sff_dict['rest-uri'] = 'http://{}:{}'.format(sff_ip, sff_rest_port)
        else:
            sff_dict['service-function-forwarder-ovs:ovs-bridge'] = {"bridge-name": sff_bridge_name}
        sff_dict['service-function-dictionary'] = list()
        for sf_dp in sff.sf_dp_list:
            sff_sf_dict = {}
            sff_sf_dp_loc = {}
            sff_sf_dict['name'] = sf_dp.vnfr_name

            # Below set of lines are reqd for Lithium
            #sff_sf_dict['type'] = sf_dp.service_function_type
            #sff_sf_dp_loc['ip'] = sf_dp.address
            #sff_sf_dp_loc['port'] = sf_dp.port
            #sff_sf_dp_loc['transport'] = sf_dp.transport_type
            #sff_sf_dp_loc['service-function-forwarder-ovs:ovs-bridge'] = {}

            # Below two lines are enabled only for ODL Beryillium
            sff_sf_dp_loc['sff-dpl-name'] = sff_dp_name
            sff_sf_dp_loc['sf-dpl-name'] = sf_dp.name

            sff_sf_dict['sff-sf-data-plane-locator'] = sff_sf_dp_loc
            sff_dict['service-function-dictionary'].append(sff_sf_dict)

        sff_dict['sff-data-plane-locator'] = list()
        sff_dp = {}
        dp_loc = {} 
        sff_dp['name'] = sff_dp_name 
        dp_loc['ip'] = sff_dp_ip
        dp_loc['port'] = sff_port
        dp_loc['transport'] = 'service-locator:vxlan-gpe'
        sff_dp['data-plane-locator'] = dp_loc
        if sff_rest_based is False:
            sff_dp['service-function-forwarder-ovs:ovs-options'] = sff_ovs_op
            #sff_dp["service-function-forwarder-ovs:ovs-bridge"] = {'bridge-name':sff_bridge_name}
            sff_dp["service-function-forwarder-ovs:ovs-bridge"] = {}
        sff_dict['sff-data-plane-locator'].append(sff_dp)

        sff_json['service-function-forwarder'] = sff_dict
        sff_data = json.dumps(sff_json)
        print(sff_data)
        r=requests.put(sff_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'}, data=sff_data)
        r.raise_for_status()

    def _create_sfc(self,account,vnffg_chain):
        "Create SFC"
        sfc_json = {}
        sfc_dict = {}
        sfc_dict['name'] = vnffg_chain.name
        sfc_dict['sfc-service-function'] = list()
        vnf_chain_list = sorted(vnffg_chain.vnf_chain_path, key = lambda x: x.order)
        for vnf in vnf_chain_list:
            sfc_sf_dict = {}
            sfc_sf_dict['name'] = vnf.service_function_type
            sfc_sf_dict['type'] = vnf.service_function_type
            sfc_sf_dict['order'] = vnf.order 
            sfc_dict['sfc-service-function'].append(sfc_sf_dict)
        sfc_json['service-function-chain'] = sfc_dict
        sfc_data = json.dumps(sfc_json)
        sfc_url = self._get_rest_url(account,self._service_function_chain_path.format(vnffg_chain.name))
        print(sfc_url)
        print(sfc_data)
        r=requests.put(sfc_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'}, data=sfc_data)
        r.raise_for_status()
       
    def _create_sfp_metadata(self,account,sfc_classifier):
        " Create SFP metadata"
        sfp_meta_json = {}
        sfp_meta_dict = {}
        sfp_meta_dict['name'] = sfc_classifier.name
        if sfc_classifier.vnffg_metadata.ctx1:
            sfp_meta_dict['context-header1'] = sfc_classifier.vnffg_metadata.ctx1
        if sfc_classifier.vnffg_metadata.ctx2:
            sfp_meta_dict['context-header2'] = sfc_classifier.vnffg_metadata.ctx2
        if sfc_classifier.vnffg_metadata.ctx3:
            sfp_meta_dict['context-header3'] = sfc_classifier.vnffg_metadata.ctx3
        if sfc_classifier.vnffg_metadata.ctx4:
            sfp_meta_dict['context-header4'] = sfc_classifier.vnffg_metadata.ctx4

        sfp_meta_json['context-metadata'] = sfp_meta_dict
        sfp_meta_data = json.dumps(sfp_meta_json)
        sfp_meta_url = self._get_rest_url(account,self._sfp_metadata_path.format(sfc_classifier.name))
        print(sfp_meta_url)
        print(sfp_meta_data)
        r=requests.put(sfp_meta_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'}, data=sfp_meta_data)
        r.raise_for_status()

    def _create_sfp(self,account,vnffg_chain, sym_chain=False,classifier_name=None,vnffg_metadata_name=None):
        "Create SFP"
        sfp_json = {}
        sfp_dict = {}
        sfp_dict['name'] = vnffg_chain.name
        sfp_dict['service-chain-name'] = vnffg_chain.name
        sfp_dict['symmetric'] = sym_chain
        sfp_dict['transport-type'] = 'service-locator:vxlan-gpe'
        if vnffg_metadata_name:
            sfp_dict['context-metadata'] = vnffg_metadata_name 
        if classifier_name: 
            sfp_dict['classifier'] = classifier_name 

        sfp_json['service-function-path'] = sfp_dict
        sfp_data = json.dumps(sfp_json)
        sfp_url = self._get_rest_url(account,self._sfp_path.format(vnffg_chain.name))
        print(sfp_url)
        print(sfp_data)
        r=requests.put(sfp_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'}, data=sfp_data)
        r.raise_for_status()

    def _create_rsp(self,account,vnffg_chain_name, sym_chain=True):
        "Create RSP"
        rsp_json = {}
        rsp_input = {}
        rsp_json['input'] = {}
        rsp_input['name'] = vnffg_chain_name
        rsp_input['parent-service-function-path'] = vnffg_chain_name
        rsp_input['symmetric'] = sym_chain

        rsp_json['input'] = rsp_input
        rsp_data = json.dumps(rsp_json)
        self._rsp_data = rsp_json
        rsp_url = self._get_rest_url(account,self._create_rsp_path)
        print(rsp_url)
        print(rsp_data)
        r=requests.post(rsp_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'}, data=rsp_data)
        r.raise_for_status()
        print(r.json())
        output_json = r.json()
        return output_json['output']['name']
        
    def _get_sff_list_for_chain(self, account,sf_dp_list):
        """
        Get List of all SFF that needs to be created based on VNFs included in VNFFG chain.
        """

        sff_list = {}
        if sf_dp_list is None:
            logger.error("VM List for vnffg chain is empty while trying to get SFF list")
        url = self._network_topology_rest_url(account)
        r=requests.get(url,auth=(account.odl.username,account.odl.password))
        r.raise_for_status()
        nw_topo = r.json()

        for topo in nw_topo['network-topology']['topology']:
            if ('node' in topo and len(topo['node']) > 0):
                for node in topo['node']:
                    if ('termination-point' in node and len(node['termination-point']) > 0):
                        for term_point in node['termination-point']:
                            if 'ovsdb:interface-external-ids' in term_point:
                                vm_id = [res for res in term_point['ovsdb:interface-external-ids'] if res['external-id-key'] == 'vm-id']
                                if len(vm_id) == 0:
                                    continue
                                vmid = vm_id[0]['external-id-value']
                                intf_id = [res for res in term_point['ovsdb:interface-external-ids'] if res['external-id-key'] == 'iface-id']
                                if len(intf_id) == 0:
                                    continue 
                                intfid = intf_id[0]['external-id-value'] 
                                if intfid not in sf_dp_list.keys():
                                    continue
                                if sf_dp_list[intfid].vm_id != vmid:
                                    logger.error("Intf ID %s is not present in VM %s", intfid, vmid)  
                                    continue 
                                sf_dp_list[intfid].ovsdb_tp_name = term_point['ovsdb:name']
                           
                                if 'ovsdb:managed-by' in node:
                                    rr=re.search('network-topology:node-id=\'([-\w\:\/]*)\'',node['ovsdb:managed-by'])
                                    node_id = rr.group(1)
                                    ovsdb_node = [node  for node in topo['node'] if node['node-id'] == node_id]
                                    if ovsdb_node:
                                        if 'ovsdb:connection-info' in ovsdb_node[0]:
                                            sff_ip = ovsdb_node[0]['ovsdb:connection-info']['local-ip']
                                            sff_br_name = node['ovsdb:bridge-name']
                                            sff_br_uuid = node['ovsdb:bridge-uuid']
                                            sff_dp_ip = sff_ip

                                            if 'ovsdb:openvswitch-other-configs' in  ovsdb_node[0]: 
                                                for other_key in ovsdb_node[0]['ovsdb:openvswitch-other-configs']:
                                                    if other_key['other-config-key'] == 'local_ip':
                                                        local_ip_str = other_key['other-config-value']
                                                        sff_dp_ip = local_ip_str.split(',')[0]
                                                        break

                                            sff_name = socket.getfqdn(sff_ip)
                                            if sff_br_uuid in sff_list:
                                                sff_list[sff_name].add_sf_dp_to_sff(sf_dp_list[intfid])
                                                sf_dp_list[intfid]._update_sff_name(sff_name)
                                            else:
                                                sff_dp_ip = sff_ip   #overwrite sff_dp_ip to SFF ip for now
                                                sff_list[sff_name] = Sff(sff_name,sff_ip,6000, sff_dp_ip, 4790,sff_br_uuid,sff_br_name)
                                                sf_dp_list[intfid]._update_sff_name(sff_name)
                                                sff_list[sff_name].add_sf_dp_to_sff(sf_dp_list[intfid])
        return sff_list
                                         

    def _get_sf_dp_list_for_chain(self,account,vnffg_chain):
        """
        Get list of all Service Function Data Plane Locators present in VNFFG 
        useful for easy reference while creating SF and SFF
        """
        sfdp_list = {}
        for vnf in vnffg_chain.vnf_chain_path:
            for vnfr in vnf.vnfr_ids:
                for vdu in vnfr.vdu_list:
                    sfdp = SfDpLocator(vdu.name,vdu.port_id,vnfr.vnfr_name, vdu.vm_id)
                    sfdp._update_vnf_params(vnf.service_function_type, vdu.address, vdu.port, vnf.transport_type)
                    if vnfr.sff_name:
                        sfdp._update_sff_name(vnfr.sff_name)
                    sfdp_list[vdu.port_id] = sfdp 
        return sfdp_list

    def create_sfc(self, account, vnffg_chain):
        "Create SFC chain"

        sff_list = {}
        sf_dp_list = {}

        sf_dp_list = self._get_sf_dp_list_for_chain(account,vnffg_chain)

        if sff_rest_based is False and len(vnffg_chain.sff) == 0:
            # Get the list of all SFFs required for vnffg chain
            sff_list = self._get_sff_list_for_chain(account,sf_dp_list)

        for sff in vnffg_chain.sff:
          sff_list[sff.name] = Sff(sff.name, sff.mgmt_address,sff.mgmt_port,sff.dp_endpoints[0].address, sff.dp_endpoints[0].port, sff.name)
          for _,sf_dp in sf_dp_list.items():
              if sf_dp.sff_name and sf_dp.sff_name == sff.name:
                  sff_list[sff.name].add_sf_dp_to_sff(sf_dp) 

        #Create all the SF in VNFFG chain
        self._create_sf(account,vnffg_chain,sf_dp_list)

        for _,sff in sff_list.items():
            self._create_sff(account,vnffg_chain,sff)


        self._create_sfc(account,vnffg_chain)

        self._create_sfp(account,vnffg_chain,classifier_name=vnffg_chain.classifier_name,
                                   vnffg_metadata_name=vnffg_chain.classifier_name)

        ## Update to SFF could have deleted some RSP; so get list of SFP and 
        ## check RSP exists for same and create any as necessary
        #rsp_name = self._create_rsp(account,vnffg_chain)
        #return rsp_name
        self._create_all_rsps(account)
        self._recreate_all_sf_classifiers(account)
        return vnffg_chain.name

    def _recreate_all_sf_classifiers(self,account):
        """
        Re create all SF classifiers
        """
        sfcl_url = self._get_rest_url(account,self._service_function_classifiers_path)
        print(sfcl_url)
        #Get the classifier
        r=requests.get(sfcl_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'})
        if r.status_code == 200:
            print(r)
            sfcl_json = r.json()
        elif r.status_code == 404:
            return         
        else: 
            r.raise_for_status()

        #Delete the classifiers and re-add same back
        r=requests.delete(sfcl_url,auth=(account.odl.username,account.odl.password))
        r.raise_for_status()
        #Readd it back
        time.sleep(3)
        print(sfcl_json)
        sfcl_data = json.dumps(sfcl_json)
        r=requests.put(sfcl_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'}, data=sfcl_data)
        r.raise_for_status()

    def _create_all_rsps(self,account):
        """
        Create all the RSPs for SFP found
        """
        sfps_url = self._get_rest_url(account,self._sfps_path)
        r=requests.get(sfps_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'})
        r.raise_for_status()
        sfps_json = r.json()
        if 'service-function-path' in sfps_json['service-function-paths']:
            for sfp in sfps_json['service-function-paths']['service-function-path']:
                rsp_url = self._get_rest_url(account,self._get_rsp_path.format(sfp['name']))
                r = requests.get(rsp_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'})
                if r.status_code == 404:
                    # Create the RSP
                    logger.info("Creating RSP for Service Path with name %s",sfp['name'])
                    self._create_rsp(account,sfp['name'])

    def delete_all_sf(self, account):
        "Delete all the SFs"
        sf_url = self._get_rest_url(account,self._service_functions_path)
        print(sf_url)
        r=requests.delete(sf_url,auth=(account.odl.username,account.odl.password))
        r.raise_for_status()


    def delete_all_sff(self, account):
        "Delete all the SFFs"
        sff_url = self._get_rest_url(account,self._service_function_forwarders_path)
        print(sff_url)
        r=requests.delete(sff_url,auth=(account.odl.username,account.odl.password))
        r.raise_for_status()

    def delete_all_sfc(self, account):
        "Delete all the SFCs"
        sfc_url = self._get_rest_url(account,self._service_function_chains_path)
        print(sfc_url)
        r=requests.delete(sfc_url,auth=(account.odl.username,account.odl.password))
        r.raise_for_status()

    def delete_all_sfp_metadata(self, account):
        "Delete all the SFPs metadata"
        sfp_metadata_url = self._get_rest_url(account,self._sfps_metadata_path)
        print(sfp_metadata_url)
        r=requests.delete(sfp_metadata_url,auth=(account.odl.username,account.odl.password))
        r.raise_for_status()

    def delete_all_sfp(self, account):
        "Delete all the SFPs"
        sfp_url = self._get_rest_url(account,self._sfps_path)
        print(sfp_url)
        r=requests.delete(sfp_url,auth=(account.odl.username,account.odl.password))
        r.raise_for_status()

    def delete_all_rsp(self, account):
        "Delete all the RSP"
        #rsp_list = self.get_rsp_list(account)
        url = self._get_rest_url(account,self._get_rsp_paths)
        print(url)
        r = requests.get(url,auth=(account.odl.username,account.odl.password)) 
        r.raise_for_status()
        print(r.json())
        rsp_list = r.json()

        #for vnffg in rsp_list.vnffg_rendered_path: 
        for sfc_rsp in rsp_list['rendered-service-paths']['rendered-service-path']:
            rsp_json = {}
            rsp_input = {}
            rsp_json['input'] = {}
            rsp_input['name'] = sfc_rsp['name']

            rsp_json['input'] = rsp_input
            rsp_data = json.dumps(rsp_json)
            self._rsp_data = rsp_json
            rsp_url = self._get_rest_url(account,self._delete_rsp_path)
            print(rsp_url)
            print(rsp_data)

            r=requests.post(rsp_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'}, data=rsp_data)
            r.raise_for_status()
            print(r.json())
            #output_json = r.json()
            #return output_json['output']['name']
            
    def terminate_all_sfc(self, account):
        "Terminate SFC chain"
        self.delete_all_rsp(account)
        self.delete_all_sfp(account)
        self.delete_all_sfc(account)
        self.delete_all_sff(account)
        self.delete_all_sf(account)

    def _fill_rsp_list(self,sfc_rsp_list,sff_list):
        vnffg_rsps = RwsdnYang.VNFFGRenderedPaths()
        for sfc_rsp in sfc_rsp_list['rendered-service-paths']['rendered-service-path']:
            rsp = vnffg_rsps.vnffg_rendered_path.add()
            rsp.name = sfc_rsp['name']
            rsp.path_id = sfc_rsp['path-id']
            for sfc_rsp_hop in sfc_rsp['rendered-service-path-hop']:
                rsp_hop = rsp.rendered_path_hop.add()
                rsp_hop.hop_number =  sfc_rsp_hop['hop-number']
                rsp_hop.service_index = sfc_rsp_hop['service-index']
                rsp_hop.vnfr_name =  sfc_rsp_hop['service-function-name']
                rsp_hop.service_function_forwarder.name = sfc_rsp_hop['service-function-forwarder']
                for sff in sff_list['service-function-forwarders']['service-function-forwarder']:
                    if sff['name'] == rsp_hop.service_function_forwarder.name:
                        rsp_hop.service_function_forwarder.ip_address = sff['sff-data-plane-locator'][0]['data-plane-locator']['ip']
                        rsp_hop.service_function_forwarder.port = sff['sff-data-plane-locator'][0]['data-plane-locator']['port']
                        break
        return vnffg_rsps
             

    def get_rsp_list(self,account):
        "Get RSP list"

        sff_url = self._get_rest_url(account,self._service_function_forwarders_path)
        print(sff_url)
        r=requests.get(sff_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'})
        r.raise_for_status()
        sff_list = r.json()

        url = self._get_rest_url(account,self._get_rsp_paths)
        print(url)
        r = requests.get(url,auth=(account.odl.username,account.odl.password)) 
        r.raise_for_status()
        print(r.json())
        return self._fill_rsp_list(r.json(),sff_list)

    def create_sfc_classifier(self, account, sfc_classifiers):
        "Create SFC Classifiers"
        self._create_sfp_metadata(account,sfc_classifiers)
        self._add_acl_rules(account, sfc_classifiers)
        self._create_sf_classifier(account, sfc_classifiers)
        return sfc_classifiers.name

    def terminate_sfc_classifier(self, account, sfc_classifier_name):
        "Create SFC Classifiers"
        self.delete_all_sfp_metadata(account)
        self._terminate_sf_classifier(account, sfc_classifier_name)
        self._del_acl_rules(account, sfc_classifier_name)

    def _del_acl_rules(self,account,sfc_classifier_name):
        " Terminate SF classifiers"
        acl_url = self._get_rest_url(account,self._access_lists_path)
        print(acl_url)
        r=requests.delete(acl_url,auth=(account.odl.username,account.odl.password))
        r.raise_for_status()

    def _terminate_sf_classifier(self,account,sfc_classifier_name):
        " Terminate SF classifiers"
        sfcl_url = self._get_rest_url(account,self._service_function_classifiers_path)
        print(sfcl_url)
        r=requests.delete(sfcl_url,auth=(account.odl.username,account.odl.password))
        r.raise_for_status()

    def _create_sf_classifier(self,account,sfc_classifiers):
        " Create SF classifiers"
        sf_classifier_json = {}
        sf_classifier_dict = {}
        sf_classifier_dict['name'] = sfc_classifiers.name
        sf_classifier_dict['access-list'] = sfc_classifiers.name
        sf_classifier_dict['scl-service-function-forwarder'] = list()
        scl_sff = {}
        scl_sff_name = ''

        if sfc_classifiers.has_field('sff_name') and sfc_classifiers.sff_name is not None:
            scl_sff_name = sfc_classifiers.sff_name
        elif  sfc_classifiers.has_field('port_id') and sfc_classifiers.has_field('vm_id'):
            sf_dp = SfDpLocator(sfc_classifiers.port_id, sfc_classifiers.port_id,'', sfc_classifiers.vm_id)
            sf_dp_list= {}
            sf_dp_list[sfc_classifiers.port_id] = sf_dp
            self._get_sff_list_for_chain(account,sf_dp_list)

            if sf_dp.sff_name is None:
                logger.error("SFF not found for port %s, VM: %s",sfc_classifiers.port_id,sfc_classifiers.vm_id) 
            else:
                logger.info("SFF with name %s  found for port %s, VM: %s",sf_dp.sff_name, sfc_classifiers.port_id,sfc_classifiers.vm_id) 
                scl_sff_name = sf_dp.sff_name
        else:
            rsp_url = self._get_rest_url(account,self._get_rsp_path.format(sfc_classifiers.rsp_name))
            r = requests.get(rsp_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'})
            if r.status_code == 200:
                rsp_data = r.json()
                if 'rendered-service-path' in rsp_data and len(rsp_data['rendered-service-path'][0]['rendered-service-path-hop']) > 0:
                    scl_sff_name = rsp_data['rendered-service-path'][0]['rendered-service-path-hop'][0]['service-function-forwarder']
        
        logger.debug("SFF for classifer %s found is %s",sfc_classifiers.name, scl_sff_name)        
        scl_sff['name'] = scl_sff_name
        #scl_sff['interface'] = sff_intf_name
        sf_classifier_dict['scl-service-function-forwarder'].append(scl_sff)

        sf_classifier_json['service-function-classifier'] = sf_classifier_dict

        sfcl_data = json.dumps(sf_classifier_json)
        sfcl_url = self._get_rest_url(account,self._service_function_classifier_path.format(sfc_classifiers.name))
        print(sfcl_url)
        print(sfcl_data)
        r=requests.put(sfcl_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'}, data=sfcl_data)
        r.raise_for_status()

    def _add_acl_rules(self, account,sfc_classifiers):
        "Create ACL rules"
        access_list_json = {}
        access_list_dict = {}
        acl_entry_list = list()
        acl_list_dict = {}
        for acl_rule in sfc_classifiers.match_attributes:
            acl_entry = {} 
            acl_entry['rule-name']  = acl_rule.name
            acl_entry['actions'] = {}
            #acl_entry['actions']['netvirt-sfc-acl:rsp-name'] = sfc_classifiers.rsp_name
            acl_entry['actions']['service-function-acl:rendered-service-path'] = sfc_classifiers.rsp_name

            matches = {}
            for field, value in acl_rule.as_dict().items():
                if field == 'ip_proto':
                    matches['protocol'] = value
                elif field == 'source_ip_address':
                    matches['source-ipv4-network'] = value
                elif field == 'destination_ip_address':
                    matches['destination-ipv4-network'] = value
                elif field == 'source_port':
                    matches['source-port-range'] = {'lower-port':value, 'upper-port':value}
                elif field == 'destination_port':
                    matches['destination-port-range'] = {'lower-port':value, 'upper-port':value}
            acl_entry['matches'] = matches
            acl_entry_list.append(acl_entry)    
        acl_list_dict['ace'] = acl_entry_list 
        access_list_dict['acl-name'] = sfc_classifiers.name
        access_list_dict['access-list-entries'] = acl_list_dict
        access_list_json['acl'] = access_list_dict

        acl_data = json.dumps(access_list_json)
        acl_url = self._get_rest_url(account,self._access_list_path.format(sfc_classifiers.name))
        print(acl_url)
        print(acl_data)
        r=requests.put(acl_url,auth=(account.odl.username,account.odl.password),headers={'content-type': 'application/json'}, data=acl_data)
        r.raise_for_status()
