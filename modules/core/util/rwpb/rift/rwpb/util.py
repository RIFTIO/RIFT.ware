from enum import Enum
import importlib

import gi

gi.require_version('RwKeyspec', '1.0')
from gi.repository import (
    RwKeyspec,
)

def get_protobuf_type(xpath, protobuf_schema):
    '''
    Given the protobuf schema, return the python type corresponding to the given xpath.
    '''

    desc = RwKeyspec.get_pbcmd_from_xpath(xpath, protobuf_schema)
    module_name, obj_type = desc.get_gi_typename().split(".", 1)

    try:
        gi_module = getattr(gi.repository, module_name)
    except AttributeError:
        # first time we've tried to construct something from this module, so load the python overrides
        gi_module_name = "gi.repository.%s" % module_name
        importlib.import_module(gi_module_name)
        gi_module = getattr(gi.repository, module_name)

    protobuf_type = getattr(gi_module, obj_type)

    return protobuf_type

class KeyCategory(Enum):
    '''
    Used to specifiy the type of the key
    '''
    Any = "A"
    Config = "C"
    Data = "D"
    Input = "I"
    Output = "O"

