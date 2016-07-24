#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License. You may obtain
# a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.
#
# Copyright 2016 RIFT.io Inc


import os

from rift.mano.tosca_translator.common.utils import _
from rift.mano.tosca_translator.common.utils import ChecksumUtils
from rift.mano.tosca_translator.common.utils import convert_keys_to_python
from rift.mano.tosca_translator.rwmano.syntax.mano_resource import ManoResource

from toscaparser.common.exception import ValidationError
from toscaparser.elements.scalarunit import ScalarUnit_Size

# Name used to dynamically load appropriate map class.
TARGET_CLASS_NAME = 'ToscaCompute'


class ToscaCompute(ManoResource):
    '''Translate TOSCA node type RIFT.io VDUs.'''

    REQUIRED_PROPS = ['name', 'id', 'image', 'count', 'vm-flavor']
    OPTIONAL_PROPS = ['external-interface', 'image-checksum', 'cloud-init']
    IGNORE_PROPS = []

    toscatype = 'tosca.nodes.Compute'

    def __init__(self, log, nodetemplate, metadata=None):
        super(ToscaCompute, self).__init__(log,
                                           nodetemplate,
                                           type_='vdu',
                                           metadata=metadata)
        # List with associated port resources with this server
        self.assoc_port_resources = []
        self._image = None  # Image to bring up the VDU
        self._image_cksum = None
        self._vnf = None
        self._yang = None
        self._id = self.name

    @property
    def image(self):
        return self._image

    @property
    def vnf(self):
        return self._vnf

    @vnf.setter
    def vnf(self, vnf):
        if self._vnf:
            err_msg = (_('VDU {0} already has a VNF {1} associated').
                       format(self, self._vnf))
            self.log.error(err_msg)
            raise ValidationError(message=err_msg)
        self._vnf = vnf

    def handle_properties(self):
        tosca_props = self.get_tosca_props()
        self.log.debug(_("VDU {0} tosca properties: {1}").
                       format(self.name, tosca_props))
        vdu_props = {}
        for key, value in tosca_props.items():
            if key == 'cloud_init':
                vdu_props['cloud-init'] = value
            else:
                vdu_props[key] = value

        if 'name' not in vdu_props:
            vdu_props['name'] = self.name

        if 'id' not in vdu_props:
            vdu_props['id'] = self.id

        if 'count' not in vdu_props:
            vdu_props['count'] = 1

        self.log.debug(_("VDU {0} properties: {1}").
                       format(self.name, vdu_props))
        self.properties = vdu_props

    def handle_capabilities(self):

        def get_vm_flavor(specs):
            vm_flavor = {}
            if 'num_cpus' in specs:
                vm_flavor['vcpu-count'] = specs['num_cpus']
            else:
                vm_flavor['vcpu-count'] = 1

            if 'mem_size' in specs:
                vm_flavor['memory-mb'] = (ScalarUnit_Size(specs['mem_size']).
                                          get_num_from_scalar_unit('MB'))
            else:
                vm_flavor['memory-mb'] = 512

            if 'disk_size' in specs:
                vm_flavor['storage-gb'] = (ScalarUnit_Size(specs['disk_size']).
                                           get_num_from_scalar_unit('GB'))
            else:
                vm_flavor['storage-gb'] = 4

            return vm_flavor

        tosca_caps = self.get_tosca_caps()
        self.log.debug(_("VDU {0} tosca capabilites: {1}").
                       format(self.name, tosca_caps))

        if 'host' in tosca_caps:
            self.properties['vm-flavor'] = get_vm_flavor(tosca_caps['host'])
            self.log.debug(_("VDU {0} properties: {1}").
                           format(self.name, self.properties))

    def handle_artifacts(self):
        if self.artifacts is None:
            return
        self.log.debug(_("VDU {0} tosca artifacts: {1}").
                       format(self.name, self.artifacts))
        arts = {}
        for key in self.artifacts:
            props = self.artifacts[key]
            if isinstance(props, dict):
                details = {}
                for name, value in props.items():
                    if name == 'type':
                        prefix, type_ = value.rsplit('.', 1)
                        if type_ == 'QCOW2':
                            details['type'] = 'qcow2'
                        else:
                            err_msg = _("VDU {0}, Currently only QCOW2 images "
                                        "are supported in artifacts ({1}:{2})"). \
                                        format(self.name, key, value)
                            self.log.error(err_msg)
                            raise ValidationError(message=err_msg)
                    elif name == 'file':
                        details['file'] = value
                    elif name == 'image_checksum':
                        details['image_checksum'] = value
                    else:
                        self.log.warn(_("VDU {0}, unsuported attribute {1}").
                                      format(self.name, name))
                if len(details):
                    arts[key] = details
            else:
                arts[key] = self.artifacts[key]

        self.log.debug(_("VDU {0} artifacts: {1}").
                       format(self.name, arts))
        self.artifacts = arts

    def handle_interfaces(self):
        # Currently, we support only create operation
        operations_deploy_sequence = ['create']

        operations = ManoResource._get_all_operations(self.nodetemplate)

        # use the current ManoResource for the first operation in this order
        # Currently we only support image in create operation
        for operation in operations.values():
            if operation.name in operations_deploy_sequence:
                self.operations[operation.name] = None
                try:
                    self.operations[operation.name] = operation.implementation
                    for name, details in self.artifacts.items():
                        if name == operation.implementation:
                            self._image = details['file']
                except KeyError as e:
                    self.log.exception(e)
        return None

    def update_image_checksum(self, in_file):
        # Create image checksum
        # in_file is the TOSCA yaml file location
        if self._image is None:
            return
        self.log.debug("Update image: {}".format(in_file))
        if os.path.exists(in_file):
            in_dir = os.path.dirname(in_file)
            img_dir = os.path.dirname(self._image)
            abs_dir = os.path.normpath(
                os.path.join(in_dir, img_dir))
            self.log.debug("Abs path: {}".format(abs_dir))
            if os.path.isdir(abs_dir):
                img_path = os.path.join(abs_dir,
                                        os.path.basename(self._image))
                self.log.debug(_("Image path: {0}").
                               format(img_path))
                if os.path.exists(img_path):
                    # TODO (pjoseph): To be fixed when we can retrieve
                    # the VNF image in Launchpad.
                    # Check if the file is not size 0
                    # else it is a dummy file and to be ignored
                    if os.path.getsize(img_path) != 0:
                        self._image_cksum = ChecksumUtils.get_md5(img_path,
                                                                  log=self.log)

    def get_mano_attribute(self, attribute, args):
        attr = {}
        # Convert from a TOSCA attribute for a nodetemplate to a MANO
        # attribute for the matching resource.  Unless there is additional
        # runtime support, this should be a one to one mapping.

        # Note: We treat private and public IP  addresses equally, but
        # this will change in the future when TOSCA starts to support
        # multiple private/public IP addresses.
        self.log.debug(_('Converting TOSCA attribute for a nodetemplate to a MANO \
                  attriute.'))
        if attribute == 'private_address' or \
           attribute == 'public_address':
                attr['get_attr'] = [self.name, 'networks', 'private', 0]

        return attr

    def _update_properties_for_model(self):
        if self._image:
            self.properties['image'] = os.path.basename(self._image)
            if self._image_cksum:
                self.properties['image-checksum'] = self._image_cksum

        for key in ToscaCompute.IGNORE_PROPS:
            if key in self.properties:
                self.properties.pop(key)

    def generate_yang_submodel_gi(self, vnfd):
        if vnfd is None:
            return None
        self._update_properties_for_model()
        props = convert_keys_to_python(self.properties)
        try:
            vnfd.vdu.add().from_dict(props)
        except Exception as e:
            err_msg = _("{0} Exception vdu from dict {1}: {2}"). \
                      format(self, props, e)
            self.log.error(err_msg)
            raise e

    def generate_yang_submodel(self):
        """Generate yang model for the VDU"""
        self.log.debug(_("Generate YANG model for {0}").
                       format(self))

        self._update_properties_for_model()

        vdu = self.properties

        return vdu
