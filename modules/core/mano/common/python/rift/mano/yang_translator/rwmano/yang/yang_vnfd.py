# Copyright 2016 RIFT.io Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


from copy import deepcopy

from rift.mano.yang_translator.common.exception import ValidationError
from rift.mano.yang_translator.common.utils import _
from rift.mano.yang_translator.rwmano.syntax.tosca_resource \
    import ToscaResource
from rift.mano.yang_translator.rwmano.yang.yang_vdu import YangVdu

TARGET_CLASS_NAME = 'YangVnfd'


class YangVnfd(ToscaResource):
    '''Class for RIFT.io YANG VNF descriptor translation to TOSCA type.'''

    yangtype = 'vnfd'

    CONFIG_TYPES = ['script', 'netconf', 'rest', 'juju']

    OTHER_KEYS = (MGMT_INTF, HTTP_EP, MON_PARAM) = \
                 ('mgmt_interface', 'http_endpoint', 'monitoring_param')


    def __init__(self,
                 log,
                 name,
                 type_,
                 yang):
        super(YangVnfd, self).__init__(log,
                                       name,
                                       type_,
                                       yang)
        self.props = {}
        self.vdus = []
        self.mgmt_intf = {}
        self.mon_param = []
        self.http_ep = []

    def handle_yang(self):
        self.log.debug(_("Process VNFD desc {0}: {1}").format(self.name,
                                                              self.yang))

        def process_vnf_config(conf):
            vnf_conf = {}
            if self.CONFIG_ATTR in conf:
                for key, value in conf.pop(self.CONFIG_ATTR).items():
                    vnf_conf[key] = value

            if self.CONFIG_TMPL in conf:
                vnf_conf[self.CONFIG_TMPL] = conf.pop(self.CONFIG_TMPL)

            def copy_config_details(conf_type, conf_details):
                vnf_conf[self.CONFIG_TYPE] = conf_type
                vnf_conf[self.CONFIG_DETAILS] = conf_details

            for key in self.CONFIG_TYPES:
                if key in conf:
                    copy_config_details(key, conf.pop(key))
                    break

            if len(conf):
                self.log.warn(_("{0}, Did not process all in VNF "
                                "configuration {1}").
                              format(self, conf))
            self.log.debug(_("{0}, vnf config: {1}").format(self, vnf_conf))
            self.props[self.VNF_CONFIG] = vnf_conf

        def process_mgmt_intf(intf):
            if len(self.mgmt_intf) > 0:
                err_msg(_("{0}, Already processed another mgmt intf {1}, "
                          "got another {2}").
                        format(self, self.msmg_intf, intf))
                self.log.error(err_msg)
                raise ValidationError(message=err_msg)

            self.mgmt_intf['protocol'] = 'tcp'
            if self.VNF_CONFIG in intf:
                process_vnf_config(intf.pop(self.VNF_CONFIG))

            if self.PORT in intf:
                self.mgmt_intf[self.PORT] = intf.pop(self.PORT)
                self.props[self.PORT] = self.mgmt_intf[self.PORT]

            if 'vdu_id' in intf:
                for vdu in self.vdus:
                    if intf['vdu_id'] == vdu.id:
                        self.mgmt_intf[self.VDU] = vdu.get_name(self.name)
                        intf.pop('vdu_id')
                        break

            if self.DASHBOARD_PARAMS in intf:
                self.mgmt_intf[self.DASHBOARD_PARAMS] = \
                                            intf.pop(self.DASHBOARD_PARAMS)

            if len(intf):
                self.log.warn(_("{0}, Did not process all in mgmt "
                                "interface {1}").
                              format(self, intf))
            self.log.debug(_("{0}, Management interface: {1}").
                           format(self, self.mgmt_intf))

        def process_http_ep(eps):
            self.log.debug("{}, HTTP EP: {}".format(self, eps))
            for ep in eps:
                http_ep = {'protocol': 'http'}  # Required for TOSCA
                http_ep[self.PATH] = ep.pop(self.PATH)
                http_ep[self.PORT] = ep.pop(self.PORT)
                http_ep[self.POLL_INTVL] = ep.pop(self.POLL_INTVL_SECS)
                if len(ep):
                    self.log.warn(_("{0}, Did not process the following for "
                                    "http ep {1}").format(self, ep))
                    self.log.debug(_("{0}, http endpoint: {1}").format(self, http_ep))
                self.http_ep.append(http_ep)

        def process_mon_param(params):
            for param in params:
                monp = {}
                fields = [self.NAME, self.ID, 'value_type', 'units', 'group_tag',
                          'json_query_method', 'http_endpoint_ref', 'widget_type',
                          self.DESC]
                for key in fields:
                    if key in param:
                        monp[key] = param.pop(key)

                if len(param):
                    self.log.warn(_("{0}, Did not process the following for "
                                    "monitporing-param {1}").
                                  format(self, param))
                    self.log.debug(_("{0}, Monitoring param: {1}").format(self, monp))
                self.mon_param.append(monp)

        def process_cp(cps):
            for cp_dic in cps:
                self.log.debug("{}, CP: {}".format(self, cp_dic))
                name = cp_dic.pop(self.NAME)
                for vdu in self.vdus:
                    if vdu.has_cp(name):
                        vdu.set_cp_type(name, cp_dic.pop(self.TYPE_Y))
                        break
                if len(cp_dic):
                    self.log.warn(_("{0}, Did not process the following for "
                                    "connection-point {1}: {2}").
                                  format(self, name, cp_dic))

        ENDPOINTS_MAP = {
            self.MGMT_INTF: process_mgmt_intf,
            self.HTTP_EP:  process_http_ep,
            self.MON_PARAM: process_mon_param,
            'connection_point': process_cp
        }

        dic = deepcopy(self.yang)
        try:
            for key in self.REQUIRED_FIELDS:
                self.props[key] = dic.pop(key)

            self.id = self.props[self.ID]

            # Process VDUs before CPs so as to update the CP struct in VDU
            # when we process CP later
            if self.VDU in dic:
                for vdu_dic in dic.pop(self.VDU):
                    vdu = YangVdu(self.log, vdu_dic.pop(self.NAME),
                                  self.VDU, vdu_dic)
                    vdu.process_vdu()
                    self.vdus.append(vdu)

            for key in ENDPOINTS_MAP.keys():
                if key in dic:
                    ENDPOINTS_MAP[key](dic.pop(key))

            self.remove_ignored_fields(dic)
            if len(dic):
                self.log.warn(_("{0}, Did not process the following for "
                                "VNFD: {1}").
                              format(self, dic))
            self.log.debug(_("{0}, VNFD: {1}").format(self, self.props))
        except Exception as e:
            err_msg = _("Exception processing VNFD {0} : {1}"). \
                      format(self.name, e)
            self.log.error(err_msg)
            raise ValidationError(message=err_msg)

    def update_cp_vld(self, cp_name, vld_name):
        for vdu in self.vdus:
            cp = vdu.get_cp(cp_name)
            if cp:
                vdu.set_vld(cp_name, vld_name)
                break

    def generate_tosca_type(self, tosca):
        self.log.debug(_("{0} Generate tosa types").
                       format(self))

        for vdu in self.vdus:
            tosca = vdu.generate_tosca_type(tosca)

        # Add data_types
        if self.T_VNF_CONFIG not in tosca[self.DATA_TYPES]:
            tosca[self.DATA_TYPES][self.T_VNF_CONFIG] = {
                self.PROPERTIES:
                {self.CONFIG_TYPE:
                 {self.TYPE: self.STRING},
                 'config_delay':
                 {self.TYPE: self.INTEGER,
                  self.DEFAULT: 0,
                  self.REQUIRED: self.NO,
                  self.CONSTRAINTS:
                  [{'greater_or_equal': 0}]},
                 'config_priority':
                 {self.TYPE: self.INTEGER,
                  self.CONSTRAINTS:
                  [{'greater_than': 0}]},
                 self.CONFIG_DETAILS:
                 {self.TYPE: self.MAP},
                 self.CONFIG_TMPL:
                 {self.TYPE: self.STRING,
                  self.REQUIRED: self.NO},
                }
            }

        # Add capability types
        if self.CAPABILITY_TYPES not in tosca:
            tosca[self.CAPABILITY_TYPES] = {}
        if self.T_HTTP_EP not in tosca[self.CAPABILITY_TYPES]:
            tosca[self.CAPABILITY_TYPES][self.T_HTTP_EP] = {
                self.DERIVED_FROM: 'tosca.capabilities.Endpoint',
                self.PROPERTIES: {
                    'polling_interval':
                    {self.TYPE: self.INTEGER},
                    'path':
                    {self.TYPE: self.STRING},
                },
            }

        if self.T_MGMT_INTF not in tosca[self.CAPABILITY_TYPES]:
            tosca[self.CAPABILITY_TYPES][self.T_MGMT_INTF] = {
                self.DERIVED_FROM: 'tosca.capabilities.Endpoint',
                self.PROPERTIES: {
                    self.DASHBOARD_PARAMS:
                    {self.TYPE: self.MAP},
                    self.VDU:
                    {self.TYPE: self.STRING},
                },
            }

        if self.T_MON_PARAM not in tosca[self.CAPABILITY_TYPES]:
            tosca[self.CAPABILITY_TYPES][self.T_MON_PARAM] = {
                self.DERIVED_FROM: 'tosca.capabilities.nfv.Metric',
                self.PROPERTIES: {
                    'id':
                    {self.TYPE: self.INTEGER},
                    'name':
                    {self.TYPE: self.STRING},
                    'value_type':
                    {self.TYPE: self.STRING,
                     self.DEFAULT: 'INT'},
                    'group_tag':
                    {self.TYPE: self.STRING,
                     self.DEFAULT: 'Group1'},
                    'units':
                    {self.TYPE: self.STRING},
                    'description':
                    {self.TYPE: self.STRING},
                    'json_query_method':
                    {self.TYPE: self.STRING,
                     self.DEFAULT: 'NAMEKEY'},
                    'http_endpoint_ref':
                    {self.TYPE: self.STRING},
                    'widget_type':
                    {self.TYPE: self.STRING,
                     self.DEFAULT: 'COUNTER'},
                }
            }

        # Define the VNF type
        if self.T_VNF1 not in tosca[self.NODE_TYPES]:
            tosca[self.NODE_TYPES][self.T_VNF1] = {
                self.DERIVED_FROM: 'tosca.nodes.nfv.VNF',
                self.PROPERTIES: {
                    'vnf_configuration':
                    {self.TYPE: self.T_VNF_CONFIG},
                    'port':
                    {self.TYPE: self.INTEGER,
                     self.CONSTRAINTS:
                     [{'in_range': '[1, 65535]'}]},
                    self.START_BY_DFLT:
                    {self.TYPE: self.BOOL,
                     self.DEFAULT: self.TRUE},
                },
                self.CAPABILITIES: {
                    'mgmt_interface':
                    {self.TYPE: self.T_MGMT_INTF},
                    'http_endpoint':
                    {self.TYPE: self.T_HTTP_EP},
                    'monitoring_param_0':
                    {self.TYPE: self.T_MON_PARAM},
                    'monitoring_param_1':
                    {self.TYPE: self.T_MON_PARAM},
                },
                self.REQUIREMENTS: [
                    {'vdus':
                     {self.TYPE: 'tosca.capabilities.nfv.VirtualLinkable',
                      self.RELATIONSHIP:
                      'tosca.relationships.nfv.VirtualLinksTo',
                      self.NODE: self.T_VDU1,
                      self.OCCURENCES: '[1, UNBOUND]'}}
                ],
            }

        return tosca

    def generate_vnf_template(self, tosca, index):
        self.log.debug(_("{0}, Generate tosca template for VNF {1}").
                       format(self, index, tosca))

        for vdu in self.vdus:
            tosca = vdu.generate_vdu_template(tosca, self.name)

        node = {}
        node[self.TYPE] = self.T_VNF1

        # Remove fields not required in TOSCA
        self.props.pop(self.DESC)

        # Update index to the member-vnf-index
        self.props[self.ID] = index
        node[self.PROPERTIES] = self.props

        caps = {}
        if len(self.mgmt_intf):
            caps[self.MGMT_INTF] = {
                self.PROPERTIES: self.mgmt_intf
            }

        if len(self.http_ep):
            caps[self.HTTP_EP] = {
                self.PROPERTIES: self.http_ep[0]
            }
            if len(self.http_ep) > 1:
                self.log.warn(_("{0}: Currently only one HTTP endpoint "
                                "supported: {1}").
                              format(self, self.http_ep))

        if len(self.mon_param):
            count = 0
            for monp in self.mon_param:
                name = "{}_{}".format(self.MON_PARAM, count)
                caps[name] = {self.PROPERTIES: monp}
                count += 1

        node[self.CAPABILITIES] = caps

        if len(self.vdus):
            reqs = []
            for vdu in self.vdus:
                reqs.append({'vdus': {self.NODE: vdu.get_name(self.name)}})

            node[self.REQUIREMENTS] = reqs
        else:
            self.log.warn(_("{0}, Did not find any VDUS with this VNF").
                          format(self))

        self.log.debug(_("{0}, VNF node: {1}").format(self, node))

        tosca[self.TOPOLOGY_TMPL][self.NODE_TMPL][self.name] = node

        return tosca

    def get_supporting_files(self):
        files = []

        for vdu in self.vdus:
            f = vdu.get_supporting_files()
            if f and len(f):
                files.extend(f)

        return files
