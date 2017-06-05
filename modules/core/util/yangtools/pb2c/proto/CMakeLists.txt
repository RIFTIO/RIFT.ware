# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

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
