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
# Creation Date: 2015/09/11
# 

include(rift_yang)

set(source_yang_files
  rw-memlog.yang
)

rift_add_yang_target(
  TARGET rwmemlog_yang
  YANG_FILES ${source_yang_files}
  NO_LIB_COMPONENT 
  COMPONENT ${INSTALL_COMPONENT}
  LIBRARIES rwext_yang_gen  
)

rift_gen_yang_tree(rwmemlog-pyang-trees
  OUTFILE_PREFIX rw-memlog.yang
  YANG_FILES ${CMAKE_CURRENT_SOURCE_DIR}/rw-memlog.yang
)

