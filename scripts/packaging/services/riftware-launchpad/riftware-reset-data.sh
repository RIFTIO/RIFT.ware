#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#


function stopServices {

	echo " "
	echo "Stopping RIFT.ware Services"
	systemctl stop rwlp
	echo " "

}

function resetData {

	# for RPM based installs, attempt to determine RIFT_ROOT
	if [ -z ${RIFT_ROOT} ]; then
		if [ -d /home/rift ]; then
			RIFT_ROOT=/home/rift
		else
			echo "Can't determine RIFT_ROOT, please enter a rift-shell."
			exit 1
		fi
	fi

	echo "Resetting data, RIFT_ROOT=${RIFT_ROOT}"

	INSTALLDIR=${RIFT_ROOT}/.install

    	rm -f ${INSTALLDIR}/*.db

	# unique
	#${RIFT_ROOT}/rift-shell -- rwyangutil --rm-unique-mgmt-ws 2>/dev/null >/dev/null
	#${RIFT_ROOT}/rift-shell -- rwyangutil --remove-schema-dir 2>/dev/null >/dev/null

	# unique AND persistent
	${RIFT_ROOT}/rift-shell -- rwyangutil --rm-unique-mgmt-ws --rm-persist-mgmt-ws 2>/dev/null >/dev/null
	${RIFT_ROOT}/rift-shell -- rwyangutil --remove-schema-dir 			 2>/dev/null >/dev/null

	#echo "Checking:"
	#ls -all ${INSTALLDIR}/*_persist_*
    	#ls -all ${INSTALLDIR}/*.db
    	#ls -all ${INSTALLDIR}/var/rift/schema

}

function finish {
	echo " "
	echo "Configurations deleted. You may now restart the RIFT.ware services. "
	echo " "
}


echo " "
echo "!!! This script will stop all RIFT.ware services and delete all of your RIFT.ware data and configurations. It will not remove any software. !!!"
echo " "

read -r -p "Are you sure you wish to proceed? [y/N] " input

case $input in
	[yY][eE][sS]|[yY])
		stopServices
		resetData
		finish
		;;
	*)
		echo "Quitting."
		exit 0;
		;;
esac
