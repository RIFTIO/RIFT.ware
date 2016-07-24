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
from rift.mano.tosca_translator.rwmano.syntax.mano_template import ManoTemplate
from rift.mano.tosca_translator.rwmano.translate_inputs import TranslateInputs
from rift.mano.tosca_translator.rwmano.translate_node_templates \
    import TranslateNodeTemplates
from rift.mano.tosca_translator.rwmano.translate_outputs \
    import TranslateOutputs


class TOSCATranslator(object):
    '''Invokes translation methods.'''

    def __init__(self, log, tosca, parsed_params, deploy=None, use_gi=False):
        super(TOSCATranslator, self).__init__()
        self.log = log
        self.tosca = tosca
        self.mano_template = ManoTemplate(log)
        self.parsed_params = parsed_params
        self.deploy = deploy
        self.use_gi = use_gi
        self.node_translator = None
        log.info(_('Initialized parmaters for translation.'))

    def translate(self):
        self._resolve_input()
        self.mano_template.description = self.tosca.description
        self.mano_template.parameters = self._translate_inputs()
        self.node_translator = TranslateNodeTemplates(self.log,
                                                      self.tosca,
                                                      self.mano_template)
        self.mano_template.resources = self.node_translator.translate()
        # TODO(Philip): Currently doing groups and policies seperately
        # due to limitations with parser
        self.mano_template.groups = self.node_translator.translate_groups()
        self.mano_template.policies = self.node_translator.translate_policies()
        self.mano_template.metadata = self.node_translator.metadata
        # Currently we do not use outputs, so not processing them
        # self.mano_template.outputs = self._translate_outputs()
        return self.mano_template.output_to_yang(use_gi=self.use_gi)

    def _translate_inputs(self):
        translator = TranslateInputs(self.log,
                                     self.tosca.inputs,
                                     self.parsed_params,
                                     self.deploy)
        return translator.translate()

    def _translate_outputs(self):
        translator = TranslateOutputs(self.log,
                                      self.tosca.outputs,
                                      self.node_translator)
        return translator.translate()

    # check all properties for all node and ensure they are resolved
    # to actual value
    def _resolve_input(self):
        for n in self.tosca.nodetemplates:
            for node_prop in n.get_properties_objects():
                if isinstance(node_prop.value, dict):
                    if 'get_input' in node_prop.value:
                        try:
                            self.parsed_params[node_prop.value['get_input']]
                        except Exception:
                            msg = (_('Must specify all input values in '
                                     'TOSCA template, missing %s.') %
                                   node_prop.value['get_input'])
                        self.log.error(msg)
                        raise ValueError(msg)
