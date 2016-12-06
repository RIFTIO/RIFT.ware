
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

