#!/usr/bin/env python3

import sys
import os
import argparse
import uuid
import rift.vcs.component as vcs

import gi
  
gi.require_version('RwNsdYang', '1.0') 
gi.require_version('RwYang', '1.0') 
gi.require_version('RwVnfdYang', '1.0') 
gi.require_version('VnfdYang', '1.0') 
gi.require_version('VldYang', '1.0')

from gi.repository import (
    RwYang,
    VnfdYang,
    RwVnfdYang,
    RwNsdYang,
    VldYang)

class UnknownVNFError(Exception):
    pass

class ManoDescriptor(object):
    def __init__(self, name):
        self.name = name
        self.descriptor = None

    def write_to_file(self, module_list, outdir, output_format):
        model = RwYang.Model.create_libncx()
        for module in module_list:
            model.load_module(module)

        if output_format == 'json':
            with open('%s/%s.json' % (outdir, self.name), "w") as fh:
                fh.write(self.descriptor.to_json(model))
        elif output_format == 'yaml':
            with open('%s/%s.yml' % (outdir, self.name), "w") as fh:
                fh.write(self.descriptor.to_yaml(model))
        elif output_format.strip() == 'xml':
            with open('%s/%s.xml' % (outdir, self.name), "w") as fh:
                fh.write(self.descriptor.to_xml_v2(model, pretty_print=True))
        else:
            raise("Invalid output format for the descriptor")

class VirtualLink(ManoDescriptor):
    def __init__(self, name):
        super(VirtualLink, self).__init__(name)

    def compose(self, cptuple_list):
        self.descriptor = VldYang.YangData_Vld_VldCatalog()
        vld = self.descriptor.vld.add()
        vld.id = str(uuid.uuid1())
        vld.name = self.name
        vld.short_name = self.name
        vld.vendor = 'RIFT.io'
        vld.description = 'Fabric VL'
        vld.version = '1.0'
        vld.type_yang = 'ELAN'

        for cp in cptuple_list:
            cpref = vld.vnfd_connection_point_ref.add()
            cpref.member_vnf_index_ref = cp[0]
            cpref.vnfd_id_ref = cp[1]
            cpref.vnfd_connection_point_ref = cp[2]


    def write_to_file(self, outdir, nsd_name, output_format):
        dirpath = "%s/%s/vld" % (outdir, nsd_name)
        if not os.path.exists(dirpath):
            os.makedirs(dirpath)
        super(VirtualLink, self).write_to_file(["vld", "rw-vld"],
                                               "%s/%s/vld" % (outdir, nsd_name),
                                               output_format)

class RwVirtualLink(VirtualLink):
    def __init__(self, name):
        super(RwVirtualLink, self).__init__(name)

class VirtualNetworkFunction(ManoDescriptor):
    def __init__(self, name):
        self.vnfd_catalog = None
        self.vnfd = None
        super(VirtualNetworkFunction, self).__init__(name)

    def is_exists(self, component_name):
        for component in self.vnfd.component:
            if component_name == component.component_name:
                return True
        return False

    def read_vnf_config(self, filename):
      with open(filename, 'r') as content_file:
        content = content_file.read()
        return content

    def default_config(self, vnfd, src_dir, vnf_info, template_info):
        vnf_config = vnfd.mgmt_interface.vnf_configuration

        conf_file_list = []
        conf_file = ""
        found_CONFIG = False
        for key,value in template_info.items():
           if key.startswith('CONFIG'):
              found_CONFIG = True
              print("Starts With CONFIG %s, value %s" % (key, value['config_file']))
              conf_file_list.append(value['config_file']) 

        if found_CONFIG == False:
            try:                                                                                                                                                           
                conf_file = vnf_info['config_file']
            except KeyError:
                return

        vnf_config.config_access.username = 'admin'
        vnf_config.config_access.password = 'admin'
        vnf_config.config_attributes.config_priority = 0
        vnf_config.config_attributes.config_delay = 0

        # Select "netconf" configuration
        vnf_config.netconf.target = 'running'
        vnf_config.netconf.port = 2022

        # print("### TBR ### vnfd.name = ", vnfd.name)

        if (conf_file != "") or (conf_file_list != []):
            vnf_config.config_attributes.config_priority = int(vnf_info['config_priority'])
            # First priority config delay will delay the entire NS config delay
            vnf_config.config_attributes.config_delay = int(vnf_info['config_delay'])
            if conf_file != "":
               conf_file = src_dir + "/config/" + conf_file
               vnf_config.config_template = self.read_vnf_config(conf_file)
            else:
               for conf_file in conf_file_list:
                  # Read new file
                  print("Reading conf file %s", conf_file)
                  conf_file = src_dir + "/config/" + conf_file
                  if vnf_config.config_template is None:
                      vnf_config.config_template = self.read_vnf_config(conf_file)
                  else:
                      vnf_config.config_template += self.read_vnf_config(conf_file)

    def get_multivm_port_mon_info(self, index, port, min_value, max_value):
        return {
                'id': '%d' % ((index+1)*2-1),
                'name': '%s Tx Rate' % port,
                'json_query_method': "OBJECTPATH",
                'value_type': "INT",
                'numeric_constraints': {'min_value':min_value, 'max_value':max_value},
                'json_query_params': {'object_path':"$..*[@.portname is '%s'].counters.'tx-rate-mbps'" % port},
                'description': 'Outgoing byte rate of interface',
                'group_tag': 'Group%d' % (index+1),
                'widget_type': 'GAUGE',
                'units': 'mbps'
               }, {
                'id': '%d' % ((index+1)*2),
                'name': '%s Rx Rate' % port,
                'json_query_method': "OBJECTPATH",
                'value_type': "INT",
                'numeric_constraints': {'min_value':min_value, 'max_value':max_value},
                'json_query_params': {'object_path':"$..*[@.portname is '%s'].counters.'rx-rate-mbps'" % port},
                'description': 'Incoming byte rate of interface',
                'group_tag': 'Group%d' % (index+1),
                'widget_type': 'GAUGE',
                'units': 'mbps' }

    def get_multivm_port_latency_info(self, index, port):
        return {
                'id': '%d' % (((index+1)*2-1) + 200),
                'name': '%s Avg Pkt Latency' % port,
                'json_query_method': "OBJECTPATH",
                'value_type': "INT",
                'json_query_params': {'object_path':"$..*[@.portname is '%s'].'rw-trafgen-data:trafgen-info'.'average-latency'" % port},
                'description': 'Average round trip packet latency',
                'group_tag': 'Group%d' % ((index+1) + 200),
                'widget_type': 'COUNTER',
                'units': 'usec'
               }, {
                'id': '%d' % (((index+1)*2) + 200),
                'name': '%s Mean Latency deviation' % port,
                'json_query_method': "OBJECTPATH",
                'value_type': "INT",
                'json_query_params': {'object_path':"$..*[@.portname is '%s'].'rw-trafgen-data:trafgen-info'.'mean-deviation'" % port},
                'description': 'mean deviation of round trip latency',
                'group_tag': 'Group%d' % ((index+1) + 200),
                'widget_type': 'COUNTER',
                'units': 'usec' }

    def add_port_mon(self, vnfd, credentials, bool_port, bool_latency, mon_info):

        min_value = 0
        max_value = 1000
        try:
          min_value = int(mon_info[0])
        except IndexError:
          pass
        try:
          max_value = int(mon_info[1])
        except IndexError:
          pass

        port_mon = []
        i = 0
        if bool_port == 1:
          for cp in vnfd.connection_point:
            x,y = self.get_multivm_port_mon_info(i, cp.name, min_value, max_value);
            port_mon.append(x);
            port_mon.append(y);
            i = i + 1
        i = 0                                                                                                                                                                                           
        if bool_latency == 1:
          for cp in vnfd.connection_point:
            x,y = self.get_multivm_port_latency_info(i, cp.name);
            port_mon.append(x);
            port_mon.append(y);
            i = i + 1


        mon_param  = [
            {'path': "api/readonly/operational/vnf-opdata/vnf/%s,0/port-state" % vnfd.name,
           # {'path': "api/operational/vnf-opdata/vnf/%s,0/port-state" % vnfd.name,
             'port': 8008,
             'https': True,
             'mon-params': port_mon,
             },
            ]

        # HTTP endpoint of Monitoring params
        for endpoint in mon_param:
            endp = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_HttpEndpoint(
                    path=endpoint['path'], port=endpoint['port'], username=credentials['username'],
                    password=credentials['password'],polling_interval_secs=3, https=endpoint['https']
                    )
            vnfd.http_endpoint.append(endp)
            hdr1 = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_HttpEndpoint_Headers(
                            key='Content-type', value='application/vnd.yang.data+json')
            endp.headers.append(hdr1)
            hdr2 = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_HttpEndpoint_Headers(
                            key='Accept', value='json')
            endp.headers.append(hdr2)

            # Monitoring params
            for monp_dict in endpoint['mon-params']:
                monp = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_MonitoringParam.from_dict(monp_dict)
                monp.http_endpoint_ref = endpoint['path']
                vnfd.monitoring_param.append(monp)
        return

    def add_ike1_mon(self, vnfd, credentials):
        ikekey_mon = [{ 
                'id': '1',
                'name': 'IKE re-key Rate',
                'json_query_method': "JSONPATH",
                'value_type': "INT",
                'numeric_constraints': {'min_value':0, 'max_value':10000},
                'json_query_params': {'json_path':"$..statistics.rekey.rate"},
                'description': 'IKEv2 Diffie Hellman key exchange rate',
                'group_tag': 'Group100',
                'widget_type': 'GAUGE',
                'units': 'rekeys/sec'
               },
               { 
                'id': '2',
                'name': 'IPSec Tunnels',
                'json_query_method': "JSONPATH",
                'value_type': "INT",
                'json_query_params': {'json_path':"$..statistics.state.'ike-sas'"},
                'description': 'IPSec Tunnels',
                'group_tag': 'Group100',
                'widget_type': 'COUNTER',
                'units': 'tunnels'
               } ]
        mon_param  = [
              {'path': "api/readonly/operational/vnf-opdata/vnf/%s,0/ipsec-service-statistics/statistics" % vnfd.name,
               'port': 8008,
               'https': True,
               'mon-params': ikekey_mon,
               },
              ]
        # HTTP endpoint of Monitoring params
        for endpoint in mon_param:
              endp = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_HttpEndpoint(
                      path=endpoint['path'], port=endpoint['port'], username=credentials['username'],
                    password=credentials['password'],polling_interval_secs=3, https=endpoint['https']
                      )
              vnfd.http_endpoint.append(endp)
              hdr1 = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_HttpEndpoint_Headers( 
                              key='Content-type', value='application/vnd.yang.data+json')
              endp.headers.append(hdr1)
              hdr2 = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_HttpEndpoint_Headers(
                              key='Accept', value='json')
              endp.headers.append(hdr2)
  
              # Monitoring params
              for monp_dict in endpoint['mon-params']:
                  monp = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_MonitoringParam.from_dict(monp_dict)
                  monp.http_endpoint_ref = endpoint['path']
                  vnfd.monitoring_param.append(monp)
        return

    def add_ike2_client_mon(self, vnfd, credentials):
        ikekey2_mon = [{ 
                'id': '1',
                'name': 'IKE Client0 re-key Rate',
                'json_query_method': "OBJECTPATH",
                'value_type': "INT",
                'numeric_constraints': {'min_value':0, 'max_value':25000},
                'json_query_params': {'object_path':"$..*[@.name is 'client0'].statistics.rekey.rate"},
                'description': 'IKEv2 Diffie Hellman key exchange rate',
                'group_tag': 'Group100',
                'widget_type': 'GAUGE',
                'units': 'rekeys/sec'
               },
               { 
                'id': '2',
                'name': 'IPSec Client0 Tunnels',
                'json_query_method': "OBJECTPATH",
                'value_type': "INT",
                'json_query_params': {'object_path':"$..*[@.name is 'client0'].statistics.state.'ike-sas'"},
                'description': 'IPSec Tunnels',
                'group_tag': 'Group101',
                'widget_type': 'COUNTER',
                'units': 'tunnels'
               } ,
               {
                'id': '3',
                'name': 'IKE Client1 re-key Rate',
                'json_query_method': "OBJECTPATH",
                'value_type': "INT",
                'numeric_constraints': {'min_value':0, 'max_value':25000},
                'json_query_params': {'object_path':"$..*[@.name is 'client1'].statistics.rekey.rate"},
                'description': 'IKEv2 Diffie Hellman key exchange rate',
                'group_tag': 'Group100',
                'widget_type': 'GAUGE',
                'units': 'rekeys/sec'
               },
               { 
                'id': '4',
                'name': 'IPSec Client1 Tunnels',
                'json_query_method': "OBJECTPATH",
                'value_type': "INT",
                'json_query_params': {'object_path':"$..*[@.name is 'client1'].statistics.state.'ike-sas'"},
                'description': 'IPSec Tunnels',
                'group_tag': 'Group101',
                'widget_type': 'COUNTER',
                'units': 'tunnels'
                },
               {
                'id': '5',
                'name': 'IKE Client2 re-key Rate',
                'json_query_method': "OBJECTPATH",
                'value_type': "INT",
                'numeric_constraints': {'min_value':0, 'max_value':25000},
                'json_query_params': {'object_path':"$..*[@.name is 'client2'].statistics.rekey.rate"},
                'description': 'IKEv2 Diffie Hellman key exchange rate',
                'group_tag': 'Group100',
                'widget_type': 'GAUGE',
                'units': 'rekeys/sec'
               },
               { 
                'id': '6',
                'name': 'IPSec Client2 Tunnels',
                'json_query_method': "OBJECTPATH",
                'value_type': "INT",
                'json_query_params': {'object_path':"$..*[@.name is 'client2'].statistics.state.'ike-sas'"},
                'description': 'IPSec Tunnels',
                'group_tag': 'Group101',
                'widget_type': 'COUNTER',
                'units': 'tunnels'
                },
                {
                 'id': '7',
                 'name': 'IKE Client3 re-key Rate',
                 'json_query_method': "OBJECTPATH",
                 'value_type': "INT",
                 'numeric_constraints': {'min_value':0, 'max_value':25000},
                 'json_query_params': {'object_path':"$..*[@.name is 'client3'].statistics.rekey.rate"},
                 'description': 'IKEv2 Diffie Hellman key exchange rate',
                 'group_tag': 'Group100',
                 'widget_type': 'GAUGE',
                 'units': 'rekeys/sec'
                 },
               { 
                  'id': '8',
                  'name': 'IPSec Client3 Tunnels',
                  'json_query_method': "OBJECTPATH",
                  'value_type': "INT",
                  'json_query_params': {'object_path':"$..*[@.name is 'client3'].statistics.state.'ike-sas'"},
                  'description': 'IPSec Tunnels',
                  'group_tag': 'Group101',
                  'widget_type': 'COUNTER',
                 'units': 'tunnels'
                  }
               ]
        mon_param  = [
              {'path': "api/readonly/operational/vnf-opdata/vnf/%s,0/ipsec-service-statistics" % vnfd.name,
               'port': 8008,
               'https': True,
               'mon-params': ikekey2_mon,
               },
              ]
        # HTTP endpoint of Monitoring params
        for endpoint in mon_param:
                endp = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_HttpEndpoint(
                        path=endpoint['path'], port=endpoint['port'], username=credentials['username'],
                      password=credentials['password'],polling_interval_secs=3, https=endpoint['https']
                        )
                vnfd.http_endpoint.append(endp)
                hdr1 = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_HttpEndpoint_Headers(
                                key='Content-type', value='application/vnd.yang.data+json')
                endp.headers.append(hdr1)
                hdr2 = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_HttpEndpoint_Headers(
                                key='Accept', value='json')
                endp.headers.append(hdr2)
  
                # Monitoring params
                for monp_dict in endpoint['mon-params']:
                    monp = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_MonitoringParam.from_dict(monp_dict)
                    monp.http_endpoint_ref = endpoint['path']
                    vnfd.monitoring_param.append(monp)
        return

        
    def add_ike2_server_mon(self, vnfd, credentials):
        ikekey2_mon = [{ 
                'id': '1',
                'name': 'IKE Server0 re-key Rate',
                'json_query_method': "OBJECTPATH",
                'value_type': "INT",
                'numeric_constraints': {'min_value':0, 'max_value':25000},
                'json_query_params': {'object_path':"$..*[@.name is 'server0'].statistics.rekey.rate"},
                'description': 'IKEv2 Diffie Hellman key exchange rate',
                'group_tag': 'Group100',
                'widget_type': 'GAUGE',
                'units': 'rekeys/sec'
               },
               { 
                'id': '2',
                'name': 'IPSec Server0 Tunnels',
                'json_query_method': "OBJECTPATH",
                'value_type': "INT",
                'json_query_params': {'object_path':"$..*[@.name is 'server0'].statistics.state.'ike-sas'"},
                'description': 'IPSec Tunnels',
                'group_tag': 'Group101',
                'widget_type': 'COUNTER',
                'units': 'tunnels'
               } ,
               {
                'id': '3',
                'name': 'IKE Server1 re-key Rate',
                'json_query_method': "OBJECTPATH",
                'value_type': "INT",
                'numeric_constraints': {'min_value':0, 'max_value':25000},
                'json_query_params': {'object_path':"$..*[@.name is 'server1'].statistics.rekey.rate"},
                'description': 'IKEv2 Diffie Hellman key exchange rate',
                'group_tag': 'Group100',
                'widget_type': 'GAUGE',
                'units': 'rekeys/sec'
               },
               { 
                'id': '4',
                'name': 'IPSec Server1 Tunnels',
                'json_query_method': "OBJECTPATH",
                'value_type': "INT",
                'json_query_params': {'object_path':"$..*[@.name is 'server1'].statistics.state.'ike-sas'"},
                'description': 'IPSec Tunnels',
                'group_tag': 'Group101',
                'widget_type': 'COUNTER',
                'units': 'tunnels'
                },
               {
                'id': '5',
                'name': 'IKE Server2 re-key Rate',
                'json_query_method': "OBJECTPATH",
                'value_type': "INT",
                'numeric_constraints': {'min_value':0, 'max_value':25000},
                'json_query_params': {'object_path':"$..*[@.name is 'server2'].statistics.rekey.rate"},
                'description': 'IKEv2 Diffie Hellman key exchange rate',
                'group_tag': 'Group100',
                'widget_type': 'GAUGE',
                'units': 'rekeys/sec'
               },
               { 
                'id': '6',
                'name': 'IPSec Server2 Tunnels',
                'json_query_method': "OBJECTPATH",
                'value_type': "INT",
                'json_query_params': {'object_path':"$..*[@.name is 'server2'].statistics.state.'ike-sas'"},
                'description': 'IPSec Tunnels',
                'group_tag': 'Group101',
                'widget_type': 'COUNTER',
                'units': 'tunnels'
               },
               {
                 'id': '7',
                 'name': 'IKE Server3 re-key Rate',
                 'json_query_method': "OBJECTPATH",
                 'value_type': "INT",
                 'numeric_constraints': {'min_value':0, 'max_value':25000},
                 'json_query_params': {'object_path':"$..*[@.name is 'server3'].statistics.rekey.rate"},
                 'description': 'IKEv2 Diffie Hellman key exchange rate',
                 'group_tag': 'Group100',
                 'widget_type': 'GAUGE',
                 'units': 'rekeys/sec'
                 },
               { 
                  'id': '8',
                  'name': 'IPSec Server3 Tunnels',
                  'json_query_method': "OBJECTPATH",
                  'value_type': "INT",
                  'json_query_params': {'object_path':"$..*[@.name is 'server3'].statistics.state.'ike-sas'"},
                  'description': 'IPSec Tunnels',
                  'group_tag': 'Group101',
                  'widget_type': 'COUNTER',
                  'units': 'tunnels'
                 }
               ]
        mon_param  = [
              {'path': "api/readonly/operational/vnf-opdata/vnf/%s,0/ipsec-service-statistics" % vnfd.name,
               'port': 8008,
               'https': True,
               'mon-params': ikekey2_mon,
               },
              ]
        # HTTP endpoint of Monitoring params
        for endpoint in mon_param:
                endp = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_HttpEndpoint(
                        path=endpoint['path'], port=endpoint['port'], username=credentials['username'],
                      password=credentials['password'],polling_interval_secs=3, https=endpoint['https']
                        )
                vnfd.http_endpoint.append(endp)
                hdr1 = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_HttpEndpoint_Headers(
                                key='Content-type', value='application/vnd.yang.data+json')
                endp.headers.append(hdr1)
                hdr2 = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_HttpEndpoint_Headers(
                                key='Accept', value='json')
                endp.headers.append(hdr2)
  
                # Monitoring params
                for monp_dict in endpoint['mon-params']:
                    monp = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_MonitoringParam.from_dict(monp_dict)
                    monp.http_endpoint_ref = endpoint['path']
                    vnfd.monitoring_param.append(monp)
        return

    def add_aggr_port_mon(self, vnfd, credentials):
        port_mon = [{ 
                'id': '100',
                'name': 'Aggr Port Tx Rate',
                'json_query_method': "JSONPATH",
                'value_type': "INT",
                'numeric_constraints': {'min_value':0, 'max_value':10000},
                'json_query_params': {'json_path':"$..'tx-rate-mbps'"},
                'description': 'Outgoing byte rate of interfaces',
                'group_tag': 'Group100',
                'widget_type': 'GAUGE',
                'units': 'mbps'
               }, {
                'id': '101',
                'name': 'Aggr Rx Rate',
                'json_query_method': "JSONPATH",
                'value_type': "INT",
                'numeric_constraints': {'min_value':0, 'max_value':10000},
                'json_query_params': {'json_path':"$..'rx-rate-mbps'"},
                'description': 'Incoming byte rate of interfaces',
                'group_tag': 'Group1001',
                'widget_type': 'GAUGE',
                'units': 'mbps' }]

        mon_param  = [
             {'path': "api/readonly/operational/vnf-opdata/vnf/%s,0/aggregate-port-stats/external-port-stats" % vnfd.name,
            # {'path': "api/operational/vnf-opdata/vnf/%s,0/aggregate-port-stats/external-port-stats" % vnfd.name,
             'port': 8008,
             'https': True,
             'mon-params': port_mon,
             },
            ]

        # HTTP endpoint of Monitoring params
        for endpoint in mon_param:
            endp = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_HttpEndpoint(
                    path=endpoint['path'], port=endpoint['port'], username=credentials['username'],
                    password=credentials['password'],polling_interval_secs=3, https=endpoint['https']
                    )
            vnfd.http_endpoint.append(endp)
            hdr1 = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_HttpEndpoint_Headers(
                            key='Content-type', value='application/vnd.yang.data+json')
            endp.headers.append(hdr1)
            hdr2 = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_HttpEndpoint_Headers(
                            key='Accept', value='json')
            endp.headers.append(hdr2)                                                                                                               

            # Monitoring params
            for monp_dict in endpoint['mon-params']:
                monp = VnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd_MonitoringParam.from_dict(monp_dict)
                monp.http_endpoint_ref = endpoint['path']
                vnfd.monitoring_param.append(monp)
        return

    def get_cloud_init(self, vnfd, vdu, master_vdu_id):
        if (vdu.name.split(".", -1))[2] == 'LEAD':
         return '''#cloud-config
password: fedora
chpasswd: { expire: False }
ssh_pwauth: True
write_files:
  - path: /opt/rift/.vnf_start_conf
    content: |
        VNFNAME=%s
        VDUNAME={{ vdu.name }}
        MASTERIP=
runcmd:
  - [ systemctl, daemon-reload ]
  - [ systemctl, enable, multivmvnf.service ]
  - [ systemctl, start, --no-block, multivmvnf.service ]
''' % vnfd.name
        else:
         return '''#cloud-config
password: fedora
chpasswd: { expire: False }
ssh_pwauth: True
write_files:
  - path: /opt/rift/.vnf_start_conf
    content: |
        VNFNAME=%s
        VDUNAME={{ vdu.name }}
        MASTERIP={{ vdu[%s].mgmt.ip }}
runcmd:
  - [ systemctl, daemon-reload ]
  - [ systemctl, enable, multivmvnf.service ]
  - [ systemctl, start, --no-block, multivmvnf.service ]
''' % (vnfd.name, master_vdu_id)

    def get_internal_cp_count(self, vnfd):
        count=0
        for vdu in vnfd.vdu:
          for internal_cp in vdu.internal_connection_point:
            count = count+1
        return count

    def get_external_if_count(self, vnfd):
        count=0
        for vdu in vnfd.vdu:
          for ext_int in vdu.external_interface:
            count = count+1
        return count

    def create_vdu(self, vnfd, vdu_info, vnf_info, master_vdu_id):
        # VDU Specification
        vdu = vnfd.vdu.add()
        vdu.name = vdu_info['vduname']
        vdu.id = vdu.name + str(len(vnfd.vdu))
        vdu.count = 1

        # specify the VM flavor
        vdu.vm_flavor.vcpu_count = 4
        vdu.vm_flavor.memory_mb = 8192
        vdu.vm_flavor.storage_gb = 32

        try:
            vcpu = vdu_info['vcpu']
        except KeyError:
            pass
        else:
            vdu.vm_flavor.vcpu_count = int(vcpu)

        try:
            vmem = vdu_info['vmem']
        except KeyError:
            pass
        else:
            vdu.vm_flavor.memory_mb = int(vmem)

        vdu.image = vdu_info['image']

        vdu.cloud_init = self.get_cloud_init(vnfd, vdu, master_vdu_id)

        cur_external_cp = self.get_external_if_count(vnfd)

        for i in range(0, int(vdu_info['external_port'])):
            external_interface = vdu.external_interface.add()
            external_interface.name = 'eth%d' % (i+1)
            external_interface.vnfd_connection_point_ref = '%s/cp%d' % (self.name, cur_external_cp + i)
            if vnf_info['ext_port_type'] != 'virtio':
                external_interface.virtual_interface.type_yang = 'SR_IOV'
            else:
                external_interface.virtual_interface.type_yang = 'VIRTIO'

        cur_internal_cp = self.get_internal_cp_count(vnfd)

        for j in range(0, int(vdu_info['fabric_port'])):
            internal_cp = vdu.internal_connection_point.add()
            internal_cp.id = str(uuid.uuid1())
            internal_cp.name = self.name + "/icp{}".format(cur_internal_cp+j)
            internal_cp.type_yang = 'VPORT'
            internal_interface = vdu.internal_interface.add()
            internal_interface.name = 'eth%d' % (j + int(vdu_info['external_port']) + 1)
            internal_interface.vdu_internal_connection_point_ref = internal_cp.id
            if vnf_info['fab_port_type'] != 'virtio':
                internal_interface.virtual_interface.type_yang = 'SR_IOV'
            else:
                internal_interface.virtual_interface.type_yang = 'VIRTIO'

        try:
             vdu.guest_epa.mempage_size = vdu_info['epa_memory']
        except KeyError:
             pass

        try:
             vdu.guest_epa.cpu_pinning_policy = vdu_info['epa_cpu_pinning']
        except KeyError:
             pass

        if 'epa_pci' in vdu_info:
            # specify PCI passthru devices
            pcie_dev = vdu.guest_epa.pcie_device.add()
            pcie_dev.device_id = vdu_info['epa_pci']
            pcie_dev.count = 32

        print(vdu)
        return vdu


    def compose(self, template_info, src_dir, credentials, mgmt_port=8888):
        self.descriptor = RwVnfdYang.YangData_Vnfd_VnfdCatalog()
        self.id = str(uuid.uuid1())
        vnfd = self.descriptor.vnfd.add()
        vnfd.id = self.id
        vnfd.name = self.name
        vnfd.short_name = self.name
        vnfd.vendor = 'RIFT.io'
        vnfd.description = 'This is a RIFT.ware %s VNF' % (self.name)
        vnfd.version = '1.0'
        vnfd.logo = 'riftio.png'
        self.vnfd = vnfd

        vnf_info = template_info['VNF']
        num_external_ports = 0

        for key,value in template_info.items():
           if key.startswith('VDU'):
              print("Starts With VDU %s" % key)
              vdu_name = value['vduname'] 
              if (vdu_name.split(".", -1))[2] == 'LEAD':
                print("LeadVM found")
                lead_vdu = self.create_vdu(vnfd, value, vnf_info, "")
                num_external_ports = num_external_ports + int(value['external_port'])

        for key,value in template_info.items():
           if key.startswith('VDU'):
              print("Starts With VDU %s" % key)
              vdu_name = value['vduname'] 
              if (vdu_name.split(".", -1))[2] != 'LEAD':
                print("nonLeadVM found")
                nonlead_vdu = self.create_vdu(vnfd, value, vnf_info, lead_vdu.id)
                num_external_ports = num_external_ports + int(value['external_port'])
                 
        # Management interface
        mgmt_intf = vnfd.mgmt_interface
        mgmt_intf.vdu_id = lead_vdu.id
        mgmt_intf.port = mgmt_port

        print("Composing VNFD for %s" %  vnfd.name)

        # For fabric - Internal vld
        in_vld_list = []
        #for all VDUs
        for vdu in vnfd.vdu:
          vld_index = 0
          for internal_cp in vdu.internal_connection_point:
            try:
              internal_vld = in_vld_list[vld_index]
            except IndexError:
              internal_vld = vnfd.internal_vld.add()
              internal_vld.id = str(uuid.uuid1())
              internal_vld.name = 'fabric%d' % vld_index
              internal_vld.short_name = 'fabric%d' % vld_index
              internal_vld.description = 'Virtual link for internal fabric'
              internal_vld.type_yang = 'ELAN'
              internal_vld.internal_connection_point_ref.append(internal_cp.id)
              internal_vld.provider_network.physical_network = 'physnet1'
              try:                                                                                                                                                           
                 internal_vld.provider_network.physical_network = vnf_info['fab_net']
              except KeyError:
                 pass

              internal_vld.provider_network.overlay_type = 'VLAN'

              in_vld_list.append(internal_vld)
            else:
              internal_vld.internal_connection_point_ref.append(internal_cp.id)
            vld_index = vld_index+1

        for i in range(0, num_external_ports):
            cp = vnfd.connection_point.add()
            cp.type_yang = 'VPORT'
            cp.name = '%s/cp%d' % (self.name, i)

        # fillin default-config
        self.default_config(vnfd, src_dir, vnf_info, template_info)

        bool_port = 0
        bool_latency = 0
        port_mon_info = []
        for key,value in vnf_info.items():
           if key.startswith('monitor'):
              print("Starts With monitor %s" % key)
              mon_info = value.split(":", -1)
              if mon_info[0] == 'ports':
                 port_mon_info = mon_info[1:]
                 bool_port = 1;
              elif mon_info[0] == 'ports-latency':
                 bool_latency = 1;
              elif mon_info[0] == 'aggr-ports':
                 self.add_aggr_port_mon(vnfd, credentials)
              elif value == 'ike1':
                 self.add_ike1_mon(vnfd, credentials)
              elif mon_info[0] == 'ike2_clients':
                 self.add_ike2_client_mon(vnfd, credentials)
              elif mon_info[0] == 'ike2_servers':
                 self.add_ike2_server_mon(vnfd, credentials)

        if bool_port == 1 or bool_latency == 1:
          self.add_port_mon(vnfd, credentials, bool_port, bool_latency, port_mon_info)
#
#        # spcify the guest EPA
#        if use_epa:
#            #vdu.guest_epa.trusted_execution = True
#            vdu.guest_epa.mempage_size = 'PREFER_LARGE'
#            vdu.guest_epa.cpu_pinning_policy = 'DEDICATED'
#            vdu.guest_epa.cpu_thread_pinning_policy = 'PREFER'
#            vdu.guest_epa.numa_node_policy.node_cnt = 2
#            vdu.guest_epa.numa_node_policy.mem_policy = 'STRICT'
#
#            node = vdu.guest_epa.numa_node_policy.node.add()
#            node.id = 0
#            node.memory_mb = 8192
#            node.vcpu = [ 0, 1 ]
#
#            node = vdu.guest_epa.numa_node_policy.node.add()
#            node.id = 1
#            node.memory_mb = 8192
#            node.vcpu = [ 2, 3 ]

            # specify PCI passthru devices
            #pcie_dev = vdu.guest_epa.pcie_device.add()
            #pcie_dev.device_id = "ALIAS_ONE"
            #pcie_dev.count = 1
            #pcie_dev = vdu.guest_epa.pcie_device.add()
            #pcie_dev.device_id = "ALIAS_TWO"
            #pcie_dev.count = 3

            # specify the vswitch EPA
            #vdu.vswitch_epa.ovs_acceleration = 'DISABLED'
            #vdu.vswitch_epa.ovs_offload = 'DISABLED'

            # Specify the hypervisor EPA
            #vdu.hypervisor_epa.type_yang = 'PREFER_KVM'

            # Specify the host EPA
            #vdu.host_epa.cpu_model = 'PREFER_SANDYBRIDGE'
            #vdu.host_epa.cpu_arch = 'PREFER_X86_64'
            #vdu.host_epa.cpu_vendor = 'PREFER_INTEL'
            #vdu.host_epa.cpu_socket_count = 'PREFER_TWO'
            #vdu.host_epa.cpu_feature = [ 'PREFER_AES', 'PREFER_CAT' ]

#        # spcify the guest EPA
#        if use_epa:
#            #vdu.guest_epa.trusted_execution = True
#            vdu.guest_epa.mempage_size = 'PREFER_LARGE'
#            vdu.guest_epa.cpu_pinning_policy = 'DEDICATED'
#            vdu.guest_epa.cpu_thread_pinning_policy = 'PREFER'
#            vdu.guest_epa.numa_node_policy.node_cnt = 2
#            vdu.guest_epa.numa_node_policy.mem_policy = 'STRICT'
#
#            node = vdu.guest_epa.numa_node_policy.node.add()
#            node.id = 0
#            node.memory_mb = 4096
#            node.vcpu = [ 0, 1 ]
#
#            node = vdu.guest_epa.numa_node_policy.node.add()
#            node.id = 1
#            node.memory_mb = 4096
#            node.vcpu = [ 2, 3 ]

            # specify PCI passthru devices
            #pcie_dev = vdu.guest_epa.pcie_device.add()
            #pcie_dev.device_id = "ALIAS_ONE"
            #pcie_dev.count = 1
            #pcie_dev = vdu.guest_epa.pcie_device.add()
            #pcie_dev.device_id = "ALIAS_TWO"
            #pcie_dev.count = 3

            # specify the vswitch EPA
            #vdu.vswitch_epa.ovs_acceleration = 'DISABLED'
            #vdu.vswitch_epa.ovs_offload = 'DISABLED'

            # Specify the hypervisor EPA
            #vdu.hypervisor_epa.type_yang = 'PREFER_KVM'

            # Specify the host EPA
            #vdu.host_epa.cpu_model = 'PREFER_SANDYBRIDGE'
            #vdu.host_epa.cpu_arch = 'PREFER_X86_64'
            #vdu.host_epa.cpu_vendor = 'PREFER_INTEL'
            #vdu.host_epa.cpu_socket_count = 'PREFER_TWO'
            #vdu.host_epa.cpu_feature = [ 'PREFER_AES', 'PREFER_CAT' ]

    def write_to_file(self, outdir, filename, output_format):
        dirpath = "%s/%s_vnfd/vnfd" % (outdir, filename)
        print("Dirpath %s" % dirpath)
        if not os.path.exists(dirpath):
            os.makedirs(dirpath)
        super(VirtualNetworkFunction, self).write_to_file(['vnfd', 'rw-vnfd'],
                                                          "%s/%s_vnfd/vnfd" % (outdir, filename),
                                                          output_format)


def generate_multivm_descriptors(fmt="json", write_to_file=False, out_dir="./", template_name="", template_info = {}, src_dir = "./"):
    vnfname = template_info['VNF']['vnfname']
    multivm = VirtualNetworkFunction(vnfname)
    multivm.compose(
             template_info,
             src_dir,
             {'username':'admin', 'password':'admin'},
             mgmt_port=2022,
             )

    if write_to_file:
        multivm.write_to_file(out_dir, template_name, fmt)

    return (multivm)


def main(argv=sys.argv[1:]):
    global outdir, output_format, vnfname
    parser = argparse.ArgumentParser()
    parser.add_argument('-o', '--outdir', default='.')
    parser.add_argument('-i', '--srcdir', default='.')
    parser.add_argument('-f', '--format', default='json')
    parser.add_argument('-v', '--vnfname', default = "multivm" )
    parser.add_argument('-t', '--template', default = "" )
    args = parser.parse_args()
    outdir = args.outdir
    output_format = args.format

    template_info={}
    with open(args.srcdir + "/templates/" + args.template ) as fd:
      s = [line.strip().split(None, 1) for line in fd]
      for l in s:
        j = [k.strip().split("=", -1) for k in l[1].strip().split(",",-1)]
        template_info[l[0]] = dict(j)
      #for keys,values in template_info.items():
      #  print(keys)
      #  print(values)
    print(template_info)

    try:
      vnf = template_info['VNF']
    except KeyError:
      print("Template does not have VNF specified");
      raise

    generate_multivm_descriptors(args.format, True, args.outdir, args.template, template_info, args.srcdir)

if __name__ == "__main__":
    main()

