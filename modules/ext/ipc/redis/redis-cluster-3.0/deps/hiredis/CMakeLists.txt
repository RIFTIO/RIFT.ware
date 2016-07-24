# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Vijay Nag
# Creation Date: 09/20/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

include (ExternalProject)
#set(PKG_NAME hiredis)
#PROJECT(hiredis)
#set(HIREDIS_SRC_DIR .)
#set(HIREDIS_SRC_DIR ${PROJECT_SRC_DIR})

set(CMAKE_C_FLAGS "-O3 -g -ggdb -fPIC -Wall -W -Wstrict-prototypes -Wwrite-strings")

set(hiredis_src
    ${CMAKE_CURRENT_SOURCE_DIR}/net.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hiredis.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sds.c
    ${CMAKE_CURRENT_SOURCE_DIR}/async.c
   )

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/adapters)

add_library(hiredis STATIC ${hiredis_src})
add_library(hiredis_s SHARED ${hiredis_src})

set(HIREDIS_VERSION_MAJOR 0)
set(HIREDIS_VERSION_MINOR 10)
set(HIREDIS_VERSION_STRING ${HIREDIS_VERSION_MAJOR}.${HIREDIS_VERSION_MINOR})
set_target_properties(hiredis_s  PROPERTIES VERSION ${HIREDIS_VERSION_STRING}
                                          SOVERSION ${HIREDIS_VERSION_MAJOR})

add_executable(hiredis-test
              ${CMAKE_CURRENT_SOURCE_DIR}/test.c)
target_link_libraries(hiredis-test hiredis)

add_executable(hiredis-example
              ${CMAKE_CURRENT_SOURCE_DIR}/examples/example.c)
target_link_libraries(hiredis-example hiredis)

set(install_hiredis_include_files
    ${CMAKE_CURRENT_SOURCE_DIR}/dict.h
    ${CMAKE_CURRENT_SOURCE_DIR}/async.h
    ${CMAKE_CURRENT_SOURCE_DIR}/net.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hiredis.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fmacros.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zmalloc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sds.h
   )

set(install_hiredis_adapter_include_files
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/libuv.h
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/libevent.h
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/libev.h
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/ae.h)

install(FILES ${install_hiredis_include_files} DESTINATION usr/include/redis/hiredis COMPONENT ${PKG_NAME})
install(FILES ${install_hiredis_adapter_include_files} DESTINATION usr/include/redis/hiredis/adapter COMPONENT ${PKG_NAME})
install(TARGETS hiredis_s LIBRARY DESTINATION usr/lib/ COMPONENT ${PKG_NAME})
install(TARGETS hiredis ARCHIVE DESTINATION usr/lib/ COMPONENT ${PKG_NAME})

