
# STANDARD_RIFT_IO_COPYRIGHT

cmake_minimum_required(VERSION 2.8)

include(rift_submodule NO_POLICY_SCOPE)

# These should not be detected at runtime.  Doing so means we
# can't build VM images from jenkins or developer workstations
# where the kernel has been updated.
set(FPATH_KERNEL_VERSION 3.12.9-301.fc20.x86_64)
set(FPATH_KERNEL_SRC /lib/modules/3.12.9-301.fc20.x86_64/build)


set(subdirs
  gperftools
  stream
  )

if ($ENV{RIFT_PLATFORM} STREQUAL "fc20")
   list(APPEND subdirs dpdk vtune)
endif()

rift_add_subdirs(SUBDIR_LIST ${subdirs})

##
# Set up package details
##

rift_set_component_package_fields(
  "gperftools"
  DESCRIPTION "RIFT.ware gperftools"
  )

rift_set_component_package_fields(
  "stream"
  DESCRIPTION "RIFT.ware stream"
  )

rift_add_submodule_targets(
  SUBMODULE_PACKAGE_NAME "rw.ext.util"
  )
