# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 03/26/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

##
# This file should only be included once at the top level
# submodule CMakeLists.txt
##

##
# Include the cmake files. The following are commonly used include files
# rift_topdir        - Provides global variables
# rift_plugin        - provides infrastructre for adding
# rift_install       - Provides a wrapper macro around cmake install function. This function intercepts CMAKE
#                      install function for RIFT specific processing.
# rift_unittest      - Provides rift_unittest function for adding unitests
# rift_code_coverage - Provides rift_code_coverage function for code coverage
# rift_documentation - Provides functions for documentation. Specifically rift_add_doxygen_target
#                      to add doxygen targets, rift_asciidoc_to_pdf and rift_asciidoc_to_html
#                      for asciidoc conversion routines.
##
include(rift_topdir NO_POLICY_SCOPE)
include(rift_utils)
include(rift_install)
## rift_install MUST PRECEDE THE REST OF THE INCLUDES
include(CTest)
include(FindVala)
include(UseVala)
include(rift_code_coverage)
include(rift_documentation)
include(rift_externalproject)
include(rift_logging)
include(rift_pkg_config)
include(rift_plugin)
include(rift_proto)
include(rift_python)
include(rift_systemtest)
include(rift_unittest)
include(rift_yang)

##
# Variables provided by the submodules
##

if(NOT DEFINED RIFT_SUBMODULE_NAME)
    message(FATAL_ERROR "RIFT_SUBMODULE_NAME must be provded")
endif()

string(REPLACE "${CMAKE_SOURCE_DIR}" "" RIFT_SUBMODULE_PREFIX ${CMAKE_CURRENT_SOURCE_DIR})
set(RIFT_SUBMODULE_PREFIX "modules/${RIFT_SUBMODULE_NAME}")

set(RIFT_SUBMODULE_SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(RIFT_SUBMODULE_BINARY_ROOT ${CMAKE_CURRENT_BINARY_DIR})
set(RIFT_SUBMODULE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/${RIFT_SUBMODULE_PREFIX})
message("RIFT_SUBMODULE_INSTALL_PREFIX = ${RIFT_SUBMODULE_INSTALL_PREFIX}")

##
# Set the global compiler flags
# Enable the all warnings, enaple fPIC
##

## DETECT UNDERLINKING
#set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")

if(${CMAKE_BUILD_TYPE} MATCHES "Release")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fPIC -ggdb -O2")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -fPIC -ggdb -O2")
else()
  # set the coverage related flags
  if(COVERAGE_BUILD MATCHES TRUE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fPIC -ggdb -O0 -fprofile-arcs -ftest-coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -fPIC -ggdb -O0 -fprofile-arcs -ftest-coverage")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
  else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fPIC -ggdb -O0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -fPIC -ggdb -O0")
  endif()
endif()

message("${RIFT_SUBMODULE_NAME} CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
message("${RIFT_SUBMODULE_NAME} CMAKE_C_FLAGS=${CMAKE_C_FLAGS}")
message("${RIFT_SUBMODULE_NAME} CMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}")
message("${RIFT_SUBMODULE_NAME} CMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}")
message("${RIFT_SUBMODULE_NAME} CMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}")

##
# Set the global include directories for yang files.
# These include directories are from the install directory.
##
set(RIFT_GLOBAL_YANG_INCLUDE_DIRECTORIES
  /usr/share/yuma/modules/netconfcentral
  ${CMAKE_INSTALL_PREFIX}/usr/data/yang
  ${CMAKE_INSTALL_PREFIX}/usr/local/pyang-1.5/share/yang/modules
  /usr/share/yuma/modules/ietf
  )

##
# Find the submodule specific yang include directories
# The convention is to include all the directories that match
# the pattern */yang.
##
execute_process(
  COMMAND find . -type f -name *.yang
  COMMAND perl -pe "s/^..(.*)\\/.*\$/\$1/"
  COMMAND uniq
  OUTPUT_VARIABLE yang_dirs
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
if(result)
  message("Didn't find */yang in ${CMAKE_CURRENT_SOURCE_DIR}")
else()
  string(REGEX REPLACE "\n" ";" yang_dirs "${yang_dirs}")
  rift_files_prepend_path(RIFT_SUBMODULE_YANG_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR} ${yang_dirs})
  rift_files_prepend_path(RIFT_SUBMODULE_YANG_OUTPUT_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR} ${yang_dirs})
endif()
set(yang_dirs)

set(RIFT_YANG_DIRS ${RIFT_SUBMODULE_YANG_INCLUDE_DIRECTORIES} ${RIFT_GLOBAL_YANG_INCLUDE_DIRECTORIES})

message("RIFT_SUBMODULE_YANG_INCLUDE_DIRECTORIES = ${RIFT_SUBMODULE_YANG_INCLUDE_DIRECTORIES}")
message("RIFT_SUBMODULE_YANG_OUTPUT_DIRECTORIES = ${RIFT_SUBMODULE_YANG_OUTPUT_DIRECTORIES}")

##
# Set the global include directories for xsd files.
# These include directories are from the install directory.
##
set(RIFT_GLOBAL_XSD_INCLUDE_DIRECTORIES
  ${CMAKE_INSTALL_PREFIX}/usr/data/xsd
  )

##
# Set the submodule specific xsd include directories.  These are
# currently just equivalent to yang binary directories, because we do
# not have xsd source files - only generated xsd files.
##
if(RIFT_SUBMODULE_YANG_OUTPUT_DIRECTORIES)
  set(RIFT_SUBMODULE_XSD_INCLUDE_DIRECTORIES ${RIFT_SUBMODULE_YANG_OUTPUT_DIRECTORIES})
endif()

set(RIFT_XSD_DIRS ${RIFT_SUBMODULE_XSD_INCLUDE_DIRECTORIES} ${RIFT_GLOBAL_XSD_INCLUDE_DIRECTORIES})

message("RIFT_SUBMODULE_XSD_INCLUDE_DIRECTORIES = ${RIFT_SUBMODULE_XSD_INCLUDE_DIRECTORIES}")

##
# Set the global include directories for proto files.
# These include directories are from the install directory.
##
set(RIFT_GLOBAL_PROTO_INCLUDE_DIRECTORIES
  ${CMAKE_INSTALL_PREFIX}/usr/data/proto
  )

##
# Find the submodule specific proto include directories
# The convention is to include all the directories that match
# the pattern */proto.
##
execute_process(
  COMMAND find . -type f -name *.proto
  COMMAND perl -pe "s/^..(.*)\\/.*\$/\$1/"
  COMMAND uniq
  OUTPUT_VARIABLE proto_dirs
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
if(result)
  message("Didn't find */proto in ${CMAKE_CURRENT_SOURCE_DIR}")
else()
  string(REGEX REPLACE "\n" ";" proto_dirs "${proto_dirs}")
  rift_files_prepend_path(RIFT_SUBMODULE_PROTO_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR} ${proto_dirs})
  rift_files_prepend_path(RIFT_SUBMODULE_PROTO_OUTPUT_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR} ${proto_dirs})
endif()

if(RIFT_SUBMODULE_YANG_OUTPUT_DIRECTORIES)
  # proto will be generated from yang, so must include yang output dirs
  list(APPEND RIFT_SUBMODULE_PROTO_INCLUDE_DIRECTORIES ${RIFT_SUBMODULE_YANG_OUTPUT_DIRECTORIES})
  list(APPEND RIFT_SUBMODULE_PROTO_OUTPUT_DIRECTORIES ${RIFT_SUBMODULE_YANG_OUTPUT_DIRECTORIES})
endif()

set(RIFT_PROTO_DIRS ${RIFT_SUBMODULE_PROTO_INCLUDE_DIRECTORIES} ${RIFT_GLOBAL_PROTO_INCLUDE_DIRECTORIES})

message("RIFT_SUBMODULE_PROTO_INCLUDE_DIRECTORIES = ${RIFT_SUBMODULE_PROTO_INCLUDE_DIRECTORIES}")
message("RIFT_SUBMODULE_PROTO_OUTPUT_DIRECTORIES = ${RIFT_SUBMODULE_PROTO_OUTPUT_DIRECTORIES}")

# linux package config (.pc) files location
set(RIFT_PKG_CONFIG_DIRS ${CMAKE_INSTALL_PREFIX}/usr/lib/pkgconfig)
foreach(dir ${RIFT_PKG_CONFIG_DIRS})
  set(RIFT_PKG_CONFIG_PATH "${RIFT_PKG_CONFIG_PATH}:${dir}")
endforeach(dir)


##
# VALA plugin related stuff
##
set(RIFT_VAPI_DIRS 
  ${CMAKE_INSTALL_PREFIX}/usr/share/rift/vapi
  ${CMAKE_INSTALL_PREFIX}/usr/share/vala-0.22/vapi)
set(RIFT_VALA_PKGS rwplugin-1.0 rw_types-1.0)
set(RIFT_GLOBAL_LIBPEAS_LIBRARY_DIRS ${CMAKE_INSTALL_PREFIX}/usr/lib)
set(RIFT_GLOBAL_LIBPEAS_LIBRARIES 
  peas-1.0
  gmodule-2.0
  rt
  gio-2.0
  girepository-1.0
  gobject-2.0
  glib-2.0
  )
set(RIFT_GLOBAL_LIBPEAS_INCLUDE_DIRS
  /usr/include/libpeas-1.0
  /usr/include/glib-2.0
  /usr/lib64/glib-2.0/include
  /usr/include/gobject-introspection-1.0
  ${CMAKE_INSTALL_PREFIX}/usr/lib64/libffi-3.0.5/include
  )

##
# Set the global include directories for the compiler
# These include directories are from the install directory
##
set(RIFT_GLOBAL_INCLUDE_DIRECTORIES
  /usr/include
  ${CMAKE_INSTALL_PREFIX}/usr/include
  /usr/include/libxml2
  /usr/include/redis/
  /usr/include/redis/hiredis/
  ${CMAKE_INSTALL_PREFIX}/usr/include/redis/lua/
  ${CMAKE_INSTALL_PREFIX}/usr/include/riftware
  /usr/include/dispatch
  ${CMAKE_INSTALL_PREFIX}/usr/include/c++
  /usr/include/libpeas-1.0
  ${CMAKE_INSTALL_PREFIX}/usr/include/rift/plugins
  /usr/include/glib-2.0
  /usr/lib64/glib-2.0/include
  /usr/include/gio-unix-2.0
  /usr/include/gobject-introspection-1.0
  /usr/include/pygobject-3.0
  ${CMAKE_INSTALL_PREFIX}/usr/include/zookeeper
  ${CMAKE_INSTALL_PREFIX}/usr/local/include
  ${CMAKE_INSTALL_PREFIX}/usr/include/rwut
  )
include_directories(${RIFT_GLOBAL_INCLUDE_DIRECTORIES})
message("RIFT_GLOBAL_INCLUDE_DIRECTORIES = ${RIFT_GLOBAL_INCLUDE_DIRECTORIES}")

##
# Set the submodule specific include directories
# The convention is to include all the directories that match
# the pattern */include/riftware/.
##
execute_process(
  COMMAND find ${CMAKE_CURRENT_SOURCE_DIR} -type d -path */include/riftware*
  OUTPUT_VARIABLE foo
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
if(result)
  message("Didn't find */include/riftware in ${CMAKE_CURRENT_SOURCE_DIR}")
else()
  string(REGEX REPLACE "\n" ";" foo "${foo}")
  set(RIFT_SUBMODULE_INCLUDE_DIRECTORIES ${foo})
endif()

##
# Set the submodule specific vala include directories
# The convention is to include all the directories that match
# the pattern */vala.
##
execute_process(
  COMMAND find -type d -path */vala
  OUTPUT_VARIABLE vala_dirs
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
if(result)
  message("Didn't find */vala in ${CMAKE_CURRENT_SOURCE_DIR}")
else()
  string(REGEX REPLACE "\n" ";" vala_dirs "${vala_dirs}")
  foreach(vala_dir ${vala_dirs})
    list(APPEND RIFT_SUBMODULE_INCLUDE_DIRECTORIES 
      ${CMAKE_CURRENT_BINARY_DIR}/${vala_dir})
  endforeach(vala_dir)
endif()

if(RIFT_SUBMODULE_PROTO_OUTPUT_DIRECTORIES)
  # This also effectively includes the yang output directories
  list(APPEND RIFT_SUBMODULE_INCLUDE_DIRECTORIES ${RIFT_SUBMODULE_PROTO_OUTPUT_DIRECTORIES})
endif()

if(RIFT_SUBMODULE_INCLUDE_DIRECTORIES)
  include_directories(BEFORE ${RIFT_SUBMODULE_INCLUDE_DIRECTORIES})
endif()
message("RIFT_SUBMODULE_INCLUDE_DIRECTORIES = ${RIFT_SUBMODULE_INCLUDE_DIRECTORIES}")

##
# Set the target link (library search) directories
##
set(RIFT_GLOBAL_LINK_DIRECTORIES
  /usr/lib
  /usr/lib64
  ${CMAKE_INSTALL_PREFIX}/usr/lib
  ${CMAKE_INSTALL_PREFIX}/usr/lib64
  ${CMAKE_INSTALL_PREFIX}/usr/lib/rwut
  ${CMAKE_INSTALL_PREFIX}/usr/lib/rift/plugins
  ${CMAKE_INSTALL_PREFIX}/usr/local/lib
  ${CMAKE_INSTALL_PREFIX}/usr/local/mariadb-5.5.30/lib
  ${RIFT_SUBMODULE_IPSEC_INSTALL_PREFIX}/usr/lib
  )
link_directories(${RIFT_GLOBAL_LINK_DIRECTORIES})
message("RIFT_GLOBAL_LINK_DIRECTORIES = ${RIFT_GLOBAL_LINK_DIRECTORIES}")

foreach(dir ${RIFT_GLOBAL_LINK_DIRECTORIES})
  set(RIFT_LDPATH "${RIFT_LDPATH}:${dir}")
endforeach(dir)

##
# Initialize the global variables used by build system
##
set(RIFT_RIFT_COVERAGE_TARGET_LIST CACHE INTERNAL "Coverage Target List" FORCE)
set(RIFT_DOCUMENTATION_TARGET_LIST CACHE INTERNAL "Documentation Target List" FORCE)
set(RIFT_QUICK_UNITTEST_TARGET_LIST CACHE INTERNAL "Unittest Target List" FORCE)
set(RIFT_LONG_UNITTEST_TARGET_LIST CACHE INTERNAL "Long Unittest Target List" FORCE)
set(RIFT_SYSTEMTEST_TARGET_LIST CACHE INTERNAL "Systemtest Target List" FORCE)
set(RIFT_EXTERNAL_PACKAGE_LIST CACHE INTERNAL "External Project Target List" FORCE)
set(RIFT_COMPONENT_LIST CACHE INTERNAL "CMAKE Install Component List" FORCE)

##
# rift_add_dirs(<subdir-list>)
##
macro(rift_add_subdirs)
  foreach(rift_add_dirs_subdir ${ARGN})
    if(NOT rift_add_dirs_subdir STREQUAL SUBDIR_LIST) ## compatibility with old form
      #message(STATUS "rift_add_subdirs ${CMAKE_CURRENT_SOURCE_DIR} ${rift_add_dirs_subdir}")
      add_subdirectory(${rift_add_dirs_subdir})
    endif()
  endforeach()
endmacro(rift_add_subdirs)

##
# rift_create_packages(SUBMODULE_PACKAGE_NAME <name-of-package>
#                      CMAKE_COMPONENTS <component-list>
#                      EXTERNAL_COMPONENTS <external-compoenets>)
##
macro(rift_create_packages)
  # Parse the function arguments
  set(parse_options "")
  set(parse_onevalargs SUBMODULE_PACKAGE_NAME)
  set(parse_multivalueargs CMAKE_COMPONENTS EXTERNAL_COMPONENTS)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(CPACK_COMPONENTS_ALL ${ARGS_CMAKE_COMPONENTS} ${ARGS_EXTERNAL_COMPONENTS})
  
  # Deb config
  set(CPACK_DEB_COMPONENT_INSTALL ON)
  set(CPACK_DEBIAN_PACKAGE_DEBUG "ON")
  set(CPACK_DEBIAN_PACKAGE_VENDOR ${RIFT_VENDOR_NAME})

  # RPM config
  set(CPACK_RPM_COMPONENT_INSTALL ON)
  set(CPACK_RPM_PACKAGE_DEBUG "ON")
  set(CPACK_RPM_PACKAGE_VENDOR ${RIFT_VENDOR_NAME})
  set(CPACK_RPM_COMPRESSION_TYPE gzip)
  set(CPACK_RPM_COMPRESSION_LEVEL 1)

  # Exclude the following directories from generated RPM's to avoid conflicts
  # with system RPM's.
  set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
      /usr/lib/python${RIFT_PYTHON3}
      /usr/lib/python${RIFT_PYTHON3}/site-packages
      /usr/lib64/python${RIFT_PYTHON3}
      /usr/lib64/python${RIFT_PYTHON3}/site-packages
      /usr/lib/python${RIFT_PYTHON2}
      /usr/lib/python${RIFT_PYTHON2}/site-packages
      /usr/lib64/python${RIFT_PYTHON2}
      /usr/lib64/python${RIFT_PYTHON2}/site-packages
      /etc/ssl
      ${CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION}
      )

  # In developer mode, we are using a patched version of CPackRPM.cmake that does not
  # support CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST.  Because of this, /usr ends
  # up in the rpm file list which causes conflicts with the system filesystem package.
  # Patch this version of cpack to include a find filter to exclude this directory.
  set(RIFT_CPACK_RPM_PATCH_FIND_FILTER -a -not -path \"./usr\")


  set(CPACK_GENERATOR ${RIFT_PACKAGE_GENERATOR})

  set(CPACK_PACKAGE_NAME ${ARGS_SUBMODULE_PACKAGE_NAME})
  set(CPACK_PACKAGE_VERSION ${RIFT_VERSION})
  set(CPACK_PACKAGE_RELEASE 1)
  set(CPACK_PACKAGE_CONTACT support@riftio.com)
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/rift")
  set(CPACK_OUTPUT_CONFIG_FILE "CPackConfig-${CPACK_PACKAGE_NAME}.cmake")

  foreach(external_component ${ARGS_EXTERNAL_COMPONENTS})
    # Additional directories to package are given via a pair.  The first directory
    # is the location of the installed files and the second is a relative path to
    # the build subdirectory from the submodule binary dir.

    # Package up the external project so that it contains the /usr/rift prefix for all files
    get_filename_component(ex_inst_dir "${RIFT_SUBMODULE_INSTALL_PREFIX}/${external_component}/${CMAKE_INSTALL_PREFIX}/../../" ABSOLUTE)

    list(
      APPEND CPACK_INSTALLED_DIRECTORIES
      "${ex_inst_dir};${external_component}"
      )
  endforeach(external_component)

  if(${RIFT_DEVELOPER_BUILD})
    set(pkg_name ${ARGS_SUBMODULE_PACKAGE_NAME}-install-symlink-workaround)
    get_filename_component(ex_inst_dir "${RIFT_SUBMODULE_INSTALL_PREFIX}/install-symlink-workaround/${CMAKE_INSTALL_PREFIX}/../../" ABSOLUTE)
    list(APPEND CPACK_INSTALLED_DIRECTORIES "${ex_inst_dir};${pkg_name}")
    list(APPEND CPACK_COMPONENTS_ALL "${pkg_name}")
  endif()

  include(CPack)
endmacro(rift_create_packages)

##
# This function appends a target name to external package list
##
function(rift_append_to_external_package_list)
  # Parse the function arguments
  set(parse_options "")
  set(parse_onevalargs TARGET_NAME)
  set(parse_multivalueargs)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(RIFT_EXTERNAL_PACKAGE_LIST ${RIFT_EXTERNAL_PACKAGE_LIST}
    ${ARGS_TARGET_NAME} CACHE INTERNAL "External Project Target List")

endfunction(rift_append_to_external_package_list)

##
# This function adds the top level submodule targets for
# doxygen documentation, code coverage, and unittests
##
macro(rift_add_submodule_targets)
  # Parse the function arguments
  set(parse_options "")
  set(parse_onevalargs SUBMODULE_PACKAGE_NAME)
  set(parse_multivalueargs)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  ##
  # submodule target for rift doxygen
  ##

  message("List of documentation targets: ${RIFT_DOCUMENTATION_TARGET_LIST}")
  if(RIFT_DOCUMENTATION_TARGET_LIST)
    add_custom_target(rw.doxygen DEPENDS ${RIFT_DOCUMENTATION_TARGET_LIST})
  else()
    add_custom_target(rw.doxygen)
  endif()

  ##
  # submodule target for rift unit tests
  ##
  message("List of unittest targets: ${RIFT_QUICK_UNITTEST_TARGET_LIST}")
  if(RIFT_QUICK_UNITTEST_TARGET_LIST)
    add_custom_target(rw.unittest DEPENDS ${RIFT_QUICK_UNITTEST_TARGET_LIST})
  else()
    add_custom_target(rw.unittest)
  endif()

  ##
  # submodule target for rift unit tests
  ##
  message("List of unittest targets: ${RIFT_QUICK_UNITTEST_TARGET_LIST}")
  message("List of long unittest targets: ${RIFT_LONG_UNITTEST_TARGET_LIST}")
  if(RIFT_QUICK_UNITTEST_TARGET_LIST OR RIFT_LONG_UNITTEST_TARGET_LIST)
    add_custom_target(rw.unittest_long DEPENDS ${RIFT_QUICK_UNITTEST_TARGET_LIST} ${RIFT_LONG_UNITTEST_TARGET_LIST})
  else()
    add_custom_target(rw.unittest_long)
  endif()

  ##
  # submodule target for rift system tests
  ##
  message("List of systemtest targets: ${RIFT_SYSTEMTEST_TARGET_LIST}")
  if(RIFT_SYSTEMTEST_TARGET_LIST)
    add_custom_target(rw.systemtest DEPENDS ${RIFT_SYSTEMTEST_TARGET_LIST})
  else()
    add_custom_target(rw.systemtest)
  endif()
  
  ##
  # submodule target for long rift system tests
  ##
  message("List of systemtest targets: ${RIFT_SYSTEMTEST_TARGET_LIST}")
  message("List of long systemtest targets: ${RIFT_LONG_SYSTEMTEST_TARGET_LIST}")
  if(RIFT_SYSTEMTEST_TARGET_LIST OR RIFT_LONG_SYSTEMTEST_TARGET_LIST)
    add_custom_target(rw.systemtest_long DEPENDS ${RIFT_LONG_SYSTEMTEST_TARGET_LIST} ${RIFT_SYSTEMTEST_TARGET_LIST})
  else()
    add_custom_target(rw.systemtest_long)
  endif()

  ##
  # submodule target for rift code coverage
  ##
  message("List of coverage targets: ${RIFT_COVERAGE_TARGET_LIST}")
  if(RIFT_COVERAGE_TARGET_LIST)
    add_custom_target(rw.coverage DEPENDS ${RIFT_COVERAGE_TARGET_LIST})
  else()
    add_custom_target(rw.coverage)
  endif()

  ##
  # submodule target for packaging
  ##
  add_custom_target(rw.package
    COMMAND
    # cpack --debug --verbose --config CPackConfig-${ARGS_SUBMODULE_PACKAGE_NAME}.cmake
    cpack --config CPackConfig-${ARGS_SUBMODULE_PACKAGE_NAME}.cmake
    )

  ##
  # script for populating build cache
  ##
  configure_file(
    ${RIFT_CMAKE_MODULE_DIR}/rift_build_cache.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/rift_build_cache.cmake
    ESCAPE_QUOTES @ONLY
    )

  ##
  # submodule target for build cache
  ##
  add_custom_target(rw.bcache
    COMMAND
      ${CMAKE_COMMAND} -P rift_build_cache.cmake
      -DRIFT_CMAKE_MODULE_DIR=${RIFT_CMAKE_MODULE_DIR}
    )

  if(NOT RIFT_COMPONENT_LIST)
    install(CODE "MESSAGE(\"Dummy install for ${ARGS_SUBMODULE_PACKAGE_NAME}
    submodule.\")" COMPONENT "dummy_component")
  endif()

  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/foss.txt)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/foss.txt
      DESTINATION foss/${RIFT_SUBMODULE_PREFIX}
      COMPONENT ${ARGS_SUBMODULE_PACKAGE_NAME}_foss)
    rift_set_component_package_fields("${ARGS_SUBMODULE_PACKAGE_NAME}_foss"
      DESCRIPTION "FOSS license for ${ARGS_SUBMODULE_PACKAGE_NAME}")
  endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/foss.txt)

  ##
  # This function creates the magic incantation for creating CMAKE pacakges
  # SUBMODULE_PACKAGE_NAME - The generated pacakges will be prefixed with this name
  # CMAKE_COMPONENTS - The list of CMAKE components that need packaginh
  # EXTERNAL_COMPONENTS - The list of external components that need to be packaged.
  #                       These are targets added from ExternalProject_Add function
  #
  ##

  message("CMAKE Component List: ${RIFT_COMPONENT_LIST}")
  message("CMAKE External Project List: ${RIFT_EXTERNAL_PACKAGE_LIST}")
  rift_create_packages(
    SUBMODULE_PACKAGE_NAME ${ARGS_SUBMODULE_PACKAGE_NAME}
    CMAKE_COMPONENTS ${RIFT_COMPONENT_LIST}
    EXTERNAL_COMPONENTS ${RIFT_EXTERNAL_PACKAGE_LIST})

endmacro(rift_add_submodule_targets)

##
# This function sets per-component values that are put into the package files (.deb, .rpm, etc.)
# DESCRIPTION - A one-line description for the package
#
# These named arguments provide scripting.  The same script is used for all targets, e.g. for rpm and deb.
# PRE_INSTALL_SCRIPT - name of a script to run before install.  The script basename must be preinst.
# POST_INSTALL_SCRIPT - name of a script to run after install.  The script basename must be postinst.
# PRE_RM_SCRIPT      - name of a script to run before removal.  The script basename must be prerm.
# POST_RM_SCRIPT      - name of a script to run after removal.  The script basename must be postrm.
##

function(rift_set_component_package_fields component)
  # Parse the function arguments
  set(parse_options "")
  set(parse_onevalargs DESCRIPTION PRE_INSTALL_SCRIPT POST_INSTALL_SCRIPT PRE_RM_SCRIPT POST_RM_SCRIPT)
  set(parse_multivalueargs "")
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  string(TOUPPER ${component} COMPONENT_UPPER)
  
  set(CPACK_COMPONENT_${COMPONENT_UPPER}_DESCRIPTION "${ARGS_DESCRIPTION}" PARENT_SCOPE)
  
  # Make sure the basename of the preinstall script is 'preinst' - this is required by the Debian packaging.
  if(ARGS_PRE_INSTALL_SCRIPT)
    get_filename_component(basename "${ARGS_PRE_INSTALL_SCRIPT}" NAME)
    if(NOT basename STREQUAL "preinst")
      message(FATAL_ERROR "Pre install script must be named 'preinst': ${ARGS_PRE_INSTALL_SCRIPT}")
    endif()
    
    LIST(APPEND EXTRA_SCRIPTS "${ARGS_PRE_INSTALL_SCRIPT}")
    set(CPACK_RPM_${COMPONENT_UPPER}_PRE_INSTALL_SCRIPT_FILE "${ARGS_PRE_INSTALL_SCRIPT}" PARENT_SCOPE)
    
  endif()
  
  # Make sure the basename of the postinstall script is 'postinst' - this is required by the Debian packaging.
  if(ARGS_POST_INSTALL_SCRIPT)
    get_filename_component(basename "${ARGS_POST_INSTALL_SCRIPT}" NAME)
    if(NOT basename STREQUAL "postinst")
      message(FATAL_ERROR "Post install script must be named 'postinst': ${ARGS_POST_INSTALL_SCRIPT}")
    endif()
    
    LIST(APPEND EXTRA_SCRIPTS "${ARGS_POST_INSTALL_SCRIPT}")
    set(CPACK_RPM_${COMPONENT_UPPER}_POST_INSTALL_SCRIPT_FILE "${ARGS_POST_INSTALL_SCRIPT}" PARENT_SCOPE)
    
  endif()
  
  # Make sure the basename of the prerm script is 'prerm' - this is required by the Debian packaging.
  if(ARGS_PRE_RM_SCRIPT)
    get_filename_component(basename "${ARGS_PRE_RM_SCRIPT}" NAME)
    if(NOT basename STREQUAL "prerm")
      message(FATAL_ERROR "Pre removal script must be named 'prerm': ${ARGS_PRE_RM_SCRIPT}")
    endif()
    
    LIST(APPEND EXTRA_SCRIPTS "${ARGS_PRE_RM_SCRIPT}")
    set(CPACK_RPM_${COMPONENT_UPPER}_PRE_UNINSTALL_SCRIPT_FILE "${ARGS_PRE_RM_SCRIPT}" PARENT_SCOPE)

  endif()
  
  # Make sure the basename of the postrm script is 'postrm' - this is required by the Debian packaging.
  if(ARGS_POST_RM_SCRIPT)
    get_filename_component(basename "${ARGS_POST_RM_SCRIPT}" NAME)
    if(NOT basename STREQUAL "postrm")
      message(FATAL_ERROR "Post removal script must be named 'postrm': ${ARGS_POST_RM_SCRIPT}")
    endif()
    
    LIST(APPEND EXTRA_SCRIPTS "${ARGS_POST_RM_SCRIPT}")
    set(CPACK_RPM_${COMPONENT_UPPER}_POST_UNINSTALL_SCRIPT_FILE "${ARGS_POST_RM_SCRIPT}" PARENT_SCOPE)

  endif()
  
  set(CPACK_DEBIAN_${COMPONENT_UPPER}_PACKAGE_CONTROL_EXTRA "${EXTRA_SCRIPTS}" PARENT_SCOPE)
  
endfunction(rift_set_component_package_fields)

##
# This function returns the path relative to the root of the submodule
##
function(rift_submodule_relative_source_path var source_path)
      set(retval)
      string(REPLACE ${RIFT_SUBMODULE_SOURCE_ROOT} "" retval ${source_path})
      set(${var} "${retval}" PARENT_SCOPE)
endfunction(rift_submodule_relative_source_path)

