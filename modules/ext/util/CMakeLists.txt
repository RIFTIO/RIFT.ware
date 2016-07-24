
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/modules")
include(rift_submodule)

# These should not be detected at runtime.  Doing so means we
# can't build VM images from jenkins or developer workstations
# where the kernel has been updated.
set(FPATH_KERNEL_VERSION 3.12.9-301.fc20.x86_64)
set(FPATH_KERNEL_SRC /lib/modules/3.12.9-301.fc20.x86_64/build)


set(subdirs
  talloc
  libkqueue
  libBlocksRuntime
  libpthread_workqueue
  libdispatch
  cflite
  ck
  dpdk
  gmock
  gperftools
  uthash
  libedit
  libuinet
  libunwind
  xerces-c
  xalan
  curl
  urcu
  tcpdump
  lua
  vtune
  libtimer 
  stream
  coredumper
  )
rift_add_subdirs(SUBDIR_LIST ${subdirs})

rift_add_submodule_targets(SUBMODULE_PACKAGE_NAME "rw.ext.util")
