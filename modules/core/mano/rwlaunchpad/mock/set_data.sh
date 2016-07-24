#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

#
# This script posts descriptor data (NSD, VNFD, VLD) to the RESTConf server
#
# Provide the RESTConf hostname as the argument or default to localhost
#

if [ $# -eq 0 ] ; then
    HOST=localhost
else
    HOST=$1
fi

echo "Posting descriptor data to $HOST"


#for descriptor in nsd vnfd vld

for descriptor in nsd vnfd
do
    echo "Assigning data to descriptor \"$descriptor\""

    curl --user admin:admin \
        -H "Content-Type: application/vnd.yang.data+json" \
        -X POST \
        -d @data/${descriptor}_catalog.json \
        http://$HOST:8008/api/running/$descriptor-catalog/ -v

done

for rectype in ns
do
    echo "Assigning data to instance config \"$rectype\""

    curl --user admin:admin \
        -H "Content-Type: application/vnd.yang.data+json" \
        -X POST \
        -d @data/${rectype}-instance-config.json \
        http://$HOST:8008/api/running/$rectype-instance-config/ -v

done

