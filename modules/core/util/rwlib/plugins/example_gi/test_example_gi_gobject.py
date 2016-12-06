#!/usr/bin/python

# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#



from gi.repository import ExampleGi

integer = 47
def integer_changed(gobject, param_spec):
    global integer

    integer = gobject.get_property('integer')

gobject = ExampleGi.Object.new('hello world')
assert gobject.get_property('string') == 'hello world'

gobject.connect('notify::integer', integer_changed)
assert gobject.get_property('integer') == integer
gobject.set_property('integer', 42)
assert gobject.get_property('integer') == integer
