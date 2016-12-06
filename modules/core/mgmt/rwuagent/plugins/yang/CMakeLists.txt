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
# Creation Date: 2014/04/08
# 

##
# Yang targets
##
set(yang_files
    rw-mgmtagt.yang
    rw-mgmtagt-dts.yang
    rw-mgmtagt-log.yang
    rw-mgmtagt-composite.yang
   )

if(NOT RIFT_AGENT_BUILD STREQUAL "XML_ONLY")
  list(APPEND yang_files rw-mgmtagt-confd.yang)
endif()

rift_add_yang_target(
  TARGET
    rwuagent_yang
  YANG_FILES
    ${yang_files}
  LIBRARIES
    rwschema_yang_gen
    rwmanifest_yang_gen
    rwlog-mgmt_yang_gen
  NO_LIB_COMPONENT
  COMPONENT
    ${INSTALL_COMPONENT}
)

