
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(rift_plugin)

set(PKG_NAME rwcal-aws)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

rift_install_python_plugin(rwcal_aws rwcal_aws.py)

rift_python_install_tree(
  FILES
    rift/rwcal/aws/__init__.py
    rift/rwcal/aws/aws_table.py
    rift/rwcal/aws/aws_drv.py
    rift/rwcal/aws/exceptions.py
    rift/rwcal/aws/prepare_vm.py
    rift/rwcal/aws/delete_vm.py
  PYTHON3_ONLY
  COMPONENT ${PKG_LONG_NAME})

