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
# Creation Date: 2016/06/22
# 

include(rift_yang)

##
# Parse the yang files
##
set(ha_yang_files rw-ha.yang)

rift_add_yang_target(
  TARGET
    rwha_yang
  YANG_FILES
    ${ha_yang_files}
  LIBRARIES
    rwschema_yang_gen
    rw_vcs_yang_gen
  NO_LIB_COMPONENT
  COMPONENT
    ${INSTALL_COMPONENT}
  )
