#!/bin/bash

# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
# Author(s): Max Beckett
# Creation Date: 7/27/2015
# 


restconfIp=$1
restconfPort=$2



echo "-- PUT /api/running/colony/trafgen/port/trafgen%252F2%252F1/trafgen/range-template/packet-size --"
echo ""
curl -D /dev/stdout \
     -H "Accept: application/vnd.yang.data+xml" \
     -H "Content-Type: application/vnd.yang.data+json" \
     -X PUT \
     -d "{\"packet-size\":{\"increment\":1,\"start\":512,\"minimum\":512,\"maximum\":512}}" \
     http://${restconfIp}:${restconfPort}/api/running/colony/trafgen/port/trafgen%252F2%252F1/trafgen/range-template/packet-size
echo ""
echo ""

echo "-- POST /api/operations/start --"
echo ""
curl -D /dev/stdout \
     -H "Accept: application/vnd.yang.data+xml" \
     -H "Content-Type: application/vnd.yang.data+json" \
     -X POST \
     -d "{\"input\":{\"colony\":{\"name\":\"trafgen\",\"traffic\":{\"all\":\"\"}}}}" \
     http://${restconfIp}:${restconfPort}/api/operations/start
echo ""

echo "-- POST /api/running/colony/trafgen/port/trafgen%252F2%252F1/trafgen/range-template/packet-size --"
echo ""
curl -D /dev/stdout \
     -H "Accept: application/vnd.yang.data+xml" \
     -H "Content-Type: application/vnd.yang.data+json" \
     -X POST \
     -d "{\"packet-size\":{\"increment\":1,\"start\":512,\"minimum\":512,\"maximum\":512}}" \
     http://${restconfIp}:${restconfPort}/api/running/colony/trafgen/port/trafgen%252F2%252F1/trafgen/range-template/packet-size
echo ""
echo ""

curl -D /dev/stdout \
     -H "Accept: application/vnd.yang.collection+json" \
     -X GET http://${restconfIp}:${restconfPort}/api/operational/colony/trafsink/port-state?select=portname

curl -D /dev/stdout \
     -H "Accept: application/vnd.yang.data+xml" \
     -X GET http://${restconfIp}:${restconfPort}/api/operational/colony/trafsink/load-balancer

echo ""

echo "-- GET /api/operational/colony/vsap"
echo ""
curl -D /dev/stdout \
     -H "Accept: application/vnd.yang.collection+json" \
     -X GET http://${restconfIp}:${restconfPort}/api/operational/colony/vsap
echo ""
echo ""

echo "-- GET /api/operational/vcs/resources?deep"
echo ""
curl -D /dev/stdout \
     -H "Accept: application/vnd.yang.data+xml" \
     -X GET http://${restconfIp}:${restconfPort}/api/operational/vcs/resources?deep
echo ""
echo ""

echo "-- GET /api/running/colony/trafgen/port"
echo ""
curl -D /dev/stdout \
     -H "Accept: application/vnd.yang.collection+xml" \
     -H "Content-Type: application/vnd.yang.collection+xml" \
     -X GET http://${restconfIp}:${restconfPort}/api/running/colony/trafgen/port
echo ""
echo ""

echo "-- GET /api/operational/tasklet?deep"
echo ""
curl -D /dev/stdout \
     -H "Accept: application/vnd.yang.data+xml" \
     -X GET http://${restconfIp}:${restconfPort}/api/operational/tasklet?deep
echo ""
echo ""

echo "-- GET /api/schema/rw-base:vcs"
echo ""
curl -D /dev/stdout \
  -H "Content-Type: application/json" \
  -H "Accept-Type: application/json" \
  -X GET http://${restconfIp}:${restconfPort}/api/schema/rw-base:vcs
echo ""
echo ""
