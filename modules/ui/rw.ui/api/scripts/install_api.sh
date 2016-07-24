#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


source_dir=$1
dest_dir=$2
bcache_dir=$3

mkdir -p $dest_dir
mkdir -p $bcache_dir
cp -Lrf $source_dir/* $dest_dir
cp -Lrf $source_dir/* $bcache_dir

