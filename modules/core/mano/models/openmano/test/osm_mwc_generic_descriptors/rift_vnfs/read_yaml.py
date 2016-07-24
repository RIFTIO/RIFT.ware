#!/usr/bin/env python3

import os

import gi
gi.require_version('RwYang', '1.0')
gi.require_version('RwVnfdYang', '1.0')
from gi.repository import RwYang, RwVnfdYang

model = RwYang.model_create_libncx()
model.load_schema_ypbc(RwVnfdYang.get_schema())

for filename in os.listdir("."):
    if not filename.endswith(".yaml"):
        continue

    with open(filename) as hdl:
        yaml_str = hdl.read()

    try:
        RwVnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd.from_yaml(model, yaml_str)
        print("Deserialized %s" % filename)
    except Exception:
        print("Failed to deserialize: %s" % filename)
        raise
