
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(rift_externalproject)

rift_externalproject_add(
  libpthread_workqueue
  SOURCE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/libpthread_workqueue-0.8.2
  CONFIGURE_COMMAND
    ./configure --prefix=${CMAKE_INSTALL_PREFIX}/usr
)

