
# STANDARD_RIFT_IO_COPYRIGHT

include(rift_plugin)

rift_install_python_plugin(
  basic_functor-python basic_functor-python.py
  COMPONENT ${INSTALL_COMPONENT}
)
