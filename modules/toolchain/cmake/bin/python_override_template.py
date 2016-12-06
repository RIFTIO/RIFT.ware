
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
ProtoGi = get_introspection_module("NAMESPACE")

__all__ = []

_modname = globals()['__name__']
_module = sys.modules[_modname]

class BoxedHelperList(collections.UserList):
    """A helper list class which wraps an original_list returned by a boxed parent.

    In the original protobuf implementation, a setter for a list is ONLY invoked
    when a new list is assigned to the boxed parent:
      e.g: boxed.list = new_list

    This means that calling list methods (such as append, etc) on that list
    would not actually get propogated down to the system.

    To solve this, we ensure that when any list method is called
    that could modify the original list data, that we invoke the boxed parent setter
    for that list in order to propogate the changes down to the system.  Since the list
    type in Python is read-only, it was neccessary to wrap this list in this new class.
    """


    # All list methods that will modify list data.
    MUTATING_LIST_METHODS = ["__setitem__", "__setslice__", "__delslice__",
                            "__iadd__", "__imul__", "append", "insert", "pop",
                            "remove", "reverse", "sort", "extend"]

    def __init__(self, original_list, parent, parent_setter, create_fn=None):
        """Inits the BoxedHelperList using the original list, a reference to
          the boxed parent, and the list setter function of that parent.

        Args:
          parent (object): The instance of the boxed parent which holds the list.
          parent_setter (function): The list setter function of the boxed parent that
              should be called whenever the list is modified.
          create_fn (function): The function to be called to create an element of the
              correc type to add to the list.
        """

        # UserList holds the list in the 'data' attribute
        super(BoxedHelperList, self).__init__(original_list)
        self._parent_setter = parent_setter
        self._parent = parent

        self._wrap_mutating_list_methods()
        self._create_fn = create_fn

        # If the create_fn actually exists (message type), then expose the _add method publically.
        if self._create_fn:
            self.add = self._add

    def _call_parent_setter_wrapper(self, list_method):
        """ Creates a wrapper function to call boxed parent setter after a list method

        Args:
          list_method (function): The list method to wrap.

        Returns:
          A function that wraps the original list method which first
          calls the method and subsequently calls the parent setter.
        """
        @functools.wraps(list_method)
        def call_setter_after_method(*args, **kwargs):
            ret_val = list_method(*args, **kwargs)
            self._parent_setter(self._parent, self.data)
            return ret_val

        return call_setter_after_method

    def _wrap_mutating_list_methods(self):
        """Wraps each list method in MUTATING_LIST_METHODS.

        For each list method in MUTATING_LIST_METHODS, wrap the method
        in a function that will call the boxed parent setter after the
        list method is invoked.
        """

        for method_name in BoxedHelperList.MUTATING_LIST_METHODS:
            if sys.version_info >= (3, 0) and method_name in ('__setslice__', '__delslice__'):
                continue

            method = getattr(self, method_name, None)
            assert method is not None

            wrapper = self._call_parent_setter_wrapper(method)
            setattr(self, method_name, wrapper)

    def _create(self):
        """ Creates and returns a new object of the correct type for this list."""
        return self._create_fn(self._parent)

    def _add(self):
        """ Creates and adds a new object of the correct element type to this list."""
        new_obj = self._create()
        self.append(new_obj)
        return new_obj


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
    """ Wraps the boxed parent getter method to wrap returned list with BoxedHelperList

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
        if isinstance(box, list):
            return BoxedHelperList(box, self, box_set_fn, create_fn)

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

for name in dir(ProtoGi):
    boxed = getattr(ProtoGi, name)

    if not hasattr(boxed, '__gtype__'):
        continue

    if not boxed.__gtype__.is_a(GObject.TYPE_BOXED):
        continue

    #DEBUG print("****************", name, boxed, "****************")

    class ProtoGiWrappedBoxed(boxed):
        pass

    def getters(boxed):
        return [attr for attr in dir(boxed) if attr.startswith('get_') and callable(getattr(boxed, attr)) and ( ( hasattr(boxed, 'has_field') and len(getattr(boxed, attr).get_arguments())==1 ) or len(getattr(boxed, attr).get_arguments())==0 )]
        #DEBUG The below lines are used during debugging
        #attrs = []
        #for attr in dir(boxed):
        #    #if attr.startswith('get_') and callable(getattr(boxed, attr)) and ( ( hasattr(boxed, 'has_field') and len(getattr(boxed, attr).get_arguments())==1 ) or len(getattr(boxed, attr).get_arguments())==0 ) :
        #    if attr.startswith('get_') and callable(getattr(boxed, attr)) and len(getattr(boxed, attr).get_arguments())==0 :
        #        attrs.append(attr)
        #print(attrs)
        #return attrs

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
            #DEBUG print("^^^^^^^^^^^^^^^^^^", method, "^^^^^^^^^^^^^^^^^^")
            name = simplify(method[4:])
            if hasattr(value, "has_field"):
              if value.has_field(name):
                try:
                  attr = simplify(getattr(value, method)())
                except TypeError:
                  continue
                d[name] = attr
            else :
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

