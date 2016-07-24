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


import uuid

from collections import OrderedDict

import six

from rift.mano.tosca_translator.common.utils import _

from toscaparser.common.exception import ValidationError
from toscaparser.elements.interfaces import InterfacesDef
from toscaparser.functions import GetInput


SECTIONS = (TYPE, PROPERTIES, MEDADATA, DEPENDS_ON, UPDATE_POLICY,
            DELETION_POLICY) = \
           ('type', 'properties', 'metadata',
            'depends_on', 'update_policy', 'deletion_policy')


class ManoResource(object):
    '''Base class for TOSCA node type translation to RIFT.io MANO type.'''

    def __init__(self,
                 log,
                 nodetemplate,
                 name=None,
                 type_=None,
                 properties=None,
                 metadata=None,
                 artifacts=None,
                 depends_on=None,
                 update_policy=None,
                 deletion_policy=None):
        self.log = log
        self.nodetemplate = nodetemplate
        if name:
            self.name = name
        else:
            self.name = nodetemplate.name
        self.type_ = type_
        self._id = None
        self._version = None
        self.properties = properties or {}
        self.metadata = metadata
        self._artifacts = artifacts

        # The difference between depends_on and depends_on_nodes is
        # that depends_on defines dependency in the context of the
        # HOT template and it is used during the template output.
        # Depends_on_nodes defines the direct dependency between the
        # tosca nodes and is not used during the output of the
        # HOT template but for internal processing only. When a tosca
        # node depends on another node it will be always added to
        # depends_on_nodes but not always to depends_on. For example
        # if the source of dependency is a server, the dependency will
        # be added as properties.get_resource and not depends_on
        if depends_on:
            self.depends_on = depends_on
            self.depends_on_nodes = depends_on
        else:
            self.depends_on = []
            self.depends_on_nodes = []
        self.update_policy = update_policy
        self.deletion_policy = deletion_policy
        self.group_dependencies = {}
        self.operations = {}
        # if hide_resource is set to true, then this resource will not be
        # generated in the output yaml.
        self.hide_resource = False
        log.debug(_('Translating TOSCA node %(name)s of type %(type)s') %
                  {'name': self.name,
                   'type': self.type_})

    # Added the below property menthods to support methods that
    # works on both toscaparser.NodeType and translator.ManoResource
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
        if self._id is None:
            self._id = str(uuid.uuid1())
        return self._id

    @property
    def description(self):
        return _("Translated from TOSCA")

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

    @property
    def artifacts(self):
        return self._artifacts

    @artifacts.setter
    def artifacts(self, value):
        self._artifacts = value

    def __str__(self):
        return "%s(%s)"%(self.name, self.type)

    def map_tosca_name_to_mano(self, name):
        new_name = name.replace("_", "-")
        return new_name

    def map_keys_to_mano(self, d):
        if isinstance(d, dict):
            for key in d.keys():
                d[self.map_tosca_name_to_mano(key)] = \
                                    self.map_keys_to_mano(d.pop(key))
            return d
        elif isinstance(d, list):
            arr = []
            for memb in d:
                arr.append(self.map_keys_to_mano(memb))
            return arr
        else:
            return d

    def validate_properties(self, properties, required=None, optional=None):
        if not isinstance(properties, dict):
            err_msg = _("Properties for {0}({1}) is not right type"). \
                      format(self.name, self.type_)
            self.log.error(err_msg)
            raise ValidationError(message=err_msg)

        if required:
            # Check if the required properties are present
            if not set(required).issubset(properties.keys()):
                for key in required:
                    if key not in properties:
                        err_msg = _("Property {0} is not defined "
                                    "for {1}({2})"). \
                                  format(key, self.name, self.type_)
                        self.log.error(err_msg)
                        raise ValidationError(message=err_msg)

            # Check for unknown properties
            for key in properties.keys():
                if (key not in required or
                    key not in optional):
                    self.log.warn(_("Property {0} not supported for {1}({2}), "
                                    "will be ignored.").
                                  format(key, self.name, self.type_))

    def handle_properties(self):
        pass

    def handle_artifacts(self):
        pass

    def handle_capabilities(self):
        pass

    def handle_requirements(self, nodes):
        pass

    def handle_interfaces(self):
        pass

    def update_image_checksum(self, in_file):
        pass

    def generate_yang_model(self, nsd, vnfds, use_gi=False):
        """Generate yang model for the node"""
        self.log.debug(_("{0}: Not doing anything for YANG model generation").
                       format(self))

    def get_supporting_files(self, files, desc_id=None):
        pass

    def top_of_chain(self):
        dependent = self.group_dependencies.get(self)
        if dependent is None:
            return self
        else:
            return dependent.top_of_chain()

    def get_dict_output(self):
        resource_sections = OrderedDict()
        resource_sections[TYPE] = self.type
        if self.properties:
            resource_sections[PROPERTIES] = self.properties
        if self.metadata:
            resource_sections[MEDADATA] = self.metadata
        if self.depends_on:
            resource_sections[DEPENDS_ON] = []
            for depend in self.depends_on:
                resource_sections[DEPENDS_ON].append(depend.name)
        if self.update_policy:
            resource_sections[UPDATE_POLICY] = self.update_policy
        if self.deletion_policy:
            resource_sections[DELETION_POLICY] = self.deletion_policy

        return {self.name: resource_sections}

    def get_tosca_props(self):
        tosca_props = {}
        for prop in self.nodetemplate.get_properties_objects():
            if isinstance(prop.value, GetInput):
                tosca_props[prop.name] = {'get_param': prop.value.input_name}
            else:
                tosca_props[prop.name] = prop.value
        return tosca_props

    def get_tosca_caps(self):
        tosca_caps = {}
        for cap in self.nodetemplate.get_capabilities_objects():
            properties = cap.get_properties()
            if len(properties):
                tosca_caps[cap.name] = {}
                for name in properties:
                    tosca_caps[cap.name][name] = properties[name].value
        return tosca_caps

    def get_tosca_reqs(self):
        tosca_reqs = []
        for requirement in self.nodetemplate.requirements:
            for endpoint, details in six.iteritems(requirement):
                req = {}
                relation = None
                interfaces = None
                if isinstance(details, dict):
                    target = details.get('node')
                    relation = details.get('relationship')
                else:
                    target = details
                if (target and relation and
                    not isinstance(relation, six.string_types)):
                    interfaces = relation.get('interfaces')
                req[endpoint] = {'target': target}
                if relation:
                    req[endpoint] = {'relation': relation}
                if interfaces:
                    req[endpoint] = {'interfaces': interfaces}
            tosca_reqs.append(req)
        return tosca_reqs

    def get_property(self, args):
        # TODO(Philip): Should figure out how to get this resolved
        # by tosca-parser using GetProperty
        if isinstance(args, list):
            if len(args) == 2 and \
               args[0] == 'SELF':
                if args[1] in self.properties:
                    return self.properties[args[1]]
                else:
                    self.log.error(_("{0}, property {} not defined").
                                   format(self.name, args[1]))
                    return
        self.log.error(_("Get property for {0} of type {1} not supported").
                       format(self.name, args))

    def get_node_with_name(self, name, nodes):
        """Get the node instance with specified name"""
        for node in nodes:
            if node.name == name:
                return node

    def get_nodes_related(self, target, type_, nodes):
        """Get list of nodes related to target node"""
        dep_nodes = []
        for node in nodes:
            if (node.name == target.name or
                type_ != node.type):
                continue
            for rel in node.nodetemplate.related_nodes:
                if rel.name == target.name:
                    dep_nodes.append(node)
                    break
        return dep_nodes

    def get_mano_attribute(self, attribute, args):
        # this is a place holder and should be implemented by the subclass
        # if translation is needed for the particular attribute
        raise Exception(_("No translation in TOSCA type {0} for attribute "
                          "{1}").format(self.nodetemplate.type, attribute))

    @staticmethod
    def _get_all_operations(node):
        operations = {}
        for operation in node.interfaces:
            operations[operation.name] = operation

        node_type = node.type_definition
        if (isinstance(node_type, str) or
            node_type.type == "tosca.policies.Placement"):
            return operations

        while True:
            type_operations = ManoResource._get_interface_operations_from_type(
                node_type, node, 'Standard')
            type_operations.update(operations)
            operations = type_operations

            if node_type.parent_type is not None:
                node_type = node_type.parent_type
            else:
                return operations

    @staticmethod
    def _get_interface_operations_from_type(node_type, node, lifecycle_name):
        operations = {}
        if (isinstance(node_type, str) or
            node_type.type == "tosca.policies.Placement"):
            return operations
        if node_type.interfaces and lifecycle_name in node_type.interfaces:
            for name, elems in node_type.interfaces[lifecycle_name].items():
                # ignore empty operations (only type)
                # ignore global interface inputs,
                # concrete inputs are on the operations themselves
                if name != 'type' and name != 'inputs':
                    operations[name] = InterfacesDef(node_type,
                                                     lifecycle_name,
                                                     node, name, elems)
        return operations

    @staticmethod
    def get_parent_type(node_type):
        if node_type.parent_type is not None:
            return node_type.parent_type
        else:
            return None

    @staticmethod
    def get_base_type(node_type):
        parent_type = ManoResource.get_parent_type(node_type)
        if parent_type is not None:
            if parent_type.type.endswith('.Root'):
                return node_type
            else:
                return ManoResource.get_base_type(parent_type)
        else:
            return node_type
