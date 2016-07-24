#!/usr/bin/bash

# To get 

# To get Websocket stream with Json-content location
URL="https://localhost:8888/api/operational/restconf-state/streams/stream/NETCONF/access/ws_json/location"

# To get Websocket stream with Json-content location
#URL= "http://localhost:8888/api/operational/restconf-state/streams/stream/NETCONF/access/ws_xml/location"

# To get HTTP stream with Json-content location
#URL= "http://localhost:8888/api/operational/restconf-state/streams/stream/NETCONF/access/json/location"

curl -k -u admin:admin \
  -H "Accept: application/vnd.yang.data+json" \
  -X GET -D /dev/stdout \
  ${URL}

echo 

