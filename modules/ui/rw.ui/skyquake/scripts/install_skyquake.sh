#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


source_dir=$1
dest_dir=$2
bcache_dir=$3

# Create destination and build cache directories
mkdir -p $dest_dir
mkdir -p $bcache_dir

# Create necessary directories under dest and cache dirs
mkdir -p $dest_dir/framework
mkdir -p $bcache_dir/framework

# Copy over skyquake-core components
cp -Lrf $source_dir/package.json $dest_dir/.
cp -Lrf $source_dir/node_modules $dest_dir/.
cp -Lrf $source_dir/skyquake.js $dest_dir/.
cp -Lrf $source_dir/framework/core $dest_dir/framework/.
cp -Lrf $source_dir/scripts $dest_dir/.

cp -Lrf $source_dir/package.json $bcache_dir/.
cp -Lrf $source_dir/node_modules $bcache_dir/.
cp -Lrf $source_dir/skyquake.js $bcache_dir/.
cp -Lrf $source_dir/framework/core $bcache_dir/framework/.
cp -Lrf $source_dir/scripts $bcache_dir/.
