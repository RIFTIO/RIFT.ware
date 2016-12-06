# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
# Creation Date: 2014/21/05
# 

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
