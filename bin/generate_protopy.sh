# /bin/bash
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
PROTOC=$INSTALL_DIR/usr/bin/protoc

cd $PROTO_DIR

find . -name "*.proto" | while read line; do
  $PROTOC --python_out=. $line
done

