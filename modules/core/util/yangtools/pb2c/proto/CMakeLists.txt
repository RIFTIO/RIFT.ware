# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
  PROTO_COMPONENT pb2c-1.0
  PCCH_COMPONENT pb2c-1.0
)

add_library(pb2c_proto STATIC ${proto_c_files})
# ATTN: Install the lib?

add_dependencies(pb2c_proto rwlib_targets)
