
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include (FindPythonInterp)

find_package(PythonInterp REQUIRED)

# Mark python files for installation.
#
# This function will create a new module in the install root with a path
# that matches the system python package directory.  An __init__.py file
# is required and no compiled or python object files are allowed.
#
# @param MODULE_NAME  - Name of the module to install
# @param MODULE_FILES - Files to install into the module
function (rift_install_python)
  set(parse_options "")
  set(parse_onevalargs MODULE_NAME COMPONENT)
  set(parse_multivalueargs MODULE_FILES)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  if ("${ARG_MODULE_NAME}" STREQUAL "")
    message(FATAL_ERROR "No MODULE_NAME defined")
  endif()

  set(__have_init FALSE)
  foreach(__file ${ARG_MODULE_FILES})
    get_filename_component(__filename ${__file} NAME)

    if (${__filename} STREQUAL "__init__.py")
      set(__have_init TRUE)
    endif()
    unset(__filename)

    set(__compiled_python "")
    STRING(REGEX MATCH ".*\\.py[co]$" __compiled_python ${__file})
    if (NOT ${__compiled_python} STREQUAL "")
      message(FATAL_ERROR ".pyc or .pyo files included in MODULE_FILES")
    endif()
    unset(__compiled_python)
  endforeach(__file)

  if (NOT ${__have_init})
    message(FATAL_ERROR "No __init__.py included in MODULE_FILES")
  endif()
  unset(__have_init)

  install(
    FILES ${ARG_MODULE_FILES}
    DESTINATION usr/lib64/python${RIFT_PYTHON3}/site-packages/${ARG_MODULE_NAME}
    COMPONENT ${ARG_COMPONENT})

  if (RIFT_SUPPORT_PYTHON2)
    install(
      FILES ${ARG_MODULE_FILES}
      DESTINATION usr/lib64/python${RIFT_PYTHON2}/site-packages/${ARG_MODULE_NAME}
      COMPONENT ${ARG_COMPONENT})
  endif()

  unset(parse_options)
  unset(parse_onevalargs)
  unset(parse_multivalueargs)
endfunction()

# rift_python_install_tree - Install a list of python files in the same tree structure.
#
# @param PYTHON3_ONLY - Ignore the global RIFT_SUPPORT_PYTHON2 flag and only install files for python3
# @param COMPONENT    - Install component name
# @param FILES        - List of files to install.
function (rift_python_install_tree)
  set(parse_options PYTHON3_ONLY)
  set(parse_onevalargs COMPONENT)
  set(parse_multivalueargs FILES)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  if (NOT ARG_COMPONENT)
    message(FATAL_ERROR "No COMPONENT defined")
  endif()

  foreach(__file ${ARG_FILES})
    set(__compiled_python "")
    STRING(REGEX MATCH ".*\\.py[co]$" __compiled_python ${__file})
    if (NOT ${__compiled_python} STREQUAL "")
      message(FATAL_ERROR ".pyc or .pyo files included in MODULE_FILES")
    endif()
    unset(__compiled_python)
  endforeach(__file)

  foreach(__file ${ARG_FILES})
    get_filename_component(dir ${__file} DIRECTORY)

    install(FILES ${__file}
      DESTINATION usr/lib64/python${RIFT_PYTHON3}/site-packages/${dir}
      COMPONENT ${ARG_COMPONENT})

    if (RIFT_SUPPORT_PYTHON2 AND NOT ARG_PYTHON3_ONLY)
      install(FILES ${__file}
        DESTINATION usr/lib64/python${RIFT_PYTHON2}/site-packages/${dir}
        COMPONENT ${ARG_COMPONENT})
    endif()
  endforeach(__file)
endfunction()


