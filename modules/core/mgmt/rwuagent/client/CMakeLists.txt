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
# Author(s): Tom Seidenberg
# Creation Date: 2014/04/03
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwuagentclient)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

add_library(rwuagent_msg_client SHARED rwuagent_msg_client.c)
target_link_libraries(rwuagent_msg_client PRIVATE
  rwlib
  rwmsg
  rwuagentproto
  rwtasklet
  rwvcs

  CoreFoundation
  glib-2.0
  protobuf-c)
install(FILES ../include/riftware/rwuagent_msg_client.h DESTINATION usr/include COMPONENT ${INSTALL_COMPONENT})
install(TARGETS rwuagent_msg_client DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})

