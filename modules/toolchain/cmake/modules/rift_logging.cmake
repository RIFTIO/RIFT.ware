# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 12/10/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)
# This file contains cmake macros for generating documentation

include(CMakeParseArguments)

##
# Create a top level include file for event logging
# rift_generate_event_log_header(HFILE_NAME <header-filename>
#                                EVT_XML_FILES <xml-file-list>)
##
function(rift_generate_event_log_header TARGET)
  set(parse_options "")
  set(parse_onevalargs HFILE_NAME)
  set(parse_multivalueargs EVT_XML_FILES)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})


  # Build a script that will "build" our header for us.
  get_filename_component(_base_header ${ARGS_HFILE_NAME} NAME_WE)
  set(fname ${CMAKE_CURRENT_BINARY_DIR}/build_${_base_header}.sh)
  file(WRITE ${fname} "#!/bin/bash\n\n")
  file(APPEND ${fname} "cat <<-EOF > ${ARGS_HFILE_NAME}\n")
  file(APPEND ${fname} "/**\n")
  file(APPEND ${fname} " * This is an auto-generated file\n")
  file(APPEND ${fname} " * DO NOT EDIT\n")
  file(APPEND ${fname} " *\n")
  file(APPEND ${fname} " * All the subsystems using event logging must include this header file\n")
  file(APPEND ${fname} " *\n")
  file(APPEND ${fname} " */\n")

  file(APPEND ${fname} "#ifndef __RWLOG_EVENT_AUTO_H__\n")
  file(APPEND ${fname} "#define __RWLOG_EVENT_AUTO_H__\n")
  file(APPEND ${fname} "\n")
  file(APPEND ${fname} "#include \"rwlog.h\"\n")
  file(APPEND ${fname} "\n")

  foreach(f ${ARGS_EVT_XML_FILES})
    get_filename_component(basename ${f} NAME_WE)
    file(APPEND ${fname} "#include \"rwlog/${basename}_auto.h\"\n")
  endforeach(f)

  file(APPEND ${fname} "\n")
  file(APPEND ${fname} "#endif //__RWLOG_EVENT_AUTO_H__\n")
  file(APPEND ${fname} "EOF\n")


  add_custom_command(
    OUTPUT ${ARGS_HFILE_NAME}
    COMMAND /bin/bash ${fname}
    DEPENDS ${ARGS_EVT_XML_FILES}
    COMMENT "Generating ${ARGS_HFILE_NAME}")

  add_custom_target(${TARGET} ALL DEPENDS ${ARGS_HFILE_NAME})
endfunction(rift_generate_event_log_header)

##
# This function adds target for processing xml event files
# rift_process_event_log_target(TARGET
#                               EVT_SCHEMA <name-of-schema-file>
#                               COMMON_PROTOBUF_HDR <proto buf coomon log header>
#                               EVT_XML_FILES <list-of-xml-evt-file>
#                               DEPENDS <target-dependencies>
#                               OUT_AUTO_C_VAR <name-of-the-var-to-hold-auto-c-file-list
#                               OUT_AUTO_H_VAR <name-of-the-var-to-hold-auto-h-file-list)
##
function(rift_process_event_log_target TARGET)
  set(parse_options "")
  set(parse_onevalargs EVT_SCHEMA OUT_AUTO_C_VAR OUT_AUTO_H_VAR COMMON_PROTOBUF_HDR)
  set(parse_multivalueargs EVT_XML_FILES DEPENDS)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(files)
  set(c_files)
  set(h_files)

  # Compile the common protobuf header
  get_filename_component(common_protobuf_basename ${ARGS_COMMON_PROTOBUF_HDR} NAME_WE)
  rift_add_proto_target(
    PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_COMMON_PROTOBUF_HDR}
    DEPENDS ${ARGS_DEPENDS}
    OUT_C_FILES_VAR common_c_files
    OUT_H_FILES_VAR common_h_files
  )
  list(APPEND files ${common_c_files} ${common_h_files})
  list(APPEND c_files ${common_c_files})

  make_directory(${CMAKE_CURRENT_BINARY_DIR}/rwlog)

  foreach(rwlog_evt_file ${ARGS_EVT_XML_FILES})
    # parse the xml file and generate .c and .h files
    get_filename_component(basename ${rwlog_evt_file} NAME_WE)
    add_custom_command(
      OUTPUT 
        ${CMAKE_CURRENT_BINARY_DIR}/${basename}_auto.c
        ${CMAKE_CURRENT_BINARY_DIR}/rwlog/${basename}_auto.h
        ${CMAKE_CURRENT_BINARY_DIR}/${basename}_auto.proto
      COMMAND xmllint --noout --schema ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_EVT_SCHEMA}
              ${CMAKE_CURRENT_SOURCE_DIR}/${rwlog_evt_file}
      COMMAND 
        ${CMAKE_CURRENT_BINARY_DIR}/rwlog_event_parser 
        ${CMAKE_CURRENT_SOURCE_DIR}/${rwlog_evt_file} ${basename}_auto
      COMMAND mv ${CMAKE_CURRENT_BINARY_DIR}/${basename}_auto.h  ${CMAKE_CURRENT_BINARY_DIR}/rwlog

      DEPENDS
        # causes the command to re-run when ever this target recompiles
        # this command also gets executed whenever xml schema changes
        ${ARGS_DEPENDS} 
        ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_EVT_SCHEMA}
        ${rwlog_evt_file}
        ${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_COMMON_PROTOBUF_HDR}
      COMMENT "Generating
        ${CMAKE_CURRENT_BINARY_DIR}/${basename}_auto.c
        ${CMAKE_CURRENT_BINARY_DIR}/rwlog/${basename}_auto.h
        ${CMAKE_CURRENT_BINARY_DIR}/${basename}_auto.proto"

    )
    list(APPEND files ${CMAKE_CURRENT_BINARY_DIR}/rwlog/${basename}_auto.h)
    list(APPEND files ${CMAKE_CURRENT_BINARY_DIR}/${basename}_auto.c)
    list(APPEND files ${CMAKE_CURRENT_BINARY_DIR}/${basename}_auto.proto)
    list(APPEND c_files ${CMAKE_CURRENT_BINARY_DIR}/${basename}_auto.c)
    list(APPEND h_files ${CMAKE_CURRENT_BINARY_DIR}/rwlog/${basename}_auto.h)

    # Compile the auto generated protobufs
    rift_add_proto_target(
      PROTO_FILES ${CMAKE_CURRENT_BINARY_DIR}/${basename}_auto.proto
      OUT_C_FILES_VAR auto_c_files
      OUT_H_FILES_VAR auto_h_files
    )
    list(APPEND files ${auto_c_files} ${auto_h_files})
    list(APPEND c_files ${auto_c_files})
  endforeach(rwlog_evt_file)

  add_custom_target(${TARGET} DEPENDS ${files})
  add_dependencies(${TARGET} ${ARGS_DEPENDS})

  set(${ARGS_OUT_AUTO_C_VAR} ${c_files} PARENT_SCOPE)
  set(${ARGS_OUT_AUTO_H_VAR} ${h_files} PARENT_SCOPE)


endfunction(rift_process_event_log_target)


##
# This function adds target for generating a rwlog yang file
# with a new category name which can be used by rwlog handle
# in python (via rwlog.set_category()).
#
# Update rw-log.yang whenever this function is used to keep it
# up to date.
#
# rift_generate_python_log_yang(LOG_CATEGORY_NAME
#                               START_EVENT_ID
#                               OUT_YANG_FILE_VAR)
##
function(rift_generate_python_log_yang)
  set(parse_options "")
  set(parse_onevalargs TARGET LOG_CATEGORY_NAME START_EVENT_ID OUT_YANG_FILE_VAR)
  set(parse_multivalueargs)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  if(NOT ARGS_LOG_CATEGORY_NAME)
      message(FATAL "Must specify LOG_CATEGORY_NAME")
  endif()

  if(NOT ARGS_START_EVENT_ID)
      message(FATAL "Must specify START_EVENT_ID")
  endif()

  set(out_yang_file ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_LOG_CATEGORY_NAME}.yang)

  ##
  # Because add_yang_target expects the files to exist during dependency
  # generation time, we have no choice but to regenerate the yang file on
  # every run.  However this is not frequently called and is relatively inexpensive.
  ##
  execute_process(
    COMMAND python3 ${RIFT_CMAKE_BIN_DIR}/generate_python_log_yang.py
        --output-file ${out_yang_file}
        --category-name ${ARGS_LOG_CATEGORY_NAME}
        --start-event-id ${ARGS_START_EVENT_ID}
      RESULT_VARIABLE result
  )

  if(result)
    message(FATAL_ERROR "Error: failed to generate log yang file")
  endif()

  set(${ARGS_OUT_YANG_FILE_VAR} ${out_yang_file} PARENT_SCOPE)

endfunction(rift_generate_python_log_yang)
