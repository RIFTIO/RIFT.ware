# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

include(CMakeParseArguments)

#macro to construct the rift cache path
macro(rift_cache_path
    cache_path base_dir build_type submodule hash)
  set(${cache_path} ${base_dir}/${build_type}/${submodule}/${hash})
endmacro(rift_cache_path)

# function to determine the checkedout submodules
# Read the .gitmodules file to get the list of all the submodules
# Capture all the lines with the following format:
#     "path = modules/app/ipsec"
# Prune the list of all submodules and determine which are checked out
# rift_find_checkedout_submodules(PROJECT_TOP_DIR <top project directory>
#                                 OUT_SUBMODULES <out variable, list of checkedout submodules>)
function(rift_find_checkedout_submodules)

  # Parse the function arguments
  set(parse_options "")
  set(parse_onevalargs PROJECT_TOP_DIR OUT_SUBMODULES)
  set(parse_multivalueargs "")
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  file(STRINGS ${ARGS_PROJECT_TOP_DIR}/.gitmodules _rows REGEX "^.*path.*$")
  unset(_submodules)
  foreach(_row ${_rows}) 
    # clean up the line and just capture the path
    string(REGEX REPLACE "^.*= ([^ ]+)$" "\\1" _submodule ${_row})
    if(EXISTS ${ARGS_PROJECT_TOP_DIR}/${_submodule}/CMakeLists.txt)
      list(APPEND _submodules ${_submodule})
    endif(EXISTS ${ARGS_PROJECT_TOP_DIR}/${_submodule}/CMakeLists.txt)
  endforeach(_row)

  message("Submodules checked out: ${_submodules}")
  set(${ARGS_OUT_SUBMODULES} "${_submodules}" PARENT_SCOPE)

endfunction(rift_find_checkedout_submodules)

# function to read the submodule dependencies or dependents
# read the .gitmodules.dep file to get the dependencies.
#
# If GET_DEPENDENTS option is provided, return the dependents of the
# submodule instead of the dependencies.
#
# rift_find_submodule_deps(PROJECT_TOP_DIR <top project directory>
#                          SUBMODULE <submodule>
#                          OUT_DEPS <out variable, list of dependencies>
#                          [GET_DEPENDENTS]
#                          )
function(rift_find_submodule_deps)

  # Parse the function arguments
  set(parse_options GET_DEPENDENTS)
  set(parse_onevalargs PROJECT_TOP_DIR SUBMODULE OUT_DEPS)
  set(parse_multivalueargs "")
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(print_dep_flag "--print-dependency")
  if(ARGS_GET_DEPENDENTS)
    set(print_dep_flag "--print-dependents")
  endif(ARGS_GET_DEPENDENTS)

  if (NOT ${ARGS_SUBMODULE} MATCHES "rwbase")
    execute_process(
      COMMAND
      ${ARGS_PROJECT_TOP_DIR}/bin/dependency_parser.py
      --dependency-file=${ARGS_PROJECT_TOP_DIR}/.gitmodules.deps
      --submodule=${ARGS_SUBMODULE}
      ${print_dep_flag}
      RESULT_VARIABLE result
      OUTPUT_VARIABLE _deps
      )

    if(result)
      message("Failed to get dependency information for submodule ${ARGS_SUBMODULE}")
      message(FATAL_ERROR "Error: ${result}")
    endif(result)

    if(_deps)
      message("Submodule ${ARGS_SUBMODULE} deps: ${_deps}")
    endif(_deps)
    set(${ARGS_OUT_DEPS} "${_deps}" PARENT_SCOPE)
  endif()

endfunction(rift_find_submodule_deps)

# Get the build cache hash for the submodule
# The build cache for submodule depends on the
# build cache for the submodule, and on all the submodules
# that it depends on. The dependency_parser.py helps
# in generating a unique hash based on the git hashes
# of all the dependent submodules
# rift_get_submodule_hash(PROJECT_TOP_DIR <project top directory>
#                         SUBMODULE <submodule>
#                         OUT_HASH <out variable, git hash>)
function(rift_get_submodule_hash)

  # Parse the function arguments
  set(parse_options "")
  set(parse_onevalargs PROJECT_TOP_DIR SUBMODULE OUT_HASH)
  set(parse_multivalueargs "")
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})
 
  if (${ARGS_SUBMODULE} MATCHES "rwbase")
    execute_process(
      COMMAND git log -n1 --format=%H .
      OUTPUT_VARIABLE _hash
      ERROR_VARIABLE error
      WORKING_DIRECTORY ${ARGS_PROJECT_TOP_DIR})
  else()
    execute_process(
      COMMAND 
      ${ARGS_PROJECT_TOP_DIR}/bin/dependency_parser.py 
      --dependency-file=${ARGS_PROJECT_TOP_DIR}/.gitmodules.deps
      --submodule=${ARGS_SUBMODULE}
      --output-dir=${CMAKE_CURRENT_BINARY_DIR}
      --print-hash
      OUTPUT_VARIABLE _hash
      ERROR_VARIABLE _error
      WORKING_DIRECTORY ${ARGS_PROJECT_TOP_DIR}
      )
  endif()

  if(_error) 
    message("${_error}")
    message(FATAL_ERROR "Failed to get the git hash for ${ARGS_SUBMODULE}")
  endif(_error)

  # string(REGEX REPLACE "^[+ -]([0-9a-zA-Z]*).*$" "\\1" _hash ${_hash})

  set(${ARGS_OUT_HASH} ${_hash} PARENT_SCOPE)

endfunction(rift_get_submodule_hash)

# function to checkout the submodule
# this function calls "git init submodule", "git update submodule", and
# "git checkout master"
# rift_checkout_submodule(PROJECT_TOP_DIR <project top directory>
#                         SUBMODULE <submodule>)
function(rift_checkout_submodule)

  # Parse the function arguments
  set(parse_options "")
  set(parse_onevalargs PROJECT_TOP_DIR SUBMODULE)
  set(parse_multivalueargs "")
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  unset(_result_code)

  if(_result_code)
    message("Failed to checkout local branch for submodule ${ARGS_SUBMODULE}")
    message(FATAL_ERROR "Result Code: ${_result_code} Error: ${_error}  Output: ${_output}")
  endif(_result_code)

endfunction(rift_checkout_submodule)

# This function fetches the build cache for a submodule
# rift_fetch_submodule_cache(PROJECT_TOP_DIR <project top directory>
#                            BUILD_CACHE_DIR  <base directory for the rift cache>
#                            BUILD_CACHE_TYPE <rift build cache type (Debug, Debug_Coverage, Release)>
#                            SUBMODULE <submodule path>
#                            INSTALL_PREFIX  <prefix for installation directory>
#                            OUT_FOUND <out variable, set to true if found>)
function(rift_fetch_submodule_cache)

  set(${ARGS_OUT_FOUND} 0 PARENT_SCOPE)

endfunction(rift_fetch_submodule_cache)

# This function fetches the build cache for a submodule
# rift_populate_submodule_cache(PROJECT_TOP_DIR <project-root-dir>
#                               BUILD_CACHE_DIR <build-cache-dir>
#                               BUILD_CACHE_TYPE <build-cache-type>
#                               SUBMODULE <submodule>
#                               RPM_DIR <rpm-dir>
#                               OUT_ALREADY_EXISTS <out-variable>)
function(rift_populate_submodule_cache)

  # Parse the function arguments
  set(parse_options "")
  set(parse_onevalargs PROJECT_TOP_DIR BUILD_CACHE_DIR BUILD_CACHE_TYPE SUBMODULE RPM_DIR OUT_ALREADY_EXISTS)
  set(parse_multivalueargs "")
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})
  
  message("Populating the build cache:")
  message("    source rpm dir: ${ARGS_RPM_DIR}")
  message("    submodule: ${ARGS_SUBMODULE}")
  rift_get_submodule_hash(
    PROJECT_TOP_DIR ${ARGS_PROJECT_TOP_DIR} 
    SUBMODULE ${ARGS_SUBMODULE} 
    OUT_HASH _hash
    )
  message("    git hash: ${_hash}")

  rift_cache_path(_cache
    ${ARGS_BUILD_CACHE_DIR} ${ARGS_BUILD_CACHE_TYPE} ${ARGS_SUBMODULE} ${_hash})
  message("    cache dir: ${_cache}")
  if(EXISTS ${_cache})
    message("Build cache already exists for ${ARGS_SUBMODULE}@${_hash}")
    set(${out_alreay_exists} true PARENT_SCOPE)    
  else(EXISTS ${_cache})
    # first create the base directory
    # follow a two step process in the directory creation
    # to avoid two processes trampling on each other
    file(MAKE_DIRECTORY ${ARGS_BUILD_CACHE_DIR}/${ARGS_BUILD_CACHE_TYPE}/${ARGS_SUBMODULE})
    # note that mkdir is called without -p option, to catch an error
    # if the directory already exists
    execute_process(
      COMMAND
      mktemp -d --tmpdir=${ARGS_BUILD_CACHE_DIR}/${ARGS_BUILD_CACHE_TYPE}/${ARGS_SUBMODULE}
      RESULT_VARIABLE _result_code
      WORKING_DIRECTORY ${ARGS_BUILD_CACHE_DIR}/${ARGS_BUILD_CACHE_TYPE}/${ARGS_SUBMODULE}
      OUTPUT_VARIABLE _tmp
      )

    string(STRIP ${_tmp} _tmpdir)
    message("temp dir is ${_tmpdir}")

    set(${ARGS_OUT_ALREADY_EXISTS} false PARENT_SCOPE)
    file(GLOB _rpm_files ${ARGS_RPM_DIR}/*.rpm)
    file(GLOB_RECURSE _rpm_spec_files ${ARGS_RPM_DIR}/*.spec)
    list(APPEND _rpm_files ${_rpm_spec_files})
    file(COPY ${_rpm_files} DESTINATION ${_tmpdir})

    # Generate a log file called cache.log for debugging
    # string(TIMESTAMP _timestamp)
    file(WRITE ${_tmpdir}/cache_${ARGS_BUILD_CACHE_TYPE}_${_hash}.log "Populating the build cache:\n")
    # file(APPEND ${_tmpdir}/cache.log "    timestamp: ${_timestamp}")
    file(APPEND ${_tmpdir}/cache_${ARGS_BUILD_CACHE_TYPE}_${_hash}.log "    submodule: ${ARGS_SUBMODULE}\n")
    file(APPEND ${_tmpdir}/cache_${ARGS_BUILD_CACHE_TYPE}_${_hash}.log "    build cache hash: ${_hash}\n")
    file(APPEND ${_tmpdir}/cache_${ARGS_BUILD_CACHE_TYPE}_${_hash}.log "    source rpm dir: ${ARGS_RPM_DIR}\n")
    file(APPEND ${_tmpdir}/cache_${ARGS_BUILD_CACHE_TYPE}_${_hash}.log "    cache dir: ${_cache}\n")

    file(APPEND ${_tmpdir}/cache_${ARGS_BUILD_CACHE_TYPE}_${_hash}.log "\nThe following files are copied:\n")
    string(LENGTH ${ARGS_RPM_DIR} _len)
    foreach(_rpm_file ${_rpm_files})
      string(SUBSTRING ${_rpm_file} ${_len} -1 _rel_rpm_file)
      file(APPEND ${_tmpdir}/cache_${ARGS_BUILD_CACHE_TYPE}_${_hash}.log "    .${_rel_rpm_file}\n")
    endforeach(_rpm_file)

    string(REPLACE "/" "_" submodule_hash_fname ${ARGS_SUBMODULE})
    string(REPLACE "modules_" "" submodule_hash_fname ${submodule_hash_fname})
    set(submodule_hash_fname ${CMAKE_CURRENT_BINARY_DIR}/${submodule_hash_fname}.hash)
    file(COPY ${submodule_hash_fname} DESTINATION ${_tmpdir})

    
    message("mv ${_tmpdir} ${_cache}")
    # mv silently fails
    execute_process(
      COMMAND
      mv -uTn ${_tmpdir} ${_cache}
      )
    if(EXISTS ${_tmpdir})
      message("Somebody else won the race to populate the build cache for:")
      message("    ${ARGS_SUBMODULE}@${_hash}")
      set(${OUT_ALREADY_EXISTS} true PARENT_SCOPE)    
      execute_process(
        COMMAND
        rm -rf ${_tmpdir}
        )
    else(EXISTS ${_tmpdir})
      execute_process(
        COMMAND
        chmod 755 ${_cache}
        )
    endif(EXISTS ${_tmpdir})

  endif(EXISTS ${_cache})
endfunction(rift_populate_submodule_cache)

# this function determines if the curent build is cacheable
# for a build to be cachable:
#    - the stage should be pristine, i.e., no modifications
#    - build cache is not already populated
# rift_is_cacheable(PROJECT_TOP_DIR <project-top-dir>
#                   CURRENT_DIR <current-dir>
#                   BUILD_CACHE_DIR <build-cache-dir>
#                   BUILD_CACHE_TYPE <build-cache-type>
#                   OUT_IS_CACHEABLE <out-variable>)
function(rift_is_cacheable) 

  # Parse the function arguments
  set(parse_options "")
  set(parse_onevalargs PROJECT_TOP_DIR CURRENT_DIR BUILD_CACHE_DIR BUILD_CACHE_TYPE OUT_IS_CACHEABLE)
  set(parse_multivalueargs "")
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})
  
  # check if this package has been modified after checkout
  execute_process(
    COMMAND
    git status -s
    RESULT_VARIABLE _result_code
    OUTPUT_VARIABLE _output
    ERROR_VARIABLE _error
    WORKING_DIRECTORY ${ARGS_CURRENT_DIR}
    )

  if(_result_code)
    message("Failed to git status for ${ARGS_CURRENT_DIR}")
    message(FATAL_ERROR "Error: ${error}")
  endif(_result_code)

  string(REGEX REPLACE "^${ARGS_PROJECT_TOP_DIR}/(.*)$"  "\\1" _submodule ${ARGS_CURRENT_DIR})

  if(_output)
    message("Local modifications exist, not cacheable")  
    message("    submodule: ${_submodule}")  
    set(${ARGS_OUT_IS_CACHEABLE} 0 PARENT_SCOPE)
  else(_output)
    rift_get_submodule_hash(
      PROJECT_TOP_DIR ${ARGS_PROJECT_TOP_DIR} 
      SUBMODULE ${_submodule} 
      OUT_HASH _hash
      )

    rift_cache_path(_cache
      ${ARGS_BUILD_CACHE_DIR} ${ARGS_BUILD_CACHE_TYPE} ${_submodule} ${_hash})
      
    if(EXISTS ${_cache})  
      message("Cache already exists, no need to cache") 
      message("    submodule: ${_submodule}")
      message("    hash: ${_hash}") 
      set(${ARGS_OUT_IS_CACHEABLE} 0 PARENT_SCOPE)
    else(EXISTS ${_cache})

      # Check if a submodule had a unit test failure
      execute_process(
        COMMAND ${ARGS_PROJECT_TOP_DIR}/bin/submodule_has_failed_tests.sh ${_submodule}
        RESULT_VARIABLE _result_code
        OUTPUT_VARIABLE _output
        ERROR_VARIABLE _error
        WORKING_DIRECTORY ${ARGS_PROJECT_TOP_DIR}
      )
      message ("${_output}")

      # If the script returned 
      if(_result_code AND NOT ${_submodule} MATCHES "rwbase")
        set(${ARGS_OUT_IS_CACHEABLE} 1 PARENT_SCOPE)
        message("Can be cached") 
        message("    submodule: ${_submodule}")
        message("    hash: ${_hash}")      
      else()
        set(${ARGS_OUT_IS_CACHEABLE} 0 PARENT_SCOPE)
      endif()

    endif(EXISTS ${_cache}) 

  endif(_output)

endfunction(rift_is_cacheable)

# this function invokes the cpack command to build the rpm packages
# searches for CPackConfig-*.cmake files in the current bin hierarchy and
# calls cpack.
# rift_build_packages(CURRENT_BIN_DIR <current-bin-dir>)
function(rift_build_packages)

  # Parse the function arguments
  set(parse_options "")
  set(parse_onevalargs CURRENT_BIN_DIR)
  set(parse_multivalueargs "")
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  file(GLOB_RECURSE files "${ARGS_CURRENT_BIN_DIR}/CPackConfig-*.cmake")
  foreach(file ${files})
    get_filename_component(cpack_file ${file} NAME)
    get_filename_component(cpack_dir ${file} PATH)

    message("Generating RPM package:")
    message("    ${cpack_dir}")
    message("    ${cpack_file}")

    # OUTPUT_* and ERROR_* variables are NOT specified
    # They are piped to the CMAKE process
    execute_process(
      COMMAND
      # cpack --debug --verbose --config ${pkg_file}
      cpack --config ${cpack_file}
      RESULT_VARIABLE _result_code
      WORKING_DIRECTORY ${cpack_dir}
      )
    if(_result_code)
      message(FATAL_ERROR "Failed to generate RPM for ${file}")
    endif(_result_code)

  endforeach(file)
endfunction(rift_build_packages)
