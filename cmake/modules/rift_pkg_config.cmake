
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(FindPkgConfig)

include(rift_globals)

# Generic wrapper around making reasonable pkg-config files.  If you are building a
# riftware package, see rift_make_pc().
#
# Parameters -- Default value in [] where relevant.
#
# @param PKG                - Name of the package
# @param VERSION            - Version of the package
# @param PREFIX             - Prefix for paths [${RIFT_INSTALL}]
# @param LIBDIR             - Path for libraries relative to PREFIX [${RIFT_LIBDIR}]
# @param INCLUDEDIR         - Path for include files relative to PREFIX [${RIFT_INCLUDEDIR}]
# @param DESC               - Package description
# @param REQUIRES           - Package requirements.  These are the names of other pkg-config files
#                             that should be loaded as anyone using this package will also need them.
# @param REQUIRES_PRIVATE   - Similar to REQUIRES but only necessary when linking statically.
# @param CFLAGS             - Additional CFLAGS to pass aside from the include directory.
# @param LDFLAGS            - Additional LDFLAGS to pass aside from the library directory.
# @param LIBS               - List of library names to be linked against.  These should be in the format:
#                             "-l${libraryA} -l${libraryB}"
# @param INCLUDEDIRS        - List of extra directories that should be added to the search path in
#                             addition to INCLUDEDIR. The '-I' prefix will be added to each.
# @param COMPONENT          - Passed to the install command.
function(rift_make_pc_ext PKG)
  set(parse_singleargs VERSION PREFIX LIBDIR INCLUDEDIR DESC COMPONENT)
  set(parse_multiargs REQUIRES REQUIRES_PRIVATE CFLAGS LDFLAGS LIBS INCLUDEDIRS)
  cmake_parse_arguments(ARGS "" "${parse_singleargs}" "${parse_multiargs}" ${ARGN})

  if (NOT ARGS_VERSION)
    message(FATAL_ERROR "rift_make_pc_ext missing VERSION parameter")
  endif()

  if (NOT ARGS_PREFIX)
    set(ARGS_PREFIX ${RIFT_INSTALL})
  endif()

  if (NOT ARGS_DESC)
    message(FATAL_ERROR "rift_make_pc_ext missing DESC parameter")
  endif()

  if (NOT ARGS_LIBDIR)
    set(ARGS_LIBDIR ${RIFT_LIBDIR})
  endif()

  if (NOT ARGS_INCLUDEDIR)
    set(ARGS_INCLUDEDIR ${RIFT_INCLUDE_DIR})
  endif()

  if (NOT ARGS_COMPONENT)
    message(FATAL_ERROR "rift_make_pc_ext:  no COMPONENT specified")
  endif()


  set(dest ${CMAKE_CURRENT_BINARY_DIR}/${PKG}.pc)
  file(WRITE ${dest} "prefix=${ARGS_PREFIX}\n")
  file(APPEND ${dest} "exec_prefix=\${prefix}\n")
  file(APPEND ${dest} "libdir=\${prefix}/${ARGS_LIBDIR}\n")
  file(APPEND ${dest} "includedir=\${prefix}/${ARGS_INCLUDEDIR}\n")
  file(APPEND ${dest} "\n")
  file(APPEND ${dest} "Name: ${PKG}\n")
  file(APPEND ${dest} "Description: ${ARGS_DESC}\n")
  file(APPEND ${dest} "Version: ${ARGS_VERSION}\n")
  file(APPEND ${dest} "Requires: ${ARGS_REQUIRES}\n")
  file(APPEND ${dest} "Requires.private ${ARGS_REQUIRES_PRIVATE}\n")

  file(APPEND ${dest} "Cflags: ${ARGS_CFLAGS} -I\${includedir}")
  foreach(dir ${ARGS_INCLUDEDIRS})
    file(APPEND ${dest} " -I${dir}")
  endforeach()
  file(APPEND ${dest} "\n")

  file(APPEND ${dest} "Libs: ${ARGS_LDFLAGS} ${ARGS_LIBS} -L\${libdir}\n")

  install(FILES ${dest}
    DESTINATION usr/${RIFT_LIBDIR}/pkgconfig
    COMPONENT ${ARGS_COMPONENT})
endfunction()



# Create a pkg-config file for a rift package.  This will handle setting
# appropriate paths for the library and include directories based on the
# CMAKE_INSTALL_PREFIX.
#
# @param PKG              - Name of package
# @param VERSION          - Package version
# @param REQUIRES         - List of packages used directly by this package.  Note that the package
#                           names should map to other pkg-config files
# @param REQUIRES_PRIVATE - List of packages used indirectly by this package.  These packages are
#                           only linked against when statically compiling
# @param CFLAGS           - List of flags to pass to the compiler when using this package.  This does
#                           not need to include a list of "-I" flags.
# @param LIBS             - List of flags to pass to the linker when using this package.  This should
#                           include "-lX" where X is a library owned by this package.
# @param INCLUDEDIRS      - List of extra directories that should be added to the search path in
#                           addition to INCLUDEDIR. The '-I' prefix will be added to each.
# @param COMPONENT        - Passed to the install command.
# @param LIBDIR           - Set the library directory relative to ${RIFT_INSTALL}.  [Default ${RIFT_LIBDIR}]
function(rift_make_pc PKG)
  set(parse_singleargs VERSION COMPONENT LIBDIR)
  set(parse_multiargs REQUIRES REQUIRES_PRIVATE CFLAGS LIBS INCLUDEDIRS)
  cmake_parse_arguments(ARGS "" "${parse_singleargs}" "${parse_multiargs}" ${ARGN})

  if (NOT ARGS_VERSION)
    set(ARGS_VERSION 1.0)
  endif()

  if (NOT ARGS_COMPONENT)
    message(FATAL_ERROR "rift_make_pc:  no COMPONENT specified")
  endif()

  if (NOT ARGS_LIBDIR)
    set(ARGS_LIBDIR ${RIFT_LIBDIR})
  endif()

  set(dest ${CMAKE_CURRENT_BINARY_DIR}/${PKG}.pc)
  file(WRITE ${dest} "prefix=${RIFT_INSTALL}\n")
  file(APPEND ${dest} "exec_prefix=\${prefix}\n")
  file(APPEND ${dest} "libdir=\${prefix}/${ARGS_LIBDIR}\n")
  file(APPEND ${dest} "includedir=\${prefix}/${RIFT_INCLUDEDIR}\n")

  if (ARGS_YANG_MODULES)
    file(APPEND ${dest} "yang_modules=")
    foreach(ymod ${ARGS_YANG_MODULES})
      file(APPEND ${dest} "${ymod} ")
    endforeach()
    file(APPEND ${dest} "\n")
  endif()
  file(APPEND ${dest} "\n")

  file(APPEND ${dest} "Name: ${PKG}\n")
  file(APPEND ${dest} "Description: Rift.io package ${PKG}\n")
  file(APPEND ${dest} "Version: ${ARGS_VERSION}\n")
  file(APPEND ${dest} "Requires: ${ARGS_REQUIRES}\n")
  file(APPEND ${dest} "Requires.private ${ARGS_REQUIRES_PRIVATE}\n")

  file(APPEND ${dest} "Cflags: ${ARGS_CFLAGS} -I\${includedir}")
  foreach(dir ${ARGS_INCLUDEDIRS})
    file(APPEND ${dest} " -I${dir}")
  endforeach()
  file(APPEND ${dest} "\n")

  file(APPEND ${dest} "Libs: ${ARGS_LIBS} -L\${libdir}\n")

  install(FILES ${dest}
    DESTINATION usr/${RIFT_LIBDIR}/pkgconfig
    COMPONENT ${ARGS_COMPONENT})
endfunction()

# Parse pkg-config files.  This is a wrapper around the CMake pkg_check_modules
# which changes behavior in two ways.  First, it only accepts a single module
# at a time.  Second, there is special handling of libraries when the module
# was built locally (is in the ${CMAKE_INSTALL_PREFIX}.  Rather than just use
# library names for linking, each is converted into a full path.  This
# guarantees that any consumers will be relinked if the library is rebuilt.
#
#
# Parameters:
# PREFIX    - Prefix to use for each variable set, see variables below
# REQUIRED  - If set and the module is not found a fatal error will be raised
# QUIET     - If set, no status messages will be printed
# MODULE    - The module (package) to search for.  Version strings as used by
#             pkg-config are accepted.  For instance 'pkg >= 1.2.3' or
#             'pkg <= 0.5'.
#
# Variables:
# On success the following variables will be set.
# <PREFIX>_FOUND          - 1 if the module was found
# <PREFIX>_LIBRARIES      - only the libraries (w/o the '-l')
# <PREFIX>_LIBRARY_DIRS   - the paths of the libraries (w/o the '-L')
# <PREFIX>_LDFLAGS        - all required linker flags
# <PREFIX>_LDFLAGS_OTHER  - all other linker flags
# <PREFIX>_INCLUDE_DIRS   - the '-I' preprocessor flags (w/o the '-I')
# <PREFIX>_CFLAGS         - all required cflags
# <PREFIX>_CFLAGS_OTHER   - the other compiler flags
#
function(rift_pkg_check_modules PREFIX)
  set(opts REQUIRED QUIET)
  cmake_parse_arguments(ARGS "${opts}" "" "" ${ARGN})
  set(module ${ARGS_UNPARSED_ARGUMENTS})
  set(args)

  if (${ARGS_REQUIRED})
    list(APPEND args REQUIRED)
  endif()

  if (${ARGS_QUIET})
    list(APPEND args QUIET)
  endif()

  list(LENGTH module module_len)
  if (NOT ${module_len} EQUAL 1)
    message(FATAL_ERROR "Only one module can be passed to rift_pkg_check_modules at a time: ${module} ${module_len}")
  endif()

  pkg_check_modules(${PREFIX} ${args} ${module})
  string(REGEX REPLACE "[=><].*" "" module_name ${module})

  if (EXISTS ${RIFT_INSTALL_LIBDIR}/pkgconfig/${module_name}.pc)
    # If this is a module that was built by us then we need to be sure
    # to return the full path to each library rather than just the library
    # name and link directory.  This is to make sure that CMake will relink
    # when the module is updated.

    set(full_libs)
    foreach(lib ${${PREFIX}_LIBRARIES})
      set(found FALSE)

      foreach(libdir ${${PREFIX}_LIBRARY_DIRS})
        if (EXISTS ${libdir}/lib${lib}.so)
          list(APPEND full_libs ${libdir}/lib${lib}.so)
          set(found TRUE)
          break()
        endif()

      endforeach()

      if (NOT ${found})
        if (NOT ARGS_QUIET)
          message(WARNING "rift_pkg_check_modules(${module}) failed to find ${lib}")
        endif()
        list(APPEND full_libs ${lib})
      endif()
    endforeach()

    set(${PREFIX}_LIBRARIES ${full_libs} PARENT_SCOPE)

    execute_process(
      COMMAND pkg-config --variable yang_modules ${module_name}
      OUTPUT_VARIABLE pc_yang
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE pc_yang_rc
      ERROR_VARIABLE pc_yang_err)

    if (NOT ${pc_yang_rc} EQUAL 0)
      message(FATAL_ERROR "Failed to get yang_modules variable from ${module_name}: ${pc_yang_err}")
    endif()

    set(${PREFIX}_YANG ${pc_yang} PARENT_SCOPE)

  endif()
endfunction()

