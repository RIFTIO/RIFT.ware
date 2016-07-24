# STANDARD_RIFT_IO_COPYRIGHT

# Modified from https://github.com/openstack/heat-translator (APL 2.0)
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


from rift.mano.tosca_translator.common.utils import _
from rift.mano.tosca_translator.rwmano.syntax.mano_parameter import ManoParameter

from toscaparser.dataentity import DataEntity
from toscaparser.elements.scalarunit import ScalarUnit_Size
from toscaparser.parameters import Input
from toscaparser.utils.validateutils import TOSCAVersionProperty


INPUT_CONSTRAINTS = (CONSTRAINTS, DESCRIPTION, LENGTH, RANGE,
                     MIN, MAX, ALLOWED_VALUES, ALLOWED_PATTERN) = \
                    ('constraints', 'description', 'length', 'range',
                     'min', 'max', 'allowed_values', 'allowed_pattern')

TOSCA_CONSTRAINT_OPERATORS = (EQUAL, GREATER_THAN, GREATER_OR_EQUAL, LESS_THAN,
                              LESS_OR_EQUAL, IN_RANGE, VALID_VALUES, LENGTH,
                              MIN_LENGTH, MAX_LENGTH, PATTERN) = \
                             ('equal', 'greater_than', 'greater_or_equal',
                              'less_than', 'less_or_equal', 'in_range',
                              'valid_values', 'length', 'min_length',
                              'max_length', 'pattern')

TOSCA_TO_MANO_CONSTRAINTS_ATTRS = {'equal': 'allowed_values',
                                   'greater_than': 'range',
                                   'greater_or_equal': 'range',
                                   'less_than': 'range',
                                   'less_or_equal': 'range',
                                   'in_range': 'range',
                                   'valid_values': 'allowed_values',
                                   'length': 'length',
                                   'min_length': 'length',
                                   'max_length': 'length',
                                   'pattern': 'allowed_pattern'}

TOSCA_TO_MANO_INPUT_TYPES = {'string': 'string',
                             'integer': 'number',
                             'float': 'number',
                             'boolean': 'boolean',
                             'timestamp': 'string',
                             'scalar-unit.size': 'number',
                             'version': 'string',
                             'null': 'string',
                             'PortDef': 'number'}


class TranslateInputs(object):

    '''Translate TOSCA Inputs to RIFT MANO input Parameters.'''

    def __init__(self, log, inputs, parsed_params, deploy=None):
        self.log = log
        self.inputs = inputs
        self.parsed_params = parsed_params
        self.deploy = deploy

    def translate(self):
        return self._translate_inputs()

    def _translate_inputs(self):
        mano_inputs = []
        if 'key_name' in self.parsed_params and 'key_name' not in self.inputs:
            name = 'key_name'
            type = 'string'
            default = self.parsed_params[name]
            schema_dict = {'type': type, 'default': default}
            input = Input(name, schema_dict)
            self.inputs.append(input)

        self.log.info(_('Translating TOSCA input type to MANO input type.'))
        for input in self.inputs:
            mano_default = None
            mano_input_type = TOSCA_TO_MANO_INPUT_TYPES[input.type]

            if input.name in self.parsed_params:
                mano_default = DataEntity.validate_datatype(
                    input.type, self.parsed_params[input.name])
            elif input.default is not None:
                mano_default = DataEntity.validate_datatype(input.type,
                                                            input.default)
            else:
                if self.deploy:
                    msg = _("Need to specify a value "
                            "for input {0}.").format(input.name)
                    self.log.error(msg)
                    raise Exception(msg)
            if input.type == "scalar-unit.size":
                # Assumption here is to use this scalar-unit.size for size of
                # cinder volume in heat templates and will be in GB.
                # should add logic to support other types if needed.
                input_value = mano_default
                mano_default = (ScalarUnit_Size(mano_default).
                                get_num_from_scalar_unit('GiB'))
                if mano_default == 0:
                    msg = _('Unit value should be > 0.')
                    self.log.error(msg)
                    raise Exception(msg)
                elif int(mano_default) < mano_default:
                    mano_default = int(mano_default) + 1
                    self.log.warning(_("Cinder unit value should be in"
                                       " multiples of GBs. So corrected"
                                       " %(input_value)s to %(mano_default)s"
                                       " GB.")
                                     % {'input_value': input_value,
                                        'mano_default': mano_default})
            if input.type == 'version':
                mano_default = TOSCAVersionProperty(mano_default).get_version()

            mano_constraints = []
            if input.constraints:
                for constraint in input.constraints:
                    if mano_default:
                        constraint.validate(mano_default)
                    hc, hvalue = self._translate_constraints(
                        constraint.constraint_key, constraint.constraint_value)
                    mano_constraints.append({hc: hvalue})

            mano_inputs.append(ManoParameter(self.log,
                                             name=input.name,
                                             type=mano_input_type,
                                             description=input.description,
                                             default=mano_default,
                                             constraints=mano_constraints))
        return mano_inputs

    def _translate_constraints(self, name, value):
        mano_constraint = TOSCA_TO_MANO_CONSTRAINTS_ATTRS[name]

        # Offset used to support less_than and greater_than.
        # TODO(anyone):  when parser supports float, verify this works
        offset = 1

        if name == EQUAL:
            mano_value = [value]
        elif name == GREATER_THAN:
            mano_value = {"min": value + offset}
        elif name == GREATER_OR_EQUAL:
            mano_value = {"min": value}
        elif name == LESS_THAN:
            mano_value = {"max": value - offset}
        elif name == LESS_OR_EQUAL:
            mano_value = {"max": value}
        elif name == IN_RANGE:
            # value is list type here
            min_value = min(value)
            max_value = max(value)
            mano_value = {"min": min_value, "max": max_value}
        elif name == LENGTH:
            mano_value = {"min": value, "max": value}
        elif name == MIN_LENGTH:
            mano_value = {"min": value}
        elif name == MAX_LENGTH:
            mano_value = {"max": value}
        else:
            mano_value = value
        return mano_constraint, mano_value
