#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

usage() {
	echo "usage: launch_ui.sh [--enable-https --keyfile-path=<keyfile_path> --certfile-path=<certfile-path>]"
}

start_servers() {
	cd $THIS_DIR
	echo "Killing any previous instance of server_composer_ui.py"
	ps -ef | awk '/[s]cripts\/server_composer_ui.py/{print $2}' | xargs kill -9
	
	echo "Running Python webserver. HTTPS Enabled: ${ENABLE_HTTPS}"
	cd ../dist
	if [ ! -z "${ENABLE_HTTPS}" ]; then
		../scripts/server_composer_ui.py --enable-https --keyfile-path="${KEYFILE_PATH}" --certfile-path="${CERTFILE_PATH}"&
	else
		../scripts/server_composer_ui.py
	fi
}

# Begin work
for i in "$@"
do
case $i in
    -k=*|--keyfile-path=*)
    KEYFILE_PATH="${i#*=}"
    shift # past argument=value
    ;;
    -c=*|--certfile-path=*)
    CERTFILE_PATH="${i#*=}"
    shift # past argument=value
    ;;
    -h|--help)
    usage
    exit
    ;;
    -e|--enable-https)
    ENABLE_HTTPS=YES
    shift # past argument=value
    ;;
    *)
        # unknown option
    ;;
esac
done

if [[ ! -z "${ENABLE_HTTPS}" ]]; then
	if [ -z "${KEYFILE_PATH}" ] || [ -z "{CERTFILE_PATH}" ]; then
		usage
		exit
	fi
fi

# change to the directory of this script
THIS_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

# Call function to start web and API servers
start_servers

while true; do
  sleep 5
done
