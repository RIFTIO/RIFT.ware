#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

plugin=composer
source_dir=$1
dest_dir=$2
bcache_dir=$3

# Create destination and build cache directories
mkdir -p $dest_dir
mkdir -p $bcache_dir

# Create necessary directories under dest and cache dirs
mkdir -p $dest_dir/framework
mkdir -p $dest_dir/plugins
mkdir -p $bcache_dir/framework
mkdir -p $bcache_dir/plugins

# Copy over built plugin's public folder, config.json, routes.js and api folder as per installed_plugins.txt
mkdir -p $dest_dir/plugins/$plugin
cp -Lrf $source_dir/public $dest_dir/plugins/$plugin/.
cp -Lrf $source_dir/config.json $dest_dir/plugins/$plugin/.
cp -Lrf $source_dir/routes.js $dest_dir/plugins/$plugin/.
#   cp -Lrp $source_dir/api $dest_dir/plugins/$plugin/.
mkdir -p $bcache_dir/plugins/$plugin
cp -Lrf $source_dir/public $bcache_dir/plugins/$plugin/.
cp -Lrf $source_dir/config.json $bcache_dir/plugins/$plugin/.
cp -Lrf $source_dir/routes.js $bcache_dir/plugins/$plugin/.
#   cp -Lrp $source_dir/api $bcache_dir/plugins/$plugin/.

