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
# Author(s): Austin Cormier
# Creation Date: 01/04/2015
# 


##
# Parse the yang files
##
rift_generate_python_log_yang(
  LOG_CATEGORY_NAME rw-generic-log
  START_EVENT_ID 6000
  OUT_YANG_FILE_VAR rw_generic_log_file
  )

rift_add_yang_target(
  TARGET rwgeneric_yang
  YANG_FILES ${rw_generic_log_file}
  NO_LIB_COMPONENT
  COMPONENT ${INSTALL_COMPONENT}
  LIBRARIES
    rwschema_yang_gen
  )
