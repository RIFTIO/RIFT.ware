# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Creation Date: 9/13/15
# 

rift_python_install_tree(
  FILES
  rift/rwlib/testing/__init__.py
  rift/rwlib/testing/xml_helpers.py
  rift/rwlib/translation/__init__.py
  rift/rwlib/translation/json2xml.py
  rift/rwlib/translation/xml2json.py
  rift/rwlib/schema/__init__.py
  rift/rwlib/schema/helpers.py
  rift/rwlib/xml/__init__.py
  rift/rwlib/xml/helpers.py
  rift/rwlib/util/__init__.py
  rift/rwlib/util/certs.py
  rift/rwlib/util/iteration.py
  PYTHON3_ONLY
  COMPONENT
    ${PKG_NAME}
  )
