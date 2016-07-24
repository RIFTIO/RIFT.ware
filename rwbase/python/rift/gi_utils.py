
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import collections
import functools
import sys

import gi.repository.GObject
import six

if sys.version_info < (3, 0):
  import UserList
  collections.UserList = UserList.UserList


# Derive from collections.UserList instead of list as otherwise
# we cannot use functools.wraps when replacing the list mutating
# methods.
class GIList(collections.UserList):
    """
    Wrapper for lists owned by GI objects.  The API being exposed by GI doesn't
    allow for any in-line modification of a list.  This means that to modify a
    list, one must retrieve it, do the modification and then replace the list
    on the parent.  That's a pain to remember but we can wrap it all here.

    @param base_list      - list owned by the GI object
    @param parent         - Object which owns the list
    @param prop_name     - property name of the fiels in the parent
    """
    def __init__(self, base_list, parent, prop_name):
        super(GIList, self).__init__(base_list)
        self._parent = parent
        self._prop_name = prop_name

        mutating_methods = [
            '__setitem__',
            '__iadd__',
            '__imul__',
            'append',
            'insert',
            'pop',
            'remove',
            'reverse',
            'sort',
            'extend']

        if sys.version_info < (3, 0):
            mutating_methods.extend(['__setslice__', '__delslice__'])

        def wrap_list_modification(f):
            @functools.wraps(f)
            def wrapper(*args, **kwds):
                ret = f(*args, **kwds)
                setattr(parent, prop_name, self)
                return ret
            return wrapper

        for m in mutating_methods:
            setattr(self, m, wrap_list_modification(getattr(self, m)))

    @property
    def parent(self):
        return self._parent

    @property
    def property_name(self):
        return self._prop_name


class GIPBCMList(GIList):
    """
    Wrapper for lists owned by PBCM backed GI objects.  The API being exposed by GI doesn't
    allow for any in-line modification of a list owned by a PBCM object.  This means that
    to modify a list, one must retrieve it, do the modification and then replace the list
    owned by the PBCM object with the new list.  That's a pain to remember but we can
    wrap it all here.
    """
    def __str__(self):
        return str(simplify(self))

    def add(self, **kwargs):
        """
        Create a new element of the appropriate type for the list and append it.

        @return - new element which has been appended to the list
        """
        createf = getattr(self.parent, '_create_' + self.property_name)
        element = createf()
        element.from_dict(kwargs)
        self.append(element)
        return element


def simplify(obj, unmangle=False):
    """
    Simplify an object so that it is fully represented by base Python types.

    @param obj  - object to simplify
    @return     - base Python types (string, list, dictionary) representing
                  the given object
    """
    if isinstance(obj, str):
        return obj

    if isinstance(obj, six.string_types):
        return obj.encode('utf-8')

    if isinstance(obj, collections.Mapping):
        return {simplify(k, unmangle): simplify(v, unmangle) for k, v in obj.items()}

    if isinstance(obj, collections.Sequence):
        return [simplify(x, unmangle) for x in obj]

    if not isgibox(obj):
        return obj

    if not hasattr(obj, 'fields'):
        raise DeprecationWarning(
            'Objects of type %s are using old GI overrides, cannot print' % (
                type(obj)))
        return

    d = {}
    # Reserved field names (e.g. type) will be postfixed with _yang.  Unfortunately has_field()
    # uses the non-postfixed version.
    for field in (field for field in obj.fields if obj.has_field(field.replace("type_yang", "type"))):
        value = getattr(obj, field)

        if unmangle:
            field = field.replace("_","-")
            if field.endswith("-yang"):
                field = field[:-len("-yang")]

        d[field] = simplify(value, unmangle)

    return d




def create_property(box, name, getter=None, setter=None):
    """
    Create a class property given a getter and/or setter function.

    - If the functions are not already prefixed with an underscore to mark them
      as private, they will be renamed.
    - If the getter returns a list, it will be wrapped in a GIList instance so
      that list modification functions continue to work as expected.

    @param box    - type/class to add the property to
    @param name   - property name
    @param getter - function used to get the property (gi.FunctionInfo)
    @param setter - function used to set the property (gi.FunctionInfo)
    """
    if getter is not None and not getter.get_name().startswith('_'):
        priv_name = '_%s' % (getter.get_name(),)
        setattr(box, priv_name, getter)
        delattr(box, getter.get_name())
        getter = getattr(box, priv_name)

    if setter is not None and not setter.get_name().startswith('_'):
        priv_name = '_%s' % (setter.get_name(),)
        setattr(box, priv_name, setter)
        delattr(box, setter.get_name())
        setter = getattr(box, priv_name)

    def setter_wrapper(self, value):
        priv_name = '_%s' % (setter.get_name(),)
        f = getattr(self, priv_name)
        return f(value)

    def getter_wrapper(self):
        priv_name = '_%s' % (getter.get_name(),)
        f = getattr(self, priv_name)
        ret = f()
        if isinstance(ret, list) and setter is not None:
            ret = GIList(ret, self, name)
        return ret

    prop = property(
            fget=getter_wrapper if getter is not None else None,
            fset=setter_wrapper if setter is not None else None)
    setattr(box, name, prop)


def isgibox(obj):
    """
    Test if a type is a GI box type.
    """
    return (hasattr(obj, '__gtype__')
            and obj.__gtype__.is_a(gi.repository.GObject.TYPE_BOXED))


def isgienum(obj):
    """
    Test if a type is a GI enum type.
    """
    return (hasattr(obj, '__gtype__')
            and obj.__gtype__.is_a(gi.repository.GObject.TYPE_ENUM))


# vim: sw=4
