
# STANDARD_RIFT_IO_COPYRIGHT

include(rift_plugin)

rift_install_python_plugin(
  rwdtstasklet rwdtstasklet.py
  COMPONENT ${INSTALL_COMPONENT}
)

