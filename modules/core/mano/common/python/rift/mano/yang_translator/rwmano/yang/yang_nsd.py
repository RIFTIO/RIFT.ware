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
import os

from rift.mano.yang_translator.common.exception import ValidationError
from rift.mano.yang_translator.common.utils import _
from rift.mano.yang_translator.rwmano.syntax.tosca_resource \
    import ToscaResource
from rift.mano.yang_translator.rwmano.yang.yang_vld import YangVld

TARGET_CLASS_NAME = 'YangNsd'


class YangNsd(ToscaResource):
    '''Class for RIFT.io YANG NS descriptor translation to TOSCA type.'''

    yangtype = 'nsd'

    OTHER_FIELDS = (SCALE_GRP, CONF_PRIM,
                    USER_DEF_SCRIPT, SCALE_ACT,
                    TRIGGER, NS_CONF_PRIM_REF,
                    CONST_VNFD, VNFD_MEMBERS,
                    MIN_INST_COUNT, MAX_INST_COUNT,
                    INPUT_PARAM_XPATH, CONFIG_ACTIONS,
                    INITIAL_CFG,) = \
                   ('scaling_group_descriptor', 'config_primitive',
                    'user_defined_script', 'scaling_config_action',
                    'trigger', 'ns_config_primitive_name_ref',
                    'constituent_vnfd', 'vnfd_member',
                    'min_instance_count', 'max_instance_count',
                    'input_parameter_xpath', 'config_actions',
                    'initial_config_primitive',)

    def __init__(self,
                 log,
                 name,
                 type_,
                 yang):
        super(YangNsd, self).__init__(log,
                                      name,
                                      type_,
                                      yang)
        self.props = {}
        self.inputs = []
        self.vnfds = {}
        self.vlds = []
        self.conf_prims = []
        self.scale_grps = []
        self.initial_cfg = []

    def handle_yang(self, vnfds):
        self.log.debug(_("Process NSD desc {0}: {1}").
                       format(self.name, self.yang))

        def process_input_param(param):
            if self.XPATH in param:
                val = param.pop(self.XPATH)
                # Strip namesapce, catalog and nsd part
                self.inputs.append({
                    self.NAME:
                    self.map_yang_name_to_tosca(
                        val.replace('/nsd:nsd-catalog/nsd:nsd/nsd:', ''))})
            if len(param):
                self.log.warn(_("{0}, Did not process the following for "
                                "input param {1}: {2}").
                              format(self, self.inputs, param))
            self.log.debug(_("{0}, inputs: {1}").format(self, self.inputs[-1]))

        def process_const_vnfd(cvnfd):
            # Get the matching VNFD
            vnfd_id = cvnfd.pop(self.VNFD_ID_REF)
            for vnfd in vnfds:
                if vnfd.type == self.VNFD and vnfd.id == vnfd_id:
                    self.vnfds[cvnfd.pop(self.MEM_VNF_INDEX)] = vnfd
                    if self.START_BY_DFLT in cvnfd:
                        vnfd.props[self.START_BY_DFLT] = \
                                            cvnfd.pop(self.START_BY_DFLT)
                    break

            if len(cvnfd):
                self.log.warn(_("{0}, Did not process the following for "
                                "constituent vnfd {1}: {2}").
                              format(self, vnfd_id, cvnfd))
            self.log.debug(_("{0}, VNFD: {1}").format(self, self.vnfds))

        def process_scale_grp(dic):
            sg = {}
            self.log.debug(_("{0}, scale group: {1}").format(self, dic))
            fields = [self.NAME, self.MIN_INST_COUNT, self.MAX_INST_COUNT]
            for key in fields:
                if key in dic:
                    sg[key] = dic.pop(key)

            membs = {}
            for vnfd_memb in dic.pop(self.VNFD_MEMBERS):
                vnfd_idx = vnfd_memb[self.MEM_VNF_INDEX_REF]
                if vnfd_idx in self.vnfds:
                        membs[self.vnfds[vnfd_idx].name] = \
                                                    vnfd_memb[self.COUNT]
            sg['vnfd_members'] = membs

            trigs = {}
            if self.SCALE_ACT in dic:
                for sg_act in dic.pop(self.SCALE_ACT):
                    # Validate the primitive
                    prim = sg_act.pop(self.NS_CONF_PRIM_REF)
                    for cprim in self.conf_prims:
                        if cprim[self.NAME] == prim:
                            trigs[sg_act.pop(self.TRIGGER)] = prim
                            break
                    if len(sg_act):
                        err_msg = (_("{0}, Did not find config-primitive {1}").
                                   format(self, prim))
                        self.log.error(err_msg)
                        raise ValidationError(message=err_msg)
            sg[self.CONFIG_ACTIONS] = trigs

            if len(dic):
                self.log.warn(_("{0}, Did not process all fields for {1}").
                              format(self, dic))
            self.log.debug(_("{0}, Scale group {1}").format(self, sg))
            self.scale_grps.append(sg)

        def process_initial_config(dic):
            icp = {}
            self.log.debug(_("{0}, initial config: {1}").format(self, dic))
            for key in [self.NAME, self.SEQ, self.USER_DEF_SCRIPT]:
                if key in dic:
                    icp[key] = dic.pop(key)

            params = {}
            if self.PARAM in dic:
                for p in dic.pop(self.PARAM):
                    if (self.NAME in p and
                        self.VALUE in p):
                        params[p[self.NAME]] = p[self.VALUE]
                    else:
                        # TODO (pjoseph): Need to add support to read the
                        # config file and get the value from that
                        self.log.warn(_("{0}, Got parameter without value: {1}").
                                      format(self, p))
                if len(params):
                    icp[self.PARAM] = params

            if len(dic):
                self.log.warn(_("{0}, Did not process all fields for {1}").
                              format(self, dic))
            self.log.debug(_("{0}, Initial config {1}").format(self, icp))
            self.initial_cfg.append(icp)

        dic = deepcopy(self.yang)
        try:
            for key in self.REQUIRED_FIELDS:
                self.props[key] = dic.pop(key)

            self.id = self.props[self.ID]

            # Process constituent VNFDs
            if self.CONST_VNFD in dic:
                for cvnfd in dic.pop(self.CONST_VNFD):
                    process_const_vnfd(cvnfd)

            # Process VLDs
            if self.VLD in dic:
                for vld_dic in dic.pop(self.VLD):
                    vld = YangVld(self.log, vld_dic.pop(self.NAME),
                                  self.VLD, vld_dic)
                    vld.process_vld(self.vnfds)
                    self.vlds.append(vld)

            # Process config primitives
            if self.CONF_PRIM in dic:
                for cprim in dic.pop(self.CONF_PRIM):
                    conf_prim = {self.NAME: cprim.pop(self.NAME)}
                    if self.USER_DEF_SCRIPT in cprim:
                        conf_prim[self.USER_DEF_SCRIPT] = \
                                        cprim.pop(self.USER_DEF_SCRIPT)
                        self.conf_prims.append(conf_prim)
                    else:
                        err_msg = (_("{0}, Only user defined script supported "
                                     "in config-primitive for now {}: {}").
                                   format(self, conf_prim, cprim))
                        self.log.error(err_msg)
                        raise ValidationError(message=err_msg)

            # Process scaling group
            if self.SCALE_GRP in dic:
                for sg_dic in dic.pop(self.SCALE_GRP):
                    process_scale_grp(sg_dic)

            # Process initial config primitives
            if self.INITIAL_CFG in dic:
                for icp_dic in dic.pop(self.INITIAL_CFG):
                    process_initial_config(icp_dic)

            # Process the input params
            if self.INPUT_PARAM_XPATH in dic:
                for param in dic.pop(self.INPUT_PARAM_XPATH):
                    process_input_param(param)

            self.remove_ignored_fields(dic)
            if len(dic):
                self.log.warn(_("{0}, Did not process the following for "
                                "NSD {1}: {2}").
                              format(self, self.props, dic))
            self.log.debug(_("{0}, NSD: {1}").format(self, self.props))
        except Exception as e:
            err_msg = _("Exception processing NSD {0} : {1}"). \
                      format(self.name, e)
            self.log.error(err_msg)
            self.log.exception(e)
            raise ValidationError(message=err_msg)

    def generate_tosca_type(self):
        self.log.debug(_("{0} Generate tosa types").
                       format(self))

        tosca = {}
        tosca[self.DATA_TYPES] = {}
        tosca[self.NODE_TYPES] = {}

        for idx, vnfd in self.vnfds.items():
            tosca = vnfd.generate_tosca_type(tosca)

        for vld in self.vlds:
            tosca = vld.generate_tosca_type(tosca)

        # Generate type for config primitives
        if self.GROUP_TYPES not in tosca:
            tosca[self.GROUP_TYPES] = {}
        if self.T_CONF_PRIM not in tosca[self.GROUP_TYPES]:
            tosca[self.GROUP_TYPES][self.T_CONF_PRIM] = {
                self.DERIVED_FROM: 'tosca.policies.Root',
                self.PROPERTIES: {
                    'primitive': self.MAP
                }}

        # Generate type for scaling group
        if self.POLICY_TYPES not in tosca:
            tosca[self.POLICY_TYPES] = {}
        if self.T_SCALE_GRP not in tosca[self.POLICY_TYPES]:
            tosca[self.POLICY_TYPES][self.T_SCALE_GRP] = {
                self.DERIVED_FROM: 'tosca.policies.Root',
                self.PROPERTIES: {
                    self.NAME:
                    {self.TYPE: self.STRING},
                    self.MAX_INST_COUNT:
                    {self.TYPE: self.INTEGER},
                    self.MIN_INST_COUNT:
                    {self.TYPE: self.INTEGER},
                    'vnfd_members':
                    {self.TYPE: self.MAP},
                    self.CONFIG_ACTIONS:
                    {self.TYPE: self.MAP}
                }}

        if self.T_INITIAL_CFG not in tosca[self.POLICY_TYPES]:
            tosca[self.POLICY_TYPES][self.T_INITIAL_CFG] = {
                self.DERIVED_FROM: 'tosca.policies.Root',
                self.PROPERTIES: {
                    self.NAME:
                    {self.TYPE: self.STRING},
                    self.SEQ:
                    {self.TYPE: self.INTEGER},
                    self.USER_DEF_SCRIPT:
                    {self.TYPE: self.STRING},
                    self.PARAM:
                    {self.TYPE: self.MAP},
                }}

        return tosca

    def generate_tosca_template(self, tosca):
        self.log.debug(_("{0}, Generate tosca template").
                       format(self, tosca))

        # Add the standard entries
        tosca['tosca_definitions_version'] = \
                                    'tosca_simple_profile_for_nfv_1_0_0'
        tosca[self.DESC] = self.props[self.DESC]
        tosca[self.METADATA] = {
            'ID': self.name,
            self.VENDOR: self.props[self.VENDOR],
            self.VERSION: self.props[self.VERSION],
        }

        tosca[self.TOPOLOGY_TMPL] = {}

        # Add input params
        if len(self.inputs):
            if self.INPUTS not in tosca[self.TOPOLOGY_TMPL]:
                tosca[self.TOPOLOGY_TMPL][self.INPUTS] = {}
            for inp in self.inputs:
                entry = {inp[self.NAME]: {self.TYPE: self.STRING,
                                          self.DESC:
                                          'Translated from YANG'}}
                tosca[self.TOPOLOGY_TMPL][self.INPUTS] = entry

        tosca[self.TOPOLOGY_TMPL][self.NODE_TMPL] = {}

        # Add the VNFDs and VLDs
        for idx, vnfd in self.vnfds.items():
            vnfd.generate_vnf_template(tosca, idx)

        for vld in self.vlds:
            vld.generate_tosca_template(tosca)

        # add the config primitives
        if len(self.conf_prims):
            if self.GROUPS not in tosca[self.TOPOLOGY_TMPL]:
                tosca[self.TOPOLOGY_TMPL][self.GROUPS] = {}

            conf_prims = {
                self.TYPE: self.T_CONF_PRIM
            }
            conf_prims[self.MEMBERS] = [vnfd.name for vnfd in
                                        self.vnfds.values()]
            prims = {}
            for confp in self.conf_prims:
                prims[confp[self.NAME]] = {
                    self.USER_DEF_SCRIPT: confp[self.USER_DEF_SCRIPT]
                }
            conf_prims[self.PROPERTIES] = {
                self.PRIMITIVES: prims
            }

            tosca[self.TOPOLOGY_TMPL][self.GROUPS][self.CONF_PRIM] = conf_prims


        # Add the scale group
        if len(self.scale_grps):
            if self.POLICIES not in tosca[self.TOPOLOGY_TMPL]:
                tosca[self.TOPOLOGY_TMPL][self.POLICIES] = []

            for sg in self.scale_grps:
                sgt = {
                    self.TYPE: self.T_SCALE_GRP,
                }
                sgt.update(sg)
                tosca[self.TOPOLOGY_TMPL][self.POLICIES].append({
                    self.SCALE_GRP: sgt
                })

        # Add initial configs
        if len(self.initial_cfg):
            if self.POLICIES not in tosca[self.TOPOLOGY_TMPL]:
                tosca[self.TOPOLOGY_TMPL][self.POLICIES] = []

            for icp in self.initial_cfg:
                icpt = {
                    self.TYPE: self.T_INITIAL_CFG,
                }
                icpt.update(icp)
                tosca[self.TOPOLOGY_TMPL][self.POLICIES].append({
                    self.INITIAL_CFG: icpt
                })

        return tosca

    def get_supporting_files(self):
        files = []

        for vnfd in self.vnfds.values():
            f = vnfd.get_supporting_files()
            if f and len(f):
                files.extend(f)

        # Get the config files for initial config
        for icp in self.initial_cfg:
            if self.USER_DEF_SCRIPT in icp:
                script = os.path.basename(icp[self.USER_DEF_SCRIPT])
                files.append({
                    self.TYPE: 'script',
                    self.NAME: script,
                    self.DEST: "{}/{}".format(self.SCRIPT_DIR, script),
                })

        # TODO (pjoseph): Add support for config scripts,
        # charms, etc

        self.log.debug(_("{0}, supporting files: {1}").format(self, files))
        return files
