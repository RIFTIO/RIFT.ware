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
# Author(s): Max Beckett
# Creation Date: 7/10/2015
# 

include(rift_python)
include(rift_plugin)


rift_python_install_tree(
  FILES
    rift/restconf/__init__.py  
    rift/restconf/streamingdata/__init__.py
    rift/restconf/streamingdata/websocket_handler.py
    rift/restconf/translation/__init__.py
    rift/restconf/translation/query.py
    rift/restconf/translation/result.py
    rift/restconf/translation/subscription.py
    rift/restconf/util/__init__.py
    rift/restconf/util/schema.py
    rift/restconf/util/util.py
    rift/restconf/util/web.py
    rift/restconf/util/xml.py
    rift/restconf/webserver/__init__.py
    rift/restconf/webserver/configuration.py
    rift/restconf/webserver/connection_manager.py
    rift/restconf/webserver/event_source.py
    rift/restconf/webserver/http_handler.py
    rift/restconf/webserver/logout_handler.py
    rift/restconf/webserver/netconf_wrapper.py
    rift/restconf/webserver/state_provider.py
    rift/restconf/webserver/statistics.py
    rift/restconf/webserver/xml_wrapper.py    
    rift/tasklets/restconf/__init__.py
    rift/tasklets/restconf/restconf.py
    rift/tasklets/restportforward/__init__.py
    rift/tasklets/restportforward/restportforward.py
    rift/tasklets/reststream/__init__.py
    rift/tasklets/reststream/reststream.py
  PYTHON3_ONLY
  COMPONENT
    ${INSTALL_COMPONENT}
  )

set(
  RESTCONF_BIN_FILES
    bin/rwrestconf_standalone
  )

install(
  FILES
    ${RESTCONF_BIN_FILES}
  DESTINATION
    usr/local/bin
  COMPONENT
    ${INSTALL_COMPONENT}
  PERMISSIONS
    OWNER_READ OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
  )

rift_add_subdirs(
  SUBDIR_LIST
    test
    plugins
  )

