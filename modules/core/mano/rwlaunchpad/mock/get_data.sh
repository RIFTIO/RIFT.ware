#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

#
# This is a convenience script to get descriptors from the RESTConf server
#
# Provide the RESTConf hostname as the argument or default to localhost

if [ $# -eq 0 ] ; then
    HOST=localhost
else
    HOST=$1
fi

echo "Getting descriptor data from $IP"

for descriptor in nsd vnfd vld
do

    printf "retrieving $descriptor:\n\n"

    curl --user admin:admin \
        -H "Content-Type: application/vnd.yang.data+json" \
        -H "accept: application/vnd.yang.data+json" \
        http://$HOST:8008/api/running/$descriptor-catalog/

done

rectype='ns'

    curl --user admin:admin \
        -H "Content-Type: application/vnd.yang.data+json" \
        -H "accept: application/vnd.yang.data+json" \
        http://$HOST:8008/api/running/$rectype-instance-config/


    curl --user admin:admin \
        -H "Content-Type: application/vnd.yang.data+json" \
        -H "accept: application/vnd.yang.data+json" \
        http://$HOST:8008/api/operational/$rectype-instance-opdata/



