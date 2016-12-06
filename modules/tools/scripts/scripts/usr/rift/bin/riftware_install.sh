#!/bin/bash



#REPO_URL=jmordkof@jeremy-pc:/home/jmordkof/workspace/osm/riftware
REPO_URL=https://osm.etsi.org/gerrit/osm/riftware.git
USE_RIFT_MIRRORS=false

set -e
echo -e '\n\n=========== Installing base packages ========\n\n'
yum --nogpgcheck install -y git curl python3 screen which wget

echo -e '\n\n=========== Cloning RIFT.ware ========\n\n'
if [ -e riftware ]; then
    cd riftware 
    git pull
else
    git clone $REPO_URL riftware
    cd riftware
fi

echo -e '\n\n=========== Setting up YUM repositories ========\n\n'
if $USE_RIFT_MIRRORS; then
    rm -f /etc/yum.repos.d/*
    cp scripts/vm_image/riftware-mirrors.repo /etc/yum.repos.d/
fi
cp scripts/vm_image/riftware.repo /etc/yum.repos.d/
yum clean all
yum makecache

echo -e '\n\n=========== Installing prerequisites ========\n\n'
if ! $USE_RIFT_MIRRORS; then
    opts="--latest"
else
    opts=""
fi

./rift-shell -r -e -- ./scripts/vm_image/mkcontainer $opts --no-repo-file --modes ui  

echo -e '\n\n=========== Installing RIFT.ware Launchpad ========\n\n'
yum --nogpgcheck install -y riftware-launchpad
echo -e '\n\n=========== Starting RIFT.ware Launchpad ========\n\n'
systemctl start rwlp 









