#
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
#


from rift.mano.tosca_translator.common.utils import _
from rift.mano.tosca_translator.common.utils import convert_keys_to_python
from rift.mano.tosca_translator.rwmano.syntax.mano_resource import ManoResource

from toscaparser.common.exception import ValidationError

# Name used to dynamically load appropriate map class.
TARGET_CLASS_NAME = 'ToscaConfigPrimitives'


class ToscaConfigPrimitives(ManoResource):
    '''Translate TOSCA node type tosca.groups.riftio.config_primitives.'''

    toscatype = 'tosca.groups.riftio.ConfigPrimitives'

    def __init__(self, log, name, details, metadata=None):
        # TODO(Philip):Not inheriting for ManoResource, as there is no
        # instance from parser
        self.log = log
        self.name = name
        self.details = details
        self.type_ = 'config-prim'
        self.metadata = metadata
        self.nodes = []

    def __str__(self):
        return "%s(%s)" % (self.name, self.type)

    def handle_properties(self, nodes):
        tosca_props = self.details['properties']
        self.log.debug(_("{0} with tosca properties: {1}").
                       format(self, tosca_props))

        members = self.details['members']
        for member in members:
            found = False
            for node in nodes:
                if member == node.name:
                    self.nodes.append(node)
                    found = True
                    break
            if not found:
                err_msg = _("{0}: Did not find the member node {1} in "
                            "resources list"). \
                          format(self, node)
                self.log.error(err_msg)
                raise ValidationError(message=err_msg)

        self.primitives = tosca_props['primitives']

    def get_primitive(self, primitive):
        if primitive in self.primitives:
            return self.primitives[primitive]

    def validate_primitive(self, primitive):
        if primitive in self.primitives:
            return True
        return False

    def generate_yang_model_gi(self, nsd, vnfds):
        for name, value in self.primitives.items():
            prim = {'name': name}
            props = convert_keys_to_python(value)
            try:
                prim.update(props)
            except Exception as e:
                err_msg = _("{0} Exception nsd config primitives {1}: {2}"). \
                          format(self, props, e)
                self.log.error(err_msg)
                raise e
            nsd.config_primitive.add().from_dict(prim)

    def generate_yang_model(self, nsd, vnfds, use_gi=False):
        """Generate yang model for the node"""
        self.log.debug(_("Generate YANG model for {0}").
                       format(self))

        if use_gi:
            return self.generate_yang_model_gi(nsd, vnfds)

        nsd['config-primitive'] = []
        for name, value in self.primitives.items():
            prim = {'name': name}
            prim.update(self.map_keys_to_mano(value))
            nsd['config-primitive'].append(prim)
