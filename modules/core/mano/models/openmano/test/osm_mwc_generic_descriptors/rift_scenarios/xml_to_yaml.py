#!/usr/bin/env python3

import os
from gi.repository import RwYang, RwNsdYang
model = RwYang.model_create_libncx()
model.load_schema_ypbc(RwNsdYang.get_schema())

for filename in os.listdir("."):
    if not filename.endswith(".xml"):
        continue

    yaml_filename, _ = os.path.splitext(filename)
    yaml_filename += ".yaml"

    with open(filename) as hdl:
        xml_str = hdl.read()

    nsd = RwNsdYang.YangData_Nsd_NsdCatalog_Nsd.from_xml_v2(model, xml_str)

    yaml_str = nsd.to_yaml(model)

    with open(yaml_filename, 'w') as hdl:
        hdl.write(yaml_str)
