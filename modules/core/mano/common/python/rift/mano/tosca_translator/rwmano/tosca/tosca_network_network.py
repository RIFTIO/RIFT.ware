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


from rift.mano.tosca_translator.common.utils import _
from rift.mano.tosca_translator.common.utils import convert_keys_to_python
from rift.mano.tosca_translator.rwmano.syntax.mano_resource import ManoResource


# Name used to dynamically load appropriate map class.
TARGET_CLASS_NAME = 'ToscaNetwork'


class ToscaNetwork(ManoResource):
    '''Translate TOSCA node type tosca.nodes.network.Network.'''

    toscatype = 'tosca.nodes.network.Network'
    NETWORK_PROPS = ['network_name', 'network_id']
    REQUIRED_PROPS = ['name', 'id', 'type', 'version', 'short-name',
                      'description', 'vendor']
    OPTIONAL_PROPS = ['vnfd-connection-point-ref']
    IGNORE_PROPS = ['ip_version', 'dhcp_enabled']
    VALID_TYPES = ['ELAN']

    def __init__(self, log, nodetemplate, metadata=None):
        super(ToscaNetwork, self).__init__(log,
                                           nodetemplate,
                                           type_='vld',
                                           metadata=metadata)

    def handle_properties(self):
        tosca_props = self.get_tosca_props()

        if 'cidr' in tosca_props.keys():
            self.log.warn(_("Support for subnet not yet "
                            "available. Ignoring it"))
        net_props = {}
        for key, value in tosca_props.items():
            if key in self.NETWORK_PROPS:
                if key == 'network_name':
                    net_props['name'] = value
                elif key == 'network_id':
                    net_props['id'] = value
            else:
                net_props[key] = value

        net_props['type'] = self.get_type()

        if 'name' not in net_props:
            # Use the node name as network name
            net_props['name'] = self.name

        if 'short_name' not in net_props:
            # Use the node name as network name
            net_props['short-name'] = self.name

        if 'id' not in net_props:
            net_props['id'] = self.id

        if 'description' not in net_props:
            net_props['description'] = self.description

        if 'vendor' not in net_props:
            net_props['vendor'] = self.vendor

        if 'version' not in net_props:
            net_props['version'] = self.version

        self.log.debug(_("Network {0} properties: {1}").
                       format(self.name, net_props))
        self.properties = net_props

    def get_type(self):
        """Get the network type based on propery or type derived from"""
        node = self.nodetemplate
        tosca_props = self.get_tosca_props()
        try:
            if tosca_props['network_type'] in ToscaNetwork.VALID_TYPES:
                return tosca_props['network_type']
        except KeyError:
            pass

        node_type = node.type_definition

        while node_type.type:
            self.log.debug(_("Node name {0} with type {1}").
                           format(self.name, node_type.type))
            prefix, nw_type = node_type.type.rsplit('.', 1)
            if nw_type in ToscaNetwork.VALID_TYPES:
                return nw_type
            else:
                # Get the parent
                node_type = ManoResource.get_parent_type(node_type)

        return "ELAN"

    def generate_yang_model_gi(self, nsd, vnfds):
        props = convert_keys_to_python(self.properties)
        try:
            nsd.vld.add().from_dict(props)
        except Exception as e:
            err_msg = _("{0} Exception vld from dict {1}: {2}"). \
                      format(self, props, e)
            self.log.error(err_msg)
            raise e

    def generate_yang_model(self, nsd, vnfds, use_gi=False):
        """Generate yang model for the node"""
        self.log.debug(_("Generate YANG model for {0}").
                       format(self))

        # Remove the props to be ignroed:
        for key in ToscaNetwork.IGNORE_PROPS:
            if key in self.properties:
                self.properties.pop(key)

        if use_gi:
            return self.generate_yang_model_gi(nsd, vnfds)

        vld = self.properties

        if 'vld' not in nsd:
            nsd['vld'] = []
        nsd['vld'].append(vld)
