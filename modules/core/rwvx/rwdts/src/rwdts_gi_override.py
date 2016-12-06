
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

import collections
import functools
import json
import sys

if sys.version_info < (3, 0):
  import UserList
  collections.UserList = UserList.UserList

from gi.repository import GObject
import six

from ..overrides import override
from ..module import get_introspection_module

# Fill in the NAME_SPACE field with the current namespace we're generating.
ProtoGi = get_introspection_module("RwDts")

__all__ = []

_modname = globals()['__name__']
_module = sys.modules[_modname]


def disable_setattr_on_unknown_fields(obj):
    """ Raise NameError exception whenever setattr is called with unknown field

    Arguments:
        obj - The object to disable setattr on
    """
    original_setattr = obj.__setattr__
    def raise_unknown_field_attribute_error(self, field, value):
        """ Raise AttributeError if field is not found in self object

        Arguments:
            field - Field to raise attribute error if not found
            value - The value the obj field will be assigned to.
        """
        getattr(self, field)

        # If we've made it through getattr without an exception being thrown
        # we can now call the original __setattr__ function to set the attribute.
        original_setattr(self, field, value)

    obj.__setattr__ = raise_unknown_field_attribute_error


def get_intercept_wrapper(box_get_fn, box_set_fn, create_fn):
    """
      Args:
        box_get_fn: The boxed getter for a field.
        box_set_fn: The boxed setter for a field.

      Returns:
        A function that wraps the getter which intercepts any lists returned by the
        getter and wraps the list with a BoxedHelperList object.
    """
    @functools.wraps(box_get_fn)
    def get_wrapper_fn(self):
        box = box_get_fn(self)

        return box


    return get_wrapper_fn

def create_boxed_field_property_fns(boxed, field):
    """ Create a pythonic setter and getter to wrap each field in order to avoid
    having to call set_<field>, get_<field> everywhere.

    In addition, wrap the generated getter with a list_interceptor in order to
    improve the list usability.

      Args:
        boxed: The boxed parent object.
        field: The field name to create a getter and setter for.

      Returns:
        A tuple with two items: fset for the setter and fget for the getter.
    """
    setter_name = 'set_' + field
    setter_fn = getattr(boxed, setter_name, None)

    getter_name = 'get_' + field
    getter_fn = getattr(boxed, getter_name, None)

    # Attempt to get the function to create objects for the field we're
    # working on.  If the field is not a message type, then there is no "create"
    # GI method generated so the getattr will return None.
    creater_name = 'create_' + field
    creater_fn = getattr(boxed, creater_name, None)

    return (get_intercept_wrapper(getter_fn, setter_fn, creater_fn),
            setter_fn)


classes_n_names = []

"""
**************** Api <class 'gi.repository.RwDts.Api'> ****************
**************** Appconf <class 'gi.repository.RwDts.Appconf'> ****************
**************** Group <class 'gi.repository.RwDts.Group'> ****************
**************** MemberCursor <class 'gi.repository.RwDts.MemberCursor'> ****************
**************** MemberRegHandle <class 'gi.repository.RwDts.MemberRegHandle'> ****************
**************** QueryResult <class 'gi.repository.RwDts.QueryResult'> ****************
**************** QueryError <class 'gi.repository.RwDts.QueryError'> ****************
**************** Xact <class 'gi.repository.RwDts.Xact'> ****************
**************** XactBlock <class 'gi.repository.RwDts.XactBlock'> ****************
**************** XactInfo <class 'gi.repository.RwDts.XactInfo'> ****************
**************** XactStatus <class 'gi.repository.RwDts.XactStatus'> ****************
"""

class OverrideApi(getattr(ProtoGi, 'Api')):
    def __str__(self):
        #print("Xact--Xact--Xact")
        d = {}
        d['state'] = str(self.get_state())
        return json.dumps(d, sort_keys=True, indent=4, separators=(',', ': '))
classes_n_names.append((OverrideApi, 'Api')

class OverrideAppconf(getattr(ProtoGi, 'Appconf')):
    def __str__(self):
        #print("Xact--Xact--Xact")
        d = {}
        return json.dumps(d, sort_keys=True, indent=4, separators=(',', ': '))
classes_n_names.append((OverrideAppconf, 'Appconf')

class OverrideGroup(getattr(ProtoGi, 'Group')):
    def __str__(self):
        #print("Xact--Xact--Xact")
        d = {}
        return json.dumps(d, sort_keys=True, indent=4, separators=(',', ': '))
classes_n_names.append((OverrideGroup, 'Group')

class OverrideMemberCursor(getattr(ProtoGi, 'MemberCursor')):
    def __str__(self):
        #print("Xact--Xact--Xact")
        d = {}
        return json.dumps(d, sort_keys=True, indent=4, separators=(',', ': '))
classes_n_names.append((OverrideMemberCursor, 'MemberCursor')

class OverrideMemberRegHandle(getattr(ProtoGi, 'MemberRegHandle')):
    def __str__(self):
        #print("Xact--Xact--Xact")
        d = {}
        return json.dumps(d, sort_keys=True, indent=4, separators=(',', ': '))
classes_n_names.append((OverrideMemberRegHandle, 'MemberRegHandle')

class OverrideQueryResult(getattr(ProtoGi, 'QueryResult')):
    def __str__(self):
        #print("Xact--Xact--Xact")
        d = {}
        return json.dumps(d, sort_keys=True, indent=4, separators=(',', ': '))
classes_n_names.append((OverrideQueryResult, 'QueryResult')

class OverrideQueryResult(getattr(ProtoGi, 'QueryError')):
    def __str__(self):
        #print("Xact--Xact--Xact")
        d = {}
        return json.dumps(d, sort_keys=True, indent=4, separators=(',', ': '))
classes_n_names.append((OverrideQueryResult, 'QueryError')

class OverrideXact(getattr(ProtoGi, 'Xact')):
    def __str__(self):
        #print("Xact--Xact--Xact")
        d = {}
        d['status'] = str(self.get_status())
        d['more-results'] = str(self.get_more_results())
        return json.dumps(d, sort_keys=True, indent=4, separators=(',', ': '))
classes_n_names.append((OverrideXact, 'Xact'))

class OverrideXactBlock(getattr(ProtoGi, 'XactBlock')):
    def __str__(self):
        d = {}
        d['status'] = str(self.get_status())
        d['more-results'] = str(self.get_more_results())
        return json.dumps(d, sort_keys=True, indent=4, separators=(',', ': '))
classes_n_names.append((OverrideXactBlock, 'XactBlock'))

class OverrideXactInfo(getattr(ProtoGi, 'XactInfo')):
    def __str__(self):
        #print("XactInfo--XactInfo--XactInfo")
        d = {}
        d['credits'] = str(self.get_credits())
        d['xact'] = str(self.get_xact())
        return json.dumps(d, sort_keys=True, indent=4, separators=(',', ': '))
classes_n_names.append((OverrideXactInfo, 'XactInfo'))

class OverrideXactStatus(getattr(ProtoGi, 'XactStatus')):
    def __str__(self):
        #print("XactStatus--XactStatus--XactStatus")
        d = {}
        d['status'] = str(self.get_status())
        return json.dumps(d, sort_keys=True, indent=4, separators=(',', ': '))
classes_n_names.append((OverrideXactStatus, 'XactStatus'))


for cls, name in classes_n_names :
    disable_setattr_on_unknown_fields(cls)
    setattr(_module, name, override(cls))
    __all__.append(name)

for name in dir(ProtoGi):
    boxed = getattr(ProtoGi, name)


    if not hasattr(boxed, '__gtype__'):
        continue

    if not boxed.__gtype__.is_a(GObject.TYPE_BOXED):
        continue

    print "****************", name, boxed, "****************"

"""
    class ProtoGiWrappedBoxed(boxed):
        pass

    def getters(boxed):
        return [attr for attr in dir(boxed) if attr.startswith('get_') and callable(getattr(boxed, attr))]

    def setters(boxed):
        return [attr for attr in dir(boxed) if attr.startswith('set_') and callable(getattr(boxed, attr))]

    # Create a set of attribute names by iterating over the getter and setters
    # on the boxed object and stripping off the first 4 characters, i.e. the
    # 'set_' or 'get_' prefix.
    fields = set(attr[4:] for attr in getters(boxed) + setters(boxed))

    # Add all of the fields to the class as properties
    for field in fields:
        (fget, fset) = create_boxed_field_property_fns(boxed, field)
        prop = property(fget=fget, fset=fset)
        setattr(ProtoGiWrappedBoxed, field, prop)

    # This helper function recurses through an objects attributes and returns a
    # nested structure of basic types, e.g. list, dicts, and strings.
    def simplify(value):
        if isinstance(value, str):
            return value

        if isinstance(value, six.string_types):
            return value.encode('utf-8')

        if isinstance(value, collections.Mapping):
            return {simplify(k): simplify(v) for k, v in value.items()}

        if isinstance(value, collections.Sequence):
            return [simplify(x) for x in value]

        if not hasattr(value, '__gtype__'):
            return value

        if not value.__gtype__.is_a(GObject.TYPE_BOXED):
            return str(value)

        d = {}
        for method in getters(value):
            name = simplify(method[4:])
            if hasattr(value, "has_field") and value.has_field(name):
              try:
                attr = simplify(getattr(value, method)())
              except TypeError:
                continue
              d[name] = attr

        return d

    # A function that returns a representation of the object
    def __repr__(boxed):
        return repr(simplify(boxed))

    # A function that returns the object as a string
    def __str__(boxed):
        print("********************************************************************",boxed)
        return json.dumps(simplify(boxed), sort_keys=True,
                          indent=4, separators=(',', ': '))

    # Otherwise we get the wrapped version, might be a better way to do it
    setattr(ProtoGiWrappedBoxed, '__str__', __str__)
    setattr(ProtoGiWrappedBoxed, '__repr__', __repr__)
    setattr(ProtoGiWrappedBoxed, 'gi_fields', list(fields))

    ProtoGiWrappedBoxed.__name__ = boxed.__name__

    # Prevent anyone from setting unknown fields on a protoGI object
    disable_setattr_on_unknown_fields(ProtoGiWrappedBoxed)

    setattr(_module, name, override(ProtoGiWrappedBoxed))
    __all__.append(name)

"""
