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
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

##
# Protobuf compile to C and C++.  C++ is just for yucks; we don't use em.
##
set(proto_files descriptor.proto rwpbapi.proto)
rift_add_proto_target(
  WITH_DSO WITH_GPROTOC
  PROTO_FILES ${proto_files}
  OUT_C_FILES_VAR proto_c_files
  PROTO_COMPONENT ${INSTALL_COMPONENT} 
  PCCH_COMPONENT ${INSTALL_COMPONENT}
)

add_library(pb2c_proto STATIC ${proto_c_files})
# ATTN: Install the lib?

add_dependencies(pb2c_proto rwlib_targets)
