
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(rift_python)

_install(
  FILES sitecustomize.py
  DESTINATION usr/${RIFT_PYDIR}
  COMPONENT rwbase)

rift_python_install_tree(
  FILES
    rift/__init__.py
    rift/gi_utils.py
    rift/gi/pbcm_override.py
  COMPONENT rwbase)

