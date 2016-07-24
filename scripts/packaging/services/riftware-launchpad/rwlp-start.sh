#!/bin/sh

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

SCREEN_NAME='rwlp'
SCREENRC='/home/rift/.install/var/rift/services/riftware-launchpad/rwlp.screenrc'

# setup /etc/hosts on first boot
FIRSTBOOTFILE=/home/rift/.install/.firstboot
UPDATEHOSTSCRIPT=/home/rift/scripts/cloud/UpdateHostsFile

if [ ! -f ${FIRSTBOOTFILE} ]; then
	# yes firstboot
	if [ -f ${UPDATEHOSTSCRIPT} ]; then
		echo "Running ${UPDATEHOSTSCRIPT} on first boot."
		${UPDATEHOSTSCRIPT}
		rc=$?

		if [ $rc -eq 0 ]; then
			echo "`date`" > ${FIRSTBOOTFILE}
		else
			echo "Firstboot error. ${UPDATEHOSTSCRIPT} returned $rc"
		fi
	else
		echo "Could not find ${UPDATEHOSTSCRIPT}"
	fi

fi

echo " ";
echo "Starting RiftWare Launchpad in a detached screen, to connect to #rift-shell: screen -r ${SCREEN_NAME}";
echo " ";
sleep 1;
/usr/bin/screen -d -m -S ${SCREEN_NAME} -c ${SCREENRC}
