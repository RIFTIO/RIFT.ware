#!/bin/bash
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Austin Cormier
# Creation Date: 2015/01/26
#
# This script generates a supermodule hash which is used as part of
# the submodule hash.  This ensures that when any of the following
# files/folders change in the supermodule all of the submodule
# build caches are flushed.

cd "$(dirname ${BASH_SOURCE[0]})"

read -r -d '\n' CACHE_FILE_FIND_LIST <<END
./*.py
./*.sh
../rift*
../Makefile*
../CMakeLists.txt
../cmake/modules
../.gitmodules.deps
END

# This line can be uncommented and used whenever we can suffer a submodule build cache invalidation
#find ${CACHE_FILE_FIND_LIST} -type f -print0 | sort -z | xargs -0 -i sh -c 'git show HEAD:{} | sha1sum -' | sha1sum - | cut -f 1 -d " "

# Create a combined sha1hash using the sha1hash's of the files listed above
# Sort is locale-dependent (Tom found this out the hard way).  Force the locale to produce consistent results across locales.
export LC_ALL="en_US.UTF-8"
find ${CACHE_FILE_FIND_LIST} -type f -print0 | sort -z | xargs -0 -i sh -c 'echo $(git show HEAD:{} | sha1sum - | cut -d" " -f1)" " {}' | sha1sum - | cut -f 1 -d " "

