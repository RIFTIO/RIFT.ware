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

from rift.mano.yang_translator.common.exception import YangClassAttributeError
from rift.mano.yang_translator.common.exception import YangClassImportError
from rift.mano.yang_translator.common.exception import YangModImportError
from rift.mano.yang_translator.common.utils import _
from rift.mano.yang_translator.conf.config import ConfigProvider \
    as translatorConfig
from rift.mano.yang_translator.rwmano.syntax.tosca_resource \
    import ToscaResource


class TranslateDescriptors(object):
    '''Translate YANG NodeTemplates to RIFT.io MANO Resources.'''

    YANG_DESC = (NSD, VNFD) = ('nsd', 'vnfd')

    ###########################
    # Module utility Functions
    # for dynamic class loading
    ###########################

    YANG_TO_TOSCA_TYPE = None

    def _load_classes(log, locations, classes):
        '''Dynamically load all the classes from the given locations.'''

        for cls_path in locations:
            # Use the absolute path of the class path
            abs_path = os.path.dirname(os.path.abspath(__file__))
            abs_path = abs_path.replace('rift/mano/yang_translator/rwmano',
                                        cls_path)
            log.debug(_("Loading classes from %s") % abs_path)

            # Grab all the yang type module files in the given path
            mod_files = [f for f in os.listdir(abs_path) if (
                f.endswith('.py') and
                not f.startswith('__init__') and
                f.startswith('yang_'))]

            # For each module, pick out the target translation class
            for f in mod_files:
                f_name, ext = f.rsplit('.', 1)
                mod_name = cls_path + '/' + f_name
                mod_name = mod_name.replace('/', '.')
                try:
                    mod = importlib.import_module(mod_name)
                    target_name = getattr(mod, 'TARGET_CLASS_NAME')
                    clazz = getattr(mod, target_name)
                    classes.append(clazz)
                except ImportError:
                    raise YangModImportError(mod_name=mod_name)
                except AttributeError:
                    if target_name:
                        raise YangClassImportError(name=target_name,
                                                    mod_name=mod_name)
                    else:
                        # TARGET_CLASS_NAME is not defined in module.
                        # Re-raise the exception
                        raise

    def _generate_type_map(log):
        '''Generate YANG translation types map.

        Load user defined classes from location path specified in conf file.
        Base classes are located within the yang directory.
        '''

        # Base types directory
        BASE_PATH = 'rift/mano/yang_translator/rwmano/yang'

        # Custom types directory defined in conf file
        custom_path = translatorConfig.get_value('DEFAULT',
                                                 'custom_types_location')

        # First need to load the parent module, for example 'contrib.mano',
        # for all of the dynamically loaded classes.
        classes = []
        TranslateDescriptors._load_classes(log,
                                             (BASE_PATH, custom_path),
                                             classes)
        try:
            types_map = {clazz.yangtype: clazz for clazz in classes}
            log.debug(_("Type maps loaded: {}").format(types_map.keys()))
        except AttributeError as e:
            raise YangClassAttributeError(message=e.message)

        return types_map

    def __init__(self, log, yangs, tosca_template):
        self.log = log
        self.yangs = yangs
        self.tosca_template = tosca_template
        # list of all TOSCA resources generated
        self.tosca_resources = []
        self.metadata = {}
        log.debug(_('Mapping between YANG nodetemplate and TOSCA resource.'))

    def translate(self):
        if TranslateDescriptors.YANG_TO_TOSCA_TYPE is None:
            TranslateDescriptors.YANG_TO_TOSCA_TYPE = \
                TranslateDescriptors._generate_type_map(self.log)
        return self._translate_yang()

    def translate_metadata(self):
        """Translate and store the metadata in instance"""
        FIELDS_MAP = {
            'ID': 'name',
            'vendor': 'vendor',
            'version': 'version',
        }
        metadata = {}
        # Initialize to default values
        metadata['name'] = 'yang_to_tosca'
        metadata['vendor'] = 'RIFT.io'
        metadata['version'] = '1.0'
        if 'nsd' in self.yangs:
            yang_meta = self.yang['nsd'][0]
        elif 'vnfd' in self.yangs:
            yang_meta = self.yang['vnfd'][0]
            for key in FIELDS_MAP:
                if key in yang_meta.keys():
                    metadata[key] = str(yang_meta[FIELDS_MAP[key]])
        self.log.debug(_("Metadata {0}").format(metadata))
        self.metadata = metadata

    def _translate_yang(self):
        self.log.debug(_('Translating the descriptors.'))
        for nsd in self.yangs[self.NSD]:
            self.log.debug(_("Translate descriptor of type nsd: {}").
                           format(nsd))
            tosca_node = TranslateDescriptors. \
                         YANG_TO_TOSCA_TYPE[self.NSD](
                             self.log,
                             nsd.pop(ToscaResource.NAME),
                             self.NSD,
                             nsd)
            self.tosca_resources.append(tosca_node)

        for vnfd in self.yangs[self.VNFD]:
            self.log.debug(_("Translate descriptor of type vnfd: {}").
                           format(vnfd))
            tosca_node = TranslateDescriptors. \
                         YANG_TO_TOSCA_TYPE[self.VNFD](
                             self.log,
                             vnfd.pop(ToscaResource.NAME),
                             self.VNFD,
                             vnfd)
            self.tosca_resources.append(tosca_node)

        # First translate VNFDs
        for node in self.tosca_resources:
            if node.type == self.VNFD:
                self.log.debug(_("Handle yang for {0} of type {1}").
                               format(node.name, node.type_))
                node.handle_yang()

        # Now translate NSDs
        for node in self.tosca_resources:
            if node.type == self.NSD:
                self.log.debug(_("Handle yang for {0} of type {1}").
                               format(node.name, node.type_))
                node.handle_yang(self.tosca_resources)

        return self.tosca_resources

    def find_tosca_resource(self, name):
        for resource in self.tosca_resources:
            if resource.name == name:
                return resource

    def _find_yang_node(self, yang_name):
        for node in self.nodetemplates:
            if node.name == yang_name:
                return node
