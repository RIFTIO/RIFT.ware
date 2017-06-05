# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tom Seidenberg
# Creation Date: 2014/04/04
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

include(CMakeParseArguments)

##
# function to create the protoc or protoc-c argument for the .proto search path.
# rift_proto_path_arg(
#   <var>                                    # variable to set
#   PROTO_DIRS <other-proto-import-dirs> ... # actual: CUR_SRC, CUR_BIN, PROTO_DIRS, RIFT_PROTO_DIRS
# )
##
function(rift_proto_path_arg var)
  set(parse_options)
  set(parse_onevalargs)
  set(parse_multivalueargs PROTO_DIRS)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  rift_list_to_path(proto_path
    ${ARG_PROTO_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${RIFT_PROTO_DIRS}
  )

  set(${var} "--proto_path=${proto_path}" PARENT_SCOPE)
endfunction(rift_proto_path_arg)


##
# function to create a command line for unmodified google protoc
# rift_gprotoc_command(
#   var                                 # The command variable to set
#   ENVSET <arguments> ...              # Extra envset.sh arguments
#   GPROTOC_EXE <protoc>                # default: /bin
#   PROTO_DIRS <proto-import-dirs> ...  # actual: CUR_SRC, CUR_BIN, PROTO_DIR
#   OUT_DEPENDS_VAR <name-of-the-var>   # Variable to hold extra dependencies
#   ARGV <arguments>...                 # The arguments to protoc
# )
function(rift_gprotoc_command var)
  set(parse_options)
  set(parse_onevalargs GPROTOC_EXE OUT_DEPENDS_VAR)
  set(parse_multivalueargs ARGV ENVSET PROTO_DIRS)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(depends)
  if(NOT ARG_GPROTOC_EXE)
    set(ARG_GPROTOC_EXE /usr/bin/protoc)
  endif()

  rift_proto_path_arg(proto_path_arg PROTO_DIRS ${ARG_PROTO_DIRS})

  set(${var}
    ${RIFT_CMAKE_BIN_DIR}/envset.sh
      ${ARG_ENVSET}
      -e ${ARG_GPROTOC_EXE} "${proto_path_arg}" ${ARG_ARGV}
    PARENT_SCOPE
  )

  if(ARG_OUT_DEPENDS_VAR)
    set(${ARG_OUT_DEPENDS_VAR} ${depends} PARENT_SCOPE)
  endif()
endfunction(rift_gprotoc_command)


##
# function to create a command line for rift-protoc-c
# rift_protocc_command(
#   var                                 # The command variable to set
#   ENVSET <arguments> ...              # Extra envset.sh arguments
#   PROTOCC_EXE <protoc-c>              # default: INSTALL/... due to ext/ipc module order
#   PROTO_DIRS <proto-import-dirs> ...  # actual: CUR_SRC, CUR_BIN, PROTO_DIR
#   OUT_DEPENDS_VAR <name-of-the-var>   # Variable to hold extra dependencies
#   ARGV <arguments>...                 # The arguments to protoc-c
# )
function(rift_protocc_command var)
  set(parse_options)
  set(parse_onevalargs PROTOCC_EXE OUT_DEPENDS_VAR)
  set(parse_multivalueargs ARGV ENVSET PROTO_DIRS)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(depends)
  if(NOT ARG_PROTOCC_EXE)
    set(ARG_PROTOCC_EXE /usr/bin/rift-protoc-c)
    list(APPEND depends ${ARG_PROTOCC_EXE})
  endif()

  rift_proto_path_arg(proto_path_arg PROTO_DIRS ${ARG_PROTO_DIRS})

  set(${var}
    ${RIFT_CMAKE_BIN_DIR}/envset.sh
      ${ARG_ENVSET}
      -e ${ARG_PROTOCC_EXE} "${proto_path_arg}" ${ARG_ARGV}
    PARENT_SCOPE
  )

  if(ARG_OUT_DEPENDS_VAR)
    set(${ARG_OUT_DEPENDS_VAR} ${depends} ${ARG_PROTOCC_EXE} PARENT_SCOPE)
  endif()
endfunction(rift_protocc_command)


##
# function to create a command line for pb2c
# rift_pb2c_command(
#   var                                # The command variable to set
#   ENVSET <arguments> ...             # Extra envset.sh arguments
#   PB2C_EXE <pb2c-c>                  # default: INSTALL/... or core/util
#   OUT_DEPENDS_VAR <name-of-the-var>  # Variable to hold extra dependencies
#   ARGV <arguments>...                # The arguments to pb2c
# )
function(rift_pb2c_command var)
  set(parse_options)
  set(parse_onevalargs PB2C_EXE OUT_DEPENDS_VAR)
  set(parse_multivalueargs ARGV ENVSET)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(depends)
  if(NOT ARG_PB2C_EXE)
    if(RIFT_SUBMODULE_NAME STREQUAL core/util)
      set(ARG_PB2C_EXE ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/pb2c/pb2c)
      list(APPEND depends pb2c)
    else()
      set(ARG_PB2C_EXE ${CMAKE_INSTALL_PREFIX}/usr/bin/pb2c)
    endif()
  endif()

  set(${var}
    ${RIFT_CMAKE_BIN_DIR}/envset.sh
      ${ARG_ENVSET}
      -e ${ARG_PB2C_EXE} ${ARG_ARGV}
    PARENT_SCOPE
  )

  if(ARG_OUT_DEPENDS_VAR)
    set(${ARG_OUT_DEPENDS_VAR} ${depends} PARENT_SCOPE)
  endif()
endfunction(rift_pb2c_command)


##
# function generate targets for .yang->.xsd->.proto->*.[ch]
# rift_add_proto_target(
#   TARGET <target>             # target, if desired; if missing you must use OUT*
#   PROTO_FILES <proto-files> ...
#   DEST_DIR <destination-dir>  # def: CUR_BIN
#   SRC_DIR <source-dir>        # def: CUR_SRC
#   DEPENDS <dep> ...           # Extra dependencies.  Applied to all steps
#
#   WITH_GPROTOC  # Generate c++ files with host protoc.
#   NO_PROTOCC    # Supress rift-protoc-c; usually used with WITH_PROTOC
#   WITH_DSO      # Also generate .dso files
#   WITH_PB2C     # Also generate pb2c files, implies WITH_DSO
#
#   ## Compilers.
#   GPROTOC_EXE <protoc>          ## Defaults to host
#   PROTOCC_EXE <rift-protoc-c>   ## Just defaults to INSTALL/... due to ext/ipc module order
#   PB2C_EXE <pb2c>
#
#   ## Compiler arguments.
#   GPROTOC_ARGV <args>...   ## Extra arguments (if specified, replaces any default extra arguments)
#   PROTOCC_ARGV <args>...   ## Extra arguments (if specified, replaces any default extra arguments)
#   PB2C_ARGV <args>...      ## Extra arguments (if specified, replaces any default extra arguments)
#
#   ## Extra include path search dirs
#   OTHER_DIRS <other-import-dirs-for-all> ... ## actual: CUR_SRC, CUR_BIN, OTHER_DIRS
#   PROTO_DIRS <other-proto-import-dirs> ...   ## actual: CUR_SRC, CUR_BIN, OTHER_DIRS, PROTO_DIRS, INSTALL/usr/data/proto
#
#   ## Install components.  If set, will install against the specified
#   ## component.  If not set, then not installed at all.  When not
#   ## installed, generated files can only be refereced from the same
#   ## submodule later in the build (good for unit tests).
#   PROTO_COMPONENT <comp>      # Install component for .proto
#   GPCH_COMPONENT <comp>       # Install component for google protoc .h, implies WITH_GPROTOC
#   PCCH_COMPONENT <comp>       # Install component for protoc-c .h
#   PB2CH_COMPONENT <comp>      # Install component for pb2c .h, implies WITH_PB2C
#   COMPONENT <comp>            # Install all outputs to comp
#
#   ## Output variables, to set in the calling context.  When set,
#   ## allows the caller of the function to get the file lists out to
#   ## use them for thoer things (such as compiling .c files to a
#   ## library).
#   OUT_C_FILES_VAR <name-of-the-var>    # Variable to hold .c file list
#   OUT_CC_FILES_VAR <name-of-the-var>   # Variable to hold .cc file list, implies WITH_GPROTOC
#   OUT_H_FILES_VAR <name-of-the-var>    # Variable to hold .h file list
#   OUT_DSO_FILES_VAR <name-of-the-var>  # Variable to hold .dso file list, implies WITH_DSO
# )
function(rift_add_proto_target)
  set(parse_options
    WITH_GPROTOC NO_PROTOCC WITH_DSO WITH_PB2C WITH_GI SUPPRESS_PROTO_DEPEND)
  set(parse_onevalargs
      TARGET SRC_DIR DEST_DIR
      PROTOCC_EXE PB2C_EXE
      COMPONENT PROTO_COMPONENT GPCH_COMPONENT PCCH_COMPONENT PB2CH_COMPONENT
      OUT_C_FILES_VAR OUT_CC_FILES_VAR OUT_H_FILES_VAR OUT_DSO_FILES_VAR)
  set(parse_multivalueargs
      PROTO_FILES DEPENDS
      PROTOCC_ARGV PB2C_ARGV
      OTHER_DIRS PROTO_DIRS)
  cmake_parse_arguments(ARG "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})
  #message(STATUS "TGS: v: '${ARG_DEPENDS}' '${ARG_PROTO_FILES}'")

  ########################################
  # Default build phases
  if(ARG_GPCH_COMPONENT OR OUT_CC_FILES_VAR)
    set(ARG_WITH_GPROTOC 1)
  endif()

  if(ARG_PB2CH_COMPONENT)
    set(ARG_WITH_PB2C 1)
  endif()

  if(ARG_PB2CH_COMPONENT OR ARG_WITH_PB2C OR OUT_DSO_FILES_VAR)
    set(ARG_WITH_DSO 1)
  endif()
  #message(STATUS "TGS: with: '${ARG_WITH_GPROTOC}' '${ARG_NO_PROTOCC}' '${ARG_WITH_DSO}' '${ARG_WITH_PB2C}'")

  ########################################
  # Default component (should come after WITH defaults!)
  if(ARG_COMPONENT)
    if(NOT ARG_PROTO_COMPONENT)
      set(ARG_PROTO_COMPONENT ${ARG_COMPONENT})
    endif()
    if(NOT ARG_GPCH_COMPONENT)
      set(ARG_GPCH_COMPONENT ${ARG_COMPONENT})
    endif()
    if(NOT ARG_PCCH_COMPONENT)
      set(ARG_PCCH_COMPONENT ${ARG_COMPONENT})
    endif()
    if(NOT ARG_PB2CH_COMPONENT)
      set(ARG_PB2CH_COMPONENT ${ARG_COMPONENT})
    endif()
  endif()
  #message(STATUS "TGS: inst: '${ARG_COMPONENT}' '${ARG_PROTO_COMPONENT}' "
  #              "'${ARG_GPCH_COMPONENT}' '${ARG_PCCH_COMPONENT}' "
  #              "'${ARG_PB2CH_COMPONENT}'")

  ########################################
  # Build various diretory (lists).
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

  set(PYTHON_OUTPUT_DIR)
  if(NOT ARG_SRC_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT ARG_SRC_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
      list(APPEND other_dirs ${ARG_SRC_DIR})

      # RIFT-3214 halfass workaround.  This only works if the SRC_DIR is a subdirectory
      # of CMAKE_CURRENT_SOURCE_DIR
      string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" PYTHON_OUTPUT_DIR ${ARG_SRC_DIR})
      message("SRC_DIR = ${ARG_SRC_DIR} | ${CMAKE_CURRENT_SOURCE_DIR} | ${PYTHON_OUTPUT_DIR}")
    endif()
  endif()

  if(NOT ARG_DEST_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
    list(APPEND other_dirs ${ARG_DEST_DIR})
  endif()
  #message(STATUS "TGS: od: '${other_dirs}'")

  ########################################
  # Default argument lists
  if(NOT ARG_GPROTOC_ARGV)
    set(ARG_GPROTOC_ARGV --cpp_out=${ARG_DEST_DIR})
  endif()

  if(NOT ARG_PROTOCC_ARGV)
    set(ARG_PROTOCC_ARGV --c_out=rift-enforce=1:${ARG_DEST_DIR})
  endif()

  if(NOT ARG_PB2C_ARGV)
    set(ARG_PB2C_ARGV)
  endif()
  #message(STATUS "TGS: av: '${ARG_GPROTOC_ARGV}' '${ARG_PROTOCC_ARGV}' '${ARG_PB2C_ARGV}'")

  ########################################
  set(c_file_list)
  set(cc_file_list)
  set(h_file_list)
  set(dso_file_list)

  foreach(proto_file ${ARG_PROTO_FILES})
    if(NOT proto_file MATCHES "^/")
      set(proto_file ${ARG_SRC_DIR}/${proto_file})
    endif()
    get_filename_component(name ${proto_file} NAME_WE)
    get_filename_component(proto_dir ${proto_file} DIRECTORY)
    #message(STATUS "TGS: fepf: '${name}' '${proto_file}' '${ARG_PROTO_COMPONENT}'")

    if(ARG_PROTO_COMPONENT)
      install(FILES ${proto_file} DESTINATION usr/data/proto COMPONENT ${ARG_PROTO_COMPONENT})
    endif()
    set(dso_file)

    if(ARG_SUPPRESS_PROTO_DEPEND)
      set(proto_file_depend)
    else()
      set(proto_file_depend ${proto_file})
    endif()

    # Compile the protobuf to c++?  .proto->.pb.cc, .pb.h, .dso?
    if(ARG_WITH_GPROTOC)
      set(gpc_base ${ARG_DEST_DIR}/${name}.pb)
      set(gpc_cc_file ${gpc_base}.cc)
      set(gpc_h_file ${gpc_base}.h)

      if(ARG_WITH_DSO AND ARG_NO_PROTOCC)
        set(dso_file ${ARG_DEST_DIR}/${name}.dso)
        set(dso_arg "--descriptor_set_out=${dso_file}")
        list(APPEND dso_file_list ${dso_file})
      else()
        set(dso_arg)
      endif()

      rift_gprotoc_command(command
        ARG_GPROTOC_EXE ${ARG_GPROTOC_EXE}
        PROTO_DIRS ${proto_dir} ${ARG_PROTO_DIRS} ${other_dirs}
        OUT_DEPENDS_VAR extra_depends
        ARGV ${ARG_GPROTOC_ARGV} ${dso_arg} ${proto_file}
      )
      add_custom_command(
        OUTPUT ${gpc_cc_file} ${gpc_h_file} ${dso_file}
        COMMAND ${command}
        DEPENDS ${ARG_DEPENDS} ${extra_depends} ${proto_file_depend}
      )

      list(APPEND cc_file_list ${gpc_cc_file})
      list(APPEND h_file_list ${gpc_h_file})
      if(ARG_GPCH_COMPONENT)
        install(FILES ${gpc_h_file} DESTINATION usr/include/c++ COMPONENT ${ARG_GPCH_COMPONENT})
      endif()
    endif()

    # Compile the protobuf: .proto->.pb-c.*,.dso?
    if(NOT ARG_NO_PROTOCC)
      set(pcc_base ${ARG_DEST_DIR}/${name}.pb-c)
      set(pcc_c_file ${pcc_base}.c)
      set(pcc_h_file ${pcc_base}.h)

      if(ARG_WITH_DSO)
        set(dso_file ${ARG_DEST_DIR}/${name}.dso)
        set(dso_arg "--descriptor_set_out=${dso_file}")
        list(APPEND dso_file_list ${dso_file})
      else()
        set(dso_arg)
      endif()

      rift_protocc_command(command
        ARG_PROTOCC_EXE ${ARG_PROTOCC_EXE}
        PROTO_DIRS ${proto_dir} ${ARG_PROTO_DIRS} ${other_dirs}
        OUT_DEPENDS_VAR extra_depends
        ARGV ${ARG_PROTOCC_ARGV} ${dso_arg} ${proto_file}
      )

      # Generate the proto path args for protoc(python gen)
      rift_proto_path_arg(proto_path_arg PROTO_DIRS ${ARG_PROTO_DIRS})
      set(py_name ${name})
      # protoc generates python files with underscores instead of dashes
      STRING(REGEX REPLACE "-" "_" py_name "${py_name}")
      # Create the path where the python file will be created
      set(py_file "${ARG_DEST_DIR}/${PYTHON_OUTPUT_DIR}/${py_name}_pb2.py")

      add_custom_command(
        OUTPUT ${pcc_c_file} ${pcc_h_file} ${dso_file} ${py_file}
        COMMAND ${command} && /usr/bin/protoc ${proto_path_arg} --python_out=${ARG_DEST_DIR} ${proto_file}
        DEPENDS ${ARG_DEPENDS} ${extra_depends} ${proto_file_depend}
      )

      list(APPEND c_file_list ${pcc_c_file})
      list(APPEND h_file_list ${pcc_h_file})

      if(ARG_PCCH_COMPONENT)
        install(FILES ${pcc_h_file} DESTINATION usr/include COMPONENT ${ARG_PCCH_COMPONENT})
        execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_INSTALL_PREFIX}/usr/data/proto)
        install(FILES ${py_file} DESTINATION usr/data/proto COMPONENT ${ARG_PCCH_COMPONENT})
      endif()
    endif()

    # Create the C struct and XML glue code: .dso->.pb2c.*?
    if(ARG_WITH_PB2C)
      set(p2c_base ${ARG_DEST_DIR}/${name}.pb2c)
      set(p2c_c_file ${p2c_base}.c)
      set(p2c_h_file ${p2c_base}.h)

      rift_pb2c_command(command
        ARG_PB2C_EXE ${ARG_PB2C_EXE}
        OUT_DEPENDS_VAR extra_depends
        ARGV ${ARG_PB2C_ARGV} --dsofile=${dso_file} --genfilebase=${p2c_base}
      )
      add_custom_command(
        OUTPUT ${p2c_c_file} ${p2c_h_file}
        COMMAND ${command}
        DEPENDS ${ARG_DEPENDS} ${extra_depends} ${dso_file}
      )

      list(APPEND c_file_list ${p2c_c_file})
      list(APPEND h_file_list ${p2c_h_file})
      if(ARG_PB2C_COMPONENT)
        install(FILES ${p2c_h_file} DESTINATION usr/include COMPONENT ${ARG_PB2C_COMPONENT})
      endif()
    endif()
  endforeach(proto_file)

  ########################################
  # Export lists
  if(ARG_OUT_C_FILES_VAR)
    set(${ARG_OUT_C_FILES_VAR} ${c_file_list} PARENT_SCOPE)
  endif()

  if(ARG_OUT_CC_FILES_VAR)
    set(${ARG_OUT_CC_FILES_VAR} ${cc_file_list} PARENT_SCOPE)
  endif()

  if(ARG_OUT_H_FILES_VAR)
    set(${ARG_OUT_H_FILES_VAR} ${h_file_list} PARENT_SCOPE)
  endif()

  if(ARG_OUT_DSO_FILES_VAR)
    set(${ARG_OUT_DSO_FILES_VAR} ${dso_file_list} PARENT_SCOPE)
  endif()

  if(ARG_TARGET)
     add_custom_target(${ARG_TARGET} ALL
         DEPENDS ${c_file_list} ${cc_file_list} ${h_file_list} ${dso_file_list})
  else()
    if(ARG_OUT_C_FILES_VAR OR ARG_OUT_CC_FILES_VAR OR ARG_OUT_H_FILES_VAR OR ARG_OUT_DSO_FILES_VAR)
      # nothing
    else()
      #message(STATUS "TGS: to: '${ARG_OUT_C_FILES_VAR}' '${ARG_OUT_CC_FILES_VAR}' '${ARG_OUT_H_FILES_VAR}'")
      #message(STATUS "TGS: to: '${ARG_OUT_DSO_FILES_VAR}' '${ARG_TARGET}'")
      #message(FATAL_ERROR "Must specify TARGET or OUT*")
    endif()
  endif()

endfunction(rift_add_proto_target)


##
# This function creates install rules for artifacts generated from the proto files
# rift_install_proto_artifacts(
#   COMPONENT <component>      # The component to install to.  If empty, install in root
#   PROTO_FILES <proto-files>  # The protobuf files
#   H_FILES <h-files>          # The generated h files
# )
##
function(rift_install_proto_artifacts)
  ##
  # Parse the function arguments
  ##
  set(parse_options "")
  set(parse_onevalargs COMPONENT)
  set(parse_multivalueargs H_FILES PROTO_FILES)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  ##
  # Produce install rules for each type of file
  ##
  if(ARGS_COMPONENT)
    if(ARGS_H_FILES)
      install(FILES ${ARGS_H_FILES} DESTINATION usr/data/proto
	COMPONENT ${ARGS_COMPONENT})
    endif(ARGS_H_FILES)

    if(ARGS_PROTO_FILES)
      install(FILES ${ARGS_PROTO_FILES} DESTINATION usr/data/proto
	COMPONENT ${ARGS_COMPONENT})
    endif(ARGS_PROTO_FILES)

  else(ARGS_COMPONENT)
    if(ARGS_H_FILES)
      install(FILES ${ARGS_H_FILES} DESTINATION usr/data/proto)
    endif(ARGS_H_FILES)

    if(ARGS_PROTO_FILES)
      install(FILES ${ARGS_PROTO_FILES} DESTINATION usr/data/proto)
    endif(ARGS_PROTO_FILES)
  endif(ARGS_COMPONENT)
endfunction(rift_install_proto_artifacts)

