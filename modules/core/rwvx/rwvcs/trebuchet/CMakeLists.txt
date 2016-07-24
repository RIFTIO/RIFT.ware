
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(rift_python)

add_subdirectory(src)
add_subdirectory(test)

rift_python_install_tree(
  FILES
    rift/trebuchet/__init__.py
    rift/trebuchet/app.py
    rift/trebuchet/vcs_handlers.py
  PYTHON3_ONLY
  COMPONENT rwvcs-1.0)

install(PROGRAMS
  trebuchet

  DESTINATION usr/${RIFT_BINDIR}
  COMPONENT rwvcs-1.0)
