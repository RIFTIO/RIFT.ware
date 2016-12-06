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
# Author(s): Baiju Parameswaran
# Creation Date: 5/8/2015
# 

include(rift_yang)

##
# Parse the yang files
##
set(source_yang_files rw-dtsperf.yang rw-dtsperfmgr.yang)

rift_add_yang_target(
  TARGET rwdtsperf_yang
  YANG_FILES ${source_yang_files}
  NO_LIB_COMPONENT
  COMPONENT ${INSTALL_COMPONENT}
  LIBRARIES
    rwschema_yang_gen
    rwlog
    rwmanifest_yang_gen
    rw_vcs_yang_gen
  )

