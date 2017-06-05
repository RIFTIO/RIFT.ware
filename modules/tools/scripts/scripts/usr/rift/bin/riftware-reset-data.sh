#!/bin/bash

# STANDARD_RIFT_IO_COPYRIGHT


function stopServices {

    echo " "
    echo "Stopping RIFT.ware Services"
    systemctl stop launchpad.service
    echo " "

}

function resetData {

    echo "Resetting data, RIFT_ROOT=${RIFT_ROOT}"

    #rm -f ${RIFT_INSTALL}/*.db
    #rm -rf ${RIFT_INSTALL}/zk/server*
    rm -rf ${RIFT_INSTALL}/var/

    # unique
    #rwyangutil --rm-unique-mgmt-ws 2>/dev/null >/dev/null
    #rwyangutil --remove-schema-dir 2>/dev/null >/dev/null

    # unique AND persistent
    #rwyangutil --rm-unique-mgmt-ws --rm-persist-mgmt-ws 2>/dev/null >/dev/null
    #rwyangutil --remove-schema-dir                         2>/dev/null >/dev/null

    #echo "Checking:"
    #ls -all ${RIFT_INSTALL}/*_persist_*
    #ls -all ${RIFT_INSTALL}/*.db
    #ls -all ${RIFT_INSTALL}/var/rift/schema

}

function finish {
    echo " "
    echo "Configurations deleted. You may now restart the RIFT.ware services using 'systemctl start launchpad.service'"
    echo " "
}

if [ -z "$RIFT_ROOT" ]; then
    echo "Please run from inside RIFT-SHELL"
    exit 1
fi


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

# vim: ts=4 et:
