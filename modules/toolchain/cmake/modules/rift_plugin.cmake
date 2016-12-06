# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
# Author(s): Anil Gunturu, Tim Mortsolf
# Creation Date: 8/24/2013
# 

# This file contains cmake macros for plugin infra structure

##
# This function compiles all the vala sources
# Generates the package .h/.c sources, .so library, .vapi file, .gir file, .typelib file
##
function(rift_add_vala target)
  # Parse the function arguments
  set(parse_options DONT_AUTOLINK_BASE_PACKAGES)
  set(parse_onevalargs GENERATE_HEADER_FILE GENERATE_SO_FILE 
    GENERATE_VAPI_FILE GENERATE_GIR_FILE GENERATE_TYPELIB_FILE
    DONT_AUTOLINK_BASE_PACKAGES)
  set(parse_multivalueargs VALA_FILES VALAC_FLAGS GIR_COMPILER_FLAGS DEPENDS
    VAPI_DIRS VALA_PACKAGES GIR_PATHS LIBRARIES)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" 
    "${parse_multivalueargs}" ${ARGN})

  # Deal with VALA_FILES
  if(NOT ARGS_VALA_FILES)
    message(FATAL_ERROR "rift_add_val() must specify VALA_FILES")
  endif()

  # Deal with VALAC_FLAGS
  set(valac_flags)
  if(NOT ARGS_VALAC_FLAGS)
    set(valac_flags -X -fPIC -X -I.)
  else()
    set(valac_flags -X -fPIC -X -I. ${ARGS_VALAC_FLAGS})
  endif()

  # Deal with GIR_COMPILER_FLAGS
  set(gir_compiler_flags)
  if(NOT ARGS_GIR_COMPILER_FLAGS)
    set(gir_compiler_flags --includedir=.)
  else()
    set(gir_compiler_flags ${ARGS_GIR_COMPILER_FLAGS})
  endif()

  # Deal with GENERATE_HEADER_FILE
  set(generate_header_file)
  if(NOT ARGS_GENERATE_HEADER_FILE)
    set(generate_header_file ${target}.header)
  else()
    set(generate_header_file ${ARGS_GENERATE_HEADER_FILE})
  endif()

  # Deal with GENERATE_SO_FILE
  set(generate_so_file)
  if(NOT ARGS_GENERATE_SO_FILE)
    set(generate_so_file ${target}.so)
  else()
    set(generate_so_file ${ARGS_GENERATE_SO_FILE})
  endif()

  # Deal with GENERATE_VAPI_FILE
  set(generate_vapi_file)
  if(NOT ARGS_GENERATE_VAPI_FILE)
    set(generate_vapi_file ${target}.vapi)
  else()
    set(generate_vapi_file ${ARGS_GENERATE_VAPI_FILE})
  endif()

  # Deal with GENERATE_GIR_FILE
  set(generate_gir_file)
  if(NOT ARGS_GENERATE_GIR_FILE)
    set(generate_gir_file ${target}.gir)
  else()
    set(generate_gir_file ${ARGS_GENERATE_GIR_FILE})
  endif()

  # Deal with GENERATE_TYPELIB_FILE
  set(generate_typelib_file)
  if(NOT ARGS_GENERATE_TYPELIB_FILE)
    set(generate_typelib_file ${target}.typelib)
  else()
    set(generate_typelib_file ${ARGS_GENERATE_TYPELIB_FILE})
  endif()

  # Deal with GIR_PATHS
  set(GIR_PATHS
    "--includedir=${CMAKE_INSTALL_PREFIX}/usr/share/rift/gir-1.0"
  )
  foreach(GPATH ${ARGS_GIR_PATHS})
    list(APPEND GIR_PATHS --includedir=${GPATH})
  endforeach(GPATH)

  if(NOT ARGS_DONT_AUTOLINK_BASE_PACKAGES)
    set(pkgs --pkg=rw_types-1.0)
    if(RIFT_SUBMODULE_NAME STREQUAL core/util)
      list(APPEND ARGS_VALA_PACKAGES "rwtypes_gi")
      list(APPEND ARGS_VAPI_DIRS ${RIFT_SUBMODULE_BINARY_ROOT}/rwtypes/src)
      list(APPEND GIR_PATHS
        --includedir=${RIFT_SUBMODULE_BINARY_ROOT}/rwtypes/src)
    endif()
  endif()
 
  # Append gir parhs to gir compiler flags
  list(APPEND gir_compiler_flags ${GIR_PATHS})

  ##
  # This function compiles all the vala sources
  # Generates the package .h/.c sources, .gir file, .vapi file, and the .so
  ##
  vala_add_library(
    ${target} SHARED
    "${ARGS_VALA_FILES}"
    GENERATE_HEADER ${generate_header_file}
    GENERATE_VAPI ${generate_vapi_file}
    COMPILE_FLAGS ${valac_flags} --gir=${generate_gir_file} ${pkgs}
    PACKAGES ${ARGS_VALA_PACKAGES}
    VAPI_DIRS ${ARGS_VAPI_DIRS} ${RIFT_VAPI_DIRS} 
    )


  if(NOT ARGS_DONT_AUTOLINK_BASE_PACKAGES)
    ##
    # Always link with the following vala libraries.  These are used as a
    # foundation for all vala plugins.  While some plugins may not require them,
    # the majority do and the downsides of overlinking should be minimal in the
    # plugins.
    #
    # Note:  This function is used both in the core/util module (where these
    # plugins are built) and in other modules.  So, in the first case we need to
    # link against the cachedir whereas we can use the installdir in the latter.
    ##
    if(RIFT_SUBMODULE_NAME STREQUAL core/util)
      target_link_libraries(${target} rwplugin-1.0 rwtypes)
    else()
      target_link_libraries(${target}
        ${CMAKE_INSTALL_PREFIX}/usr/lib/rift/plugins/librwplugin-1.0.so
        ${CMAKE_INSTALL_PREFIX}/usr/lib/rift/plugins/librwtypes.so
        )
    endif()
  endif()

  # link with gobject-2.0 and protobuf-c to avoid underlinking.
  target_link_libraries(${target}
    ${ARGS_LIBRARIES}
    protobuf-c
    gobject-2.0)

  if(RIFT_SUBMODULE_NAME STREQUAL core/util)
    add_dependencies(${target} protobuf-c)
  endif()

  ##
  # This defines a command to generate specified OUTPUT file(s)
  # A target created in the same directory (CMakeLists.txt file) that specifies any output of the
  # custom command as a source file is given a rule to generate the file using the command at build time.
  # 
  # The sed command will fix the GIR file generated by the vala compiler so it will not crash
  ##
  add_custom_command(
    OUTPUT ${generate_typelib_file}
    COMMAND
    sed -i -e
    's/<field name=\\\(\"[^\"]*\"\\\)>/<field name=\\1 writable=\\"1\\">/g' ${generate_gir_file}
    COMMAND
    /usr/bin/g-ir-compiler ${gir_compiler_flags} ${generate_gir_file}
    --shared-library=${generate_so_file} --output=${generate_typelib_file}
    DEPENDS ${target} ${CMAKE_CURRENT_BINARY_DIR}/${generate_gir_file}
    )

  ##
  # Adds a target with the given name that executes the given commands
  # The target has no output file and is ALWAYS CONSIDERED OUT OF DATE
  #
  # The ALL option indicates that this target should be added to the default build target
  ##

  add_custom_target(
    ${target}.typelib ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${generate_typelib_file}
    )

  ##
  # Make a top-level target depend on other top-level targets
  # A top-level target is one created by ADD_EXECUTABLE, ADD_LIBRARY, or ADD_CUSTOM_TARGET
  ##
  if(ARGS_DEPENDS)
    add_dependencies(${target}.typelib ${ARGS_DEPENDS})
    add_dependencies(${target} ${ARGS_DEPENDS})
    add_dependencies(${target}_precompile ${ARGS_DEPENDS})
  endif()
endfunction()

##
# This function builds the c plugin schema files
##
function(rift_add_c_plugin_schema PLUGIN_NAME)
  ##
  # rift_add_copy_file() will copy a file to another location
  ##
  set(parse_options "")
  set(parse_onevalargs GSCHEMA PLUGIN)
  set(parse_multivalueargs "")
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  ##
  # rift_add_copy_file() will copy a file to another location
  ##
  if(ARGS_GSCHEMA)
    rift_copy_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_GSCHEMA}
      ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_GSCHEMA}
      TARGET ${ARGS_GSCHEMA}.copy
      )
  endif()
  if(ARGS_PLUGIN)
    rift_copy_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PLUGIN}
      ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_PLUGIN}
      TARGET ${ARGS_PLUGIN}.copy
      )
  endif()
endfunction()

##
# This function builds the python plugin schema files
##
function(rift_add_python_plugin_schema PLUGIN_PREFIX)
  set(parse_options "")
  set(parse_onevalargs GSCHEMA PLUGIN PYTHON TARGET)
  set(parse_multivalueargs "")
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})


  ##
  # Copy the original python script and create sym links
  # to support both python and python3
  ##
  if(ARGS_PYTHON)

    add_custom_command(
      OUTPUT ${PLUGIN_PREFIX}-python.py ${PLUGIN_PREFIX}-python3.py
      COMMAND
      ${CMAKE_COMMAND} -E create_symlink
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PYTHON} ${PLUGIN_PREFIX}-python.py

      COMMAND
      ${CMAKE_COMMAND} -E create_symlink
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PYTHON} ${PLUGIN_PREFIX}-python3.py

      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PYTHON}
      )

    add_custom_target(${ARGS_TARGET}_${ARGS_PYTHON}.symlink ALL
      DEPENDS ${PLUGIN_PREFIX}-python.py ${PLUGIN_PREFIX}-python3.py
      )
  endif()

  if(ARGS_PLUGIN)
    add_custom_command(
      OUTPUT ${PLUGIN_PREFIX}-python.plugin ${PLUGIN_PREFIX}-python3.plugin

      COMMAND ${CMAKE_COMMAND} -E copy 
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PLUGIN} ${PLUGIN_PREFIX}-python.plugin
      COMMAND sed -i -e 's%PY_LOADER%python%g' ${PLUGIN_PREFIX}-python.plugin

      COMMAND ${CMAKE_COMMAND} -E copy 
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PLUGIN} ${PLUGIN_PREFIX}-python3.plugin
      COMMAND sed -i -e 's%PY_LOADER%python3%g' ${PLUGIN_PREFIX}-python3.plugin

      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PLUGIN}
      )
    add_custom_target(${ARGS_TARGET}_${ARGS_PLUGIN}.plugin ALL
      DEPENDS ${PLUGIN_PREFIX}-python.plugin ${PLUGIN_PREFIX}-python3.plugin
      )
  endif()

  if(ARGS_GSCHEMA)
    add_custom_command(
      OUTPUT ${PLUGIN_PREFIX}-python.gschema.xml ${PLUGIN_PREFIX}-python3.gschema.xml

      COMMAND ${CMAKE_COMMAND} -E copy 
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_GSCHEMA} ${PLUGIN_PREFIX}-python.gschema.xml
      COMMAND sed -i -e 's%PY_LOADER%python%g' ${PLUGIN_PREFIX}-python.gschema.xml

      COMMAND ${CMAKE_COMMAND} -E copy 
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_GSCHEMA} ${PLUGIN_PREFIX}-python3.gschema.xml
      COMMAND sed -i -e 's%PY_LOADER%python3%g' ${PLUGIN_PREFIX}-python3.gschema.xml

      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_GSCHEMA}
      )
    add_custom_target(${ARGS_TARGET}_${ARGS_GSCHEMA}.gschema ALL
      DEPENDS ${PLUGIN_PREFIX}-python.gschema.xml ${PLUGIN_PREFIX}-python3.gschema.xml
      )
  endif()

  if(ARGS_TARGET)
      add_custom_target(${ARGS_TARGET} ALL)
      if (ARGS_GSCHEMA)
        add_dependencies(${ARGS_TARGET} ${ARGS_TARGET}_${ARGS_GSCHEMA}.gschema)
      endif()

      if (ARGS_PYTHON)
        add_dependencies(${ARGS_TARGET} ${ARGS_TARGET}_${ARGS_PLUGIN}.plugin)
      endif()

      if (ARGS_PYTHON)
        add_dependencies(${ARGS_TARGET}
            ${ARGS_TARGET}_${ARGS_PYTHON}.symlink
            ${ARGS_TARGET}_${ARGS_PYTHON}.copy)
      endif()
  endif()
endfunction(rift_add_python_plugin_schema)

##
# This function builds the javascript plugin schema files
##
function(rift_add_js_plugin_schema PLUGIN_PREFIX)
  set(parse_options "")
  set(parse_onevalargs GSCHEMA PLUGIN JAVASCRIPT TARGET)
  set(parse_multivalueargs "")
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})


  ##
  # Copy the original python script and create sym links
  # to support both python and python3
  ##
  if(ARGS_JAVASCRIPT)
    rift_copy_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_JAVASCRIPT}
      ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_JAVASCRIPT}
      TARGET ${ARGS_TARGET}_${ARGS_JAVASCRIPT}.copy
      )

    add_custom_command(
      OUTPUT ${PLUGIN_PREFIX}-gjs.js ${PLUGIN_PREFIX}-seed.js
      COMMAND
      ${CMAKE_COMMAND} -E create_symlink 
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_JAVASCRIPT} ${PLUGIN_PREFIX}-gjs.js

      COMMAND
      ${CMAKE_COMMAND} -E create_symlink 
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_JAVASCRIPT} ${PLUGIN_PREFIX}-seed.js

      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PYTHON}
      )

    add_custom_target(${ARGS_TARGET}_${ARGS_JAVASCRIPT}.symlink ALL
      DEPENDS ${PLUGIN_PREFIX}-gjs.js ${PLUGIN_PREFIX}-seed.js
      )
  endif()

  if(ARGS_PLUGIN)
    add_custom_command(
      OUTPUT ${PLUGIN_PREFIX}-gjs.plugin ${PLUGIN_PREFIX}-seed.plugin

      COMMAND ${CMAKE_COMMAND} -E copy 
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PLUGIN} ${PLUGIN_PREFIX}-gjs.plugin
      COMMAND sed -i -e 's%JS_LOADER%gjs%g' ${PLUGIN_PREFIX}-gjs.plugin

      COMMAND ${CMAKE_COMMAND} -E copy 
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PLUGIN} ${PLUGIN_PREFIX}-seed.plugin
      COMMAND sed -i -e 's%JS_LOADER%seed%g' ${PLUGIN_PREFIX}-seed.plugin

      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PLUGIN}
      )
    add_custom_target(${ARGS_TARGET}_${ARGS_PLUGIN}.plugin ALL
      DEPENDS ${PLUGIN_PREFIX}-gjs.plugin ${PLUGIN_PREFIX}-seed.plugin
      )
  endif()

  if(ARGS_GSCHEMA)
    add_custom_command(
      OUTPUT ${PLUGIN_PREFIX}-gjs.gschema.xml ${PLUGIN_PREFIX}-seed.gschema.xml

      COMMAND ${CMAKE_COMMAND} -E copy 
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_GSCHEMA} ${PLUGIN_PREFIX}-gjs.gschema.xml
      COMMAND sed -i -e 's%JS_LOADER%gjs%g' ${PLUGIN_PREFIX}-gjs.gschema.xml

      COMMAND ${CMAKE_COMMAND} -E copy 
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_GSCHEMA} ${PLUGIN_PREFIX}-seed.gschema.xml
      COMMAND sed -i -e 's%JS_LOADER%seed%g' ${PLUGIN_PREFIX}-seed.gschema.xml

      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_GSCHEMA}
      )
    add_custom_target(${ARGS_TARGET}_${ARGS_GSCHEMA}.gschema ALL
      DEPENDS ${PLUGIN_PREFIX}-gjs.gschema.xml ${PLUGIN_PREFIX}-seed.gschema.xml
      )
  endif()

  if (ARGS_TARGET)
    add_custom_target(${ARGS_TARGET}
      DEPENDS
      ${ARGS_TARGET}_${ARGS_GSCHEMA}.gschema
      ${ARGS_TARGET}_${ARGS_PLUGIN}.plugin
      ${ARGS_TARGET}_${ARGS_JAVASCRIPT}.symlink
      ${ARGS_TARGET}_${ARGS_JAVASCRIPT}.copy
    )
  endif()
endfunction(rift_add_js_plugin_schema)

##
# This function builds the lua plugin schema files
##
function(rift_add_lua_plugin_schema PLUGIN_PREFIX)
  set(parse_options "")
  set(parse_onevalargs GSCHEMA PLUGIN LUASCRIPT TARGET)
  set(parse_multivalueargs "")
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  ##
  # Copy the original lua script and create sym links
  ##
  if(ARGS_LUASCRIPT)
    add_custom_command(
      OUTPUT ${ARGS_LUASCRIPT}
      COMMAND
      ${CMAKE_COMMAND} -E create_symlink
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_LUASCRIPT} ${ARGS_LUASCRIPT}

      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_LUASCRIPT}
      )

    add_custom_target(${ARGS_TARGET}_${ARGS_LUASCRIPT}.symlink ALL
      DEPENDS ${ARGS_LUASCRIPT}
      )
  endif()

  if(ARGS_PLUGIN)
    add_custom_command(
      OUTPUT ${ARGS_PLUGIN}
      COMMAND
      ${CMAKE_COMMAND} -E create_symlink
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PLUGIN} ${ARGS_PLUGIN}

      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PLUGIN}
      )

    add_custom_target(${ARGS_TARGET}_${ARGS_PLUGIN}.symlink ALL
      DEPENDS ${ARGS_PLUGIN}
      )
  endif()

  if(ARGS_GSCHEMA)
    add_custom_command(
      OUTPUT ${ARGS_GSCHEMA}
      COMMAND
      ${CMAKE_COMMAND} -E create_symlink
      ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_GSCHEMA} ${ARGS_GSCHEMA}

      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_GSCHEMA}
      )

    add_custom_target(${ARGS_TARGET}_${ARGS_GSCHEMA}.symlink ALL
      DEPENDS ${ARGS_GSCHEMA}
      )
  endif()

  if (ARGS_TARGET)
    add_custom_target(${ARGS_TARGET}
      DEPENDS
      ${ARGS_TARGET}_${ARGS_GSCHEMA}.symlink
      ${ARGS_TARGET}_${ARGS_PLUGIN}.symlink
      ${ARGS_TARGET}_${ARGS_LUASCRIPT}.symlink
    )
  endif()
endfunction(rift_add_lua_plugin_schema)

##
# This function creates a vapi2c target to generate Vala source/header files from .in files
##
function(rift_add_plugin_vapi2c target)
  # Parse the function arguments
  set(parse_options "")
  set(parse_onevalargs VAPI_FILE PLUGIN_SOURCE_PREFIX PLUGIN_SOURCE_EXTENSION PLUGIN_PREFIX)
  set(parse_multivalueargs DEPENDS)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  if(ARGS_PLUGIN_SOURCE_EXTENSION)
    set(fileext ${ARGS_PLUGIN_SOURCE_EXTENSION})
  else()
    set(fileext c)
  endif()

  ##
  # Set the source_file/dest_file variable names
  ##
  set(source_c_file ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PLUGIN_SOURCE_PREFIX}.in.${fileext})
  set(dest_c_file ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_PLUGIN_SOURCE_PREFIX}.${fileext})
  set(source_h_file ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PLUGIN_SOURCE_PREFIX}.in.h)
  set(dest_h_file ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_PLUGIN_SOURCE_PREFIX}.h)
  if(!ARGS_PLUGIN_PREFIX)
    set(plugin_prefix)
  else()
    set(plugin_prefix --plugin-prefix=${ARGS_PLUGIN_PREFIX})
  endif()

  ##
  # This defines a command to generate specified OUTPUT file(s)
  # directory (CMakeLists.txt file) that specifies any output of the
  # custom command as a source file is given a rule to generate the file using the command at build time.
  ##
  add_custom_command(
    OUTPUT ${dest_c_file} ${dest_h_file}
    COMMAND
    ${CMAKE_COMMAND} -E copy ${source_c_file} ${dest_c_file}
    COMMAND
    ${CMAKE_COMMAND} -E copy ${source_h_file} ${dest_h_file}
    COMMAND
    ${RIFT_CMAKE_BIN_DIR}/vapi_to_c.py ${ARGS_VAPI_FILE} --c-file=${dest_c_file} --h-file=${dest_h_file}
    ${plugin_prefix}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${ARGS_DEPENDS} ${source_c_file} ${source_h_file}# ${ARGS_VAPI_FILE}
    )
endfunction()

##
# This function creates install rules for artifacts 
# generated from the vala files
#
# rift_install_vala_artifacts(HEADER_FILES <.h files>
#                             SO_FILES <.so files>
#                             VAPI_FILES <.vapi files>
#                             GIR_FILES <.gir files>
#                             TYPELIB_FILES <.typelib files>
#                             LUA_OVERRIDES <lua override files>
#                             PYTHON_OVERRIDES <python override files>
#                             DEST_PREFIX <destination install prefix>
#                             COMPONENT <component name>)
#
# NOTE: COMPONENT name is used when cmake component based install is used
#       DEST_PREFIX will be appended to the CMAKE_INSTALL_PREFIX. DONOT
#                   use absolute path (must not start with /)
##
function(rift_install_vala_artifacts)
  ##
  # Parse the function arguments
  ##
  set(parse_options "")
  set(parse_onevalargs DEST_PREFIX COMPONENT)
  set(parse_multivalueargs HEADER_FILES SO_FILES VAPI_FILES GIR_FILES
    TYPELIB_FILES LUA_OVERRIDES PYTHON_OVERRIDES)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  ##
  # Produce install rules for each type of file
  ##

  # if (NOT ARGS_COMPONENT)
  #  message ("WARNING:rift_install_vala_artifacts(${CMAKE_CURRENT_SOURCE_DIR}): No component specified")
  # endif()

  if(ARGS_HEADER_FILES)
    foreach(val ${ARGS_HEADER_FILES})
      if(ARGS_COMPONENT)
         install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${val} 
           DESTINATION ${ARGS_DEST_PREFIX}/usr/include/rift/plugins
           COMPONENT ${ARGS_COMPONENT})
      else()
         install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${val}
           DESTINATION ${ARGS_DEST_PREFIX}/usr/include/rift/plugins)
      endif()
    endforeach(val)
  endif()
  if(ARGS_SO_FILES)
    if(ARGS_COMPONENT)
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_SO_FILES} 
        DESTINATION ${ARGS_DEST_PREFIX}/usr/lib/rift/plugins
        COMPONENT ${ARGS_COMPONENT})
    else()
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_SO_FILES} 
        DESTINATION ${ARGS_DEST_PREFIX}/usr/lib/rift/plugins)
    endif()
  endif()
  if(ARGS_VAPI_FILES)
    if(ARGS_COMPONENT)
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_VAPI_FILES} 
        DESTINATION ${ARGS_DEST_PREFIX}/usr/share/rift/vapi
        COMPONENT ${ARGS_COMPONENT})
    else()
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_VAPI_FILES} 
        DESTINATION ${ARGS_DEST_PREFIX}/usr/share/rift/vapi)
    endif()
  endif()
  if(ARGS_GIR_FILES)
    if(ARGS_COMPONENT)
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_GIR_FILES} 
        DESTINATION ${ARGS_DEST_PREFIX}/usr/share/rift/gir-1.0
        COMPONENT ${ARGS_COMPONENT})
    else()
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_GIR_FILES} 
        DESTINATION ${ARGS_DEST_PREFIX}/usr/share/rift/gir-1.0)
    endif()
  endif()
  if(ARGS_TYPELIB_FILES)
    if(ARGS_COMPONENT)
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_TYPELIB_FILES} 
        DESTINATION ${ARGS_DEST_PREFIX}/usr/lib/rift/girepository-1.0
        COMPONENT ${ARGS_COMPONENT})
    else()
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_TYPELIB_FILES} 
        DESTINATION ${ARGS_DEST_PREFIX}/usr/lib/rift/girepository-1.0)
    endif()
  endif()

  if(ARGS_PYTHON_OVERRIDES)
    set(dirs ${ARGS_DEST_PREFIX}/usr/lib64/python${RIFT_PYTHON3}/site-packages/gi/overrides)
    if (RIFT_SUPPORT_PYTHON2)
      list(APPEND dirs ${ARGS_DEST_PREFIX}/usr/lib64/python${RIFT_PYTHON2}/site-packages/gi/overrides)
    endif()

    if(ARGS_COMPONENT)
      foreach(dir ${dirs})
        install(
          FILES ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_PYTHON_OVERRIDES}
          DESTINATION ${dir}
          COMPONENT ${ARGS_COMPONENT})
      endforeach()
    else()
      foreach(dir ${dirs})
        install(
          FILES ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_PYTHON_OVERRIDES}
          DESTINATION ${dir})
      endforeach()
    endif()
  endif()

  if(ARGS_LUA_OVERRIDES)
    if(ARGS_COMPONENT)
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_LUA_OVERRIDES} 
        DESTINATION ${ARGS_DEST_PREFIX}/usr/share/lua/5.1/lgi/override
        COMPONENT ${ARGS_COMPONENT})
    else()
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_LUA_OVERRIDES} 
        DESTINATION ${ARGS_DEST_PREFIX}/usr/share/lua/5.1/lgi/override)
    endif()
  endif()
endfunction()

##
# This function creates install rules for artifacts generated 
# from the plugin files
#
# rift_install_plugin_artifacts(PLUGIN_NAME
#                               SOTARGETS <.so files>
#                               GSCHEMAFILES <.gschema files>
#                               PLUGINFILES <.plugin files>
#                               COMPONENT <component name>)
#
# NOTE: COMPONENT name is used when cmake component based install is used
##
function(rift_install_plugin_artifacts PLUGIN_NAME)
  ##
  # Parse the function arguments
  ##
  set(parse_options "")
  set(parse_onevalargs COMPONENT)
  set(parse_multivalueargs SOTARGETS GSCHEMAFILES PLUGINFILES)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  ##
  # Produce install rules for each type of file
  ##
  if(ARGS_SOTARGETS)
    if(ARGS_COMPONENT)
      install(TARGETS ${ARGS_SOTARGETS} 
        DESTINATION usr/lib/rift/plugins/${PLUGIN_NAME} 
        COMPONENT ${ARGS_COMPONENT})
    else()
      install(TARGETS ${ARGS_SOTARGETS} 
        DESTINATION usr/lib/rift/plugins/${PLUGIN_NAME})
    endif()
  endif()
  if(ARGS_GSCHEMAFILES)
    if(ARGS_COMPONENT)
      install(FILES ${ARGS_GSCHEMAFILES} 
        DESTINATION usr/lib/rift/plugins/${PLUGIN_NAME} 
        COMPONENT ${ARGS_COMPONENT})
    else()
      install(FILES ${ARGS_GSCHEMAFILES} 
        DESTINATION usr/lib/rift/plugins/${PLUGIN_NAME})
    endif()
  endif()
  if (ARGS_PLUGINFILES)
    if(ARGS_COMPONENT)
      install(FILES ${ARGS_PLUGINFILES} 
        DESTINATION usr/lib/rift/plugins/${PLUGIN_NAME} 
        COMPONENT ${ARGS_COMPONENT})
    else()
      install(FILES ${ARGS_PLUGINFILES} 
        DESTINATION usr/lib/rift/plugins/${PLUGIN_NAME})
    endif()
  endif()
endfunction()

##
# rift_add_introspection
# This function generates the GIR file from the gi annotated c/h files.
# It generates a VAPI file from the GIR, so that this namespace can
# be used in VALA bindings. In addition, this function generates 
# a typelib files from GIR, so that the namespace can be accessed from
# Python/Lua.
#
# The following tools are used in this function:
#   g-ir-scanner : generates .gir file from c/h files
#   vapigen      : generates .vapi file from .gir
#   g-ir-compiler: generates .typelib file from .gir  
#
##
function(rift_add_introspection target)
  ##
  # Parse the function arguments
  ##
  set(parse_options NO_UNPREFIXED GENERATE_GI_OVERRIDE BUILD_ALL)
  set(parse_onevalargs NAMESPACE VERSION IDENTIFIER_FILTER_CMD VAPI_PREFIX GI_PY_OVERRIDE_FILE)
  set(parse_multivalueargs HFILES CFILES CFLAGS PACKAGES LIBRARIES LIBRARY_PATHS
    DEPENDS INCLUDE_PATHS GIR_COMPILER_FLAGS SYMBOL_PREFIX IDENTIFIER_PREFIX
    GIR_PATHS GI_INCLUDE_HFILES)

  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(PACKAGES "--include=GObject-2.0")
  foreach(PKG ${ARGS_PACKAGES})
    set(PACKAGES ${PACKAGES} --include=${PKG})
  endforeach()

  set(GIR_PATHS
    "--add-include-path=${CMAKE_INSTALL_PREFIX}/usr/share/rift/gir-1.0"
    )
  foreach(GPATH ${ARGS_GIR_PATHS})
    set(GIR_PATHS ${GIR_PATHS} --add-include-path=${GPATH})
  endforeach(GPATH)

  set(INCLUDE_PATHS
    -I/usr/include
    -I${CMAKE_INSTALL_PREFIX}/usr/include
    -I/usr/include/libpeas-1.0
    -I/usr/include/gobject-introspection-1.0
    -I${CMAKE_INSTALL_PREFIX}/usr/include/rift/plugins
    -I/usr/lib64/glib-2.0/include
    -I/usr/include/glib-2.0
    )
  foreach(INC ${ARGS_INCLUDE_PATHS} ${RIFT_SUBMODULE_INCLUDE_DIRECTORIES})
    set(INCLUDE_PATHS ${INCLUDE_PATHS} -I${INC})
  endforeach()

  # Take a special notice to how the library list is built
  foreach(l ${ARGS_LIBRARIES})
    list(APPEND LIBRARIES --library)
    list(APPEND LIBRARIES ${l})
  endforeach(l)

  set(LIBRARY_PATHS
    -L${CMAKE_CURRENT_BINARY_DIR}
    -L${CMAKE_INSTALL_PREFIX}/usr/lib
    -L${CMAKE_INSTALL_PREFIX}/usr/lib/rift/plugins
  )

  foreach(LPATH ${ARGS_LIBRARY_PATHS})
    set(LIBRARY_PATHS ${LIBRARY_PATHS} -L${LPATH})
  endforeach()

  # For another library generating a gir with the same naming style see:
  # https://github.com/ebassi/graphene/blob/40994fae93406dc0da72352f834fd61ea2907cee/src/Makefile.am#L140
  set(SYMBOL_PREFIX "")
  foreach(SP ${ARGS_SYMBOL_PREFIX})
    set(SYMBOL_PREFIX ${SYMBOL_PREFIX} --symbol-prefix=${SP})
  endforeach()

  set(IDENTIFIER_PREFIX "")
  foreach(SP ${ARGS_IDENTIFIER_PREFIX})
    set(IDENTIFIER_PREFIX ${IDENTIFIER_PREFIX} --identifier-prefix=${SP})
  endforeach()

  if(NOT ARGS_IDENTIFIER_FILTER_CMD)
    set(IDENTIFIER_FILTER_CMD "")
  else()
    set(IDENTIFIER_FILTER_CMD --identifier-filter-cmd=${ARGS_IDENTIFIER_FILTER_CMD})
  endif()  

  if(NOT ARGS_BUILD_ALL)
    set(BUILD_ALL_CMD "")
  else()
    set(BUILD_ALL_CMD "ALL")
  endif()

  # Why?  Because for whatever magic reason, this was causing rwmain_gi to not be
  # able to find rw_status_t.  See RIFT-6798.
  set(unprefixed)
  if (NOT ARGS_NO_UNPREFIXED)
    set(unprefixed --accept-unprefixed)
  endif()

  # generate .gir 
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}-${ARGS_VERSION}.gir
    COMMAND /usr/bin/g-ir-scanner 
    --cflags-begin ${ARGS_CFLAGS} -fPIC ${INCLUDE_PATHS} --cflags-end
    -n ${ARGS_NAMESPACE}
    --nsversion=${ARGS_VERSION}
    ${LIBRARY_PATHS}
    ${LIBRARIES}
    ${GIR_PATHS}
    ${PACKAGES}
    --warn-all
    ${unprefixed}
    ${SYMBOL_PREFIX}
    ${IDENTIFIER_PREFIX}
    ${IDENTIFIER_FILTER_CMD}
    -o ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}-${ARGS_VERSION}.gir
    ${ARGS_HFILES} ${ARGS_CFILES}
    DEPENDS ${ARGS_DEPENDS} ${ARGS_HFILES} ${ARGS_CFILES})

  # Deal with GIR_COMPILER_FLAGS
  set(gir_compiler_flags)
  if(NOT ARGS_GIR_COMPILER_FLAGS)
    set(gir_compiler_flags --includedir=.)
  else()
    set(gir_compiler_flags ${ARGS_GIR_COMPILER_FLAGS})
  endif()

  # If VAPI_PREFIX is not specified, infer it from the namespace
  # VAPI_PREFIX is used to construct the VAPI file name.
  if(NOT ARGS_VAPI_PREFIX)
    # If prefix for the vapi file is not specified, derive it from
    # namespace.
    rift_uncamel(${ARGS_NAMESPACE} UNCAMEL_NAMESPACE)
  else()
    set(UNCAMEL_NAMESPACE ${ARGS_VAPI_PREFIX})
  endif()

  # The GI_INCLUDE_HFILE is used to control the name of the c header
  # file in the generated .vapi file
  if(ARGS_GI_INCLUDE_HFILES)
    # A custom GI include file is passed. Generate a metadata file so that
    # vapigen overrides the default header file with the custom header file.
    set(VAPI_METADATA_FILE
      ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}-${ARGS_VERSION}.metadata)
    foreach(val ${ARGS_GI_INCLUDE_HFILES})
      if(cheaders)
        set(cheaders "${cheaders},${val}")
      else()
        set(cheaders "${val}")
      endif()
    endforeach(val)
    add_custom_command(
      OUTPUT ${VAPI_METADATA_FILE}
      COMMAND bash -c "echo  '* cheader_filename=\"${cheaders}\"' > ${VAPI_METADATA_FILE}"
      VERBATIM
    )
  else()
    set(VAPI_METADATA_FILE)
  endif()

  set(GIR_PATHS "--girdir=${CMAKE_INSTALL_PREFIX}/usr/share/rift/gir-1.0")
  set(VAPI_DIRS "--vapidir=${CMAKE_INSTALL_PREFIX}/usr/share/rift/vapi")
  foreach(GPATH ${ARGS_GIR_PATHS})
    set(GIR_PATHS ${GIR_PATHS} --girdir=${GPATH})
    set(VAPI_DIRS ${VAPI_DIRS} --vapidir=${GPATH})
  endforeach(GPATH)

  set(PACKAGES "--pkg=gobject-2.0")
  set(PACKAGES ${PACKAGES} "--pkg=glib-2.0")
  foreach(PKG ${ARGS_PACKAGES})
    rift_uncamel(${PKG} p)
    set(PACKAGES ${PACKAGES} --pkg=${p})
  endforeach()

  # generate .vapi
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${UNCAMEL_NAMESPACE}-${ARGS_VERSION}.vapi
    COMMAND
    /usr/bin/vapigen 
    ${GIR_PATHS}
    ${VAPI_DIRS}
    ${PACKAGES}
    --library ${UNCAMEL_NAMESPACE}-${ARGS_VERSION} 
    ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}-${ARGS_VERSION}.gir
    COMMAND
    # vapigen doesn't seem to overwrite the file when this custom command
    # is excuted if the file contents din't change. This will cause cmake
    # to rebuild vapigen all the time. Touch the file to make CMAKE happy
    touch ${CMAKE_CURRENT_BINARY_DIR}/${UNCAMEL_NAMESPACE}-${ARGS_VERSION}.vapi
    DEPENDS 
    ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}-${ARGS_VERSION}.gir
    ${VAPI_METADATA_FILE}
  )

  set(GIR_PATHS
    "--includedir=${CMAKE_INSTALL_PREFIX}/usr/share/rift/gir-1.0"
    )
  foreach(GPATH ${ARGS_GIR_PATHS})
    set(GIR_PATHS ${GIR_PATHS} --includedir=${GPATH})
  endforeach(GPATH)
  set(GIR_PATHS ${GIR_PATHS} --includedir=${CMAKE_INSTALL_PREFIX}/usr/share/rift/vapi)

  # The gir scanner expects the libraries be separated by comma when there
  # are multiple libraries. For example "--shared-library=libfoo.a,libbar.a".
  unset(LIBRARIES)
  foreach(l ${ARGS_LIBRARIES})
    if(NOT LIBRARIES)
      set(LIBRARIES --shared-library=lib${l}.so)
    else()
      set(LIBRARIES ${LIBRARIES},lib${l}.so)
    endif()
  endforeach(l)

  # generate .typelib
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}-${ARGS_VERSION}.typelib
    COMMAND
    /usr/bin/g-ir-compiler ${gir_compiler_flags}
    ${GIR_PATHS}
    ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}-${ARGS_VERSION}.gir
    ${LIBRARIES}
    --output=${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}-${ARGS_VERSION}.typelib
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}-${ARGS_VERSION}.gir
    )

  # generate the Python override file, this file helps in overriding get_foo and
  # set_foo to simple attributes.
  if(NOT ARGS_GI_PY_OVERRIDE_FILE AND ARGS_GENERATE_GI_OVERRIDE)
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}.py
      COMMAND ${CMAKE_COMMAND} -E copy
      ${RIFT_CMAKE_BIN_DIR}/python_override_template.py
      ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}.py
      COMMAND sed -i -e 's/NAMESPACE/${ARGS_NAMESPACE}/g'
      ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}.py
      DEPENDS ${RIFT_CMAKE_BIN_DIR}/python_override_template.py
      )
  elseif(ARGS_GI_PY_OVERRIDE_FILE)
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}.py
      COMMAND cp ${ARGS_GI_PY_OVERRIDE_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}.py
      )
  endif()

  # generate the LUA override file, this file helps in overriding get_foo and
  # set_foo to simple attributes.
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}.lua
    COMMAND ${CMAKE_COMMAND} -E copy
    ${RIFT_CMAKE_BIN_DIR}/lua_override_template.lua
    ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}.lua
    COMMAND sed -i -e 's/NAMESPACE/${ARGS_NAMESPACE}/g'
    ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}.lua
    DEPENDS ${RIFT_CMAKE_BIN_DIR}/lua_override_template.lua
    )

  # adds custom target

  if (ARGS_GENERATE_GI_OVERRIDE OR ARGS_GI_PY_OVERRIDE_FILE)
    add_custom_target(${target} ${BUILD_ALL_CMD}
      DEPENDS
      ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}-${ARGS_VERSION}.typelib
      ${CMAKE_CURRENT_BINARY_DIR}/${UNCAMEL_NAMESPACE}-${ARGS_VERSION}.vapi
      ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}.lua
      ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}.py)
  else()
    add_custom_target(${target} ${BUILD_ALL_CMD}
      DEPENDS
      ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}-${ARGS_VERSION}.typelib
      ${CMAKE_CURRENT_BINARY_DIR}/${UNCAMEL_NAMESPACE}-${ARGS_VERSION}.vapi
      ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_NAMESPACE}.lua)
  endif()

  set_property(TARGET ${target} PROPERTY GIR_NAME ${ARGS_NAMESPACE}-${ARGS_VERSION})
  set_property(TARGET ${target} PROPERTY GIR_PATH
    ${CMAKE_CURRENT_BINARY_DIR}/${ARG_NAMESPACE}-${ARG_VERSION}.gir)

endfunction(rift_add_introspection)

# Install a python Peas plugin implementation.  This will install a plugin
# implementation using the python3 loader and any other required files.  If
# python2 support is enabled, the same will be done, using the same source, for
# python2.
#
# TARGET      - Target name
# PLUGIN      - PYthon plugin source file.
# NO_INSTALL  - If set, the install phase will not be run.  This is useful for
#               plugins only used for testing.
# COMPONENT   - Install Component name              
function(rift_install_python_plugin TARGET PLUGIN)
  cmake_parse_arguments(ARG "NO_INSTALL" "COMPONENT" "" ${ARGN})

  if ("${ARG_COMPONENT}" STREQUAL "")
    set(ARG_COMPONENT ${PKG_LONG_NAME})
  endif()

  set(deps)
  get_filename_component(pn ${PLUGIN} NAME_WE)

  add_custom_command(
    OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/${PLUGIN}
    COMMAND
      ${CMAKE_COMMAND} -E copy
      ${PLUGIN}
      ${CMAKE_CURRENT_BINARY_DIR}/${PLUGIN}
    DEPENDS
      ${PLUGIN}
    WORKING_DIRECTORY
      ${CMAKE_CURRENT_SOURCE_DIR})

  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${pn}.plugin "[Plugin]\n")
  file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${pn}.plugin "Module=${pn}\n")
  file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${pn}.plugin "Loader=python3\n")
  file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${pn}.plugin "Name=${pn}\n")

  list(APPEND deps ${CMAKE_CURRENT_BINARY_DIR}/${PLUGIN})
  list(APPEND deps ${CMAKE_CURRENT_BINARY_DIR}/${pn}.plugin)

  if (NOT ARG_NO_INSTALL)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${PLUGIN}
        ${CMAKE_CURRENT_BINARY_DIR}/${pn}.plugin
      DESTINATION usr/lib/rift/plugins/${pn}
      COMPONENT ${ARG_COMPONENT})
  endif()

  add_custom_target(${TARGET} ALL
    DEPENDS ${deps})
endfunction()

