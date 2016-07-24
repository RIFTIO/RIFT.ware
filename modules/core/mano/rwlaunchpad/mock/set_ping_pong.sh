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

for rectype in vnfd nsd
do
    echo "Assigning data to instance config \"$rectype\""

    curl --user admin:admin \
        -H "Content-Type: application/vnd.yang.data+json" \
        -X POST \
        -d @data/ping-pong-${rectype}.json \
        http://$HOST:8008/api/running/$rectype-catalog/ -v

    # Add sleep here if vnfd is not ready on server
done

curl --user admin:admin \
    -H "Content-Type: application/vnd.yang.data+json" \
    -X POST \
    -d @data/ping-pong-ns-instance-config.json \
    http://$HOST:8008/api/running/ns-instance-config/ -v

