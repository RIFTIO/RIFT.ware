
# STANDARD_RIFT_IO_COPYRIGHT

include(ExternalProject)

# Wrapper around the standard CMake externalproject_add().  Arguments are
# the same so you can refer to upstream documentation.
#
# This enhances the command to use git to see if the source tree has been
# modified since the last build.  There are two checks.  If the tree is clean,
# then the sha of the last attempted build is checked.  If the tree is dirty, a
# checksum of the current diff stat is checked against the previous.
#
# If the source has changed it will trigger a rebuild not only of this
# externalproject but also of any other projects depending on this one.
#
# As external projects should be included by pulling directly from the upstream
# repository, rift_externalproject_add() will always rsync the source to the
# build directory and then build from there.  This is because aside from CMake,
# most build systems have a pre-configure step required when using the source
# directly which cannot be done from outside of the source tree.  It's easier
# to just abstract that away here and rsync is fast.
#
# It also appends the target name to RIFT_EXTERNAL_PACKAGE_LIST
#
# Extra Parameters
#
# BCACHE_COMMAND:  Command to populate the build cache
#   make DESTDIR=${RIFT_SUBMODULE_INSTALL_PREFIX}/${target} install
#
function(rift_externalproject_add target)
  set(parse_options "")
  set(parse_onevalargs STAMP_DIR SOURCE_DIR PREFIX BINARY_DIR)
  set(parse_multivalueargs DOWNLOAD_COMMAND
    BCACHE_COMMAND INSTALL_COMMAND)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  if (NOT ARGS_SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR must be set for external projects")
  endif()

  if (NOT ARGS_STAMP_DIR)
    set(ARGS_STAMP_DIR ${CMAKE_CURRENT_BINARY_DIR}/${target}-stamp)
  endif()

  if (NOT ARGS_PREFIX)
    set(ARGS_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/${target})
  endif()

  if (NOT ARGS_BINARY_DIR)
    set(ARGS_BINARY_DIR ${ARGS_PREFIX}/${target}-build)
  endif()

  if (ARGS_DOWNLOAD_COMMAND)
    message(FATAL_ERROR "DOWNLOAD_COMMAND is not allowed for external projects")
  endif()

  if (NOT ARGS_BCACHE_COMMAND)
    set(ARGS_BCACHE_COMMAND
      make DESTDIR=${RIFT_SUBMODULE_INSTALL_PREFIX}/${target} install
    )
  endif()

  if (NOT ARGS_INSTALL_COMMAND)
    set(ARGS_INSTALL_COMMAND make install)
  endif()

  set(RIFT_EXTERNAL_PACKAGE_LIST ${RIFT_EXTERNAL_PACKAGE_LIST} ${target}
    CACHE INTERNAL "External Project Target List")

  externalproject_add(${target}
    PREFIX ${ARGS_PREFIX}
    SOURCE_DIR ${ARGS_BINARY_DIR}
    BINARY_DIR ${ARGS_BINARY_DIR}
    STAMP_DIR ${ARGS_STAMP_DIR}
    DOWNLOAD_COMMAND ""
    INSTALL_COMMAND ${ARGS_INSTALL_COMMAND}
    ${ARGS_UNPARSED_ARGUMENTS})

  rift_externalproject_sha_check(${target}
    BINARY_DIR ${ARGS_BINARY_DIR}
    SOURCE_DIR ${ARGS_SOURCE_DIR}
    STAMP_DIR ${ARGS_STAMP_DIR})

  externalproject_add_step(${target} rsync_to_build
    COMMAND rsync -av --exclude='.git*' ${ARGS_SOURCE_DIR}/ ${ARGS_BINARY_DIR}/
    DEPENDERS update patch configure)

  # Populate the build cache
  externalproject_add_step(${target} bcache
    COMMAND ${ARGS_BCACHE_COMMAND}
    DEPENDEES install
    WORKING_DIRECTORY <BINARY_DIR>)
endfunction()

# Create a new target (externalproject_${TARGET}_sha) which will
# use git to see if the source tree has been modified since the
# last build.  There are two checks.  If the tree is clean, then
# the sha of the last attempted build is checked.  If the tree is
# dirty, a checksum of the current diff stat is checked against
# the previous.
#
# If the project has been modified, the cmake stamp files will be
# removed, triggering a rebuild.
#
# If DEPENDENT_EXTERNAL_TARGETS is provided, dependent external projects
# sha's will be cleared when this external project has been modified.
#
# Parameters:
#
# TARGET                      - name of the externalproject target.
# BINARY_DIR                  - BINARY_DIR passed to externalproject_add.
# SOURCE_DIR                  - SOURCE_DIR passed to externalproject_add.
# STAMP_DIR                   - STAMP_DIR passed to externalproject_add.
# GIT_DIR                     - GIT_DIR of clone containing the externalproject.
# DEPENDENT_EXTERNAL_TARGETS  - List of dependent external project target names.
function(rift_externalproject_sha_check TARGET)
  set(parse_options "")
  set(parse_onevalargs BINARY_DIR SOURCE_DIR STAMP_DIR GIT_DIR)
  set(parse_multivalargs DEPENDENT_EXTERNAL_TARGETS)
  cmake_parse_arguments(ARGS "" "${parse_onevalargs}" "${parse_multivalargs}" ${ARGN})

  if (NOT ARGS_BINARY_DIR OR NOT ARGS_SOURCE_DIR OR NOT ARGS_STAMP_DIR)
    message(FATAL_ERROR "${TARGET}: missing arguments to rift_externalproject_sha_check")
  endif()

  if (NOT ARGS_GIT_DIR)
    set(ARGS_GIT_DIR ${CMAKE_SOURCE_DIR}/.git)
  endif()

  set(shafile ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-sha)
  set(diffsha ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-diff-sha)

  # If the DEPENDENT_EXTERNAL_TARGETS was provided, contruct a list of sha globs
  # for dependent external projects that we will delete if this external project
  # was changed.
  if(ARGS_DEPENDENT_EXTERNAL_TARGETS)
    set(dependent_sha_glob_pattern)
    foreach(dependent_target ${ARGS_DEPENDENT_EXTERNAL_TARGETS})
      LIST(APPEND dependent_sha_glob_pattern ${CMAKE_CURRENT_BINARY_DIR}/${dependent_target}-*sha)
    endforeach(dependent_target)
  endif(ARGS_DEPENDENT_EXTERNAL_TARGETS)

  add_custom_target(externalproject_${TARGET}_sha ALL
    COMMAND
      if ! GIT_DIR=${ARGS_GIT_DIR} git diff --quiet HEAD -- ${ARGS_SOURCE_DIR}\; then
        GIT_DIR=${ARGS_GIT_DIR} git diff -- ${ARGS_SOURCE_DIR} > ${diffsha}.tmp \;
        cmp ${diffsha}.tmp ${diffsha} || rm -f ${ARGS_STAMP_DIR}/${TARGET}-* ${dependent_sha_glob_pattern} \;
        mv ${diffsha}.tmp ${diffsha} \;
        rm -f ${shafile} \;
      else
        GIT_DIR=${ARGS_GIT_DIR} git log -n1 --format=%h ${ARGS_SOURCE_DIR} > ${shafile}.tmp \;
        cmp ${shafile} ${shafile}.tmp || rm -f ${ARGS_STAMP_DIR}/${TARGET}-* ${dependent_sha_glob_pattern} \;
        mv ${shafile}.tmp ${shafile} \;
        rm -f ${diffsha} \;
      fi
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Validating sha of previous build for ${TARGET}, subdirectory ${ARGS_SOURCE_DIR}")

  add_dependencies(${TARGET} externalproject_${TARGET}_sha)

endfunction()


