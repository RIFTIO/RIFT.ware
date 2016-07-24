# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

set(local_depends)

##
# Protobuf compile to C and C++.  C++ is just for yucks; we don't use em.
##
set(proto_ex1_files
  ex1.proto ex2_base1.proto ex2_base2.proto ex2_base3.proto ex2_base4.proto
  ex2.proto ipsec_state.proto sample1.proto tim.proto)

rift_add_proto_target(
  TARGET pb2c_proto_ex1
  WITH_DSO WITH_GPROTOC WITH_PB2C
  PROTO_FILES ${proto_ex1_files}
)

##
# Generate some extra pb2c conversions
##
foreach(proto_file ${proto_ex1_files})
  get_filename_component(name ${proto_file} NAME_WE)
  set(dso_file ${CMAKE_CURRENT_BINARY_DIR}/${name}.dso)
  set(flat_base ${CMAKE_CURRENT_BINARY_DIR}/${name}_flat_cstructs)
  set(flat3_base ${CMAKE_CURRENT_BINARY_DIR}/${name}_flat3_cstructs)

  rift_pb2c_command(command
    ARGV --dsofile=${name}.dso --scope-flatnames --genfilebase=${flat_base}
  )
  add_custom_command(
    OUTPUT ${flat_base}.c ${flat_base}.h
    COMMAND ${command}
    DEPENDS ${dso_file}
  )

  rift_pb2c_command(command
    ARGV --dsofile=${name}.dso --scope-flatnames --flat-msg-lvl 3 --genfilebase=${flat3_base}
  )
  add_custom_command(
    OUTPUT ${flat3_base}.c ${flat3_base}.h
    COMMAND ${command}
    DEPENDS ${dso_file}
  )

  list(APPEND local_depends ${flat_base}.c ${flat_base}.h ${flat3_base}.c ${flat3_base}.h)
endforeach()

##
# Protobuf compile to C and C++.  C++ is just for yucks; we don't use em.
##
set(proto_ex3_files ex3.proto)
rift_add_proto_target(
  TARGET pb2c_proto_ex3
  WITH_DSO
  PROTO_FILES ${proto_ex3_files}
)

add_custom_target(pb2c_protoex DEPENDS pb2c_proto_ex1 pb2c_proto_ex3 ${local_depends})

