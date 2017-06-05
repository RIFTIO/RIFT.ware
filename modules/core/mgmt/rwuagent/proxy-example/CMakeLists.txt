# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Creation Date: 2014/21/05
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

include(rift_pkg_config)
cmake_minimum_required(VERSION 2.8)

set(rwsched_libs
    rwsched
    rwlib
    talloc
    CoreFoundation
    dispatch
    BlocksRuntime
    ck
    glib-2.0
    pthread_workqueue
    kqueue)

set(rwmsg_libs
    rwmsg
    rwtrace 
    rwlog 
    nanomsg 
    protobuf-c)

set(rwdts_libs
    rwdts_api
    rwdtsproto 
    rwmemlog 
    rwdts_redis 
    kv_light_api
    rwdts_yang_gen)

set(rwyang_libs
    rwyang 
    rw_schema_pb
    rwschema_yang_gen 
    rwext_yang_gen 
    ietf_yang_gen
    rwtypes 
    ncx)

set(schema_libs
    rwuagent_yang_gen
    rwlog-mgmt_yang_gen)

add_executable(toyp toyp.c)
target_link_libraries(toyp
  ${rwsched_libs}
  ${rwmsg_libs}
  ${rwdts_libs}
  ${rwyang_libs}
  ${schema_libs})

add_dependencies(toyp
  rwuagent_yang.schema 
  rwuagent_yang.headers)

install(TARGETS toyp 
        DESTINATION usr/bin 
        COMPONENT ${INSTALL_COMPONENT})
