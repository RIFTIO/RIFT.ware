
# STANDARD_RIFT_IO_COPYRIGHT

include(rift_plugin)

rift_install_python_plugin(
  rwcli_plugin rwcli_plugin.py
  COMPONENT ${INSTALL_COMPONENT})

