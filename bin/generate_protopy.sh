# /bin/bash
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
# Creation Date: 2014/06/03
#
# Generate all .py implementations of .proto files using the 
# protoc compiler.  Temporary solution until integrating
# into build process is solved.  This must be run after 
# a make. 

RIFT_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
RIFT_ROOT="${RIFT_ROOT%/bin}"

INSTALL_DIR="$RIFT_ROOT/.install"
PROTO_DIR="$INSTALL_DIR/usr/data/proto"
PROTOC=/usr/bin/protoc

cd $PROTO_DIR

find . -name "*.proto" | while read line; do
  $PROTOC --python_out=. $line
done

