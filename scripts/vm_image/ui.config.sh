
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

# upgrade node binary manually
if [[ -f $STAGING/usr/bin/node ]]; then
	echo "===== Upgrading node binary"
	cp -v /net/strange/localdisk/nhudson/RIFT-9422/node-v4.4.2-linux-x64/bin/node $STAGING/usr/bin/node
fi

# NPM options
cmd npm set registry https://npm.riftio.com:4873/
cmd npm set strict-ssl false
cmd npm set progress=false

# NPM self-update
echo "===== NPM self-update take #1"
cmd npm install npm@3.8.6 -g
echo "===== NPM self-update take #2 (twice is required to pull in and update everything!)"
cmd npm install npm@3.8.6 -g

# upgrade node-gyp?
#npm explore npm -g -- npm install node-gyp

# NPM install forever globally
cmd npm install -g --production forever

echo "===== Checking Versions:"
echo "node:"; cmd node -v
echo "npm:"; cmd npm -v
echo "node-gyp:"; cmd node-gyp -v

# Salt
echo 'auto_accept: True' >>$STAGING/etc/salt/master
cmd systemctl enable salt-master
cmd systemctl enable salt-minion
cmd systemctl enable libvirtd
