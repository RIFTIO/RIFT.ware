
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(rift_python)

rift_install_python(
  COMPONENT ${PKG_LONG_NAME}
  MODULE_NAME rwshell
  MODULE_FILES
    exceptions.py
    perftools.py
    crashtools.py
    __init__.py)

