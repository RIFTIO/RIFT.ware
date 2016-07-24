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
TARGET_CLASS_NAME = 'ToscaInitialConfig'


class ToscaInitialConfig(ManoResource):
    '''Translate TOSCA node type tosca.policies.InitialConfigPrimitive.'''

    toscatype = 'tosca.policies.riftio.InitialConfigPrimitive'

    IGNORE_PROPS = []

    def __init__(self, log, primitive, metadata=None):
        # TODO(Philip):Not inheriting for ManoResource, as there is no
        # instance from parser
        self.log = log
        for name, details in primitive.items():
            self.name = name
            self.details = details
            break
        self.type_ = 'initial-cfg'
        self.metadata = metadata
        self.properties = {}
        self.scripts = []

    def __str__(self):
        return "%s(%s)" % (self.name, self.type)

    def handle_properties(self, nodes, groups):
        tosca_props = self.details
        self.log.debug(_("{0} with tosca properties: {1}").
                       format(self, tosca_props))
        self.properties['name'] = tosca_props['name']
        self.properties['seq'] = \
                                tosca_props['seq']
        self.properties['user-defined-script'] = \
                                tosca_props['user_defined_script']
        self.scripts.append('../scripts/{}'. \
                            format(tosca_props['user_defined_script']))

        if 'parameter' in tosca_props:
            self.properties['parameter'] = []
            for name, value in tosca_props['parameter'].items():
                self.properties['parameter'].append({
                    'name': name,
                    'value': value,
                })

        self.log.debug(_("{0} properties: {1}").format(self, self.properties))

    def get_yang_model_gi(self, nsd, vnfds):
        props = convert_keys_to_python(self.properties)
        try:
            nsd.initial_config_primitive.add().from_dict(props)
        except Exception as e:
            err_msg = _("{0} Exception nsd initial config from dict {1}: {2}"). \
                      format(self, props, e)
            self.log.error(err_msg)
            raise e

    def generate_yang_model(self, nsd, vnfds, use_gi=False):
        """Generate yang model for the node"""
        self.log.debug(_("Generate YANG model for {0}").
                       format(self))

        for key in ToscaInitialConfig.IGNORE_PROPS:
            if key in self.properties:
                self.properties.pop(key)

        if use_gi:
            return self.get_yang_model_gi(nsd, vnfds)

        if 'initial-config-primitive' not in nsd:
            nsd['initial-config-primitive'] = []
        prim = {}
        for key, value in self.properties.items():
            prim[key] = value
        nsd['initial-config-primitive'].append(prim)

    def get_supporting_files(self, files, desc_id=None):
        if not len(self.scripts):
            return

        if desc_id not in files:
            files[desc_id] = []

        for script in self.scripts:
            files[desc_id].append({
                'type': 'script',
                'name': script,
            },)
