#!/bin/bash


## For Xml
# URL="http://localhost:8888/streams/NETCONF"

# For Json
URL="https://localhost:8888/streams/NETCONF-JSON"

# To Get notifications on this stream, edit a configuration

curl -k -u admin:admin \
     -H "Accept: text/event-stream" \
     -H "Cache-Control: no-cache" \
     -H "Connection: keep-alive" \
     -X GET \
     -D /dev/stdout \
     ${URL}

echo ""
