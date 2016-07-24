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

import yaml

from rift.mano.tosca_translator.common.utils import _

from rift.mano.tosca_translator.common.utils import dict_convert_values_to_str

try:
    import gi
    gi.require_version('RwYang', '1.0')
    gi.require_version('RwNsdYang', '1.0')
    gi.require_version('NsdYang', '1.0')

    from gi.repository import NsdYang
    from gi.repository import RwNsdYang
    from gi.repository import RwYang
except ImportError:
    pass
except ValueError as e:
    pass


class ManoTemplate(object):
    '''Container for full RIFT.io MANO template.'''

    YANG_NS = (NSD, VNFD) = ('nsd', 'vnfd')
    OUTPUT_FIELDS = (NAME, ID, YANG, FILES) = ('name', 'id', 'yang', 'files')

    def __init__(self, log):
        self.log = log
        self.resources = []
        self.outputs = []
        self.parameters = []
        self.description = "Translated from TOSCA"
        self.metadata = None
        self.policies = []
        self.groups = []

    def output_to_yang(self, use_gi=False, indent=4):
        self.log.debug(_('Converting translated output to yang model.'))

        nsd_cat = None
        nsd_id = str(uuid.uuid1())
        vnfds = []

        if use_gi:
            try:
                nsd_cat = RwNsdYang.YangData_Nsd_NsdCatalog()
                nsd = nsd_cat.nsd.add()
                nsd.id = nsd_id
                nsd.name = self.metadata['name']
                nsd.description = self.description
                nsd.vendor = self.metadata['vendor']
                nsd.short_name = self.metadata['name']
                nsd.version = self.metadata['version']
            except Exception as e:
                self.log.warning(_("Unable to use YANG GI to generate "
                                   "descriptors, falling back to alternate "
                                   "method: {}").format(e))
                self.log.exception(e)
                use_gi = False

        if not use_gi:
            nsd = {
                'id': nsd_id,
                'name': self.metadata['name'],
                'description': self.description,
                'vendor': self.metadata['vendor'],
                'short-name': self.metadata['name'],
                'version': self.metadata['version'],
            }

        for resource in self.resources:
            # Do the vlds first
            if resource.type == 'vld':
                resource.generate_yang_model(nsd, vnfds, use_gi=use_gi)

        for resource in self.resources:
            # Do the vnfds next
            if resource.type == 'vnfd':
                resource.generate_yang_model(nsd, vnfds, use_gi=use_gi)

        for resource in self.resources:
            # Do the other nodes
            if resource.type != 'vnfd' and resource.type != 'vld':
                resource.generate_yang_model(nsd, vnfds, use_gi=use_gi)

        for group in self.groups:
            group.generate_yang_model(nsd, vnfds, use_gi=use_gi)

        for policy in self.policies:
            policy.generate_yang_model(nsd, vnfds, use_gi=use_gi)

        # Add input params to nsd
        if use_gi:
            for param in self.parameters:
                nsd.input_parameter_xpath.append(
                 NsdYang.YangData_Nsd_NsdCatalog_Nsd_InputParameterXpath(
                    xpath=param.get_xpath(),
                    )
                )
        else:
            nsd['input-parameter-xpath'] = []
            for param in self.parameters:
                nsd['input-parameter-xpath'].append(
                    {'xpath': param.get_xpath()})

        # Get list of supporting files referred in template
        # Returned format is {desc_id: [{type: type, name: filename}]}
        # TODO (pjoseph): Currently only images and scripts are retrieved.
        # Need to add support to get script names, charms, etc.
        other_files = {}
        for resource in self.resources:
            resource.get_supporting_files(other_files)

        for policy in self.policies:
            policy.get_supporting_files(other_files, desc_id=nsd_id)

        self.log.debug(_("List of other files: {}".format(other_files)))

        # Do the final processing and convert each descriptor to yaml string
        tpl = {}

        # Add the NSD
        if use_gi:
            nsd_pf = self.get_yaml(['nsd', 'rw-nsd'], nsd_cat)
            nsd_id = nsd_cat.nsd[0].id
            nsd_name = nsd_cat.nsd[0].name
        else:
            nsd_id = nsd['id']
            nsd_name = nsd['name']

            # In case of non gi proecssing,
            # - convert all values to string
            # - enclose in a catalog dict
            # - prefix all keys with nsd or vnfd
            # - Convert to YAML string
            nsd_pf = yaml.dump(
                self.prefix_dict(
                    self.add_cat(dict_convert_values_to_str(nsd),
                                 self.NSD),
                    self.NSD),
                default_flow_style=False)

        nsd_out = {
            self.NAME: nsd_name,
            self.ID: nsd_id,
            self.YANG: nsd_pf,
        }

        if nsd_id in other_files:
            nsd_out[self.FILES] = other_files[nsd_id]

        tpl[self.NSD] = [nsd_out]

        # Add the VNFDs
        tpl[self.VNFD] = []

        for vnfd in vnfds:
            if use_gi:
                vnfd_pf = self.get_yaml(['vnfd', 'rw-vnfd'], vnfd)
                vnfd_id = vnfd.vnfd[0].id
                vnfd_name = vnfd.vnfd[0].name

            else:
                vnfd_id = vnfd['id']
                vnfd_name = vnfd['name']

                # In case of non gi proecssing,
                # - convert all values to string
                # - enclose in a catalog dict
                # - prefix all keys with nsd or vnfd
                # - Convert to YAML string
                vnfd_pf = yaml.dump(
                    self.prefix_dict(
                        self.add_cat(dict_convert_values_to_str(vnfd),
                                     self.VNFD),
                        self.VNFD),
                    default_flow_style=False)

            vnfd_out = {
                self.NAME: vnfd_name,
                self.ID: vnfd_id,
                self.YANG: vnfd_pf,
            }

            if vnfd_id in other_files:
                vnfd_out[self.FILES] = other_files[vnfd_id]

            tpl[self.VNFD].append(vnfd_out)

        self.log.debug(_("NSD: {0}").format(tpl[self.NSD]))
        self.log.debug(_("VNFDs:"))
        for vnfd in tpl[self.VNFD]:
            self.log.debug(_("{0}").format(vnfd))

        return tpl

    def _get_field(self, d, pf, field='name'):
        '''Get the name given for the descriptor'''
        # Search within the desc for a key pf:name
        key = pf+':'+field
        if isinstance(d, dict):
            # If it is a dict, search for pf:name
            if key in d:
                return d[key]
            else:
                for k, v in d.items():
                    result = self._get_field(v, pf, field)
                    if result:
                        return result
        elif isinstance(d, list):
            for memb in d:
                result = self._get_field(memb, pf, field)
                if result:
                        return result

    def prefix_dict(self, d, pf):
        '''Prefix all keys of a dict with a specific prefix:'''
        if isinstance(d, dict):
            dic = {}
            for key in d.keys():
                # Only prefix keys without any prefix
                # so later we can do custom prefixing
                # which will not get overwritten here
                if ':' not in key:
                    dic[pf+':'+key] = self.prefix_dict(d[key], pf)
                else:
                    dic[key] = self.prefix_dict(d[key], pf)
            return dic
        elif isinstance(d, list):
            arr = []
            for memb in d:
                arr.append(self.prefix_dict(memb, pf))
            return arr
        else:
            return d

    def add_cat(self, desc, pf):
        return {pf+'-catalog': {pf: [desc]}}

    def get_yaml(self, module_list, desc):
        model = RwYang.Model.create_libncx()
        for module in module_list:
            model.load_module(module)
        return desc.to_yaml(model)
