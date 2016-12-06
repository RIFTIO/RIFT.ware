import os
import subprocess

import gi

gi.require_version('RwYang', '1.0')
from gi.repository import (
    RwYang,
)

from .pathwalker import (
    KeyBuilder,
    ProtobufBuilder,
)

def get_so_for_module(module):
    library_directory = os.environ["RIFT_INSTALL"] + "/usr"
    pkg_config_command = "pkg-config --define-variable=prefix=%s --libs %s" % (library_directory, module)
    result = subprocess.check_output(pkg_config_command, shell=True).decode('utf-8')
    so_name = "lib" + result.split("-l")[-1].strip() + ".so"
    return so_name

class Schema(object):
    '''
    Base class used to build runetime-composites for purposes of member data, key construction, and protobuf construction
    '''

    def __init__(self, schema_modules):
        self.model = RwYang.Model.create_libncx()
        self.protobuf_schema = None
        for module_name in schema_modules:
            module = self.model.load_module(module_name)
            so_filename = get_so_for_module(module_name)
            if self.protobuf_schema is None:
                self.protobuf_schema = RwYang.Model.load_schema(so_filename, module_name)
            else:
                self.protobuf_schema = RwYang.Model.load_and_merge_schema(self.protobuf_schema, so_filename, module_name)

        self.model.load_schema_ypbc(self.protobuf_schema)

        self.root_node = self.model.get_root_node()

    @property
    def key(self):
        return KeyBuilder(self)

    @property
    def protobuf(self):
        return ProtobufBuilder(self)
