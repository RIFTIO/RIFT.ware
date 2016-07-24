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

try:
    import gi
    gi.require_version('RwVnfdYang', '1.0')

    from gi.repository import RwVnfdYang
except ImportError:
    pass
except ValueError:
    pass


# Name used to dynamically load appropriate map class.
TARGET_CLASS_NAME = 'ToscaNfvVnf'


class ToscaNfvVnf(ManoResource):
    '''Translate TOSCA node type tosca.nodes.nfv.vnf.'''

    toscatype = 'tosca.nodes.nfv.VNF'

    REQUIRED_PROPS = ['name', 'short-name', 'id', 'short-name', 'description',
                      'mgmt-interface']
    OPTIONAL_PROPS = ['version', 'vendor', 'http-endpoint', 'monitoring-param',
                      'connection-point']
    IGNORE_PROPS = ['port']
    TOSCA_CAPS = ['mgmt_interface', 'http_endpoint', 'monitoring_param_0',
                  'monitoring_param_1', 'connection_point']

    def __init__(self, log, nodetemplate, metadata=None):
        super(ToscaNfvVnf, self).__init__(log,
                                          nodetemplate,
                                          type_="vnfd",
                                          metadata=metadata)
        self._const_vnfd = {}
        self._vnf_config = {}
        self._vdus = []

    def map_tosca_name_to_mano(self, name):
        new_name = super().map_tosca_name_to_mano(name)
        if new_name.startswith('monitoring-param'):
            new_name = 'monitoring-param'
        if new_name == 'polling-interval':
            new_name = 'polling_interval_secs'
        return new_name

    def handle_properties(self):
        tosca_props = self.get_tosca_props()
        self.log.debug(_("VNF {0} with tosca properties: {1}").
                       format(self.name, tosca_props))

        def get_vnf_config(config):
            vnf_config = {}
            for key, value in config.items():
                new_key = self.map_tosca_name_to_mano(key)
                if isinstance(value, dict):
                    sub_config = {}
                    for subkey, subvalue in value.items():
                        sub_config[self.map_tosca_name_to_mano(subkey)] = \
                                        subvalue
                    vnf_config[new_key] = sub_config
                else:
                    vnf_config[new_key] = value

            if vnf_config['config-type'] != 'script':
                err_msg = _("{}, Only script config supported "
                             "for now: {}"). \
                           format(self, vnf_config['config-type'])
                self.log.error(err_msg)
                raise ValidationError(message=err_msg)

            # Replace config-details with actual name (config-type)
            if ('config-type' in vnf_config and
                'config-details' in vnf_config):
                vnf_config[vnf_config['config-type']] = \
                                    vnf_config.pop('config-details')
                vnf_config.pop('config-type')

            # Update config-delay and confgig-priortiy to correct struct
            vnf_config['config-attributes'] = {}
            if 'config-delay' in vnf_config:
                vnf_config['config-attributes']['config-delay'] = \
                            vnf_config.pop('config-delay')
            else:
                vnf_config['config-attributes']['config-delay'] = 0
            if 'config-priority' in vnf_config:
                vnf_config['config-attributes']['config-priority'] = \
                            vnf_config.pop('config-priority')
            return vnf_config

        vnf_props = {}
        for key, value in tosca_props.items():
            if key == 'id':
                self._const_vnfd['member-vnf-index'] = int(value)
                self._const_vnfd['vnfd-id-ref'] = self.id
            elif key == 'vnf_configuration':
                self._vnf_config = get_vnf_config(value)
            else:
                vnf_props[key] = value

        if 'name' not in vnf_props:
            vnf_props['name'] = self.name

        if 'short-name' not in vnf_props:
            vnf_props['short-name'] = self.name

        if 'id' not in vnf_props:
            vnf_props['id'] = self.id

        if 'vendor' not in vnf_props:
            vnf_props['vendor'] = self.vendor

        if 'description' not in vnf_props:
            vnf_props['description'] = self.description

        if 'start_by_default' in vnf_props:
            self._const_vnfd['start-by-default'] = \
                                        vnf_props.pop('start_by_default')

        self.log.debug(_("VNF {0} with constituent vnf: {1}").
                       format(self.name, self._const_vnfd))
        self.log.debug(_("VNF {0} with properties: {1}").
                       format(self.name, vnf_props))
        self.properties = vnf_props

    def handle_capabilities(self):
        tosca_caps = self.get_tosca_caps()
        self.log.debug(_("VDU {0} tosca capabilites: {1}").
                       format(self.name, tosca_caps))

        def get_props(props):
            properties = {}
            for key in props.keys():
                value = props[key]
                if isinstance(value, dict):
                    if 'get_property' in value:
                        val = self.get_property(value['get_property'])
                        value = val
                properties[self.map_tosca_name_to_mano(key)] = value
            return properties

        for key, value in tosca_caps.items():
            if key in ToscaNfvVnf.TOSCA_CAPS:
                new_key = self.map_tosca_name_to_mano(key)
                props = get_props(value)
                if 'id' in props:
                    props['id'] = str(props['id'])
                if 'protocol' in props:
                    props.pop('protocol')

                # There is only one instance of mgmt interface, but others
                # are a list
                if key == 'mgmt_interface':
                    self.properties[new_key] = props
                elif key == 'http_endpoint':
                    if new_key not in self.properties:
                        self.properties[new_key] = []
                    self.properties[new_key].append(props)
                else:
                    if new_key not in self.properties:
                        self.properties[new_key] = []
                    self.properties[new_key].append(props)

        self.log.debug(_("VDU {0} properties: {1}").
                       format(self.name, self.properties))

    def handle_requirements(self, nodes):
        tosca_reqs = self.get_tosca_reqs()
        self.log.debug("VNF {0} requirements: {1}".
                       format(self.name, tosca_reqs))

        try:
            for req in tosca_reqs:
                if 'vdus' in req:
                    target = req['vdus']['target']
                    node = self.get_node_with_name(target, nodes)
                    if node:
                        self._vdus.append(node)
                        node._vnf = self
                        # Add the VDU id to mgmt-intf
                        if 'mgmt-interface' in self.properties:
                            self.properties['mgmt-interface']['vdu-id'] = \
                                            node.id
                            if 'vdu' in self.properties['mgmt-interface']:
                                # Older yang
                                self.properties['mgmt-interface'].pop('vdu')
                    else:
                        err_msg = _("VNF {0}, VDU {1} specified not found"). \
                                  format(self.name, target)
                        self.log.error(err_msg)
                        raise ValidationError(message=err_msg)

        except Exception as e:
            err_msg = _("Exception getting VDUs for VNF {0}: {1}"). \
                      format(self.name, e)
            self.log.error(err_msg)
            raise e

        self.log.debug(_("VNF {0} properties: {1}").
                       format(self.name, self.properties))

    def generate_yang_model_gi(self, nsd, vnfds):
        vnfd_cat = RwVnfdYang.YangData_Vnfd_VnfdCatalog()
        vnfd = vnfd_cat.vnfd.add()
        props = convert_keys_to_python(self.properties)
        try:
            vnfd.from_dict(props)
        except Exception as e:
            err_msg = _("{0} Exception updating vnfd from dict {1}: {2}"). \
                      format(self, props, e)
            self.log.error(err_msg)
            raise e
        vnfds.append(vnfd_cat)

        # Update the VDU properties
        for vdu in self._vdus:
            vdu.generate_yang_submodel_gi(vnfd)

        # Update constituent vnfd in nsd
        try:
            props = convert_keys_to_python(self._const_vnfd)
            nsd.constituent_vnfd.add().from_dict(props)
        except Exception as e:
            err_msg = _("{0} Exception constituent vnfd from dict {1}: {2}"). \
                      format(self, props, e)
            self.log.error(err_msg)
            raise e

        # Update the vnf configuration info in mgmt_interface
        props = convert_keys_to_python(self._vnf_config)
        try:
            vnfd.mgmt_interface.vnf_configuration.from_dict(props)
        except Exception as e:
            err_msg = _("{0} Exception vnfd mgmt intf from dict {1}: {2}"). \
                      format(self, props, e)
            self.log.error(err_msg)
            raise e

    def generate_yang_model(self, nsd, vnfds, use_gi=False):
        """Generate yang model for the node"""
        self.log.debug(_("Generate YANG model for {0}").
                       format(self))

        for key in ToscaNfvVnf.IGNORE_PROPS:
            if key in self.properties:
                self.properties.pop(key)

        if use_gi:
            return self.generate_yang_model_gi(nsd, vnfds)

        vnfd = {}
        vnfd.update(self.properties)
        # Update vnf configuration on mgmt interface
        vnfd['mgmt-interface']['vnf-configuration'] = self._vnf_config

        # Update the VDU properties
        vnfd['vdu'] = []
        for vdu in self._vdus:
            vnfd['vdu'].append(vdu.generate_yang_submodel())

        vnfds.append(vnfd)

        # Update constituent vnfd in nsd
        if 'constituent-vnfd' not in nsd:
            nsd['constituent-vnfd'] = []
        nsd['constituent-vnfd'].append(self._const_vnfd)

    def get_member_vnf_index(self):
        return self._const_vnfd['member-vnf-index']

    def get_supporting_files(self, files, desc_id=None):
        files[self.id] = []
        for vdu in self._vdus:
            if vdu.image:
                files[self.id].append({
                    'type': 'image',
                    'name': vdu.image,
                },)
