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


class ManoOutput(object):
    '''Attributes for RIFT.io MANO output section.'''

    def __init__(self, log, name, value, description=None):
        self.log = log
        self.name = name
        self.value = value
        self.description = description

    def __str__(self):
        return "%s(%s)" % (self.name, self.value)

    def get_dict_output(self):
        return {self.name: {'value': self.value,
                            'description': self.description}}
