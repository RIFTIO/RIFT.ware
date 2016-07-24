#!/usr/bin/env python3
import os
import gi
import rift.mano.config_data.config
gi.require_version('RwVnfdYang', '1.0')
gi.require_version('RwDts', '1.0')
from gi.repository import RwVnfdYang, RwYang

model = RwYang.model_create_libncx()
model.load_schema_ypbc(RwVnfdYang.get_schema())

for filename in os.listdir("."):
    if not filename.endswith(".xml"):
        continue

    with open(filename) as hdl:
        yaml_str = hdl.read()

    try:
        vnfd = RwVnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd.from_xml_v2(model, yaml_str)
        print("Deserialized %s" % filename)

        config_str = rift.mano.config_data.config.ConfigPrimitiveConvertor().extract_vnfd_config(vnfd)
        print("config data: \n%s\n" % config_str)

    except Exception:
        print("Failed to deserialize: %s" % filename)
        raise
