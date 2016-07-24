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

TARGET_CLASS_NAME = 'YangVld'


class YangVld(ToscaResource):
    '''Class for RIFT.io YANG VLD descriptor translation to TOSCA type.'''

    yangtype = 'vld'

    OTHER_KEYS = (VNFD_CP_REF) = \
                 ('vnfd_connection_point_ref')

    VLD_TYPE_MAP = {
        'ELAN': ToscaResource.T_VL1,
    }

    def __init__(self,
                 log,
                 name,
                 type_,
                 yang):
        super(YangVld, self).__init__(log,
                                      name,
                                      type_,
                                      yang)
        self.yang = yang
        self.props = {}

    def process_vld(self, vnfds):
        self.log.debug(_("Process VLD desc {0}").format(self.name))

        dic = deepcopy(self.yang)

        for key in self.REQUIRED_FIELDS:
            self.props[key] = dic.pop(key)

        self.id = self.props[self.ID]

        if self.TYPE_Y in dic:
            self.props[self.TYPE] = dic.pop(self.TYPE_Y)
            if self.props[self.TYPE] not in self.VLD_TYPE_MAP:
                err_msg = (_("{0}: VLD type {1} not supported").
                           format(self, self.props[self.TYPE]))
                self.log.error(err_msg)
                raise ValidationError(message=err_msg)

        if self.VNFD_CP_REF in dic:
            for cp_ref in dic.pop(self.VNFD_CP_REF):
                vnfd_idx = cp_ref.pop(self.MEM_VNF_INDEX_REF)
                vnfd_id = cp_ref.pop(self.VNFD_ID_REF)
                vnfd_cp = cp_ref.pop(self.VNFD_CP_REF)
                if vnfd_idx in vnfds:
                    vnfd = vnfds[vnfd_idx]
                    if vnfd.id == vnfd_id:
                        # Update the CP as linked to this VLD
                        vnfd.update_cp_vld(vnfd_cp, self.name)
                    else:
                        err_msg = (_("{0}, The VNFD memebr index {1} and id "
                                     "{2} did not match the VNFD {3} with "
                                     "id {4}").format(self, vnfd_idx, vnfd_id,
                                                      vnfd.name, vnfd.id))
                        self.log.error(err_msg)
                        raise ValidationError(message=err_msg)
                else:
                    err_msg = (_("{0}, Did not find VNFD memer index {1}").
                               format(self, vnfd_idx))
                    self.log.error(err_msg)
                    raise ValidationError(message=err_msg)

        self.remove_ignored_fields(dic)
        if len(dic):
            self.log.warn(_("{0}, Did not process the following for "
                            "VLD {1}: {2}").
                          format(self, self.props, dic))
        self.log.debug(_("{0}, VLD: {1}").format(self, self.props))

    def generate_tosca_type(self, tosca):
        self.log.debug(_("{0} Generate tosa types").
                       format(self, tosca))

        if self.T_VL1 not in tosca[self.NODE_TYPES]:
            tosca[self.NODE_TYPES][self.T_VL1] = {
                self.DERIVED_FROM: 'tosca.nodes.nfv.VL.ELAN',
                self.PROPERTIES: {
                    'description':
                    {self.TYPE: self.STRING},
                },
            }

        return tosca

    def generate_tosca_template(self, tosca):
        self.log.debug(_("{0} Generate tosa types").
                       format(self, tosca))

        node = {}
        node[self.TYPE] = self.VLD_TYPE_MAP[self.props.pop(self.TYPE)]

        # Remove
        self.props.pop(self.ID)
        self.props.pop(self.VERSION)
        node[self.PROPERTIES] = self.props

        tosca[self.TOPOLOGY_TMPL][self.NODE_TMPL][self.name] = node

        return tosca
