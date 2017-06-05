
# STANDARD_RIFT_IO_COPYRIGHT

cmake_minimum_required(VERSION 2.8)

rift_add_subdirs(SUBDIR_LIST python include)

# Generate the riftware version header
find_package(Git)

if(NOT DEFINED BUILD_HASH)
    message(FATAL_ERROR "Build hash not defined")
endif(NOT DEFINED BUILD_HASH)

execute_process(
  COMMAND date -u
  RESULT_VARIABLE DATE_RC
  OUTPUT_VARIABLE DATE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )
if(${DATE_RC})
  message(FATAL_ERROR "Couldn't get date?")
endif()

file(WRITE ${CMAKE_BINARY_DIR}/include/riftware-version.h
"\#define RIFTWARE_VERSION \"${RIFT_VERSION}\"\n"
"\#define RIFTWARE_GIT \"${BUILD_HASH}\"\n"
"\#define RIFTWARE_DATE \"${DATE}\"\n"
)

install(FILES ${CMAKE_BINARY_DIR}/include/riftware-version.h
  DESTINATION usr/include/riftware
  COMPONENT ${TOOLCHAIN_INSTALL_NAME}
  )
