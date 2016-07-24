
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(rift_plugin)

set(PKG_NAME rwcal-cloudsim)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

rift_install_python_plugin(rwcal_cloudsim rwcal_cloudsim.py)

install(
  PROGRAMS
  etc/lxc-fedora-rift.lxctemplate
  DESTINATION etc
  COMPONENT ${PKG_LONG_NAME}
  )

rift_python_install_tree(
  FILES
    rift/rwcal/cloudsim/__init__.py
    rift/rwcal/cloudsim/core.py
    rift/rwcal/cloudsim/exceptions.py
    rift/rwcal/cloudsim/image.py
    rift/rwcal/cloudsim/lvm.py
    rift/rwcal/cloudsim/lxc.py
    rift/rwcal/cloudsim/net.py
    rift/rwcal/cloudsim/shell.py
  PYTHON3_ONLY
  COMPONENT ${PKG_LONG_NAME})

