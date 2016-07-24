#! /usr/bin/bash
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 2015/10/09
#
# This shell script is used to create a descriptor package
# The main functions of this script include:
# - Generate checksums.txt file
# - Generate a tar.gz file

# Usage: generate_descriptor_pkg.sh <base-directory> <package-directory>

basedir=${1%/}
dir=${2%/}
cd ${basedir}/${dir}
rm -rf checksums.txt
find * -type f |
    while read file; do
        md5sum $file >> checksums.txt
    done
cd ..
tar -zcvf ${dir}.tar.gz ${dir}  --remove-files
