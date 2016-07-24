#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


# change to the directory of this script
THIS_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd $THIS_DIR
cd ..

echo "DIR is"
pwd
echo "Building composer webapp"
npm rebuild
./node_modules/.bin/grunt build


