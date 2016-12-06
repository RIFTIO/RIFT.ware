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

from .schema import (
    collect_children,
    find_child_by_name,
    find_child_by_path,
    find_target_type,
    load_multiple_schema_root,
    load_schema_root,    
    TargetType,
    find_target_node,
)

from .util import (
    iterate_with_lookahead,
    Result,
    PayloadType,
    NetconfOperation,
    naive_xml_to_json,
)

from .xml import (
    create_xpath_from_url,
    create_dts_xpath_from_url,
)

from .web import (
    determine_payload_type,
    is_config,
    is_schema_api,
    split_url,
    split_stream_url,
    get_username_and_password_from_auth_header,
    map_error_to_http_code,
    format_error_message,
    map_request_to_netconf_operation,
    get_json_schema,
)
