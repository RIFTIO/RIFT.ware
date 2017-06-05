
# STANDARD_RIFT_IO_COPYRIGHT

execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_SHA
  OUTPUT_STRIP_TRAILING_WHITESPACE)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rift-ec2-root-${GIT_SHA}.img
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/mkvmimg
    --builddir ${CMAKE_CURRENT_BINARY_DIR}/image-staging
    --rw-install
    --outputs disk-image
    --prefix rift-ec2-root
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mkvmimg
  COMMENT "Building EC2 Image ${CMAKE_CURRENT_BINARY_DIR}/rift-ec2-root-${GIT_SHA}.qcow2")

add_custom_target(ec2-image
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/rift-ec2-root-${GIT_SHA}.img)



add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rift-kvm-root-${GIT_SHA}.qcow2
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/mkvmimg
    --builddir ${CMAKE_CURRENT_BINARY_DIR}/image-staging
    --rw-install
    --outputs qcow2
    --prefix rift-kvm-root
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mkvmimg
  COMMENT "Building KVM Image ${CMAKE_CURRENT_BINARY_DIR}/rift-kvm-root-${GIT_SHA}.qcow2")

add_custom_target(kvm-image
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/rift-kvm-root-${GIT_SHA}.qcow2)

