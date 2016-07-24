#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


##
# This script prints the mapping of slot number, mac address and pci address
# for all the 10G ports on grunts
# This script must be run right after the boot, before the PCI devices
# are grabbed by the VMs.
##

if [ `whoami` != "root" ]; then
    echo Must be root
    exit 1
fi

# appendmode means take an existing rift_biosdevname.log and append the switch info
appendmode=0
replacemode=0
verbose=0
while getopts arv opt; do
	case $opt in 
		a) appendmode=1;;
		r) appendmode=1; replacemode=1;;
		v) verbose=1;;
	esac
done

if [ $appendmode == 1 ]; then
    if [ ! -f /var/log/rift_biosdevname.log ]; then
        echo "append mode failed: /var/log/rift_biosdevname.log does not exist"
        exit 1
    fi
    sudo chmod 666 /var/log/rift_biosdevname.log
fi

if [ $appendmode == 0 ]; then
    DEVS=`biosdevname -d | grep 'BIOS device: p' | awk -F ':' '{print $2}'`
else
    DEVS=`cut -f2 -d\  </var/log/rift_biosdevname.log `
fi
# bouncing the interface will cause it to send a gratuitous arp
# which will populate the arp cache on the switch
for dev in $DEVS
do
    ifconfig $dev up >/dev/null  2>&1
    ping -c 1 -I $dev -r 10.254.254.254 >/dev/null 2>/dev/null &
done


${RIFT_ROOT:-/usr/rift}/scripts/cloud/get_switch_known_macs.py >/tmp/switch_info.log

get_switch() { 
    mac=$1
    x="`grep -i $mac /tmp/switch_info.log`"
	if [ -z "$x" ]; then
		if [ $verbose = 1 ]; then
			echo NO PORT INFO FOUND FOR $mac >&2
		fi
		return
	fi
	set $x
	echo $2
}

if [ $appendmode == 0 ]; then

    hostname=`hostname -s`

    # biosdevname is a utility that prints the bios names
    for biosname in $DEVS
    do
        mac=`biosdevname -d | grep -A11 "BIOS device: $biosname" | grep 'Permanent MAC' | awk '{print $3}'`
        pci=`biosdevname -d | grep -A11 "BIOS device: $biosname" | grep 'PCI name' | awk '{print $4}'`
        res=`get_switch $mac`
        echo $hostname $biosname $mac $pci $res
    done
else 
    data="`cat /var/log/rift_biosdevname.log`"
    cp /var/log/rift_biosdevname.log /tmp/rift_biosdevname.log
    while read hostname biosname mac pci sw
    do
        res=`get_switch $mac`
        # use the old data if any if this query failed
        if [ -z "$res" -a $replacemode == 0 ]; then
            res="$sw"
        fi
        echo $hostname $biosname $mac $pci $res
    done </tmp/rift_biosdevname.log >/var/log/rift_biosdevname.log
fi
        
        
