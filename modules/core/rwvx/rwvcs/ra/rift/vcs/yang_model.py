
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

import rw_peas
import gi
gi.require_version('YangModelPlugin', '1.0')

yang = rw_peas.PeasPlugin('yangmodel_plugin-c', 'YangModelPlugin-1.0')
assert yang is not None

yang_model_api = yang.get_interface('Model')
assert yang_model_api is not None

yang_model = yang_model_api.alloc()
assert yang_model is not None

base_module = yang_model_api.load_module(yang_model, 'rw-base')
assert base_module is not None

manifest_module = yang_model_api.load_module(yang_model, 'rw-manifest')
assert manifest_module is not None

vcs_module = yang_model_api.load_module(yang_model, 'rw-vcs')
assert vcs_module is not None

