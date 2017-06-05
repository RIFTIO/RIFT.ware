# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tom Seidenberg
# Creation Date: 2014/02/18
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

##
# function make a path variable out of a list
# rift_list_to_pathvar(<var> <entries>...)
##
function(rift_list_to_path var) 
  set(path)
  foreach(dir ${ARGN})
    if(path)
      set(path "${path}:${dir}")
    else()
      set(path ${dir})
    endif()
  endforeach()
  set(${var} "${path}" PARENT_SCOPE)
endfunction(rift_list_to_path var)


##
# function to put a path prefix on a list of files
# rift_files_prepend_path(<var> <path> <entries>...)
#
# Example:
#   rift_files_prepend_path(v /usr bin lib):
# Result same as:
#   set(v /usr/bin /usr/lib /usr/include)
##
function(rift_files_prepend_path var root)
  set(retval)
  foreach(file ${ARGN})
    list(APPEND retval "${root}/${file}")
  endforeach()
  set(${var} "${retval}" PARENT_SCOPE)
endfunction(rift_files_prepend_path var)


# function to prepend CMAKE_CURRENT_BINARY_DIR to a list of files
# rift_files_prepend_src_dir(<var> <entries>...)
##
function(rift_files_prepend_src_dir var)
  rift_files_prepend_path(retval ${CMAKE_CURRENT_BINARY_DIR} ${ARGN})
  set(${var} "${retval}" PARENT_SCOPE)
endfunction(rift_files_prepend_src_dir var)


# function to prepend CMAKE_CURRENT_SOURCE_DIR to a list of files
# rift_files_prepend_src_dir(<var> <entries>...)
##
function(rift_files_prepend_src_dir var)
  rift_files_prepend_path(retval ${CMAKE_CURRENT_SOURCE_DIR} ${ARGN})
  set(${var} "${retval}" PARENT_SCOPE)
endfunction(rift_files_prepend_src_dir var)


##
# This macro sets the C and CXX flags to supress the given warning
##
macro(rift_allow_compiler_warning warn)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-${warn}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-${warn}")
endmacro()

macro(rift_werror)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
endmacro()

# utility macro to print the cmake variables
macro(rift_print_cmake_vars)
  get_cmake_property(_variableNames VARIABLES)
  foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
  endforeach()
endmacro(rift_print_cmake_vars)

##
# This function copies a source file to a destination and creates a 
# target that can be used as a dependency
##
function(rift_copy_file source_file dest_file)
  # Parse the function arguments
  set(parse_options "")
  set(parse_onevalargs TARGET)
  set(parse_multivalueargs "")
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" 
    "${parse_multivalueargs}" ${ARGN})

  ##
  # This defines a command to generate specified OUTPUT file(s)
  # A target created in the same directory (CMakeLists.txt file) 
  # that specifies any output of the
  # custom command as a source file is given a rule to generate the 
  # file using the command at build time.
  ##
  add_custom_command(
    OUTPUT ${dest_file}
    COMMAND ${CMAKE_COMMAND} -E copy ${source_file} ${dest_file}
    DEPENDS ${source_file}
    )

  # Deal with ARGS_TARGET
  if (ARGS_TARGET)
    ##
    # Adds a target with the given name that executes the given commands
    # The target has no output file and is ALWAYS CONSIDERED OUT OF DATE
    #
    # The ALL option indicates that this target should be added to 
    # the default build target
    ##
    add_custom_target(
      ${ARGS_TARGET} ALL
      DEPENDS ${dest_file}
      )
  endif()
endfunction()

##
# This function converts a camel case string to '_' notation.
# rift_uncamel(<source-string> <destination-string>)
##
function(rift_uncamel source destination)
    # Use python regular expressions to drive this conversion
    execute_process(
      COMMAND
      python -c "import re; print re.sub('((?<=[a-z0-9])[A-Z]|(?s)[A-Z](?=[a-z]))',
                   r'_\\1', '${source}').lower().strip('_')"
      RESULT_VARIABLE result
      OUTPUT_VARIABLE UNCAMEL_STRING
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(result)
      message("Failed to convert ${source} to uncamel_case")
      message(FATAL_ERROR "Error: ${result}")
    endif(result)
    set(${destination} ${UNCAMEL_STRING} PARENT_SCOPE)
endfunction(rift_uncamel)

##
# This function converts '_' to camel case.
# rift_uncamel(<source-string> <destination-string>)
##
function(rift_camel source destination)
    # Use python regular expressions to drive this conversion
    execute_process(
      COMMAND
      python -c "import re; temp = re.sub(r'(?!^)[-_]([0-9a-zA-Z])', lambda m:
      m.group(1).upper(), '${source}'); print temp[0].upper() + temp[1:]"
      RESULT_VARIABLE result
      OUTPUT_VARIABLE UNCAMEL_STRING
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(result)
      message("Failed to convert ${source} to CamelCase")
      message(FATAL_ERROR "Error: ${result}")
    endif(result)
    set(${destination} ${UNCAMEL_STRING} PARENT_SCOPE)
endfunction(rift_camel)

