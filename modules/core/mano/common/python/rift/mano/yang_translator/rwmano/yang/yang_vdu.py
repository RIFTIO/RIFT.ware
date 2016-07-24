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


import os
import shutil
import tempfile

from copy import deepcopy

from rift.mano.yang_translator.common.exception import ValidationError
from rift.mano.yang_translator.common.utils import _
from rift.mano.yang_translator.rwmano.syntax.tosca_resource \
    import ToscaResource

import rift.package.image

TARGET_CLASS_NAME = 'YangVdu'


class YangVdu(ToscaResource):
    '''Class for RIFT.io YANG VDU descriptor translation to TOSCA type.'''

    yangtype = 'vdu'

    OTHER_KEYS = (VM_FLAVOR, CLOUD_INIT, IMAGE, IMAGE_CHKSUM,
                  VNFD_CP_REF, CP_TYPE) = \
                 ('vm_flavor', 'cloud_init', 'image', 'image_checksum',
                  'vnfd_connection_point_ref', 'cp_type')

    TOSCA_MISC_KEYS = (VIRT_LINK, VIRT_BIND, VDU_INTF_NAME,
                       VDU_INTF_TYPE) = \
                      ('virtualLink', 'virtualBinding', 'vdu_intf_name',
                       'vdu_intf_type')

    VM_FLAVOR_MAP = {
        'vcpu_count': 'num_cpus',
        'memory_mb': 'mem_size',
        'storage_gb': 'disk_size',
    }

    VM_SIZE_UNITS_MAP = {
        'vcpu_count': '',
        'memory_mb': ' MB',
        'storage_gb': ' GB',
    }

    def __init__(self,
                 log,
                 name,
                 type_,
                 yang):
        super(YangVdu, self).__init__(log,
                                      name,
                                      type_,
                                      yang)
        self.yang = yang
        self.props = {}
        self.ext_cp = []
        self.int_cp = []
        self.images = []

    def process_vdu(self):
        self.log.debug(_("Process VDU desc {0}: {1}").format(self.name,
                                                             self.yang))

        vdu_dic = deepcopy(self.yang)
        vdu = {}

        fields = [self.ID, self.COUNT, self.CLOUD_INIT,
                  self.IMAGE, self.IMAGE_CHKSUM]
        for key in fields:
            if key in vdu_dic:
                vdu[key] = vdu_dic.pop(key)

        self.id = vdu[self.ID]

        if self.VM_FLAVOR in vdu_dic:
            vdu[self.HOST] = {}
            for key, value in vdu_dic.pop(self.VM_FLAVOR).items():
                vdu[self.HOST][self.VM_FLAVOR_MAP[key]] = "{}{}". \
                            format(value, self.VM_SIZE_UNITS_MAP[key])

        if self.EXT_INTF in vdu_dic:
            for ext_intf in vdu_dic.pop(self.EXT_INTF):
                cp = {}
                cp[self.NAME] = ext_intf.pop(self.VNFD_CP_REF)
                cp[self.VDU_INTF_NAME] = ext_intf.pop(self.NAME)
                cp[self.VDU_INTF_TYPE] = ext_intf[self.VIRT_INTF][self.TYPE_Y]
                self.log.debug(_("{0}, External interface {1}: {2}").
                               format(self, cp, ext_intf))
                self.ext_cp.append(cp)

        self.remove_ignored_fields(vdu_dic)
        if len(vdu_dic):
            self.log.warn(_("{0}, Did not process the following in "
                            "VDU: {1}").
                          format(self, vdu_dic))

        self.log.debug(_("{0} VDU: {1}").format(self, vdu))
        self.props = vdu

    def get_cp(self, name):
        for cp in self.ext_cp:
            if cp[self.NAME] == name:
                return cp
        return None

    def has_cp(self, name):
        if self.get_cp(name):
            return True
        return False

    def set_cp_type(self, name, cp_type):
        for idx, cp in enumerate(self.ext_cp):
            if cp[self.NAME] == name:
                cp[self.CP_TYPE] = cp_type
                self.ext_cp[idx] = cp
                self.log.debug(_("{0}, Updated CP: {1}").
                               format(self, self.ext_cp[idx]))
                return

        err_msg = (_("{0}, Did not find connection point {1}").
                   format(self, name))
        self.log.error(err_msg)
        raise ValidationError(message=err_msg)

    def set_vld(self, name, vld_name):
        cp = self.get_cp(name)
        if cp:
            cp[self.VLD] = vld_name
        else:
            err_msg = (_("{0}, Did not find connection point {1}").
                       format(self, name))
            self.log.error(err_msg)
            raise ValidationError(message=err_msg)

    def get_name(self, vnf_name):
        # Create a unique name incase multiple VNFs use same
        # name for the vdu
        return "{}_{}".format(vnf_name, self.name)

    def generate_tosca_type(self, tosca):
        self.log.debug(_("{0} Generate tosa types").
                       format(self, tosca))

        # Add custom artifact type
        if self.ARTIFACT_TYPES not in tosca:
            tosca[self.ARTIFACT_TYPES] = {}
        if self.T_ARTF_QCOW2 not in tosca[self.ARTIFACT_TYPES]:
            tosca[self.ARTIFACT_TYPES][self.T_ARTF_QCOW2] = {
                self.DERIVED_FROM: 'tosca.artifacts.Deployment.Image.VM.QCOW2',
                self.IMAGE_CHKSUM:
                {self.TYPE: self.STRING,
                 self.REQUIRED: self.NO},
            }

        if self.T_VDU1 not in tosca[self.NODE_TYPES]:
            tosca[self.NODE_TYPES][self.T_VDU1] = {
                self.DERIVED_FROM: 'tosca.nodes.nfv.VDU',
                self.PROPERTIES: {
                    self.COUNT:
                    {self.TYPE: self.INTEGER,
                     self.DEFAULT: 1},
                    self.CLOUD_INIT:
                    {self.TYPE: self.STRING,
                     self.DEFAULT: '#cloud-config'},
                },
                self.CAPABILITIES: {
                    self.VIRT_LINK: {
                        self.TYPE: 'tosca.capabilities.nfv.VirtualLinkable'
                    },
                },
            }

        # Add CP type
        if self.T_CP1 not in tosca[self.NODE_TYPES]:
            tosca[self.NODE_TYPES][self.T_CP1] = {
                self.DERIVED_FROM: 'tosca.nodes.nfv.CP',
                self.PROPERTIES: {
                    self.NAME:
                    {self.TYPE: self.STRING,
                     self.DESC: 'Name of the connection point'},
                    self.CP_TYPE:
                    {self.TYPE: self.STRING,
                     self.DESC: 'Type of the connection point'},
                    self.VDU_INTF_NAME:
                    {self.TYPE: self.STRING,
                     self.DESC: 'Name of the interface on VDU'},
                    self.VDU_INTF_TYPE:
                    {self.TYPE: self.STRING,
                     self.DESC: 'Type of the interface on VDU'},
                },
             }

        return tosca

    def generate_vdu_template(self, tosca, vnf_name):
        self.log.debug(_("{0} Generate tosca template for {2}").
                       format(self, tosca, vnf_name))

        name = self.get_name(vnf_name)

        node = {}
        node[self.TYPE] = self.T_VDU1

        if self.HOST in self.props:
            node[self.CAPABILITIES] = {
                self.HOST: {self.PROPERTIES: self.props.pop(self.HOST)}
            }
        else:
            self.log.warn(_("{0}, Does not have host requirements defined").
                          format(self))

        if self.IMAGE in self.props:
            img_name = "{}_{}_vm_image".format(vnf_name, self.name)
            image = "../{}/{}".format(self.IMAGE_DIR, self.props.pop(self.IMAGE))
            self.images.append(image)
            node[self.ARTIFACTS] = {img_name: {
                self.FILE: image,
                self.TYPE: self.T_ARTF_QCOW2,
            }}
            if self.IMAGE_CHKSUM in self.props:
                node[self.ARTIFACTS][img_name][self.IMAGE_CHKSUM] = \
                                            self.props.pop(self.IMAGE_CHKSUM)
            node[self.INTERFACES] = {'Standard': {
                'create': img_name
            }}

        # Remove
        self.props.pop(self.ID)
        node[self.PROPERTIES] = self.props

        self.log.debug(_("{0}, VDU node: {1}").format(self, node))
        tosca[self.TOPOLOGY_TMPL][self.NODE_TMPL][name] = node

        # Generate the connection point templates
        for cp in self.ext_cp:
            cpt = {self.TYPE: self.T_CP1}

            cpt[self.REQUIREMENTS] = []
            cpt[self.REQUIREMENTS].append({self.VIRT_BIND: {
                self.NODE: self.get_name(vnf_name)
            }})
            if self.VLD in cp:
                vld = cp.pop(self.VLD)
                cpt[self.REQUIREMENTS].append({self.VIRT_LINK: {
                    self.NODE: vld
                }})

            cpt[self.PROPERTIES] = cp
            cp_name = cp[self.NAME].replace('/', '_')

            self.log.debug(_("{0}, CP node {1}: {2}").
                           format(self, cp_name, cpt))
            tosca[self.TOPOLOGY_TMPL][self.NODE_TMPL][cp_name] = cpt

        return tosca

    def get_supporting_files(self):
        files = []

        for image in self.images:
            image_name = os.path.basename(image)

            files.append({
                self.TYPE: 'image',
                self.NAME: image_name,
                self.DEST: "{}/{}".format(self.IMAGE_DIR, image_name),
            })

        self.log.debug(_("Supporting files for {} : {}").format(self, files))
        if not len(files):
            shutil.rmtree(out_dir)

        return files
