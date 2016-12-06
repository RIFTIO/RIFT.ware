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
# 

cmake_minimum_required(VERSION 2.8)

##
# Generate the protobuf file
##
set(proto_files 
    pb2xml.proto 
    enum.proto
    gi_test.proto
    pbcopy.proto
    pbdf.proto
    oneof_test.proto
    is_equal.proto
    test.proto
    test-full.proto
)
rift_add_proto_target(
  TARGET ylib_test_proto
  PROTO_FILES ${proto_files}
  OUT_C_FILES_VAR proto_c_files
  COMPONENT ${INSTALL_COMPONENT}
  DEPENDS rwproto
)

##
# Add the target for the library
##
add_library(yangtools_test_proto STATIC ${proto_c_files})


