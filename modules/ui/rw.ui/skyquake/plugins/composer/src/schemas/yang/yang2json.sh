#!/bin/sh

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

./src-remove.sh

yfc schema -c rw-nsd.yang -f json -o json-nsd.json
yfc schema -c rw-vnfd.yang -f json -o json-vnfd.json
#yfc schema -c vnffgd.yang -f json -o json-vnffgd.json
#yfc schema -c rw-vld.yang -f json -o json-vld.json
#yfc schema -c ietf-inet-types.yang -f json -o ietf-inet-types.yang.json;
#yfc schema -c ietf-yang-types.yang -f json -o ietf-yang-types.yang.json

./src-append.sh

# todo: transform the -yang.json into a simpler json for the UI to consume
