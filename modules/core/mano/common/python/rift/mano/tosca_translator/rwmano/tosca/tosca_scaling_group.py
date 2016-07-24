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
TARGET_CLASS_NAME = 'ToscaScalingGroup'


class ToscaScalingGroup(ManoResource):
    '''Translate TOSCA node type tosca.policies.Scaling.'''

    toscatype = 'tosca.policies.riftio.ScalingGroup'

    IGNORE_PROPS = []

    def __init__(self, log, policy, metadata=None):
        # TODO(Philip):Not inheriting for ManoResource, as there is no
        # instance from parser
        self.log = log
        for name, details in policy.items():
            self.name = name
            self.details = details
            break
        self.type_ = 'scale-grp'
        self.metadata = metadata
        self.properties = {}

    def __str__(self):
        return "%s(%s)" % (self.name, self.type)

    def handle_properties(self, nodes, groups):
        tosca_props = self.details
        self.log.debug(_("{0} with tosca properties: {1}").
                       format(self, tosca_props))
        self.properties['name'] = tosca_props['name']
        self.properties['max-instance-count'] = \
                                tosca_props['max_instance_count']
        self.properties['min-instance-count'] = \
                                tosca_props['min_instance_count']
        self.properties['vnfd-member'] = []

        def _get_node(name):
            for node in nodes:
                if node.name == name:
                    return node

        for member, count in tosca_props['vnfd_members'].items():
            node = _get_node(member)
            if node:
                memb = {}
                memb['member-vnf-index-ref'] = node.get_member_vnf_index()
                memb['count'] = count
                self.properties['vnfd-member'].append(memb)
            else:
                err_msg = _("{0}: Did not find the member node {1} in "
                            "resources list"). \
                          format(self, member)
                self.log.error(err_msg)
                raise ValidationError(message=err_msg)

        def _validate_action(action):
            for group in groups:
                if group.validate_primitive(action):
                    return True
            return False

        self.properties['scaling-config-action'] = []
        for action, value in tosca_props['config_actions'].items():
            conf = {}
            if _validate_action(value):
                conf['trigger'] = action
                conf['ns-config-primitive-name-ref'] = value
                self.properties['scaling-config-action'].append(conf)
            else:
                err_msg = _("{0}: Did not find the action {1} in "
                            "config primitives"). \
                          format(self, action)
                self.log.error(err_msg)
                raise ValidationError(message=err_msg)

        self.log.debug(_("{0} properties: {1}").format(self, self.properties))

    def get_yang_model_gi(self, nsd, vnfds):
        props = convert_keys_to_python(self.properties)
        try:
            nsd.scaling_group_descriptor.add().from_dict(props)
        except Exception as e:
            err_msg = _("{0} Exception nsd scaling group from dict {1}: {2}"). \
                      format(self, props, e)
            self.log.error(err_msg)
            raise e

    def generate_yang_model(self, nsd, vnfds, use_gi=False):
        """Generate yang model for the node"""
        self.log.debug(_("Generate YANG model for {0}").
                       format(self))

        for key in ToscaScalingGroup.IGNORE_PROPS:
            if key in self.properties:
                self.properties.pop(key)

        if use_gi:
            return self.get_yang_model_gi(nsd, vnfds)

        if 'scaling-group-descriptor' not in nsd:
            nsd['scaling-group-descriptor'] = []
        scale = {}
        for key, value in self.properties.items():
            scale[key] = value
        nsd['scaling-group-descriptor'].append(scale)
