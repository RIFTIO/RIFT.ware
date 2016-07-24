
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(rift_plugin)

### rwcal-openstack package
set(PKG_NAME rwcal-openstack)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

rift_install_python_plugin(rwcal_openstack rwcal_openstack.py)

rift_python_install_tree(
  FILES
    rift/rwcal/openstack/__init__.py
    rift/rwcal/openstack/openstack_drv.py
    rift/rwcal/openstack/openstack_utils.py
    rift/rwcal/openstack/prepare_vm.py
  PYTHON3_ONLY
  COMPONENT ${PKG_LONG_NAME})

