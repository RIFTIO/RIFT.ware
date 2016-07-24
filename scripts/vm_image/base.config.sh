echo "export LC_ALL=en_US.UTF-8" >>$STAGING/etc/environment
if [ -e ${STAGING}/etc/selinux/config ]; then
    sed -i 's,^\(SELINUX=\).*,\1permissive,' ${STAGING}/etc/selinux/config || die "sed failed"
fi
cp $RIFTROOT/scripts/setup/skel/etc/sysctl.d/riftware.conf $STAGING/etc/sysctl.d/

cat >> ${STAGING}/etc/security/limits.d/90-rift.conf <<EOF
# RiftIO cranked fd limits
*  soft  nofile  8192
*  hard  nofile  32768
#
# allow core files
*   soft    core    unlimited
EOF

cmd setcap cap_net_raw+ep `which ping`
