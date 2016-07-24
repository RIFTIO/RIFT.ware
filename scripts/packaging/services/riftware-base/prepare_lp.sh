#!/bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

generate_uuid(){
   cat /proc/sys/kernel/random/uuid
}

lp_ssh_cmd(){
    lp_ip=$1
    shift
    ssh -q -o ConnectTimeout=10 -o BatchMode=yes -o StrictHostKeyChecking=no "$lp_ip" "$*"
    return $?
}


configure_lp(){
    echo "Configuring Launchpad minion"
    lp_uuid="$(generate_uuid)"

    # Get the salt master ip address from this SSH connection
    mc_ip="$(lp_ssh_cmd "$1" 'echo $SSH_CONNECTION | cut -f1 -d " "')"

    lp_ssh_cmd "$1" sed -i "s/\#*id\:.*/id\:\ ${lp_uuid}/g" /etc/salt/minion
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to set the minion id"
        exit 1
    fi

    lp_ssh_cmd "$1" sed -i "s/\#*master\:.*/master\:\ ${mc_ip}/g" /etc/salt/minion
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to set the master ip"
        exit 1
    fi

    lp_ssh_cmd "$1" systemctl restart salt-minion.service
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to restart the salt minion service"
        exit 1
    fi
}


wait_salt_minion_connection(){
    echo "Waiting for launchpad minion to come up"
    for i in $(seq 1 20); do
        out="$(salt-run manage.up | grep ${lp_uuid})"
        if [ "$out" != "" ]; then
            echo "Minion successfully connected."
            return
        fi
    done

    echo "Error: Timed out waiting for salt minion connection"
    exit 1
}


configure_mc_lp_node(){
    mkdir -p /etc/rift
    echo "MC_OPTS=\"--lp-node-id $1:${lp_uuid} --lp-public-ip $2\"" > /etc/sysconfig/rift-mission-control
}


install_mc_sw(){
  # Download and install the mission control
  if [ $TESTING == 1 ]; then
    yum-config-manager --enable RIFT.ware-testing
  fi
  if [ $NIGHTLY == 1 ]; then
    yum-config-manager --enable RIFT.ware-nightly
  fi
  yum makecache fast
  yum -y update 'rift*'
  yum -y install 'riftware-missioncontrol'
  # MC doesn't need salt minion
  systemctl stop salt-minion
  systemctl disable salt-minion
}


start_mc(){
  # Start the mission control software
  systemctl restart rwmc
}


install_lp_sw(){
  # Install the launchpad software
  if [ $TESTING == 1 ]; then
    lp_ssh_cmd $1 yum-config-manager --enable RIFT.ware-testing
  fi
  if [ $NIGHTLY == 1 ]; then
    lp_ssh_cmd $1 yum-config-manager --enable RIFT.ware-nightly
  fi
  lp_ssh_cmd $1 yum makecache fast
  lp_ssh_cmd $1 yum -y upgrade \'rift*\'
  lp_ssh_cmd $1 yum -y install riftware-launchpad
  # LP doesn't need salt master
  systemctl stop salt-master
  systemctl disable salt-master
}


check_lp_ssh(){
    echo "Checking Launchpad SSH connection to $1"
    lp_ssh_cmd "$1" echo >/dev/null
    if [ $? -ne 0 ]; then
        echo "ERROR: SSH Connection to launchpad vm IP address $1 failed"
        exit 1
    fi
}

NIGHTLY=0
TESTING=0
while [[ "$1" =~ --.* ]]; do
    case "$1" in
        --nightly) NIGHTLY=1; TESTING=1;;
        --testing) TESTING=1;
    esac
    shift
done

if [ $# -ne 2 ]; then
    echo "ERROR: You must provide launchpad private & public/floating ip addresses"
    echo 'ARGS: [--nightly|--testing] <LP_PRIVATE_IP> <LP_PUBLIC_IP>'
    exit 1
fi

if [ "$(id -u)" != "0" ]; then
    echo "You must be root in order to run this script."
    exit 1
fi

lp_ip="$1"
lp_pub_ip="$2"

check_lp_ssh "$lp_ip"
#check_lp_ssh "$lp_pub_ip"
install_lp_sw "$lp_ip"
install_mc_sw 
configure_lp "$lp_ip" 
wait_salt_minion_connection "$lp_ip"
configure_mc_lp_node "$lp_ip" "$lp_pub_ip"
echo "Launchpad successfully prepared"
start_mc
echo "mission control started"
echo "use 'screen -r' to access missioncontrol console"
exit 0
