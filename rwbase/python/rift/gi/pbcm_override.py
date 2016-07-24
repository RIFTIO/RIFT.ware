
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


import collections
import functools
import inspect
import json
import yaml
import os
import sys
import warnings

import gi.module
from gi.repository import ProtobufC
import rift.gi_utils

if sys.version_info >= (3, 0):
    try:
        from rift.rwlib.translation import (
            json_to_xml,
            xml_to_json,
        )
        from rift.rwlib.schema import (
            prune_non_schema_xml,
        )

    except Exception as e:
        print(e)

class combomethod(object):
    """
    A combomethod is a method that can be called as either a classmethod
    or an intstance method. The "receiver" of the message is akin to
    traditional ``self`` and ``cls`` parameters, but can be either.

    For example::

        class A(object):

            @combomethod
            def either(receiver, x, y):
                return x + y

    Can now be called as either:"

        A.either(1, 3)

    Or::

        a = A()
        a.either(1, 3)

    """

    def __init__(self, method):
        self.method = method

    def __get__(self, obj=None, objtype=None):
        @functools.wraps(self.method)
        def _wrapper(*args, **kwargs):
            if obj is not None:
                return self.method(obj, *args, **kwargs)
            else:
                return self.method(objtype, *args, **kwargs)
        return _wrapper

def create_pbcm_property(box, field_name):
    """
    Create a class property given a field which has both get_X and set_X
    functions.  Also mark the get/set/create_X functions a private (leading _)
    so that only one method for access is exposed.

    If the property is a list, the returned value will be wrapped in a
    rift.gi_utils.GIPBCMList so that the parent will be automatically
    updated as necessary.

    @param box        - type/class to add the property to
    @param field_name - property name
    """
    def deprecated_wrapper(f):
        @functools.wraps(f)
        def wrapper(*args, **kwds):
            warnings.warn('Deprecated, use properties', DeprecationWarning)
            return f(*args, **kwds)
        return wrapper

    # Mark the get/set/create functions as private.  Mark the public ones as
    # deprecated.
    for prefix in ('get', 'set', 'create'):
        try:
            f = getattr(box, '%s_%s' % (prefix, field_name))
            setattr(box, '_%s_%s' % (prefix, field_name), f)
            setattr(box, '%s_%s' % (prefix, field_name), deprecated_wrapper(f))
        except AttributeError:
            pass

    def getter(self):
        f = getattr(self, '_get_' + field_name)
        ret = f()
        if isinstance(ret, list):
            ret = rift.gi_utils.GIPBCMList(ret, self, field_name)
        return ret

    def setter(self, value):
        f = getattr(self, '_set_' + field_name)
        return f(value)

    prop = property(getter, setter)
    setattr(box, field_name, prop)


def wrap_module():
    """
    Wrap a GI module containing protobuf CMessages.

    - Create properties on each type corresponding to each pb field.
    - Add __str__ to each type
    - Add __eq__ to each type
    - Add __ne__ to each type
    - Add __iter__ to each type to return list of present fields
    - Add __contains__ to each type to check if field is present
    - Add to_json to dump the type to it's corresponding JSON
    - Add from_json to deserialize JSON into the PB type
    - Add to_yaml to dump the type to it's corresponding YAML
    - Add from_yaml to deserialize YAML into the PB type
    - Add from_dict() classmethod to populate message from dictionary
    - Add as_dict() method to dump a pbcm as a dictionary
    - Only allow setattr to operate on pb fields
    - Add keyword arguments to __init__() for each pb field
    - Wrap lists in GIPBCMList so that the list can be modified in-line

    It is expected that this file be named the same as the GI module that is
    being wrapped.
    """
    namespace = os.path.splitext(os.path.basename(__file__))[0]
    gi_mod = gi.module.get_introspection_module(namespace)

    # Fields named 'type' are mangled as this would conflict with GI_BOXED
    # macros.  RIFT-7377
    def fields_from_descriptor(descriptor):
        return [x if x != 'type' else 'type_yang' for x in descriptor.get_field_names()]

    # Types starting with PBCMD_ are descriptors which are essentially opaque types as far
    # as python is concerned.
    for box in (getattr(gi_mod, n) for n in dir(gi_mod) if not n.startswith('PBCMD_')):

        if not rift.gi_utils.isgibox(box):
            continue

        if not hasattr(box, 'change_to_descriptor'):
            continue

        descriptor = box.change_to_descriptor(box.schema())

        fields = fields_from_descriptor(descriptor)
        for field in fields:
            create_pbcm_property(box, field)

        box.fields = fields

        def as_dict(self,):
            return rift.gi_utils.simplify(self)
        box.as_dict = as_dict

        def __eq__(self, other):
            ''' Returns true if the two protobufs are byte-for-byte equal'''
            if other is None:
                return False

            my_base = self.to_pbcm()
            other_base = other.to_pbcm()

            return ProtobufC.Message.is_equal_deep(None,
                                                   my_base,
                                                   other_base)
        box.__eq__ = __eq__

        def __ne__(self, other):
            ''' Returns true if the two protobufs are not byte-for-byte equal'''
            if other is None:
                return True

            my_base = self.to_pbcm()
            other_base = other.to_pbcm()

            return not ProtobufC.Message.is_equal_deep(None,
                                                       my_base,
                                                       other_base)
        box.__ne__ = __ne__

        def __str__(self):
            return str(rift.gi_utils.simplify(self))
        box.__str__ = __str__

        def __iter__(self):
            return iter(rift.gi_utils.simplify(self).items())
        box.__iter__ = __iter__

        def __contains__(self, field):
            return self.has_field(field.replace("type_yang", "type"))
        box.__contains__ = __contains__

        def __setattr__(self, name, value):
            if name not in self.fields:
                raise AttributeError('PBCM has no field "%s"' % (name,))
            super(type(self), self).__setattr__(name, value)
        box.__setattr__ = __setattr__

        def add_xml_support():
            if hasattr(box, "to_xml_v2"):
                orig_to_xml_v2 = box.to_xml_v2
                orig_from_xml_v2 = box.from_xml_v2

            def to_xml_v2(self, model, pretty_print=False):
                """ Serialize Protobuf Message into XML

                :param self: GI class or instance
                :param model: Model returned by create_libncx()
                """
                xml_string = orig_to_xml_v2(self, model, pretty_print=pretty_print)

                return xml_string

            @combomethod
            def from_xml_v2(receiver, model, xml_str, strict=True):
                """ Deserialize XML message into Protobuf Message

                :param receiver: GI class or instance
                :param model: Model returned by create_libncx()
                :param xml_str: Serialized XML message for current Protobuf Type
                """
                def _from_xml(self):
                    if not strict:
                        schema_root = model.get_root_node()
                        raw_xml = prune_non_schema_xml(schema_root, xml_str) 
                    else:
                        raw_xml = xml_str

                    orig_from_xml_v2(self, model, raw_xml)

                if inspect.isclass(receiver):
                    self = receiver()
                    _from_xml(self)
                    return self

                else:
                    self = receiver
                    _from_xml(self)

            if hasattr(box, "to_xml_v2"):
                box.to_xml_v2 = to_xml_v2
                box.from_xml_v2 = from_xml_v2

        def add_json_support():
            def to_json(self, model):
                """ Serialize Protobuf Message into JSON

                :param self: GI class or instance
                :param model: Model returned by create_libncx()
                """
                schema = model.get_root_node()
                xml_string = self.to_xml_v2(model)
                json_string = xml_to_json(schema, xml_string)

                return json_string

            @combomethod
            def from_json(receiver, model, json_str, strict=True):
                """ Deserialize JSON message into Protobuf Message

                :param receiver: GI class or instance
                :param model: Model returned by create_libncx()
                :param json_str: Serialized JSON message for current Protobuf Type
                """
                def _from_json(self):
                    schema = model.get_root_node()
                    xml = json_to_xml(schema, json_str, strict)
                    self.from_xml_v2(model, xml)

                if inspect.isclass(receiver):
                    self = receiver()
                    _from_json(self)
                    return self

                else:
                    self = receiver
                    _from_json(self)


            box.to_json = to_json
            box.from_json = from_json


        def add_yaml_support():
            def to_yaml(self, model):
                """ Serialize Protobuf Message into YAML string

                :param self: GI class or instance
                :param model: Model returned by create_libncx()
                """

                json_string = self.to_json(model)
                msg_dict = json.loads(json_string)
                yaml_string = yaml.safe_dump(msg_dict, indent=4,
                                             default_flow_style=False)

                return yaml_string

            @combomethod
            def from_yaml(receiver, model, yaml_str, strict=True):
                """ Deserialize YAML message into Protobuf Message

                :param receiver: GI class or instance
                :param model: Model returned by create_libncx()
                :param yaml_str: Serialized JSON message for current Protobuf Type
                """

                def _from_yaml(self):
                    schema = model.get_root_node()
                    msg_dict = yaml.safe_load(yaml_str)
                    json_str = json.dumps(msg_dict)
                    self.from_json(model, json_str, strict)

                if inspect.isclass(receiver):
                    self = receiver()
                    _from_yaml(self)
                    return self

                else:
                    self = receiver
                    _from_yaml(self)


            box.from_yaml = from_yaml
            box.to_yaml = to_yaml

        def add_deep_copy_support():
            def deep_copy(self):
                dup = self.__class__()
                dup.copy_from(self)
                return dup

            box.deep_copy = deep_copy

        def add_kwargs_support():
            box.__old_new__ = box.__new__
            def __new__(cls, **kwds):
                return cls.__old_new__()
            box.__new__ = __new__

            # Support keyword arguments.  Note that the above __setattr__ override
            # will make sure that we only support protobuf field names.
            def __init__(self, **kwds):
                super(type(self), self).__init__()
                _ = [setattr(self, field, value) for field, value in kwds.items()]
            box.__init__ = __init__

        def add_from_dict_support():
            def fill_message_from_dict(msg, from_dict, ignore_missing_keys=False):
                """ Recursively populate message from dictionary

                Iterate across all key in the from_dict and populate the
                corresponding field in msg with the value.

                If the value is another dictionary then we assume the field
                is actually another message (a container in yang land).  In
                this case, call fill_message_from_dict() recursively with the
                child message and dictionary value.

                Arguments:
                    msg - A protobuf message to populate
                    from_dict - The dictionary to populate msg from

                Raises:
                    ValueError - If from_dict specifies a field that does not exist.
                    TypeError - If the type of the dictionary value is not correct
                                for the particular field.
                """
                if not hasattr(msg, 'change_to_descriptor'):
                    raise AttributeError('Expected change_to_descriptor to exist'
                        'in msg %s', msg)
                descriptor = msg.change_to_descriptor(msg.schema())
                fields = fields_from_descriptor(descriptor)

                for dict_key, dict_val in from_dict.items():
                    if dict_key not in fields:
                        if ignore_missing_keys:
                            continue

                        raise ValueError("%s protobuf message does not have a `%s` field" % (msg.__class__.__name__, dict_key))

                    msg_field_value = getattr(msg, dict_key)
                    if isinstance(msg_field_value, rift.gi_utils.GIPBCMList):
                        if not isinstance(dict_val, collections.Sequence):
                            raise ValueError("%s list value must be iterable: %s" % dict_key)

                        for element in dict_val:
                            if isinstance(element, collections.Mapping):
                                fill_message_from_dict(msg_field_value.add(), element)
                            else: # Support leaf-lists
                                msg_field_value.append(element)

                    elif not rift.gi_utils.isgibox(msg_field_value):
                        setattr(msg, dict_key, dict_val)

                    else:
                        fill_message_from_dict(msg_field_value, dict_val)

            @combomethod
            def from_dict(receiver, from_dict, ignore_missing_keys=False):
                """ Create and populate a protobuf message from a dictionary

                Arguments:
                    from_dict - The dictionary to populate the message from
                    ignore_missing - If the destination GI object does not have matching key
                            in the dictionary, then ignore instead of raising an exception.
                """
                if inspect.isclass(receiver):
                    obj = receiver()
                    fill_message_from_dict(obj, from_dict, ignore_missing_keys)
                    return obj
                else:
                    obj = receiver
                    fill_message_from_dict(obj, from_dict, ignore_missing_keys)

            box.from_dict = from_dict

        add_from_dict_support()
        add_deep_copy_support()
        add_xml_support()
        if sys.version_info >= (3, 0):
            add_kwargs_support()
            add_json_support()
            add_yaml_support()


    # Sanitize the enum names coming from Yang.  Workaround until
    # is resolved RIFT-7507.
    for enum_name in (n for n in dir(gi_mod) if n.startswith('__YangEnum__') and n.endswith('__E')):
        enum = getattr(gi_mod, enum_name)

        if not rift.gi_utils.isgienum(enum):
            continue

        enum_split = enum_name.split('__')
        if len(enum_split) != 4:
            warnings.warn('Invalid yang enum name %s' % (enum_name,))
            continue

        if hasattr(gi_mod, enum_split[2]):
            warnings.warn('Cannot rename enum, attribute %s already exists' % (enum_split[2],))
            continue

        setattr(gi_mod, enum_split[2], enum)


wrap_module()

# vim: sw=4

