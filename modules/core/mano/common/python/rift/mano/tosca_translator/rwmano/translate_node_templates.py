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

# Copyright 2016 RIFT.io Inc


import importlib
import os

from rift.mano.tosca_translator.common.utils import _
from rift.mano.tosca_translator.common.exception import ToscaClassAttributeError
from rift.mano.tosca_translator.common.exception import ToscaClassImportError
from rift.mano.tosca_translator.common.exception import ToscaModImportError
from rift.mano.tosca_translator.conf.config import ConfigProvider as translatorConfig
from rift.mano.tosca_translator.rwmano.syntax.mano_resource import ManoResource


class TranslateNodeTemplates(object):
    '''Translate TOSCA NodeTemplates to RIFT.io MANO Resources.'''

    ##################
    # Module constants
    ##################

    TOSCA_TO_MANO_REQUIRES = {'container': 'server',
                              'host': 'server',
                              'dependency': 'depends_on',
                              'connects': 'depends_on'}

    TOSCA_TO_MANO_PROPERTIES = {'properties': 'input'}

    TOSCA_TO_MANO_TYPE = None

    ###########################
    # Module utility Functions
    # for dynamic class loading
    ###########################

    def _load_classes(log, locations, classes):
        '''Dynamically load all the classes from the given locations.'''

        for cls_path in locations:
            # Use the absolute path of the class path
            abs_path = os.path.dirname(os.path.abspath(__file__))
            abs_path = abs_path.replace('rift/mano/tosca_translator/rwmano', cls_path)
            log.debug(_("Loading classes from %s") % abs_path)

            # Grab all the tosca type module files in the given path
            mod_files = [f for f in os.listdir(abs_path) if (
                f.endswith('.py') and
                not f.startswith('__init__') and
                f.startswith('tosca_'))]

            # For each module, pick out the target translation class
            for f in mod_files:
                # NOTE: For some reason the existing code does not use
                # the map to instantiate
                # ToscaBlockStorageAttachment. Don't add it to the map
                # here until the dependent code is fixed to use the
                # map.
                if f == 'tosca_block_storage_attachment.py':
                    continue

                # mod_name = cls_path + '/' + f.rstrip('.py')
                # Above have an issue if the mod name ends with p or y
                f_name, ext = f.rsplit('.', 1)
                mod_name = cls_path + '/' + f_name
                mod_name = mod_name.replace('/', '.')
                try:
                    mod = importlib.import_module(mod_name)
                    target_name = getattr(mod, 'TARGET_CLASS_NAME')
                    clazz = getattr(mod, target_name)
                    classes.append(clazz)
                except ImportError:
                    raise ToscaModImportError(mod_name=mod_name)
                except AttributeError:
                    if target_name:
                        raise ToscaClassImportError(name=target_name,
                                                    mod_name=mod_name)
                    else:
                        # TARGET_CLASS_NAME is not defined in module.
                        # Re-raise the exception
                        raise

    def _generate_type_map(log):
        '''Generate TOSCA translation types map.

        Load user defined classes from location path specified in conf file.
        Base classes are located within the tosca directory.
        '''

        # Base types directory
        BASE_PATH = 'rift/mano/tosca_translator/rwmano/tosca'

        # Custom types directory defined in conf file
        custom_path = translatorConfig.get_value('DEFAULT',
                                                 'custom_types_location')

        # First need to load the parent module, for example 'contrib.mano',
        # for all of the dynamically loaded classes.
        classes = []
        TranslateNodeTemplates._load_classes(log,
                                             (BASE_PATH, custom_path),
                                             classes)
        try:
            types_map = {clazz.toscatype: clazz for clazz in classes}
            log.debug(_("Type maps loaded: {}").format(types_map.keys()))
        except AttributeError as e:
            raise ToscaClassAttributeError(message=e.message)

        return types_map

    def __init__(self, log, tosca, mano_template):
        self.log = log
        self.tosca = tosca
        self.nodetemplates = self.tosca.nodetemplates
        self.mano_template = mano_template
        # list of all MANO resources generated
        self.mano_resources = []
        self.mano_policies = []
        self.mano_groups = []
        # mapping between TOSCA nodetemplate and MANO resource
        log.debug(_('Mapping between TOSCA nodetemplate and MANO resource.'))
        self.mano_lookup = {}
        self.policies = self.tosca.topology_template.policies
        self.groups = self.tosca.topology_template.groups
        self.metadata = {}

    def translate(self):
        if TranslateNodeTemplates.TOSCA_TO_MANO_TYPE is None:
            TranslateNodeTemplates.TOSCA_TO_MANO_TYPE = \
                TranslateNodeTemplates._generate_type_map(self.log)
        # Translate metadata
        self.translate_metadata()
        return self._translate_nodetemplates()

    def translate_metadata(self):
        """Translate and store the metadata in instance"""
        FIELDS_MAP = {
            'ID': 'name',
            'vendor': 'vendor',
            'version': 'version',
        }
        metadata = {}
        # Initialize to default values
        metadata['name'] = 'tosca_to_mano'
        metadata['vendor'] = 'RIFT.io'
        metadata['version'] = '1.0'
        if 'metadata' in self.tosca.tpl:
            tosca_meta = self.tosca.tpl['metadata']
            for key in FIELDS_MAP:
                if key in tosca_meta.keys():
                    metadata[FIELDS_MAP[key]] = str(tosca_meta[key])
        self.log.debug(_("Metadata {0}").format(metadata))
        self.metadata = metadata

    def _recursive_handle_properties(self, resource):
        '''Recursively handle the properties of the depends_on_nodes nodes.'''
        # Use of hashtable (dict) here should be faster?
        if resource in self.processed_resources:
            return
        self.processed_resources.append(resource)
        for depend_on in resource.depends_on_nodes:
            self._recursive_handle_properties(depend_on)

        if resource.type == "OS::Nova::ServerGroup":
            resource.handle_properties(self.mano_resources)
        else:
            resource.handle_properties()

    def _get_policy_type(self, policy):
        if isinstance(policy, dict):
            for key, details in policy.items():
                if 'type' in details:
                    return details['type']

    def _translate_nodetemplates(self):

        self.log.debug(_('Translating the node templates.'))
        # Copy the TOSCA graph: nodetemplate
        tpl = self.tosca.tpl['topology_template']['node_templates']
        for node in self.nodetemplates:
            base_type = ManoResource.get_base_type(node.type_definition)
            self.log.debug(_("Translate node %(name)s of type %(type)s with "
                             "base %(base)s") %
                           {'name': node.name,
                            'type': node.type,
                            'base': base_type.type})
            mano_node = TranslateNodeTemplates. \
                        TOSCA_TO_MANO_TYPE[base_type.type](
                            self.log,
                            node,
                            metadata=self.metadata)
            # Currently tosca-parser does not add the artifacts
            # to the node
            if mano_node.name in tpl:
                tpl_node = tpl[mano_node.name]
                self.log.debug("Check artifacts for {}".format(tpl_node))
                if 'artifacts' in tpl_node:
                    mano_node.artifacts = tpl_node['artifacts']
            self.mano_resources.append(mano_node)
            self.mano_lookup[node] = mano_node

        # The parser currently do not generate the objects for groups
        if 'groups' in self.tosca.tpl['topology_template']:
            tpl = self.tosca.tpl['topology_template']['groups']
            self.log.debug("Groups: {}".format(tpl))
            for group, details in tpl.items():
                self.log.debug(_("Translate group {}: {}").
                               format(group, details))
                group_type = details['type']
                if group_type:
                    group_node = TranslateNodeTemplates. \
                                 TOSCA_TO_MANO_TYPE[group_type](
                                     self.log,
                                     group,
                                     details,
                                     metadata=self.metadata)
                    self.mano_groups.append(group_node)

        # The parser currently do not generate the objects for policies
        if 'policies' in self.tosca.tpl['topology_template']:
            tpl = self.tosca.tpl['topology_template']['policies']
            # for policy in self.policies:
            for policy in tpl:
                self.log.debug(_("Translate policy {}").
                               format(policy))
                policy_type = self._get_policy_type(policy)
                if policy_type:
                    policy_node = TranslateNodeTemplates. \
                                  TOSCA_TO_MANO_TYPE[policy_type](
                                      self.log,
                                      policy,
                                      metadata=self.metadata)
                    self.mano_policies.append(policy_node)

        for node in self.mano_resources:
            self.log.debug(_("Handle properties for {0} of type {1}").
                           format(node.name, node.type_))
            node.handle_properties()

            self.log.debug(_("Handle capabilites for {0} of type {1}").
                           format(node.name, node.type_))
            node.handle_capabilities()

            self.log.debug(_("Handle aritfacts for {0} of type {1}").
                           format(node.name, node.type_))
            node.handle_artifacts()

            self.log.debug(_("Handle interfaces for {0} of type {1}").
                           format(node.name, node.type_))
            node.handle_interfaces()

            self.log.debug(_("Update image checksum for {0} of type {1}").
                           format(node.name, node.type_))
            node.update_image_checksum(self.tosca.path)

        for node in self.mano_resources:
            # Handle vnf and vdu dependencies first
            if node.type == "vnfd":
                try:
                    self.log.debug(_("Handle requirements for {0} of "
                                     "type {1}").
                                   format(node.name, node.type_))
                    node.handle_requirements(self.mano_resources)
                except Exception as e:
                    self.log.error(_("Exception for {0} in requirements {1}").
                                   format(node.name, node.type_))
                    self.log.exception(e)

        for node in self.mano_resources:
            # Now handle other dependencies
            if node.type != "vnfd":
                try:
                    self.log.debug(_("Handle requirements for {0} of type {1}").
                                   format(node.name, node.type_))
                    node.handle_requirements(self.mano_resources)
                except Exception as e:
                    self.log.error(_("Exception for {0} in requirements {1}").
                                   format(node.name, node.type_))
                    self.log.exception(e)

        return self.mano_resources

    def translate_groups(self):
        for group in self.mano_groups:
            group.handle_properties(self.mano_resources)
        return self.mano_groups

    def translate_policies(self):
        for policy in self.mano_policies:
            policy.handle_properties(self.mano_resources, self.mano_groups)
        return self.mano_policies

    def find_mano_resource(self, name):
        for resource in self.mano_resources:
            if resource.name == name:
                return resource

    def _find_tosca_node(self, tosca_name):
        for node in self.nodetemplates:
            if node.name == tosca_name:
                return node

    def _find_mano_resource_for_tosca(self, tosca_name,
                                      current_mano_resource=None):
        if tosca_name == 'SELF':
            return current_mano_resource
        if tosca_name == 'HOST' and current_mano_resource is not None:
            for req in current_mano_resource.nodetemplate.requirements:
                if 'host' in req:
                    return self._find_mano_resource_for_tosca(req['host'])

        for node in self.nodetemplates:
            if node.name == tosca_name:
                return self.mano_lookup[node]

        return None
