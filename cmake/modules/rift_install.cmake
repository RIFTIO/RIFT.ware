# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 2014/02/18
# 

include(CMakeParseArguments)

##
# Override the CMAKE install
# This is done to create symlinks in install directory
# instead of copying files.
##
macro(install) 
  # Parse the function arguments
  set(parse_options ARCHIVE LIBRARY RUNTIME FRAMEWORK BUNDLE PRIVATE_HEADER
    PUBLIC_HEADER RESOURCE)
  set(parse_onevalargs DESTINATION EXPORT COMPONENT)
  set(parse_multivalueargs FILES TARGETS PERMISSIONS)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  # If component is specified capture this into a global variable
  if(ARGS_COMPONENT) 
    set(found 0)
    foreach(comp ${RIFT_COMPONENT_LIST})
      if("${comp}" MATCHES "${ARGS_COMPONENT}")
        set(found 1)
      endif()
    endforeach(comp)
    if(NOT found)
      set(RIFT_COMPONENT_LIST ${RIFT_COMPONENT_LIST}
        ${ARGS_COMPONENT} CACHE INTERNAL "CMAKE Install Component List")
    endif()
  endif()

  if(NOT_DEVELOPER_BUILD MATCHES "TRUE"
      OR NOT_DEVELOPER_BUILD MATCHES "true"
      OR NOT_DEVELOPER_BUILD MATCHES "True"
      OR NOT_DEVELOPER_BUILD MATCHES "1")
    _install(${ARGN})
  else()
    set(ex_args)

    if(ARGS_ARCHIVE)
      list(APPEND ex_args ARCHIVE)
    endif()

    if(ARGS_LIBRARY)
      list(APPEND ex_args LIBRARY)
    endif()

    if(ARGS_RUNTIME)
      list(APPEND ex_args RUNTIME)
    endif()

    if(ARGS_FRAMEWORK)
      list(APPEND ex_args FRAMEWORK)
    endif()

    if(ARGS_BUNDLE)
      list(APPEND ex_args BUNDLE)
    endif()

    if(ARGS_PRIVATE_HEADER)
      list(APPEND ex_args PRIVATE_HEADER)
    endif()

    if(ARGS_PUBLIC_HEADER)
      list(APPEND ex_args PUBLIC_HEADER)
    endif()

    if(ARGS_RESOURCE)
      list(APPEND ex_args RESOURCE)
    endif()

    if(ARGS_EXPORT)
      list(APPEND ex_args EXPORT ${ARGS_EXPORT})
    endif()

    if(ARGS_PERMISSIONS)
      list(APPEND ex_args PERMISSIONS ${ARGS_PERMISSIONS})
    endif()

    if(ARGS_FILES)
      foreach(fpath ${ARGS_FILES})
        get_filename_component(fname ${fpath} NAME)
        if(IS_ABSOLUTE ${fpath})
          set(absolute_fpath ${fpath})
        else()
          set(absolute_fpath ${CMAKE_CURRENT_SOURCE_DIR}/${fpath})
        endif()
        _install(CODE "execute_process(COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/${ARGS_DESTINATION}/)")
        _install(CODE "execute_process(COMMAND rm -f ${CMAKE_INSTALL_PREFIX}/${ARGS_DESTINATION}/${fname})")
        _install(CODE "execute_process(COMMAND ln -srf ${absolute_fpath} ${CMAKE_INSTALL_PREFIX}/${ARGS_DESTINATION}/${fname})")

        _install(
          FILES ${fpath}
          DESTINATION
            ${RIFT_SUBMODULE_INSTALL_PREFIX}/install-symlink-workaround/${CMAKE_INSTALL_PREFIX}/${ARGS_DESTINATION}
          ${ex_args})
      endforeach(fpath)
    elseif(ARGS_TARGETS)
      foreach(t ${ARGS_TARGETS})
        get_property(tpath TARGET ${t} PROPERTY "LOCATION")
        get_filename_component(tname ${tpath} NAME)
        _install(CODE "execute_process(COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/${ARGS_DESTINATION}/)")
        _install(CODE "execute_process(COMMAND rm -f ${CMAKE_INSTALL_PREFIX}/${ARGS_DESTINATION}/${tname})")
        _install(CODE "execute_process(COMMAND ln -srf ${tpath} ${CMAKE_INSTALL_PREFIX}/${ARGS_DESTINATION}/${tname})")
      endforeach(t)
      _install(
        TARGETS ${ARGS_TARGETS}
        DESTINATION
          ${RIFT_SUBMODULE_INSTALL_PREFIX}/install-symlink-workaround/${CMAKE_INSTALL_PREFIX}/${ARGS_DESTINATION}
        ${ex_args})
    else()
     _install(${ARGN})
    endif()
  endif()
endmacro(install)

