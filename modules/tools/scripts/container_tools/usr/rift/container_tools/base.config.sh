echo "export LC_ALL=en_US.UTF-8" >>$STAGING/etc/environment
if [ -e ${STAGING}/etc/selinux/config ]; then
    sed -i 's,^\(SELINUX=\).*,\1permissive,' ${STAGING}/etc/selinux/config || die "sed failed"
fi
cp ${VM_DIR}/vm/etc/sysctl.d/riftware.conf $STAGING/etc/sysctl.d/

cat >> ${STAGING}/etc/security/limits.d/90-rift.conf <<EOF
# RiftIO cranked fd limits
*  soft  nofile  8192
*  hard  nofile  32768
#
# allow core files
*   soft    core    unlimited
EOF

if ! $CONTAINER; then
	cmd setcap cap_net_raw+ep `which ping`
fi

# RIFT-15443: disable automatic background package updates
# By default, Ubuntu will regularly update any installed packages.
# Since we have our own packages without (currently) good dependencies hard-coded,
# this can easily replace a riftware forked package by an updated Ubuntu package,
# which causes launchpad to fail miserably.  Until we can get proper dependencies
# organized, we will turn off the service.
if [[ ${RIFT_PLATFORM} == 'ub16' ]]; then
    echo "==> Disabling unattended upgrades"

    # someday all of these will be unnecessary
    cmd_rc systemctl stop apt-daily.service
    cmd_rc systemctl kill --kill-who=all apt-daily.service
    cmd_rc systemctl disable apt-daily.timer 
    cmd_rc systemctl stop apt-daily.timer 

    cat << EOF > ${STAGING}/etc/apt/apt.conf.d/51disable-unattended-upgrades
APT::Periodic::Update-Package-Lists "0";
APT::Periodic::Unattended-Upgrade "0";
EOF

fi
