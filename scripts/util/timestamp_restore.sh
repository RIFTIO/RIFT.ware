#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


# When the third party packages are untarred and checked into git
# the timestamps are not preserved. As a result we have observed that
# many times the "configure" script is not succeeding. Most likely
# because there is some in compatibility with the autotools. Instead
# of mucking with autotools for 3rd party software, this script just 
# restores the timestamps before the compilation to the timestamps in
# original tar file.

# Usage 
#     ./timestamp_restore 
#         <path-to-source>
#         <timestamp-file>
# the time stamp file has the following format
# -rw-rw-r-- mclasen/mclasen   57800 2013-06-09 18:53:40 glib-2.36.3/aclocal.m4
# 
# The folowing command is used to generate the timestamps from tar file
# tar -t -v --full-time -f foo.tar.xz > foo.tar.xz.timestamp

echo "$0: updating timestamps in dir $1 from the file $2"
while read line           
do           
    ts=`echo $line | awk -v path="$1" '{print $4,$5}'`
    fname=`echo $line | awk -v path="$1" '{print path"/"$6}'`
    touch --date="$ts" $fname
done < $2
