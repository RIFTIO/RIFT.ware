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


from rift.mano.yang_translator.common.utils import _


class ToscaResource(object):
    '''Base class for YANG node type translation to RIFT.io TOSCA type.'''

    # Used when creating the resource, so keeping separate
    # from REQUIRED_FIELDS below
    NAME = 'name'

    REQUIRED_FIELDS = (DESC, VERSION, VENDOR, ID) = \
                      ('description', 'version', 'vendor', 'id')

    COMMON_FIELDS = (PATH, PORT, HOST, XPATH, TYPE, COUNT, FILE) = \
                    ('path', 'port', 'host', 'xpath', 'type', 'count', 'file')

    IGNORE_FIELDS = ['short_name']

    FIELD_TYPES = (STRING, MAP, INTEGER, BOOL) = \
                  ('string', 'map', 'integer', 'boolean',)

    YANG_KEYS = (VLD, NSD, VNFD, VDU, DASHBOARD_PARAMS,
                 CONFIG_ATTR, CONFIG_TMPL,
                 CONFIG_TYPE, CONFIG_DETAILS, EXT_INTF,
                 VIRT_INTF, POLL_INTVL_SECS,
                 MEM_VNF_INDEX_REF, VNFD_ID_REF,
                 MEM_VNF_INDEX, VNF_CONFIG, TYPE_Y,
                 USER_DEF_SCRIPT, SEQ, PARAM,
                 VALUE, START_BY_DFLT,) = \
                ('vld', 'nsd', 'vnfd', 'vdu', 'dashboard_params',
                 'config_attributes', 'config_template',
                 'config_type', 'config_details', 'external_interface',
                 'virtual_interface', 'polling_interval_secs',
                 'member_vnf_index_ref', 'vnfd_id_ref',
                 'member_vnf_index', 'vnf_configuration', 'type_yang',
                 'user_defined_script', 'seq', 'parameter',
                 'value', 'start_by_default',)

    TOSCA_FIELDS = (DERIVED_FROM, PROPERTIES, DEFAULT, REQUIRED,
                    NO, CONSTRAINTS, REALTIONSHIPS,
                    REQUIREMENTS, UNBOUND, NODE,
                    OCCURENCES, PRIMITIVES, MEMBERS,
                    POLL_INTVL, DEFAULT, TRUE, FALSE,) = \
                   ('derived_from', 'properties', 'default', 'required',
                    'no', 'constraints', 'relationships',
                    'requirements', 'UNBOUND', 'node',
                    'occurences', 'primitives', 'members',
                    'polling_interval', 'default', 'true', 'false')

    TOSCA_SEC = (DATA_TYPES, CAPABILITY_TYPES, NODE_TYPES,
                 GROUP_TYPES, POLICY_TYPES, REQUIREMENTS,
                 ARTIFACTS, PROPERTIES, INTERFACES,
                 CAPABILITIES, RELATIONSHIP,
                 ARTIFACT_TYPES) = \
                ('data_types', 'capability_types', 'node_types',
                 'group_types', 'policy_types', 'requirements',
                 'artifacts', 'properties', 'interfaces',
                 'capabilities', 'relationship',
                 'artifact_types')

    TOSCA_TMPL = (INPUTS, NODE_TMPL, GROUPS, POLICIES,
                  METADATA, TOPOLOGY_TMPL, OUTPUTS) = \
                 ('inputs', 'node_templates', 'groups', 'policies',
                  'metadata', 'topology_template', 'outputs')

    TOSCA_DERIVED = (
        T_VNF_CONFIG,
        T_HTTP_EP,
        T_MGMT_INTF,
        T_MON_PARAM,
        T_VNF1,
        T_VDU1,
        T_CP1,
        T_VL1,
        T_CONF_PRIM,
        T_SCALE_GRP,
        T_ARTF_QCOW2,
        T_INITIAL_CFG,
    ) = \
        ('tosca.datatypes.network.riftio.vnf_configuration',
         'tosca.capabilities.riftio.http_endpoint_type',
         'tosca.capabilities.riftio.mgmt_interface_type',
         'tosca.capabilities.riftio.monitoring_param',
         'tosca.nodes.riftio.VNF1',
         'tosca.nodes.riftio.VDU1',
         'tosca.nodes.riftio.CP1',
         'tosca.nodes.riftio.VL1',
         'tosca.groups.riftio.ConfigPrimitives',
         'tosca.policies.riftio.ScalingGroup',
         'tosca.artifacts.Deployment.Image.riftio.QCOW2',
         'tosca.policies.riftio.InitialConfigPrimitive'
        )

    SUPPORT_FILES = ( SRC, DEST, EXISTING) = \
                    ('source', 'destination', 'existing')

    SUPPORT_DIRS = (IMAGE_DIR, SCRIPT_DIR,) = \
                   ('images', 'scripts',)

    def __init__(self,
                 log,
                 name,
                 type_,
                 yang):
        self.log = log
        self.name = name
        self.type_ = type_
        self.yang = yang
        self.id_ = None
        log.debug(_('Translating YANG node %(name)s of type %(type)s') %
                  {'name': self.name,
                   'type': self.type_})

    # Added the below property menthods to support methods that
    # works on both toscaparser.NodeType and translator.ToscaResource
    @property
    def type(self):
        return self.type_

    @type.setter
    def type(self, value):
        self.type_ = value

    def get_type(self):
        return self.type_

    @property
    def id(self):
        return self.id_

    @id.setter
    def id(self, value):
        self.id_ = value

    @property
    def description(self):
        return _("Translated from YANG")

    @property
    def vendor(self):
        if self._vendor is None:
            if self.metadata and 'vendor' in self.metadata:
                self._vendor = self.metadata['vendor']
            else:
                self._vendor = "RIFT.io"
        return self._vendor

    @property
    def version(self):
        if self._version is None:
            if self.metadata and 'version' in self.metadata:
                self._version = str(self.metadata['version'])
            else:
                self._version = '1.0'
        return self._version

    def __str__(self):
        return "%s(%s)" % (self.name, self.type)

    def map_yang_name_to_tosca(self, name):
        new_name = name.replace("_", "-")
        return new_name

    def map_keys_to_tosca(self, d):
        if isinstance(d, dict):
            for key in d.keys():
                d[self.map_yang_name_to_tosca(key)] = \
                                    self.map_keys_to_tosca(d.pop(key))
            return d
        elif isinstance(d, list):
            arr = []
            for memb in d:
                arr.append(self.map_keys_to_tosca(memb))
            return arr
        else:
            return d

    def handle_yang(self):
        self.log.debug(_("Need to implement handle_yang for {0}").
                       format(self))

    def remove_ignored_fields(self, d):
        '''Remove keys in dict not used'''
        for key in self.IGNORE_FIELDS:
            if key in d:
                d.pop(key)

    def generate_tosca_type(self, tosca):
        self.log.debug(_("Need to implement generate_tosca_type for {0}").
                       format(self))

    def generate_tosca_model(self, tosca):
        self.log.debug(_("Need to implement generate_tosca_model for {0}").
                       format(self))

    def get_supporting_files(self):
        """Get list of other required files for each resource"""
        pass

    def get_matching_item(self, name, items, key=None):
        if key is None:
            key = 'name'
        for entry in items:
            if entry[key] == name:
                return entry
        return None
