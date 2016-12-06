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
# Creation Date: 9/13/15
# 

rift_python_install_tree(
  FILES
  rift/rwlib/testing/__init__.py
  rift/rwlib/testing/xml_helpers.py
  rift/rwlib/translation/__init__.py
  rift/rwlib/translation/json2xml.py
  rift/rwlib/translation/xml2json.py
  rift/rwlib/schema/__init__.py
  rift/rwlib/schema/helpers.py
  rift/rwlib/xml/__init__.py
  rift/rwlib/xml/helpers.py
  rift/rwlib/util/__init__.py
  rift/rwlib/util/certs.py
  rift/rwlib/util/iteration.py
  PYTHON3_ONLY
  COMPONENT
    ${INSTALL_COMPONENT}
  )
