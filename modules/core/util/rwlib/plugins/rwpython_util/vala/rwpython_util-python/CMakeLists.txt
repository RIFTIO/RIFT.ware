
# STANDARD_RIFT_IO_COPYRIGHT

include(rift_plugin)

rift_install_python_plugin(
  rwpython_util rwpython_util.py
  COMPONENT ${INSTALL_COMPONENT}
)
