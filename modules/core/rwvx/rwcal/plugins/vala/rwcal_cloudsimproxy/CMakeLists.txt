
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(rift_plugin)

set(PKG_NAME rwcal-cloudsimproxy)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})


rift_install_python_plugin(rwcal_cloudsimproxy rwcal_cloudsimproxy.py)

