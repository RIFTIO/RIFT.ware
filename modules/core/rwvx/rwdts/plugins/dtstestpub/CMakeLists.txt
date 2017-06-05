
# STANDARD_RIFT_IO_COPYRIGHT

include(rift_plugin)

rift_install_python_plugin(
  dtstestpub dtstestpub.py
  COMPONENT ${INSTALL_COMPONENT}
)

# Workaround RIFT-6485 - rpmbuild defaults to python2 for
# anything not in a site-packages directory so we have to
# install the plugin implementation in site-packages and then
# import it from the actual plugin.
rift_python_install_tree(
  FILES
    rift/tasklets/dtstestpub/__init__.py
    rift/tasklets/dtstestpub/dtstestpub.py
  COMPONENT ${INSTALL_COMPONENT}
  PYTHON3_ONLY)

