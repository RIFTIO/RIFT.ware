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
# Author(s): Tim Mortsolf
# Creation Date: 2014/03/13
# 

include(rift_yang)

##
# Set various yang files
##
set(rwschema_yang_files
  mano-base.yang
  rw-log.yang rw-base.yang rw-debug.yang rw-mgmt-schema.yang
  rw-vnf-base-config.yang rw-vnf-base-opdata.yang
  toy-vnf-config.yang toy-vnf-opdata.yang rwvcs-types.yang
  rw-vnf-base-types.yang
  )

##
# function to add targets for .yang->.xsd->.proto
##
rift_add_yang_target(
  TARGET rwschema_yang
  YANG_FILES ${rwschema_yang_files}
  NO_LIB_COMPONENT
  COMPONENT ${INSTALL_COMPONENT}
)

##
# Generate various yang trees for visualization
##
rift_gen_yang_tree(rw-base-pyang-trees
  OUTFILE_PREFIX rw-base.yang
  YANG_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/mano-base.yang
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-base.yang
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-debug.yang
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-log.yang
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-mgmt-schema.yang
)
