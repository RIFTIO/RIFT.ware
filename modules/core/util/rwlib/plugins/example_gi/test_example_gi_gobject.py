#!/usr/bin/python

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
