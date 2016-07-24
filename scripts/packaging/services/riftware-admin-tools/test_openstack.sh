#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#



#
# Edit these variables to match an Admin user on your OpenStack setup.
#
# they can sometimes be found in /root/keystonerc_admin
#
ADMIN_USERNAME='admin';
ADMIN_PASSWORD='*****';
KEYSTONE_AUTH_URL='http://xxx.xxx.xxx.xxx:5000/v3/';
TENANT_NAME='demo';
MGMT_NETWORK_NAME='private';









#
# --------------------------------------------------------- END USER CONFIG ---------------------------------------------------------
#

# colors
blue='\e[0;34m'         # ${blue}
green='\e[0;32m'        # ${green}
cyan='\e[0;36m'         # ${cyan}
red='\e[0;31m'          # ${red}
purple='\e[0;35m'       # ${purple}
yellow='\e[1;33m'       # ${yellow}
white='\e[1;37m'        # ${white}
nc='\e[0m'              # ${nc} (no color - disables previous color selection)

# sanity checks
if [ "${ADMIN_PASSWORD}" == "*****" ]; then
	echo -e "${red}Please edit this script and provide your own OpenStack Authentication details!${nc}";
	exit 1;
fi

# should we check for root too?

echo -e "${yellow}"
echo "############################################"
echo "#                                          #"
echo "#   RIFT.ware OpenStack Readiness Test     #"
echo "#                                          #"
echo "############################################"
echo -e "${nc}"

echo -e "${green}Updating to the latest riftware-admin-tools:${nc}"
sudo yum -y install riftware-admin-tools

echo " "
echo -e "${green}Running the openstack check:${nc}"
RIFT_ROOT="/home/rift"
${RIFT_ROOT}/rift-shell -- python3 ${RIFT_ROOT}/.install/usr/bin/RIFT.ware-ready.py "${ADMIN_USERNAME}" "${ADMIN_PASSWORD}" "${KEYSTONE_AUTH_URL}" "${TENANT_NAME}" "${MGMT_NETWORK_NAME}"


