#!/usr/bin/env python3

import os
from gi.repository import RwYang, RwVnfdYang
model = RwYang.model_create_libncx()
model.load_schema_ypbc(RwVnfdYang.get_schema())

for filename in os.listdir("."):
    if not filename.endswith("6WindTR1.1.2.xml"):
        continue

    yaml_filename, _ = os.path.splitext(filename)
    yaml_filename += ".yaml2"

    with open(filename) as hdl:
        xml_str = hdl.read()

    vnfd = RwVnfdYang.YangData_Vnfd_VnfdCatalog_Vnfd.from_xml_v2(model, xml_str)

    yaml_str = vnfd.to_yaml(model)

    with open(yaml_filename, 'w') as hdl:
        hdl.write(yaml_str)
