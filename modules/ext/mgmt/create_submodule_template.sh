#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

## @author Austin Cormier (austin.cormier@riftio.com)
## @date 04/13/2015
## @file create_submodule_template.sh
## @brief Create the modules/ext/mgmt submodule template (without confd packages)
## @description When producing the external repository, it is neccessary to ship
## the modules/ext/mgmt submodule without the confd packages for license reasons.
## To make this easier, this script was created to tar up the everything besides
## the licences confd packages which we cannot distribute.

THIS_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

if [ $# -ne 1 ]; then
  echo "ERROR: Script takes a single output directory to store archive"
  exit 1
fi

output_dir=$1

if [ ! -d "$output_dir" ]; then
  echo "ERROR: Provided output dir is not a valid directory"
  exit 1
fi

output_dir="$(readlink -f "$output_dir")"

cd $THIS_DIR

# The .gitattributes is used to filter out the confd packages from the archive
git archive --format=tar -o "$output_dir/extmgmt_submodule_template.tar" HEAD confd libconfd CMakeLists.txt ADDING_TAILF_CONFD_PACKAGES

