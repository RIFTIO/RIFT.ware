#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

# This script tests the throughput of get operations.
# change iter and loop variables

NETCONF_CONSOLE_DIR=${RIFT_ROOT}/.install/usr/local/confd/bin

iter=100
loop=30

for i in `seq 1 $loop`;
do
    echo "Background script $i"
    ${NETCONF_CONSOLE_DIR}/netconf-console-tcp -s all --iter=$iter --get -x /opdata&
done

wait

total=$(($iter * $loop))
echo "Total number of netconf operations=$total"



