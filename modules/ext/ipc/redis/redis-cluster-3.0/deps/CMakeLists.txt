cmake_minimum_required(VERSION 2.8)

include (ExternalProject)

set(subdirs
    hiredis
    jemalloc
    linenoise
    lua)

add_subdirectory(jemalloc)
add_subdirectory(hiredis)
add_subdirectory(linenoise)
add_subdirectory(lua)

