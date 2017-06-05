
# STANDARD_RIFT_IO_COPYRIGHT

include(rift_plugin)

rift_install_python_plugin(
  standard_plugin-python standard_plugin-python.py
  COMPONENT ${INSTALL_COMPONENT}
)
