#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

PLUGIN_NAME=goodbyeworld
# change to the directory of this script
THIS_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd $THIS_DIR
cd ..

echo 'Building plugin '$PLUGIN_NAME
echo 'Fetching third-party node_modules for '$PLUGIN_NAME
npm install
echo 'Fetching third-party node_modules for '$PLUGIN_NAME'...done'
echo 'Packaging '$PLUGIN_NAME' using webpack'
ui_plugin_cmake_build=true ./node_modules/.bin/webpack --progress --config webpack.production.config.js
echo 'Packaging '$PLUGIN_NAME' using webpack... done'
echo 'Building plugin '$PLUGIN_NAME'... done'
