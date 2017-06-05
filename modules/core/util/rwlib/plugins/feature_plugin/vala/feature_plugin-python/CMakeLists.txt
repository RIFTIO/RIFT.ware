
# STANDARD_RIFT_IO_COPYRIGHT

include(rift_plugin)

rift_install_python_plugin(
  feature_plugin-python feature_plugin-python.py
  COMPONENT ${INSTALL_COMPONENT}
)

