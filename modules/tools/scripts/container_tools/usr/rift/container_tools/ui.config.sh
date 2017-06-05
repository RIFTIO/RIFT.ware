# STANDARD_RIFT_IO_COPYRIGHT


test -h /usr/rift && rm -f /usr/rift


if [[ ${RIFT_PLATFORM} == 'fc20' ]]; then
    # upgrade node binary manually
    echo "===== Upgrading node binary"
    wget -O $STAGING/usr/bin/node http://repos.riftio.com/public/npm/node
    chmod 755 $STAGING/usr/bin/node
fi

# NPM options
if $ONLY_RIFT_PKG_REPOS ; then
    cmd npm config set registry https://npm.riftio.com:4873/ --global
fi
cmd npm config set strict-ssl false --global
cmd npm config set progress=false --global
cmd npm config set cache /tmp/npm-cache --global

if ${AUFS}; then
    echo "===== AUFS fix"
    pth="/localdisk/$(hostname -s)"
    mkdir -p $pth
    tar -C /usr/lib -c -f - node_modules | tar -C $pth -x -f -
    rm -rf /usr/lib/node_modules
    ln -s $pth/node_modules /usr/lib/node_modules
fi

if [[ ${RIFT_PLATFORM} == 'fc20' ]]; then
    # NPM self-update
    echo "===== NPM self-update take #1"
    cmd npm install npm@3.8.6 -g
    echo "===== NPM self-update take #2 (twice is required to pull in and update everything!)"
    cmd npm install npm@3.8.6 -g
fi

# upgrade node-gyp?
#npm explore npm -g -- npm install node-gyp

# NPM install forever globally
cmd npm install -g --production forever

echo "===== Checking Versions:"
echo "node:"; cmd $NODEJS -v
echo "npm:"; cmd npm -v
echo "node-gyp:"; cmd node-gyp -v

if [[ -f /etc/sysconfig/libvirtd ]]; then
    cmd systemctl enable libvirtd
elif [[ -f etc/init/libvirt-bin.conf ]]; then
    cmd systemctl enable libvirt-bin
fi

# cmd gem install redis

if [[ ${RIFT_PLATFORM} == 'ub16' ]]; then
    rm -f /usr/bin/node
    ln -s /usr/bin/nodejs /usr/bin/node
fi

if [[ ${RIFT_PLATFORM} == 'ub16' ]]; then
    usermod -a -G adm www-data
    systemctl stop apache2
    /usr/sbin/a2dismod mpm_event
    /usr/sbin/a2enmod mpm_prefork
    systemctl start apache2
fi

# RIFT-14849: stop and disable glance at startup
# Disable the system glance from starting up at system boot time
# This causes a conflict with our version of glance (RIFT-14933)
if [[ ${RIFT_PLATFORM} == 'ub16' ]]; then
    
  # Ignore (and warn) if these commands fail - when installing
  # in a container using a Dockerfile, we expect these to fail.
  cmd_warn systemctl stop glance-api
  cmd_warn systemctl stop glance-registry

  cmd_warn systemctl disable glance-api
  cmd_warn systemctl disable glance-registry
fi
