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
from rift.mano.tosca_translator.rwmano.syntax.mano_resource import ManoResource

from toscaparser.common.exception import ValidationError


# Name used to dynamically load appropriate map class.
TARGET_CLASS_NAME = 'ToscaNetworkPort'
TOSCA_LINKS_TO = 'tosca.relationships.network.LinksTo'
TOSCA_BINDS_TO = 'tosca.relationships.network.BindsTo'


class ToscaNetworkPort(ManoResource):
    '''Translate TOSCA node type tosca.nodes.network.Port.'''

    toscatype = 'tosca.nodes.network.Port'

    VALID_TYPES = ['VIRTIO', 'VPORT']

    def __init__(self, log, nodetemplate, metadata=None):
        super(ToscaNetworkPort, self).__init__(log,
                                               nodetemplate,
                                               type_='port',
                                               metadata=metadata)
        # Default order
        self.order = 0
        pass

    def handle_properties(self):
        tosca_props = self.get_tosca_props()
        self.log.debug(_("Port {0} with tosca properties: {1}").
                       format(self.name, tosca_props))
        port_props = {}
        for key, value in tosca_props.items():
            port_props[key] = value

        if 'cp_type' not in port_props:
            port_props['cp_type'] = 'VPORT'
        else:
            if not port_props['cp_type'] in ToscaNetworkPort.VALID_TYPES:
                err_msg = _("Invalid port type, {0}, specified for {1}"). \
                          format(port_props['cp_type'], self.name)
                self.log.warn(err_msg)
                raise ValidationError(message=err_msg)

        if 'vdu_intf_type' not in port_props:
            port_props['vdu_intf_type'] = 'VIRTIO'
        else:
            if not port_props['vdu_intf_type'] in ToscaNetworkPort.VALID_TYPES:
                err_msg = _("Invalid port type, {0}, specified for {1}"). \
                          format(port_props['vdu_intf_type'], self.name)
                self.log.warn(err_msg)
                raise ValidationError(message=err_msg)

        self.properties = port_props

    def handle_requirements(self, nodes):
        tosca_reqs = self.get_tosca_reqs()
        self.log.debug("VNF {0} requirements: {1}".
                       format(self.name, tosca_reqs))

        vnf = None  # Need vnf ref to generate cp refs in vld
        vld = None
        if len(tosca_reqs) != 2:
            err_msg = _("Invalid configuration as incorrect number of "
                        "requirements for CP {0} are specified"). \
                        format(self)
            self.log.error(err_msg)
            raise ValidationError(message=err_msg)

        for req in tosca_reqs:
            if 'virtualBinding' in req:
                target = req['virtualBinding']['target']
                node = self.get_node_with_name(target, nodes)
                if node:
                    vnf = node.vnf
                    if not vnf:
                        err_msg = _("No vnfs linked to a VDU {0}"). \
                                    format(node)
                        self.log.error(err_msg)
                        raise ValidationError(message=err_msg)
                    cp = {}
                    cp['name'] = self.properties['name']
                    cp['type'] = self.properties['cp_type']
                    self.log.debug(_("Connection Point entry for VNF {0}:{1}").
                                   format(vnf, cp))
                    if 'connection-point' not in vnf.properties:
                        vnf.properties['connection-point'] = []
                    vnf.properties['connection-point'].append(cp)
                    ext_intf = {}
                    ext_intf['name'] = self.properties['vdu_intf_name']
                    ext_intf['virtual-interface'] = \
                                    {'type': self.properties['vdu_intf_type']}
                    ext_intf['vnfd-connection-point-ref'] = \
                                    self.properties['name']
                    if 'external-interface' not in node.properties:
                        node.properties['external-interface'] = []
                    node.properties['external-interface'].append(ext_intf)
                else:
                    err_msg = _("Connection point {0}, VDU {1} "
                                "specified not found"). \
                                format(self.name, target)
                    self.log.error(err_msg)
                    raise ValidationError(message=err_msg)
            elif 'virtualLink' in req:
                target = req['virtualLink']['target']
                node = self.get_node_with_name(target, nodes)
                if node:
                    vld = node
                else:
                    err_msg = _("CP {0}, VL {1} specified not found"). \
                              format(self, target)
                    self.log.error(err_msg)
                    raise ValidationError(message=err_msg)

        if vnf and vld:
            cp_ref = {}
            cp_ref['vnfd-connection-point-ref'] = self.properties['name']
            cp_ref['vnfd-id-ref'] = vnf.properties['id']
            cp_ref['member-vnf-index-ref'] = \
                            vnf._const_vnfd['member-vnf-index']
            if 'vnfd-connection-point-ref' not in vld.properties:
                vld.properties['vnfd-connection-point-ref'] = []
            vld.properties['vnfd-connection-point-ref'].append(cp_ref)
        else:
            err_msg = _("CP {0}, VNF {1} or VL {2} not found"). \
                      format(self, vnf, vld)
            self.log.error(err_msg)
            raise ValidationError(message=err_msg)
