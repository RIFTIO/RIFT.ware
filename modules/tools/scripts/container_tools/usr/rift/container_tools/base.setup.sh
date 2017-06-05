# Install enum34 if we are at python 3.4 or earlier

test -h /usr/rift && rm -f /usr/rift 
chmod 777 /usr/rift /usr/rift/usr/share


if python3 -c 'import sys;assert sys.version_info < (3,5)' >/dev/null 2>&1; then
    echo "found python3 older than 3.5 ... installing enum34==1.0.4"
    cmd $PIP3_INSTALLER install enum34==1.0.4
else
    echo "found python3 at least version 3.5 .. skipping enum34"
fi

if [[ $RIFT_PLATFORM == 'ub16' ]]; then
    cat <<EOF >>/etc/apt/apt.conf.d/20auto-upgrades
APT::Periodic::Unattended-Upgrade "0";
APT::Periodic::Download-Upgradeable-Packages "0";
EOF
fi

