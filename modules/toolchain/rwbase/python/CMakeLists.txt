
# STANDARD_RIFT_IO_COPYRIGHT

include(rift_python)

_install(
  FILES sitecustomize.py
  DESTINATION usr/${RIFT_PYDIR}
  COMPONENT ${TOOLCHAIN_INSTALL_NAME}
  )

rift_python_install_tree(
  FILES
    rift/__init__.py
    rift/gi_utils.py
    rift/gi/pbcm_override.py
  COMPONENT ${TOOLCHAIN_INSTALL_NAME}
  )

