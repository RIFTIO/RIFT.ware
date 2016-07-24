# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
  COMPONENT ${PKG_LONG_NAME}
  DEPENDS rwproto
)

##
# Add the target for the library
##
add_library(yangtools_test_proto STATIC ${proto_c_files})


