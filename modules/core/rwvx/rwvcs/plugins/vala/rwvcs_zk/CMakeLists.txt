
# STANDARD_RIFT_IO_COPYRIGHT

include(rift_plugin)

rift_install_python_plugin(
  rwvcs_zk rwvcs_zk.py
  COMPONENT ${INSTALL_COMPONENT})
