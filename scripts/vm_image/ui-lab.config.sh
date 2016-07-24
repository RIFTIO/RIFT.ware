#!/bin/bash

echo "installing lab scripts"
#cmd yum -y --enablerepo='*' install riftware-scripts-internal-lab
# try and force the install order
cmd yum -y --enablerepo='*' install riftware-base riftware-scripts-vm riftware-scripts-internal-lab

echo "1) checking RPMS"
cmd "rpm -qa | grep -i rift"
#echo "1a) checking scripts"
#cmd "ls -all /home/rift/scripts/"
#echo "1b) checking scripts"
#ls -all $STAGING/home/rift/scripts/

echo "running enable_lab"
cmd /usr/rift/scripts/cloud/enable_lab

echo "2) checking RPMS"
cmd "rpm -qa | grep -i rift"
#echo "2a) checking scripts"
#cmd "ls -all /home/rift/scripts/"
#echo "2b) checking scripts"
#ls -all $STAGING/home/rift/scripts/
