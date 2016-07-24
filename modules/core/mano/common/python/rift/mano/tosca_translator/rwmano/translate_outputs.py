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
from rift.mano.tosca_translator.rwmano.syntax.mano_output import ManoOutput


class TranslateOutputs(object):
    '''Translate TOSCA Outputs to Heat Outputs.'''

    def __init__(self, log, outputs, node_translator):
        log.debug(_('Translating TOSCA outputs to MANO outputs.'))
        self.log = log
        self.outputs = outputs
        self.nodes = node_translator

    def translate(self):
        return self._translate_outputs()

    def _translate_outputs(self):
        mano_outputs = []
        for output in self.outputs:
            if output.value.name == 'get_attribute':
                get_parameters = output.value.args
                mano_target = self.nodes.find_mano_resource(get_parameters[0])
                mano_value = mano_target.get_mano_attribute(get_parameters[1],
                                                            get_parameters)
                mano_outputs.append(ManoOutput(output.name,
                                               mano_value,
                                               output.description))
            else:
                mano_outputs.append(ManoOutput(output.name,
                                               output.value,
                                               output.description))
        return mano_outputs
