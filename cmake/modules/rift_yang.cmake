# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu, Tom Seidenberg
# Creation Date: 2013/05/01
# 

##
# Function to get a bare yang file name.  Can't use
# get_filename_component(WE_NAME) because yang files may have dots.
# rift_get_bare_yang_filename(
#   <var>         # variable to set
#   <name>        # the filename
# )
##
function(rift_get_bare_yang_filename var name)
  get_filename_component(name ${name} NAME)
  string(REGEX REPLACE "\\.yang$" "" name "${name}")
  set(${var} "${name}" PARENT_SCOPE)
endfunction(rift_get_bare_yang_filename)


##
# function to create a yang search path.
# rift_make_yang_path(
#   <var>                                  # variable to set
#   YANG_DIRS <other-yang-import-dirs> ... # actual: CUR_SRC, CUR_BIN, YANG_DIRS, RIFT_YANG_DIRS
# )
##
include(rift_pkg_config)
function(rift_make_yang_path var)
  set(parse_options)
  set(parse_onevalargs)
  set(parse_multivalueargs YANG_DIRS)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  rift_list_to_path(yang_path
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${ARG_YANG_DIRS}
    ${RIFT_YANG_DIRS}
  )

  set(${var} ${yang_path} PARENT_SCOPE)
endfunction(rift_make_yang_path)


##
# function to create the envset.sh yang search path.
# rift_yang_envset_yang_path(
#   <var>                                  # variable to set
#   YANG_DIRS <other-yang-import-dirs> ... # actual: CUR_SRC, CUR_BIN, YANG_DIRS, RIFT_YANG_DIRS
# )
##
function(rift_yang_envset_yang_path var)
  set(parse_options)
  set(parse_onevalargs)
  set(parse_multivalueargs YANG_DIRS)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  rift_make_yang_path(yang_path YANG_DIRS ${ARG_YANG_DIRS})

  set(${var} "YUMA_MODPATH=:-${yang_path}" PARENT_SCOPE)
endfunction(rift_yang_envset_yang_path)


##
# function to create a command line for yangdump
# rift_yangdump_command(
#   var                                    # The command variable to set
#   ENVSET <arguments> ...                 # Extra envset.sh arguments
#   YANGDUMP_EXE <yangdump>                # default: INSTALL/... due to ext/yang module order
#   YANG_DIRS <other-yang-import-dirs> ... # actual: CUR_SRC, CUR_BIN, YANG_DIRS, INSTALL/usr/data/yang
#   OUT_DEPENDS_VAR <name-of-the-var>      # Variable to hold extra dependencies
#   ARGV <arguments>...                    # The arguments to yangdump
# )
function(rift_yangdump_command var)
  set(parse_options)
  set(parse_onevalargs YANGDUMP_EXE OUT_DEPENDS_VAR)
  set(parse_multivalueargs ARGV ENVSET YANG_DIRS)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(depends)
  if(NOT ARG_YANGDUMP_EXE)
    if(RIFT_SUBMODULE_NAME STREQUAL ext/yang)
      set(ARG_YANGDUMP_EXE ${RIFT_SUBMODULE_BINARY_ROOT}/yuma123-2.2.5/usr/bin/yangdump)
      list(APPEND depends yuma123-2.2.5)
    else()
      set(ARG_YANGDUMP_EXE ${CMAKE_INSTALL_PREFIX}/usr/bin/yangdump)
    endif()
  endif()

  if(NOT DEFINED ENV{YANG_ALL_WARNINGS})
    list(APPEND ARG_ARGV --warn-off=415)
  endif()

  set(${var}
    ${PROJECT_TOP_DIR}/scripts/env/envset.sh
      ${ARG_ENVSET}
      -e ${ARG_YANGDUMP_EXE} ${ARG_ARGV}
    PARENT_SCOPE)

  if(ARG_OUT_DEPENDS_VAR)
    set(${ARG_OUT_DEPENDS_VAR} ${depends} PARENT_SCOPE)
  endif()
endfunction(rift_yangdump_command)


##
# function to create a yangdump shell program
# rift_yangdump_shell(
#   target                                 # The target
#   NAME <shell-file-name>                 # The name of the shell script
#   ENVSET <arguments> ...                 # Extra envset.sh arguments
#   YANGDUMP_EXE <yangdump>                # default: INSTALL/... due to ext/yang module order
#   YANG_DIRS <other-yang-import-dirs> ... # actual: CUR_SRC, CUR_BIN, YANG_DIRS, INSTALL/usr/data/yang
#   DEPENDS <dep> ...                     # Extra dependencies.  Applied to all steps
#   ARGV <arguments>...                    # Leadning arguments to yangdump, if any
# )
function(rift_yangdump_shell target)
  set(parse_options)
  set(parse_onevalargs NAME YANGDUMP_EXE)
  set(parse_multivalueargs ARGV ENVSET YANG_DIRS DEPENDS)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  if(NOT ARG_YANGDUMP_EXE)
    if(RIFT_SUBMODULE_NAME STREQUAL ext/yang)
      set(ARG_YANGDUMP_EXE ${RIFT_SUBMODULE_BINARY_ROOT}/yuma123-2.2.5/usr/bin/yangdump)
    else()
      set(ARG_YANGDUMP_EXE ${CMAKE_INSTALL_PREFIX}/usr/bin/yangdump)
    endif()
  endif()

  add_custom_command(
    OUTPUT ${ARG_NAME}
    COMMAND rm -f ${ARG_NAME}
    COMMAND echo "\\#!/bin/bash" > ${ARG_NAME}
    COMMAND echo "${PROJECT_TOP_DIR}/scripts/env/envset.sh \\\\" >> ${ARG_NAME}
    COMMAND echo "  ${ARG_ENVSET} \\\\" >> ${ARG_NAME}
    COMMAND echo "  ${ARG_YANGDUMP_EXE} ${ARG_ARGV} \\\\" >> ${ARG_NAME}
    COMMAND echo "  \\\"\\\$\$@\\\"" >> ${ARG_NAME}
    COMMAND chmod 755 ${ARG_NAME}
  )

  add_custom_target(${target} DEPENDS ${ARG_NAME} ${ARG_DEPENDS})

endfunction(rift_yangdump_shell)


##
# function to create a command line for yangpbc
# rift_yangpbc_command(
#   var                                    # The command variable to set
#   YANGPBC_EXE <yangpbc>                  # default: INSTALL/...
#   YANG_DIRS <other-yang-import-dirs> ... # actual: CUR_SRC, CUR_BIN, YANG_DIRS, INSTALL/usr/data/yang
#   OUT_DEPENDS_VAR <name-of-the-var>      # Variable to hold extra dependencies
#   ARGV <arguments>...                    # The arguments to yangpbc
# )
function(rift_yangpbc_command var)
  set(parse_options)
  set(parse_onevalargs YANGPBC_EXE OUT_DEPENDS_VAR)
  set(parse_multivalueargs ARGV YANG_DIRS)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(depends)
  if(NOT ARG_YANGPBC_EXE)
    if(RIFT_SUBMODULE_NAME STREQUAL core/util)
      set(ARG_YANGPBC_EXE ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/yangpbc/yangpbc)
      list(APPEND depends yangpbc)
    else()
      set(ARG_YANGPBC_EXE ${CMAKE_INSTALL_PREFIX}/usr/bin/yangpbc)
      list(APPEND depends ${ARG_YANGPBC_EXE})
    endif()
  endif()

  set(${var}
    ${ARG_YANGPBC_EXE} ${ARG_ARGV}
    PARENT_SCOPE)

  if(ARG_OUT_DEPENDS_VAR)
    set(${ARG_OUT_DEPENDS_VAR} ${depends} PARENT_SCOPE)
  endif()
endfunction(rift_yangpbc_command)


##
# function to create a yangpbc shell program
# rift_yangpbc_shell(
#   target                                 # The target
#   NAME <shell-file-name>                 # The name of the shell script
#   ENVSET <arguments> ...                 # Extra envset.sh arguments
#   YANGPBC_EXE <yangpbc>                  # default: INSTALL/...
#   YANG_DIRS <other-yang-import-dirs> ... # actual: CUR_SRC, CUR_BIN, YANG_DIRS, INSTALL/usr/data/yang
#   DEPENDS <dep> ...                      # Extra dependencies.  Applied to all steps
#   ARGV <arguments>...                    # Leadning arguments to yangpbc, if any
# )
function(rift_yangpbc_shell target)
  set(parse_options)
  set(parse_onevalargs NAME YANGPBC_EXE)
  set(parse_multivalueargs ARGV ENVSET YANG_DIRS DEPENDS)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  if(NOT ARG_YANGPBC_EXE)
    if(RIFT_SUBMODULE_NAME STREQUAL core/util)
      set(ARG_YANGPBC_EXE ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/yangpbc/yangpbc)
      list(APPEND depends yangpbc pb2c)
    else()
      set(ARG_YANGPBC_EXE ${CMAKE_INSTALL_PREFIX}/usr/bin/yangpbc)
    endif()
  endif()

  add_custom_command(
    OUTPUT ${ARG_NAME}
    COMMAND rm -f ${ARG_NAME}
    COMMAND echo "\\#!/bin/bash" > ${ARG_NAME}
    COMMAND echo "${PROJECT_TOP_DIR}/scripts/env/envset.sh \\\\" >> ${ARG_NAME}
    COMMAND echo "  ${ARG_ENVSET} \\\\" >> ${ARG_NAME}
    COMMAND echo "  ${ARG_YANGPBC_EXE} ${ARG_ARGV} \\\\" >> ${ARG_NAME}
    COMMAND echo "  \\\"\\\$\$@\\\"" >> ${ARG_NAME}
    COMMAND chmod 755 ${ARG_NAME}
  )

  add_custom_target(${target} DEPENDS ${ARG_NAME} ${ARG_DEPENDS})

endfunction(rift_yangpbc_shell)


##
# function to create a command line for pyang
# rift_pyang_command(
#   var                                    # The command variable to set
#   ENVSET <arguments> ...                 # Extra envset.sh arguments
#   PYANG_EXE <pyang>                      # default: INSTALL/... due to ext/yang module order
#   YANG_DIRS <other-yang-import-dirs> ... # actual: CUR_SRC, CUR_BIN, OTHER_DIRS, YANG_DIRS, INSTALL/usr/data/yang
#   OUT_DEPENDS_VAR <name-of-the-var>      # Variable to hold extra dependencies
#   ARGV <arguments>...                    # The arguments to pyang
# )
function(rift_pyang_command var)
  set(parse_options)
  set(parse_onevalargs PYANG_EXE OUT_DEPENDS_VAR)
  set(parse_multivalueargs ARGV ENVSET YANG_DIRS)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(depends)
  if(NOT ARG_PYANG_EXE)
    if(RIFT_SUBMODULE_NAME STREQUAL ext/yang)
      set(ARG_PYANG_EXE ${RIFT_SUBMODULE_BINARY_ROOT}/pyang/pyang/pyang-build/bin/pyang)
      list(APPEND depends pyang)
    else()
      set(ARG_PYANG_EXE pyang)
    endif()
  endif()

  rift_make_yang_path(yang_path_arg YANG_DIRS ${ARG_YANG_DIRS})

  if(NOT DEFINED ENV{YANG_ALL_WARNINGS})
    list(INSERT ARG_ARGV 0 --ignore-warning=UNUSED_IMPORT)
  endif()

  set(${var}
    ${PROJECT_TOP_DIR}/scripts/env/envset.sh
      ${ARG_ENVSET}
      -e ${ARG_PYANG_EXE} "--path=${yang_path_arg}" ${ARG_ARGV}
    PARENT_SCOPE
  )
  if(ARG_OUT_DEPENDS_VAR)
    set(${ARG_OUT_DEPENDS_VAR} ${depends} PARENT_SCOPE)
  endif()
endfunction(rift_pyang_command)

function(rift_pyang_command_parallel var)
  set(parse_options)
  set(parse_onevalargs PYANG_EXE OUT_DEPENDS_VAR)
  set(parse_multivalueargs YANG_FILES YANG_DIRS)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(depends)
  if(NOT ARG_PYANG_EXE)
    if(RIFT_SUBMODULE_NAME STREQUAL ext/yang)
      list(APPEND depends pyang)
    endif()
  endif()

  rift_make_yang_path(yang_path_arg YANG_DIRS ${ARG_YANG_DIRS})

  ## Flag to check whether to enable all warnings.
  set(all_warnings 0)
  if (DEFINED ENV{YANG_ALL_WARNINGS})
    set(all_warnings 1)
  endif()

  set(${var}
    ${PROJECT_TOP_DIR}/scripts/env/pyang_parallel.sh
      -b ${CMAKE_CURRENT_BINARY_DIR}
      -w ${all_warnings}
      -p ${yang_path_arg}
      -- ${ARG_YANG_FILES}
    PARENT_SCOPE
  )
  if(ARG_OUT_DEPENDS_VAR)
    set(${ARG_OUT_DEPENDS_VAR} ${depends} PARENT_SCOPE)
  endif()
endfunction(rift_pyang_command_parallel)


##
# function to create a pyang shell program
# rift_pyang_shell(
#   target                                 # The target
#   NAME <shell-file-name>                 # The name of the shell script
#   ENVSET <arguments> ...                 # Extra envset.sh arguments
#   PYANG_EXE <pyang>                      # default: INSTALL/... due to ext/yang module order
#   YANG_DIRS <other-yang-import-dirs> ... # actual: CUR_SRC, CUR_BIN, YANG_DIRS, INSTALL/usr/data/yang
#   DEPENDS <dep> ...                      # Extra dependencies.  Applied to all steps
#   ARGV <arguments>...                    # Leadning arguments to pyang, if any
# )
function(rift_pyang_shell target)
  set(parse_options)
  set(parse_onevalargs NAME PYANG_EXE)
  set(parse_multivalueargs ARGV ENVSET YANG_DIRS DEPENDS)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  if(NOT ARG_PYANG_EXE)
    if(RIFT_SUBMODULE_NAME STREQUAL ext/yang)
      set(ARG_PYANG_EXE ${RIFT_SUBMODULE_BINARY_ROOT}/pyang/pyang/pyang-build/bin)
    else()
      set(ARG_PYANG_EXE pyang)
    endif()
  endif()

  rift_make_yang_path(yang_path_arg YANG_DIRS ${ARG_YANG_DIRS})

  add_custom_command(
    OUTPUT ${ARG_NAME}
    COMMAND rm -f ${ARG_NAME}
    COMMAND echo "\\#!/bin/bash" > ${ARG_NAME}
    COMMAND echo "${PROJECT_TOP_DIR}/scripts/env/envset.sh \\\\" >> ${ARG_NAME}
    COMMAND echo "  ${ARG_ENVSET} \\\\" >> ${ARG_NAME}
    COMMAND echo "  ${ARG_PYANG_EXE} --path=${yang_path_arg} ${ARG_ARGV} \\\\" >> ${ARG_NAME}
    COMMAND echo "  \\\"\\\$\$@\\\"" >> ${ARG_NAME}
    COMMAND chmod 755 ${ARG_NAME}
  )

  add_custom_target(${target} DEPENDS ${ARG_NAME} ${ARG_DEPENDS})

endfunction(rift_pyang_shell)


##
# function to create a command line for confdc
# rift_confdc_command(
#   var                                    # The command variable to set
#   ENVSET <arguments> ...                 # Extra envset.sh arguments
#   CONFDC_EXE <confdc>                    # default: INSTALL/... due to ext/mgmt module order
#   OUT_DEPENDS_VAR <name-of-the-var>      # Variable to hold extra dependencies
#   ARGV <arguments>...                    # The arguments to confdc
# )
function(rift_confdc_command var)
  set(parse_options)
  set(parse_onevalargs CONFDC_EXE OUT_DEPENDS_VAR)
  set(parse_multivalueargs ARGV ENVSET YANG_DIRS)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(depends)
  if(NOT ARG_CONFDC_EXE)
    if(RIFT_SUBMODULE_NAME STREQUAL ext/mgmt)
      set(ARG_CONFDC_EXE ${RIFT_SUBMODULE_BINARY_ROOT}/confd/bin/confdc)
      list(APPEND depends confd-5.3)
    else()
      set(ARG_CONFDC_EXE ${CMAKE_INSTALL_PREFIX}/usr/local/confd/bin/confdc)
    endif()
  endif()

  set(${var}
    ${PROJECT_TOP_DIR}/scripts/env/envset.sh
      ${ARG_ENVSET}
      -e ${ARG_CONFDC_EXE} ${ARG_ARGV}
    PARENT_SCOPE
  )
  if(ARG_OUT_DEPENDS_VAR)
    set(${ARG_OUT_DEPENDS_VAR} ${depends} ${ARG_CONFDC_EXE}  PARENT_SCOPE)
  endif()
endfunction(rift_confdc_command)


##
# function to create a confdc shell program
# rift_confdc_shell(
#   target                                 # The target
#   NAME <shell-file-name>                 # The name of the shell script
#   ENVSET <arguments> ...                 # Extra envset.sh arguments
#   CONFDC_EXE <confdc>                    # default: INSTALL/... due to ext/mgmt module order
#   DEPENDS <dep> ...                      # Extra dependencies.  Applied to all steps
#   ARGV <arguments>...                    # Leadning arguments to confdc, if any
# )
function(rift_confdc_shell target)
  set(parse_options)
  set(parse_onevalargs NAME CONFDC_EXE)
  set(parse_multivalueargs ARGV ENVSET YANG_DIRS DEPENDS)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  if(NOT ARG_CONFDC_EXE)
    if(RIFT_SUBMODULE_NAME STREQUAL ext/mgmt)
      set(ARG_CONFDC_EXE ${RIFT_SUBMODULE_BINARY_ROOT}/confd/bin/confdc)
    else()
      set(ARG_CONFDC_EXE ${CMAKE_INSTALL_PREFIX}/usr/local/confd/bin/confdc)
    endif()
  endif()

  add_custom_command(
    OUTPUT ${ARG_NAME}
    COMMAND rm -f ${ARG_NAME}
    COMMAND echo "\\#!/bin/bash" > ${ARG_NAME}
    COMMAND echo "${PROJECT_TOP_DIR}/scripts/env/envset.sh \\\\" >> ${ARG_NAME}
    COMMAND echo "  ${ARG_ENVSET} \\\\" >> ${ARG_NAME}
    COMMAND echo "  ${ARG_CONFDC_EXE} ${ARG_ARGV} \\\\" >> ${ARG_NAME}
    COMMAND echo "  \\\"\\\$\$@\\\"" >> ${ARG_NAME}
    COMMAND chmod 755 ${ARG_NAME}
  )

  add_custom_target(${target} DEPENDS ${ARG_NAME} ${ARG_DEPENDS})

endfunction(rift_confdc_shell)


##
# function generate output files from .yang.  Typical usage:
#
#   rift_add_yang_target(
#     TARGET <target>
#     YANG_FILES <yang_file1> ...
#     COMPONENT <install_component>
#   )
#
#
# Complete documentation:
#
# rift_add_yang_target(
#
#   # The primary target that causes all the generated files to be
#   # built.  You must specify this argument.  Furthermore, in any rule
#   # that uses the output files directly (via the OUT_ arguments), you
#   # MUST ALSO specify the target as a dependency.  From the cmake
#   # manual, add_custom_target, in regards to custom command output
#   # files:
#   #   Do not list the output in more than one independent target that
#   #   may build in parallel or the two instances of the rule may
#   #   conflict (instead use add_custom_target to drive the command
#   #   and make the other targets depend on that one).
#
#   TARGET <target>
#
#   # The list of .yang files that serve as the inputs to this command.
#   # If they are not full paths, they are assumed to be relative to
#   # the current source dir.
#   #
#   # Order is important.  When one .yang in YANG_FILES imports another
#   # .yang in YANG_FILES, the import is translated into an equivalent
#   # include/import statement in several of the generated output files.
#   # Some of those output files are used as inputs for secondary
#   # generated output files.  Those secondary output files must have
#   # dependencies on their input files.  Therefore, the rule creates an
#   # additional dependency for secondary files, where each secondary
#   # file depends on the previous secondary file of the same type (in
#   # YANG_FILES order), in addition to whatever other dependencies it
#   # requires.  So long as YANG_FILES is in a correct order, the build
#   # will be sequenced correctly for import/include statements.  The
#   # .yang format does not allow circular import statements, so it is
#   # always possible to specify an order that satisfies all
#   # dependencies.
#
#   YANG_FILES <yang-file> ...
#
#   # The list of dependencies.  In general, this should only contain
#   # the target from other rift_add_yang_target() invocations in the
#   # same module, and only those which have .yang files imported by
#   # .yang files in this target.  Dependencies are created
#   # automatically for the compilers.  Dependencies on rwyang, rwlib,
#   # yuma, confd, libxml or xerces are superfluous.
#
#   DEPENDS <dep> ...
#
#
#   # The list of other libraries to link with the generated GI library.
#
#   LIBRARIES <dep> ...
#
#
#   # Output control options.  These options control which files get
#   # generated.  The default is to generate all files except (.pb2c.c
#   # .pb2c.h).  In the future, .xsd will also be suppressed, replaced
#   # with .dsdl and related files.
#   #
#   # In general, you should not need to specify any of these unless you
#   # want very fine control over the outputs.
#
#   WITHOUT_PROTO     # Suppress .proto .ypbc.cpp .ypbc.h .pb-c.c .pb-c.h .pb2c.c .pb2c.h .py
#   WITHOUT_DSDL      # Suppress .dsdl
#   WITHOUT_CONFD     # Suppress .fxs .confd.h
#   WITHOUT_GI        # Suppress GI
#   WITHOUT_LIB       # Suppress shared library from C files
#
#   # ATTN: Need options to generate -*.rng, -*.sch, -*.dsrl, -gdefs.rng.
#
#
#   # Output directory and extra include paths to search.  The comments
#   # indicate the resulting full search path in each case.  In general,
#   # you should not need to specify any of these unless some of your
#   # imported .yang files are generated into a non-standard place.
#
#   SRC_DIR <source-dir>                       # def: CUR_SRC
#   DEST_DIR <destination-dir>                 # def: CUR_BIN
#   OTHER_DIRS <other-import-dirs-for-all> ... # actual: CUR_SRC, CUR_BIN, OTHER_DIRS
#   YANG_DIRS <other-yang-import-dirs> ...     # actual: CUR_SRC, CUR_BIN, OTHER_DIRS, YANG_DIRS, INSTALL/usr/data/yang
#   PROTO_DIRS <other-proto-import-dirs> ...   # actual: CUR_SRC, CUR_BIN, OTHER_DIRS, PROTO_DIRS, INSTALL/usr/data/proto
#
#
#   # Install components.  If any COMPONENT is set, the files will be
#   # installed against the specified component.  If no component is
#   # set and NO_INSTALL is also not set, cmake will generate an error.
#   # Suppressing installation is really only useful for unit tests and
    # compiler testing.
#
#   NO_INSTALL              # Do not install any files. Incompatible with COMPONENT*
#   NO_INSTALL_YANG         # Do not install the yang files. They were probably already installed.
#   COMPONENT <comp>        # Install all outputs to comp
#
#   # Output variables, to set in the calling context.  When set, allows
#   # the caller of the function to get the file lists out to use them
#   # for other things (such as compiling .c files to a library).
#
#   OUT_XSD_FILES_VAR <name-of-the-var>       # Variable to hold .xsd file list
#   OUT_C_FILES_VAR <name-of-the-var>         # Variable to hold *.c file list
# )
function(rift_add_yang_target)
  set(parse_options
    EXCLUDE_RWYANG_PB_LIBRARY
    NO_INSTALL
    NO_INSTALL_YANG
    WITHOUT_PROTO
    WITHOUT_LIB
    WITHOUT_GI
    WITHOUT_DSDL
    WITHOUT_CONFD
  )
  set(parse_onevalargs
    TARGET
    SRC_DIR
    DEST_DIR
    COMPONENT
    OUT_XSD_FILES_VAR
    OUT_C_FILES_VAR
  )
  set(parse_multivalueargs
    YANG_FILES
    DEPENDS
    LIBRARIES
    GIR_PKGS
    GIR_PATHS
    LIBRARY_PATHS
    OTHER_DIRS
    YANG_DIRS
    PROTO_DIRS
  )
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  ########################################
  # Basic validation and setup

  #message(STATUS "RAYT: rift_add_yang_target(TARGET ${ARG_TARGET} ...)")
  if(NOT ARG_TARGET)
    message(FATAL "Must specify TARGET")
  endif()

  #message(STATUS "RAYT: ${ARG_TARGET}: inst: '${ARG_COMPONENT}'")
  # Default component (should come after WITH defaults!)
  if(ARG_NO_INSTALL AND ARG_COMPONENT)
    message(FATAL "NO_INSTALL and COMPONENT arguments are exclusive")
  endif()

  if(ARG_WITHOUT_PROTO)
    set(WITHOUT_GI TRUE)
  endif()

  #message(STATUS "RAYT: ${ARG_TARGET}: without: '${ARG_WITHOUT_PROTO}' '${ARG_WITHOUT_LIB}' "
  #               "'${ARG_WITHOUT_GI}' '${ARG_WITHOUT_DSDL}' '${ARG_WITHOUT_CONFD}'")

  ########################################
  # Setup library info - whether or not the rule will build the library.

  set(library_name ${ARG_TARGET}_gen)
  set(library_pkg_name ${ARG_TARGET}_ylib)
  set(library_libs ${ARG_LIBRARIES}
    ${CMAKE_INSTALL_PREFIX}/usr/lib/libpthread_workqueue.so
    ${CMAKE_INSTALL_PREFIX}/usr/lib/libkqueue.so
    ${CMAKE_INSTALL_PREFIX}/usr/lib/libprotobuf-c.so
    ${CMAKE_INSTALL_PREFIX}/usr/lib/libglib-2.0.so
    ${CMAKE_INSTALL_PREFIX}/usr/lib/libgobject-2.0.so
  )
  set(library_depends ${ARG_DEPENDS} ${library_libs})

  if(RIFT_SUBMODULE_NAME STREQUAL core/util)
    list(APPEND library_libs
      rwlib
      rwyang
      rwtypes
      rw_schema_pb
    )
  else()
    list(APPEND library_libs
      ${CMAKE_INSTALL_PREFIX}/usr/lib/librwlib.so
      ${CMAKE_INSTALL_PREFIX}/usr/lib/librwyang.so
      ${CMAKE_INSTALL_PREFIX}/usr/lib/rift/plugins/librwtypes.so
      ${CMAKE_INSTALL_PREFIX}/usr/lib/librw_schema_pb.so
    )
  endif()

  ########################################
  # Setup GI info - whether or not the rule will create GI files.
  set(GI_VERSION 1.0)

  # Create a list of GIR package names (with versions) which should not be added
  # as dependencies for any of the generated GIRs.
  set(exclude_gir_pkgs
    YumaTypesYang-1.0
    YumaAppCommonYang-1.0
    YumaNcxYang-1.0
    YangdumpYang-1.0
  )

  # ATTN: Should these just be moved to core/util?
  # Create a list of modules that parse in core/util, but live elsewhere
  if(RIFT_SUBMODULE_NAME STREQUAL core/util)
    set(phantom_core_util
      ietf-netconf
      ietf-inet-types
      ietf-yang-types
    )
  else()
    set(phantom_core_util)
  endif()

  ########################################
  # Generate list of bare file names.

  set(bare_yang_list)
  foreach(yang_file ${ARG_YANG_FILES})
    rift_get_bare_yang_filename(bare "${yang_file}")
    #message(STATUS "RAYT: bare: '${yang_file}' '${bare}'")
    list(APPEND bare_yang_list ${bare})
  endforeach()

  ########################################
  # Build various complete search paths
  if(NOT ARG_SRC_DIR)
    set(ARG_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  if(NOT ARG_DEST_DIR)
    set(ARG_DEST_DIR ${CMAKE_CURRENT_BINARY_DIR})
  endif()

  set(other_dirs)
  if(ARG_OTHER_DIRS)
    list(APPEND other_dirs ${ARG_OTHER_DIRS})
  endif()

  if(NOT ARG_SRC_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT ARG_SRC_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
      list(APPEND other_dirs ${ARG_SRC_DIR})
    endif()
  endif()

  if(NOT ARG_DEST_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
    list(APPEND other_dirs ${ARG_DEST_DIR})
  endif()
  #message(STATUS "RAYT: ${ARG_TARGET}: od: '${other_dirs}'")

  ########################################
  set(c_file_list)
  set(xsd_file_list)

  set(target_depends)
  set(target_h_depends)
  set(target_schema_depends)
  set(target_gi_depends)

  set(all_dependent_mods)

  set(all_yang_files)
  foreach(yfile ${ARG_YANG_FILES})
    if(NOT yfile MATCHES "^/")
      set(yfile ${ARG_SRC_DIR}/${yfile})
    endif()
    list(APPEND all_yang_files ${yfile})
  endforeach()

  include_directories(${CMAKE_CURRENT_BINARY_DIR})

  rift_pyang_command_parallel(pycommand
    YANG_DIRS ${ARG_YANG_DIRS} ${other_dirs}
    OUT_DEPENDS_VAR yang_depends_depends
    YANG_FILES ${all_yang_files}
    )

  #message(STATUS "RAYT: pycommand ${pycommand}")
  execute_process(
    COMMAND ${pycommand}
    RESULT_VARIABLE exitstatus
  )

  if(exitstatus)
    message(FATAL_ERROR "Error: Failed to compute Yang dependencies")
  endif()

  foreach(yang_file ${ARG_YANG_FILES})
    set(confd_h_file)
    set(fxs_file)
    set(dsdl_file)
    set(idsdl_file)
    set(proto_file)
    set(ypb_c_file)
    set(ypb_h_file)
    set(ypb_gi_c_file)
    set(pbc_c_file)
    set(pbc_h_file)
    set(bare_pbc_gi_h_file)
    set(bare_ypb_gi_h_file)
    set(doc_api_html_file)
    set(doc_api_txt_file)
    set(doc_user_html_file)
    set(doc_user_txt_file)

    if(NOT yang_file MATCHES "^/")
      set(yang_file ${ARG_SRC_DIR}/${yang_file})
    endif()

    rift_get_bare_yang_filename(name "${yang_file}")
    set(yang_meta_file "${CMAKE_CURRENT_BINARY_DIR}/${name}.meta_info.txt")

    if(ARG_COMPONENT AND NOT ARG_NO_INSTALL_YANG)
      install(FILES ${yang_file} DESTINATION usr/data/yang COMPONENT ${ARG_COMPONENT})
    endif()
    set(ypb_base ${ARG_DEST_DIR}/${name})

    set(schema_target ${name}.yang-schema)
    set(schema_depends ${ARG_DEPENDS} ${yang_file})

    set(library_target ${library_name}.lib-tgt)
    set(vapi_target ${name}.vapi-tgt)

    set(yangpbc_target ${name}.yang-ypb)
    set(yangpbc_depends ${ARG_DEPENDS} ${yang_file})

    set(protocc_target ${name}.yang-pbc)
    set(protocc_depends ${ARG_DEPENDS} ${yangpbc_target})

    if(ARG_WITHOUT_GI)
      set(gi_target)
      # Add the schema outputs to the library depends.
    else()
      set(gi_target ${name}.yang-gi)
    endif()
    set(gi_depends ${ARG_DEPENDS} ${yangpbc_target} ${protocc_target} ${library_name})

    set(all_target ${name}.yang-all)

    list (APPEND library_depends ${yangpbc_target} ${protocc_target})

    # get the package name for the yang file
    execute_process(
      COMMAND ${PROJECT_TOP_DIR}/scripts/util/yang_package.py ${yang_file}
      RESULT_VARIABLE result
      OUTPUT_VARIABLE gi_namespace_prefix
    )
    if(result)
      message(FATAL_ERROR "Error: failed to get the package name for ${yang_file}")
    endif()
    #message(STATUS "RAYT: ${name}: '${yang_file}' '${ARG_COMPONENT}'")

    set(gi_namespace ${gi_namespace_prefix}Yang)
    set(gi_namespace_version ${gi_namespace}-${GI_VERSION})
    rift_uncamel(${gi_namespace} gi_vapi_namespace)

    set(gi_gir_pkgs
      ${ARG_GIR_PKGS}
      ProtobufC-1.0
      RwTypes-1.0
      RwSchemaProto-1.0
      RwKeyspec-1.0
      RwYangPb-1.0
      RwYang-1.0
    )
    set(gi_gir_paths ${ARG_GIR_PATHS})
    set(gi_libraries ${library_name})
    set(gi_library_paths ${ARG_LIBRARY_PATHS})

    if(RIFT_SUBMODULE_NAME STREQUAL core/util)
      list(APPEND gi_library_paths
        ${RIFT_SUBMODULE_BINARY_ROOT}/rwlib/src
        ${RIFT_SUBMODULE_BINARY_ROOT}/rwtypes/src
        ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/lib
        ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/proto
        ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/yang
      )
      list(APPEND gi_gir_paths
        ${RIFT_SUBMODULE_BINARY_ROOT}/rwlib/src
        ${RIFT_SUBMODULE_BINARY_ROOT}/rwtypes/src
        ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/lib
        ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/proto
        ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/yang
      )
      list (APPEND gi_depends
        RwSchemaProto_gi
        RwKeyspec_gi
        RwYangPb_gi
        RwYang_gi
        rwtypes_gi
        ProtobufC_gi
      )
    endif()

    # ATTN: This also needs to run during build, to catch yang changes!
    set(dependency_file ${ypb_base}.yang.depends)

    file (READ ${dependency_file} yang_depends)
    string (STRIP "${yang_depends}" yang_depends)
    list (APPEND schema_depends ${yang_depends_depends} ${yang_depends})
    list (APPEND yangpbc_depends ${yang_depends_depends} ${yang_depends})

    # Check each computed dep to see if it depends on a yang file from
    # a different submodule.
    foreach (other_yang_file ${yang_depends})
      rift_get_bare_yang_filename(other_name "${other_yang_file}")
      #message(STATUS "RAYT: ${name}: other: '${other_yang_file}' '${other_name}'")

      if (NOT other_name STREQUAL name)
        rift_camel(${other_name} other_gi_namespace_prefix)
        set(other_gi_namespace_version ${other_gi_namespace_prefix}Yang-${GI_VERSION})
        set(other_schema_target ${other_name}.yang-schema)
        set(other_yangpbc_target ${other_name}.yang-ypb)
        set(other_protocc_target ${other_name}.yang-pbc)
        set(other_gi_target ${other_name}.yang-gi)

        set(same_module FALSE)
        set(same_rule FALSE)
        list (FIND bare_yang_list ${other_name} pos)
        if (${pos} EQUAL -1)
          # Not in the arguments list; try th phantom list
          list (FIND phantom_core_util ${other_name} pos)
          if (${pos} EQUAL -1)
            # Not in the argument or phantom list; try comparing submodule path
            string (FIND ${other_yang_file} ${RIFT_SUBMODULE_NAME} pos)
            if (${pos} EQUAL -1)
              # From another submodule
            else()
              # Same submodule
              set(same_module TRUE)
            endif()
          else()
            # In the phantom list; must be same submodule (which must be core/util)
            set(same_module TRUE)
          endif()
        else()
          # In the same arguments list
          set(same_module TRUE)
          set(same_rule TRUE)
        endif()

        if (NOT same_module)
          ## Not the same submodule and not same rule.
          #message(STATUS "RAYT: ${name}: use pkg_config: '${other_name}'")

          if(NOT ARG_EXCLUDE_RWYANG_PB_LIBRARY)
            list(FIND all_dependent_mods ${other_name} pos)
            #message(STATUS "RAYT: ${name}: find name: '${other_name}' in [${all_dependent_mods}] = '${pos}'")
            if (${pos} EQUAL -1)
              list (APPEND all_dependent_mods ${other_name})
              rift_pkg_check_modules (${other_name}_pkg ${other_name}>=1.0)
              #message(STATUS "RAYT: ${name}: pc results: '${other_name}' '${${other_name}_pkg_FOUND}' '${${other_name}_pkg_LIBRARIES}'")
              if (${other_name}_pkg_FOUND EQUAL 1)
                set (dep_lib_name ${${other_name}_pkg_LIBRARIES})
                list(FIND library_libs ${dep_lib_name} pos)
                #message(STATUS "RAYT: ${name}: find lib: '${dep_lib_name}' in [${library_libs}] = '${pos}'")
                if (${pos} EQUAL -1)
                  #message(STATUS "RAYT: ${name}: ${library_name} add lib: '${dep_lib_name}'")
                  list (APPEND library_libs ${dep_lib_name})
                endif()
              endif()
            endif()
          endif()
        else()
          ## Same submodule or same rule
          #message(STATUS "RAYT: ${name}: depends on other parse: '${other_name}'")
          list (APPEND protocc_depends ${other_yangpbc_target})
          list (APPEND library_depends ${other_yangpbc_target} ${other_protocc_target})
          list (APPEND schema_depends ${other_schema_target})
          list (APPEND gi_depends ${other_gi_target})

          list(FIND target_h_depends ${other_yangpbc_target} pos)
          if (${pos} EQUAL -1)
            list (APPEND target_h_depends ${other_yangpbc_target})
          endif()

          list(FIND target_h_depends ${other_protocc_target} pos)
          if (${pos} EQUAL -1)
            list (APPEND target_h_depends ${other_protocc_target})
          endif()

          list(FIND target_schema_depends ${other_schema_target} pos)
          if (${pos} EQUAL -1)
            list (APPEND target_schema_depends ${other_schema_target})
          endif()
        endif()

        # Automatically add to GI paths and depends based on the
        # generated yang dependencies, excluding those packages that
        # are in the exclude_gir_pkgs, above.  If the GIR dependency is
        # in the same submodule, then add the gir binary path the build
        # dependency target.

        list(FIND exclude_gir_pkgs ${other_gi_namespace_version} pos)
        if (${pos} EQUAL -1)

          list(FIND gi_gir_pkgs ${other_gi_namespace_version} pos)
          if (${pos} EQUAL -1)
            list(APPEND gi_gir_pkgs ${other_gi_namespace_version})
          endif()

          # If the gir module is in the same submodule
          get_filename_component(dir ${other_yang_file} DIRECTORY)
          if ("${dir}" MATCHES "${RIFT_SUBMODULE_SOURCE_ROOT}.*")
            string(REPLACE "${RIFT_SUBMODULE_SOURCE_ROOT}" "" rel_dir "${dir}")
            set(bin_dir ${RIFT_SUBMODULE_BINARY_ROOT}${rel_dir})

            # Only add the path if it doesn't already exist in the list
            list(FIND gi_gir_paths ${bin_dir} pos)
            if (${pos} EQUAL -1)
              list (APPEND gi_gir_paths ${bin_dir})
            endif()
          endif()
        endif()
      endif()
    endforeach()

    # Produce the intermediate dsdl rng: .yang->.dsdl?
    if(NOT ARG_WITHOUT_DSDL)
      set(idsdl_file ${ARG_DEST_DIR}/${name}.i.dsdl)
      set(dsdl_file ${ARG_DEST_DIR}/${name}.dsdl)
      rift_pyang_command(command
        YANG_DIRS ${ARG_YANG_DIRS} ${other_dirs}
        OUT_DEPENDS_VAR extra_depends
        ARGV ${yang_file} -f dsdl --dsdl-no-dublin-core -o ${idsdl_file}
      )
      add_custom_command(
        OUTPUT ${idsdl_file}
        COMMAND ${command}
        DEPENDS ${schema_depends} ${extra_depends}
      )
      add_custom_command(
        OUTPUT ${dsdl_file}
        COMMAND xmllint --output ${dsdl_file} --format ${idsdl_file}
        DEPENDS ${schema_depends} ${idsdl_file}
      )
      if(ARG_COMPONENT)
        install(FILES ${dsdl_file} DESTINATION usr/data/yang COMPONENT ${ARG_COMPONENT})
      endif()
      #message(STATUS "RAYT: ${name}: dsdl: '${dsdl_file}' '${idsdl_file}'")
    endif()

    # Produce .fxs: .yang->.fxs?
    # ATTN: conditional?: Produce .confd.h: .fxs->.confd.h?
    if(NOT ARG_WITHOUT_CONFD AND RIFT_AGENT_BUILD MATCHES "^CONFD_(BASIC|FULL)$")
      set(fxs_file ${ARG_DEST_DIR}/${name}.fxs)
      set(confd_h_file ${ARG_DEST_DIR}/${name}.confd.h)

      get_filename_component(tailf_yang_file ${yang_file} DIRECTORY)
      set(tailf_yang_file ${tailf_yang_file}/${name}.tailf.yang)
      set(annotate_args)
      set(annotate_depends)

      # ATTN: This dependency needs to occur at BUILD TIME!
      if(EXISTS ${tailf_yang_file})
        set(annotate_args -a ${tailf_yang_file})
        set(annotate_depends ${tailf_yang_file})
      endif()

      rift_make_yang_path(confdc_yang_path YANG_DIRS ${ARG_YANG_DIRS} ${other_dirs})
      rift_confdc_command(command
        OUT_DEPENDS_VAR extra_depends
        ARGV ${annotate_args} -c --yangpath ${confdc_yang_path} -o ${fxs_file} -- ${yang_file}
      )
      add_custom_command(
        OUTPUT ${fxs_file}
        COMMAND ${command}
        DEPENDS ${annotate_depends} ${schema_depends} ${extra_depends}
      )

      if(ARG_COMPONENT)
        install(FILES ${fxs_file} DESTINATION usr/data/yang COMPONENT ${ARG_COMPONENT})
      endif()

      rift_confdc_command(command
        OUT_DEPENDS_VAR extra_depends
        ARGV --emit-h ${confd_h_file} --macro-prefix ${name} ${fxs_file}
      )
      add_custom_command(
        OUTPUT ${confd_h_file}
        COMMAND ${command}
        COMMAND sed -i -e 's/Source: \\/.*\\//Source: /' ${confd_h_file}
        DEPENDS ${fxs_file} ${schema_depends} ${extra_depends}
      )
      #message(STATUS "RAYT: ${name}: confd: '${fxs_file}' '${confd_h_file}'")
    endif()

    if(ARG_COMPONENT)
      # Install CLI manifest files if any
      get_filename_component(base_path ${yang_file} DIRECTORY)
      set(cli_manifest_file ${base_path}/${name}.cli.xml)

      if(EXISTS ${cli_manifest_file})
        install(FILES ${cli_manifest_file} DESTINATION usr/data/yang COMPONENT ${ARG_COMPONENT})
      endif()

      set(cli_ssi_file ${base_path}/${name}.cli.ssi.sh)
      if(EXISTS ${cli_ssi_file})
        install(FILES ${cli_ssi_file} DESTINATION usr/data/yang COMPONENT ${ARG_COMPONENT})
      endif()
    endif()

    # Compile the yang: .yang->.xsd
    set(xsd_file ${ARG_DEST_DIR}/${name}.xsd)
    rift_yangdump_command(command
      YANG_DIRS ${ARG_YANG_DIRS} ${other_dirs}
      OUT_DEPENDS_VAR extra_depends
      ARGV ${yang_file} --format=xsd --defnames=true
        "--output=${ARG_DEST_DIR}" --versionnames=false --xsd-schemaloc=.
    )
    add_custom_command(
      OUTPUT ${xsd_file}
      COMMAND ${command}
      DEPENDS ${schema_depends} ${extra_depends}
    )
    list(APPEND xsd_file_list ${xsd_file})
    if(ARG_COMPONENT)
      install(FILES ${xsd_file} DESTINATION usr/data/xsd COMPONENT ${ARG_COMPONENT})
    endif()
    #message(STATUS "RAYT: ${name}: xsd: '${xsd_file}' [${xsd_file_list}]")

    # Compile the yang: .yang->.proto?
    if(NOT ARG_WITHOUT_PROTO)
      set(proto_file ${ypb_base}.proto)
      set(ypb_h_file ${ypb_base}.ypbc.h)
      set(ypb_c_file ${ypb_base}.ypbc.cpp)
      set(ypb_gi_c_file ${ypb_base}.ypbc.gi.c)
      set(bare_ypb_gi_h_file ${name}.ypbc.h)
      set(doc_user_txt_file ${ypb_base}.doc-user.txt)
      set(doc_user_html_file ${ypb_base}.doc-user.html)
      set(doc_api_txt_file ${ypb_base}.doc-api.txt)
      set(doc_api_html_file ${ypb_base}.doc-api.html)

      rift_yangpbc_command(command
        YANG_DIRS ${ARG_YANG_DIRS} ${other_dirs}
        OUT_DEPENDS_VAR extra_depends
        ARGV ${ypb_base} ${yang_file}
      )

      # workaround for RIFT-5169, RIFT-5171 and RIFT-4892
      set(workaround "YUMA_MODPATH=${CMAKE_CURRENT_SOURCE_DIR}:${CMAKE_CURRENT_BINARY_DIR}:$ENV{YUMA_MODPATH}")
      add_custom_command(
        OUTPUT ${proto_file}
          ${ypb_h_file} ${ypb_c_file} ${ypb_gi_c_file}
          ${doc_user_txt_file} ${doc_api_txt_file}
          ${doc_user_html_file} ${doc_api_html_file}
        COMMAND ${workaround} ${command}
        DEPENDS ${yangpbc_depends} ${extra_depends}
      )
      list(APPEND doc_file_list ${doc_user_txt_file})
      list(APPEND doc_file_list ${doc_user_html_file})
      list(APPEND doc_file_list ${doc_api_txt_file})
      list(APPEND doc_file_list ${doc_api_html_file})
      list(APPEND c_file_list ${ypb_c_file})
      list(APPEND c_file_list ${ypb_gi_c_file})
      if(ARG_COMPONENT)
        install(FILES ${proto_file} DESTINATION usr/data/proto COMPONENT ${ARG_COMPONENT})
        install(FILES ${ypb_h_file} DESTINATION usr/include COMPONENT ${ARG_COMPONENT})
        install(
          FILES ${doc_user_txt_file} ${doc_api_txt_file} ${doc_user_html_file} ${doc_api_html_file}
          DESTINATION usr/data/yang
          COMPONENT ${ARG_COMPONENT}
        )
      endif()

      # Compile the protobuf(s): .proto->.pb-c.*,.dso?
      set(proto_cmd_args)
      set(pbc_base ${ARG_DEST_DIR}/${name})
      # ATTN: hack file names - should come from properly refactored proto rule
      set(pbc_c_file ${pbc_base}.pb-c.c)
      set(pbc_h_file ${pbc_base}.pb-c.h)
      set(bare_pbc_gi_h_file ${name}.pb-c.h)

      list(APPEND c_file_list ${pbc_c_file})
      list(APPEND proto_cmd_args WITH_GI)

      if(ARG_COMPONENT)
        list(APPEND proto_cmd_args PROTO_COMPONENT ${ARG_COMPONENT})
        list(APPEND proto_cmd_args PCCH_COMPONENT ${ARG_COMPONENT})
      endif()
      #message(STATUS "RAYT: ${name}: pbc: '${ARG_COMPONENT}' '${proto_cmd_args}'")

      rift_add_proto_target(
        ${proto_cmd_args}
        PROTO_FILES ${proto_file}
        DEST_DIR ${ARG_DEST_DIR}
        SRC_DIR ${ARG_DEST_DIR}
        OTHER_DIRS ${other_dirs}
        PROTO_DIRS ${ARG_PROTO_DIRS}
        DEPENDS ${protocc_depends}
      )

      ##
      # Build the GI files?
      ##
      if(NOT ARG_WITHOUT_GI)
        #message(STATUS "RAYT: ${name}: gi: '${gi_namespace}' '${gi_namespace_prefix}' '${gi_vapi_namespace}'")
        #message(STATUS "RAYT: ${name}: gi pkgs: [${gi_gir_pkgs}]")
        #message(STATUS "RAYT: ${name}: gi libs: [${gi_libraries}]")
        #message(STATUS "RAYT: ${name}: gi paths: [${gi_library_paths}]")
        #message(STATUS "RAYT: ${gi_target}: [${gi_depends}]")
        rift_add_introspection(${gi_target}
          NAMESPACE ${gi_namespace}
          IDENTIFIER_PREFIX
            ${gi_namespace_prefix}
            rwpb_gi_${gi_namespace_prefix}_
            rw_ypbc_gi_${gi_namespace_prefix}_
          VAPI_PREFIX ${gi_vapi_namespace}
          PACKAGES ${gi_gir_pkgs}
          VERSION ${GI_VERSION}
          GI_INCLUDE_HFILES ${bare_ypb_gi_h_file} ${bare_pbc_gi_h_file}
          GIR_PATHS ${gi_gir_paths}
          HFILES ${ypb_h_file} ${pbc_h_file}
          LIBRARIES ${gi_libraries}
          LIBRARY_PATHS ${gi_library_paths}
          DEPENDS ${gi_depends}
        )

        rift_install_vala_artifacts(
          HEADER_FILES ${bare_ypb_gi_h_file} ${bare_pbc_gi_h_file}
          COMPONENT ${ARG_COMPONENT}
          GIR_FILES ${gi_namespace}-${GI_VERSION}.gir
          TYPELIB_FILES ${gi_namespace}-${GI_VERSION}.typelib
          VAPI_FILES ${gi_vapi_namespace}-${GI_VERSION}.vapi
          LUA_OVERRIDES ${gi_namespace}.lua
          DEST_PREFIX .
        )

        rift_symlink_python_pbcm_override(${gi_namespace})

        list (APPEND target_gi_depends ${gi_target})
      endif()
    endif()

    add_custom_target(${schema_target}
      DEPENDS
        ${schema_depends}
        ${confd_h_file} ${fxs_file}
        ${dsdl_file} ${idsdl_file}
        ${xsd_file}
    )
    add_dependencies(${schema_target} ${schema_depends})
    list (APPEND target_schema_depends ${schema_target})
    #message(STATUS "RAYT: ${schema_target}: [${schema_depends}]'")

    add_custom_target(${yangpbc_target}
      DEPENDS
        ${yangpbc_depends}
        ${ypb_h_file} ${ypb_c_file} ${ypb_gi_c_file}
        ${doc_user_txt_file} ${doc_api_txt_file}
        ${doc_user_html_file} ${doc_api_html_file}
    )
    add_dependencies(${yangpbc_target} ${yangpbc_depends})
    list (APPEND target_h_depends ${yangpbc_target})
    #message(STATUS "RAYT: ${yangpbc_target}: [${yangpbc_depends}]")

    add_custom_target(${protocc_target}
      DEPENDS
        ${protocc_depends}
        ${pbc_c_file} ${pbc_h_file}
    )
    add_dependencies(${protocc_target} ${protocc_depends})
    list (APPEND target_h_depends ${protocc_target})
    #message(STATUS "RAYT: ${protocc_target}: [${protocc_depends}]")

    add_custom_target(${all_target}
      DEPENDS
        ${schema_target}
        ${yangpbc_target}
        ${protocc_target}
        ${gi_target}
    )
    list (APPEND target_depends ${all_target})

    if(NOT ARG_WITHOUT_LIB)
      rift_make_pc(${name}
        LIBS -l${library_name}
        COMPONENT ${library_pkg_name}-1.0)
    else()
      rift_make_pc(${name}
        COMPONENT ${library_pkg_name}-1.0)
    endif()

    add_custom_target(${vapi_target} DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${name}.pc)

    set(rift_meta_cmd ${PROJECT_TOP_DIR}/cmake/modules/rwyangmeta.py)

    if(ARG_COMPONENT AND NOT ARG_NO_INSTALL_YANG)
      add_custom_command(
        OUTPUT ${yang_meta_file}
        COMMAND
          ${rift_meta_cmd} ${name} ${name}
            ${gi_namespace}-${GI_VERSION}
            ${gi_vapi_namespace}-${GI_VERSION}
            ${library_name}
            ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${schema_target}
          ${gi_target}
          ${library_target}
          ${rift_meta_cmd}
          ${vapi_target}
      )

      set(meta_target "${name}-meta-tgt")

      add_custom_target(
        ${meta_target} ALL
        DEPENDS ${yang_meta_file})

      install(FILES ${yang_meta_file} DESTINATION usr/data/yang COMPONENT ${ARG_COMPONENT})
   endif ()

  endforeach(yang_file)


  ########################################
  # Generate a library

  if(NOT ARG_WITHOUT_LIB)
    ##
    # Create library with all the source files, install it, and create the package-config
    ##
    add_library(${library_name} SHARED ${c_file_list})
    target_link_libraries(${library_name} ${library_libs})
    add_dependencies(${library_name} ${library_depends})
    install(TARGETS ${library_name} LIBRARY
      DESTINATION usr/lib COMPONENT ${ARG_COMPONENT})
    list(APPEND target_depends ${library_name})
    add_custom_target(${library_target} DEPENDS ${library_name})
  endif()


  ########################################
  # Export lists
  #message(STATUS "RAYT: ${ARG_TARGET}: var: '${ARG_OUT_XSD_FILES_VAR}' '${ARG_OUT_C_FILES_VAR}'")

  if(ARG_OUT_XSD_FILES_VAR)
    set(${ARG_OUT_XSD_FILES_VAR} ${xsd_file_list} PARENT_SCOPE)
  endif()

  if(ARG_OUT_C_FILES_VAR)
    set(${ARG_OUT_C_FILES_VAR} ${c_file_list} PARENT_SCOPE)
  endif()

  ########################################
  # Create the primary targets
  #message(STATUS "RAYT: ${ARG_TARGET}: [${target_depends}]")
  #message(STATUS "RAYT: ${ARG_TARGET}.schema: [${target_schema_depends}]")
  #message(STATUS "RAYT: ${ARG_TARGET}.headers: [${target_h_depends}]")
  #message(STATUS "RAYT: ${ARG_TARGET}.gi: [${target_gi_depends}]")

  add_custom_target(${ARG_TARGET} ALL DEPENDS ${target_depends})
  add_custom_target(${ARG_TARGET}.schema DEPENDS ${target_schema_depends})
  add_custom_target(${ARG_TARGET}.headers DEPENDS ${target_h_depends})
  add_custom_target(${ARG_TARGET}.gi DEPENDS ${target_gi_depends})

endfunction(rift_add_yang_target)

##
# This function creates install rules for artifacts generated from the yang files
# rift_install_yang_artifacts(COMPONENT <component>
#                             YANG_FILES <yang-files> ...
#                             XSD_FILES <xsd-files> ...
#                             PROTO_FILES <proto-files>)
##
function(rift_install_yang_artifacts)
  ##
  # Parse the function arguments
  ##
  set(parse_options "")
  set(parse_onevalargs COMPONENT)
  set(parse_multivalueargs YANG_FILES XSD_FILES PROTO_FILES)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  ##
  # Produce install rules for each type of file
  ##
  if(ARGS_COMPONENT)
    if(ARGS_YANG_FILES)
      install(FILES ${ARGS_YANG_FILES} DESTINATION usr/data/yang
        COMPONENT ${ARGS_COMPONENT})
    endif(ARGS_YANG_FILES)

    if(ARGS_XSD_FILES)
      install(FILES ${ARGS_XSD_FILES} DESTINATION usr/data/xsd
        COMPONENT ${ARGS_COMPONENT})
    endif(ARGS_XSD_FILES)

    if(ARGS_PROTO_FILES)
      install(FILES ${ARGS_PROTO_FILES} DESTINATION usr/data/proto
        COMPONENT ${ARGS_COMPONENT})
    endif(ARGS_PROTO_FILES)

  else(ARGS_COMPONENT)
    if(ARGS_YANG_FILES)
      install(FILES ${ARGS_YANG_FILES} DESTINATION usr/data/yang)
    endif(ARGS_YANG_FILES)

    if(ARGS_XSD_FILES)
      install(FILES ${ARGS_XSD_FILES} DESTINATION usr/data/xsd)
    endif(ARGS_XSD_FILES)

    if(ARGS_PROTO_FILES)
      install(FILES ${ARGS_PROTO_FILES} DESTINATION usr/data/proto)
    endif(ARGS_PROTO_FILES)
  endif(ARGS_COMPONENT)
endfunction(rift_install_yang_artifacts)

##
# This function generates the tree, jstree and hypertree representations
# of yang file.
# rift_gen_yang_tree(<TARGET>
#                    OUTFILE_PREFIX <ouput-file-prefix>
#                    OUTFILE_DIR <output-file-dir>
#                    YANG_FILES <yang-files> ...
#                    DEPENDS <dep> ...)
#
# This generates the following files
# ${ARGS_OUTFILE_DIR}/${ARGS_OUTFILE_PREFIX}.tree.txt
# ${ARGS_OUTFILE_DIR}/${ARGS_OUTFILE_PREFIX}.hypertree.xml
# ${ARGS_OUTFILE_DIR}/${ARGS_OUTFILE_PREFIX}.jstree.html
##
function(rift_gen_yang_tree TARGET)
  set(parse_options "")
  set(parse_onevalargs OUTFILE_PREFIX OUTFILE_DIR)
  set(parse_multivalueargs YANG_FILES DEPENDS)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  configure_file(
    ${PROJECT_TOP_DIR}/cmake/modules/rift_pyang.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/rift_pyang.cmake
    ESCAPE_QUOTES @ONLY
    )

  if(NOT ARGS_OUTFILE_DIR)
    set(ARGS_OUTFILE_DIR ${CMAKE_INSTALL_PREFIX}/documentation/config/yang)
  endif()

  set(OUTFILE_LIST)
  list(APPEND OUTFILE_LIST ${ARGS_OUTFILE_DIR}/${ARGS_OUTFILE_PREFIX}.hypertree.xml)
  list(APPEND OUTFILE_LIST ${ARGS_OUTFILE_DIR}/${ARGS_OUTFILE_PREFIX}.tree.txt)
  list(APPEND OUTFILE_LIST ${ARGS_OUTFILE_DIR}/${ARGS_OUTFILE_PREFIX}.jstree.html)

  unset(YANG_PATH)
  foreach(dir ${RIFT_YANG_DIRS})
    if(YANG_PATH)
      set(YANG_PATH ${YANG_PATH}:${dir})
    else()
      set(YANG_PATH ${dir})
    endif()
  endforeach(dir)

  add_custom_command(
    OUTPUT ${OUTFILE_LIST}
    COMMAND
      ${CMAKE_COMMAND}
        -DYANG_SOURCE_FILES="${ARGS_YANG_FILES}"
        -DOUTFILE_PREFIX=${ARGS_OUTFILE_PREFIX}
        -DYANG_PATH=${YANG_PATH}
        -DOUTFILE_DIR=${ARGS_OUTFILE_DIR}
        -P rift_pyang.cmake
    DEPENDS ${ARGS_DEPENDS}
    )

  add_custom_target(${TARGET} ALL
    DEPENDS ${OUTFILE_LIST}
    )

endfunction(rift_gen_yang_tree)


##
# function generate output files from .yang.  Typical usage:
#
#   rift_build_yang_module(
#     TARGET <target>
#     YANG_FILES <yang_file1> ...
#     DEPENDS <yang_module_name1> ...
#     COMPONENT <install_component>
#   )
#
#   Complete documentation:
# rift_build_yang_module(
#   TARGET <target>
#
#   # The list of .yang files that serve as the inputs to this command.
#   # If the YANG file is not fully qualified, it is assumed to be from
#   # the current source directory
#   #
#
#   YANG_FILES <yang-file> ...
#
#   # The list of dependencies. These are yang modules in the same submodule,
#   # but not listed as a YANG file.
#
#   DEPENDS <yang_module_name1>...
#
#   # Private YANG files denotes that the files are not visible through
#   # CLI or other Management clients
#
#   INSTALL [NO | PRIVATE | PUBLIC] # def: PUBLIC
#   WITHOUT_CONFD     # Suppress .fxs .confd.h
#   COMPONENT <comp>        # Install all outputs to comp
# )

function(rift_symlink_python_pbcm_override NAMESPACE)
  set(dir ${CMAKE_INSTALL_PREFIX}/usr/lib64/python${RIFT_PYTHON3}/site-packages/gi/overrides)

  execute_process(
    COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/gi/overrides)

  execute_process(
    COMMAND ln -snf
    ${PROJECT_TOP_DIR}/rwbase/python/rift/gi/pbcm_override.py
    ${CMAKE_CURRENT_BINARY_DIR}/gi/overrides/${NAMESPACE}.py)

  install(DIRECTORY DESTINATION ${dir})
  install(DIRECTORY DESTINATION ${RIFT_SUBMODULE_INSTALL_PREFIX}/install-symlink-workaround/${dir})
  install(CODE "execute_process(
    COMMAND ln -snf
    ../../rift/gi/pbcm_override.py ${dir}/${NAMESPACE}.py)")


  install(CODE "execute_process(
    COMMAND ln -snf
      ../../rift/gi/pbcm_override.py
      ${RIFT_SUBMODULE_INSTALL_PREFIX}/install-symlink-workaround/${dir}/${NAMESPACE}.py)")

  if (RIFT_SUPPORT_PYTHON2)
    set(dir ${CMAKE_INSTALL_PREFIX}/usr/lib64/python${RIFT_PYTHON2}/site-packages/gi/overrides)

    install(DIRECTORY DESTINATION ${dir})
    install(DIRECTORY DESTINATION ${RIFT_SUBMODULE_INSTALL_PREFIX}/install-symlink-workaround/${dir})

    install(CODE "execute_process(
      COMMAND ln -snf
      ../../rift/gi/pbcm_override.py ${dir}/${NAMESPACE}.py)")

    install(CODE "execute_process(
      COMMAND ln -snf
        ../../rift/gi/pbcm_override.py
        ${RIFT_SUBMODULE_INSTALL_PREFIX}/install-symlink-workaround/${dir}/${NAMESPACE}.py)")
  endif()
endfunction()


