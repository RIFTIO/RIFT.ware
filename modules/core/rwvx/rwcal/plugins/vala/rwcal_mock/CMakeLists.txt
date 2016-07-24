
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(rift_plugin)

### rwcal-mock package
set(PKG_NAME rwcal-mock)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})


rift_install_python_plugin(rwcal_mock rwcal_mock.py)
