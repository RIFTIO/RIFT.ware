#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


function die {
   echo "$@"
   exit 1
}

which tinyproxy 2>/dev/null || die "You must install tinyproxy (sudo yum install tinyproxy)"

tiny_cfg=$(mktemp)

trap "rm ${tiny_cfg}" EXIT

# Some default tinyproxy config to act as a very simple http proxy
cat << EOF > ${tiny_cfg}
User tinyproxy
Group tinyproxy
Port 9999
Timeout 600
DefaultErrorFile "/usr/share/tinyproxy/default.html"
StatFile "/usr/share/tinyproxy/stats.html"
LogFile "/var/log/tinyproxy/tinyproxy.log"
LogLevel Info
PidFile "/run/tinyproxy/tinyproxy.pid"
MaxClients 100
MinSpareServers 5
MaxSpareServers 20
StartServers 10
MaxRequestsPerChild 0
ViaProxyName "tinyproxy"
EOF

echo "Running TinyProxy in the foreground.  Ctrl-C to exit."
tinyproxy -c ${tiny_cfg} -d
