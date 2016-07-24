# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Vijay Nag
# Creation Date: 09/20/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)
set(CMAKE_C_FLAGS "-Os -Wall")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/)

set(lua_src_files
${CMAKE_CURRENT_SOURCE_DIR}/src/lapi.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lcode.c
${CMAKE_CURRENT_SOURCE_DIR}/src/ldebug.c
${CMAKE_CURRENT_SOURCE_DIR}/src/ldo.c
${CMAKE_CURRENT_SOURCE_DIR}/src/ldump.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lfunc.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lgc.c
${CMAKE_CURRENT_SOURCE_DIR}/src/llex.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lmem.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lobject.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lopcodes.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lparser.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lstate.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lstring.c
${CMAKE_CURRENT_SOURCE_DIR}/src/ltable.c
${CMAKE_CURRENT_SOURCE_DIR}/src/ltm.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lundump.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lvm.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lzio.c
${CMAKE_CURRENT_SOURCE_DIR}/src/strbuf.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lauxlib.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lbaselib.c
${CMAKE_CURRENT_SOURCE_DIR}/src/ldblib.c
${CMAKE_CURRENT_SOURCE_DIR}/src/liolib.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lmathlib.c
${CMAKE_CURRENT_SOURCE_DIR}/src/loslib.c
${CMAKE_CURRENT_SOURCE_DIR}/src/ltablib.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lstrlib.c
${CMAKE_CURRENT_SOURCE_DIR}/src/loadlib.c
${CMAKE_CURRENT_SOURCE_DIR}/src/linit.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lua_cjson.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lua_struct.c
${CMAKE_CURRENT_SOURCE_DIR}/src/lua_cmsgpack.c)

add_library(lua STATIC ${lua_src_files})

add_executable(luaex
              ${CMAKE_CURRENT_SOURCE_DIR}/src/lua.c)
target_link_libraries(luaex lua m)

add_executable(luaexc
              ${CMAKE_CURRENT_SOURCE_DIR}/src/luac.c
              ${CMAKE_CURRENT_SOURCE_DIR}/src/print.c)
target_link_libraries(luaexc lua m)

set(install_header_files
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lapi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lauxlib.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lcode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ldebug.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ldo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lfunc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lgc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/llex.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/llimits.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lmem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lobject.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lopcodes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lparser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lstate.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lstring.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltable.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltm.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lua.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/luaconf.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lualib.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lundump.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lvm.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lzio.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strbuf.h)

install(FILES ${install_header_files} DESTINATION usr/include/redis/lua/ COMPONENT ${PKG_NAME})

