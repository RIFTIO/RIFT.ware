# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Vijay Nag
# Creation Date: 09/20/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/myrelease.h
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/mkreleasehdr.sh
    DEPENDS jemallocmake)

set(CMAKE_C_FLAGS "-std=c99 -pedantic -Wall -O2 -g -ggdb -DUSE_JEMALLOC")

link_directories(${CMAKE_CURRENT_BINARY_DIR}/../deps/hiredis/
    ${CMAKE_CURRENT_BINARY_DIR}/../deps/lua/
    ${CMAKE_CURRENT_BINARY_DIR}/../deps/jemalloc/lib/)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/
                    ${CMAKE_CURRENT_SOURCE_DIR}/../deps/hiredis/
                    ${CMAKE_CURRENT_SOURCE_DIR}/../deps/linenoise/
                    ${CMAKE_CURRENT_SOURCE_DIR}/../deps/lua/src/
                    ${CMAKE_CURRENT_BINARY_DIR}/../deps/jemalloc/include/
                    ${CMAKE_CURRENT_BINARY_DIR}/)

set(redis_server_src_files
    adlist.c
    ae.c
    anet.c
    dict.c
    redis.c
    sds.c
    latency.c
    sparkline.c
    zmalloc.c
    lzf_c.c
    lzf_d.c
    pqsort.c
    zipmap.c
    sha1.c
    ziplist.c
    release.c
    networking.c
    util.c
    object.c
    db.c
    replication.c
    rdb.c
    t_string.c
    t_list.c
    t_set.c
    t_zset.c
    t_hash.c
    config.c
    aof.c
    pubsub.c
    multi.c
    debug.c
    sort.c
    intset.c
    syncio.c
    cluster.c
    crc16.c
    endianconv.c
    slowlog.c
    scripting.c
    bio.c
    rio.c
    rand.c
    memtest.c
    crc64.c
    bitops.c
    sentinel.c
    notify.c
    setproctitle.c
    blocked.c
    hyperloglog.c
    )

add_executable(redis-server
               ${CMAKE_CURRENT_BINARY_DIR}/myrelease.h
               ${redis_server_src_files})
target_link_libraries(redis-server
                      hiredis
                      ${CMAKE_CURRENT_BINARY_DIR}/../deps/jemalloc/lib/libjemalloc.a
                      lua
                      m
                      pthread
                      dl)

set(redis_cli_src_files
    ${CMAKE_CURRENT_BINARY_DIR}/myrelease.h
    redis-cli.c
    ae.c
    anet.c
    sds.c
    adlist.c
    zmalloc.c
    release.c
    crc64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../deps/linenoise/linenoise.c)

add_executable(redis-cli
               ${redis_cli_src_files})
target_link_libraries(redis-cli
                      hiredis
                      lua
                      ${CMAKE_CURRENT_BINARY_DIR}/../deps/jemalloc/lib/libjemalloc.a
                      m
                      pthread
                      dl)

set(redis_benchmark_src_files
    ${CMAKE_CURRENT_BINARY_DIR}/myrelease.h
    redis-benchmark.c
    ae.c
    anet.c
    sds.c
    adlist.c
    zmalloc)
add_executable(redis-benchmark
               ${redis_benchmark_src_files})
target_link_libraries(redis-benchmark
                      hiredis
                      lua
                      ${CMAKE_CURRENT_BINARY_DIR}/../deps/jemalloc/lib/libjemalloc.a
                      m
                      pthread
                      dl)

set(redis_check_dump_src_files
    ${CMAKE_CURRENT_BINARY_DIR}/myrelease.h
    redis-check-dump.c
    lzf_c.c
    lzf_d.c
    crc64)
add_executable(redis-check-dump
               ${redis_check_dump_src_files})
target_link_libraries(redis-benchmark
                      dl)
add_executable(redis-check-aof
               redis-check-aof.c)
target_link_libraries(redis-check-aof
                      m
                      pthread)

set(install_header_files
    ${CMAKE_CURRENT_SOURCE_DIR}/ae.h
    ${CMAKE_CURRENT_SOURCE_DIR}/adlist.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bio.h
    ${CMAKE_CURRENT_SOURCE_DIR}/asciilogo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/anet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster.h
    ${CMAKE_CURRENT_SOURCE_DIR}/crc64.h
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/intset.h
    ${CMAKE_CURRENT_SOURCE_DIR}/help.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fmacros.h
    ${CMAKE_CURRENT_SOURCE_DIR}/endianconv.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dict.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lzfP.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lzf.h
    ${CMAKE_CURRENT_SOURCE_DIR}/latency.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rdb.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/pqsort.h
    ${CMAKE_CURRENT_SOURCE_DIR}/redisassert.h
    ${CMAKE_CURRENT_SOURCE_DIR}/redis.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sds.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rio.h
    ${CMAKE_CURRENT_SOURCE_DIR}/solarisfixes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/slowlog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sha1.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sparkline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ziplist.h
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/testhelp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zmalloc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zipmap.h
    )

add_library(rediscrc crc16.c crc64.c)
add_dependencies(rediscrc jemallocmake)

install(FILES ${install_header_files} DESTINATION usr/include/redis COMPONENT ${PKG_NAME})
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/redis-server DESTINATION usr/bin/ COMPONENT ${PKG_NAME})
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/redis-cli DESTINATION usr/bin/ COMPONENT ${PKG_NAME})
install(TARGETS rediscrc ARCHIVE DESTINATION usr/lib/ COMPONENT ${PKG_NAME})
install(PROGRAMS redis-trib.rb DESTINATION usr/bin/ COMPONENT ${PKG_NAME})


