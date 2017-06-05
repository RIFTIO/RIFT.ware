
# STANDARD_RIFT_IO_COPYRIGHT

include(rift_python)

rift_install_python(
  COMPONENT ${INSTALL_COMPONENT}
  MODULE_NAME rwshell
  MODULE_FILES
    exceptions.py
    perftools.py
    crashtools.py
    __init__.py)

