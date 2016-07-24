#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

#
# Provide the RESTConf hostname as the argument or default to localhost

if [ $# -eq 0 ] ; then
    HOST=localhost
else
    HOST=$1
fi

echo "Getting NS instance opdata from $IP"

curl --user admin:admin \
    -H "Content-Type: application/vnd.yang.data+json" \
    -H "accept: application/vnd.yang.data+json" \
    http://$HOST:8008/api/operational/ns-instance-opdata/

