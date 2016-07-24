
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(rift_plugin)

rift_install_python_plugin(pytoytasklet pytoytasklet.py)

# Workaround RIFT-6485 - rpmbuild defaults to python2 for
# anything not in a site-packages directory so we have to
# install the plugin implementation in site-packages and then
# import it from the actual plugin.
rift_python_install_tree(
  FILES
    rift/tasklets/pytoytasklet/__init__.py
    rift/tasklets/pytoytasklet/pytoytasklet.py
  COMPONENT rwvcs-1.0
  PYTHON3_ONLY)

