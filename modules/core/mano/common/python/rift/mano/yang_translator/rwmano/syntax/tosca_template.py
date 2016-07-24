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

from collections import OrderedDict

import textwrap

from rift.mano.yang_translator.common.utils import _
from rift.mano.yang_translator.rwmano.syntax.tosca_resource \
    import ToscaResource

import yaml


class ToscaTemplate(object):
    '''Container for full RIFT.io TOSCA template.'''

    KEYS = (TOSCA, FILES) = ('tosca', 'files')

    def __init__(self, log):
        self.log = log
        self.resources = []

    def output_to_tosca(self):
        self.log.debug(_('Converting translated output to tosca template.'))

        templates = {}

        for resource in self.resources:
            # Each NSD should generate separate templates
            if resource.type == 'nsd':
                tmpl = resource.generate_tosca_type()
                tmpl = resource.generate_tosca_template(tmpl)
                self.log.debug(_("TOSCA template generated for {0}:\n{1}").
                               format(resource.name, tmpl))
                templates[resource.name] = {self.TOSCA: self.output_to_yaml(tmpl)}
                files = resource.get_supporting_files()
                if len(files):
                    templates[resource.name][self.FILES] = files

        return templates

    def represent_ordereddict(self, dumper, data):
        nodes = []
        for key, value in data.items():
            node_key = dumper.represent_data(key)
            node_value = dumper.represent_data(value)
            nodes.append((node_key, node_value))
        return yaml.nodes.MappingNode(u'tag:yaml.org,2002:map', nodes)

    def ordered_node(self, node):
        order = [ToscaResource.TYPE, ToscaResource.DERIVED_FROM,
                 ToscaResource.DESC, ToscaResource.MEMBERS,
                 ToscaResource.PROPERTIES, ToscaResource.CAPABILITIES,
                 ToscaResource.REQUIREMENTS,ToscaResource.ARTIFACTS,
                 ToscaResource.INTERFACES]
        new_node = OrderedDict()
        for ent in order:
            if ent in node:
                new_node.update({ent: node.pop(ent)})

        # Check if we missed any entry
        if len(node):
            self.log.warn(_("Did not sort these entries: {0}").
                          format(node))
            new_node.update(node)

        return new_node

    def ordered_nodes(self, nodes):
        new_nodes = OrderedDict()
        if isinstance(nodes, dict):
            for name, node in nodes.items():
                new_nodes.update({name: self.ordered_node(node)})
            return new_nodes
        else:
            return nodes

    def output_to_yaml(self, tosca):
        self.log.debug(_('Converting translated output to yaml format.'))
        dict_output = OrderedDict()

        dict_output.update({'tosca_definitions_version':
                            tosca['tosca_definitions_version']})
        # Description
        desc_str = ""
        if ToscaResource.DESC in tosca:
            # Wrap the text to a new line if the line exceeds 80 characters.
            wrapped_txt = "\n  ". \
                          join(textwrap.wrap(tosca[ToscaResource.DESC], 80))
            desc_str = ToscaResource.DESC + ": >\n  " + \
                       wrapped_txt + "\n\n"
        dict_output.update({ToscaResource.DESC: tosca[ToscaResource.DESC]})

        if ToscaResource.METADATA in tosca:
            dict_output.update({ToscaResource.METADATA:
                               tosca[ToscaResource.METADATA]})

        # Add all types
        types_list = [ToscaResource.DATA_TYPES, ToscaResource.CAPABILITY_TYPES,
                      ToscaResource.NODE_TYPES,
                      ToscaResource.GROUP_TYPES, ToscaResource.POLICY_TYPES]
        for typ in types_list:
            if typ in tosca:
                dict_output.update({typ: self.ordered_nodes(tosca[typ])})

        # Add topology template
        topo_list = [ToscaResource.INPUTS, ToscaResource.NODE_TMPL,
                     ToscaResource.GROUPS, ToscaResource.POLICIES,
                     ToscaResource.OUTPUTS]
        if ToscaResource.TOPOLOGY_TMPL in tosca:
            tmpl = OrderedDict()
            for typ in tosca[ToscaResource.TOPOLOGY_TMPL]:
                tmpl.update({typ:
                             self.ordered_nodes(
                                 tosca[ToscaResource.TOPOLOGY_TMPL][typ])})
            dict_output.update({ToscaResource.TOPOLOGY_TMPL: tmpl})

        yaml.add_representer(OrderedDict, self.represent_ordereddict)
        yaml_string = yaml.dump(dict_output, default_flow_style=False)
        # get rid of the '' from yaml.dump around numbers
        yaml_string = yaml_string.replace('\'', '')
        self.log.debug(_("YAML output:\n{0}").format(yaml_string))
        return yaml_string
