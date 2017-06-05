# install the launchpad systemd service
# these files should work on both ub16 and fc20

mkdir -p /var/log/rift
chmod 777 /var/log/rift 
touch /var/log/rift/launchpad.log
chmod 644 /var/log/rift/launchpad.log
chown syslog:adm /var/log/rift/launchpad.log

cat <<EOF >/etc/systemd/system/launchpad.service
[Unit]
Description=RIFT.ware Launchpad
After=network-online.target

[Service]
Type=forking
ExecStart=/bin/sh -c 'nohup /usr/rift/bin/start_launchpad 2>&1 | logger &'
ExecStop=/bin/sh -c 'killall rwmain'

[Install]
WantedBy=default.target
EOF

cat <<EOF >/etc/logrotate.d/rift
/var/log/rift/*.log {
        daily
        missingok
        rotate 14
        compress
        delaycompress
        notifempty
        create 666 syslog adm
        sharedscripts
        postrotate
                invoke-rc.d rsyslog rotate > /dev/null
        endscript
}
EOF

if ! grep -q "RIFT Disk File" /etc/loganalyzer/config.php; then 
    sed -i '/\?>/i $CFG["Sources"]["Source2"]["ID"] = "Source2";\n$CFG["Sources"]["Source2"]["Name"] = "RIFT Disk File";\n$CFG["Sources"]["Source2"]["Description"] = "RIFT.log";\n$CFG["Sources"]["Source2"]["SourceType"] = SOURCE_DISK;\n$CFG["Sources"]["Source2"]["LogLineType"] = "syslog23";\n$CFG["Sources"]["Source2"]["MsgParserList"] = "";\n$CFG["Sources"]["Source2"]["MsgNormalize"] = 0;\n$CFG["Sources"]["Source2"]["DiskFile"] = "/var/log/rift/rift.log";\n$CFG["Sources"]["Source2"]["ViewID"] = "SYSLOG";\n\n$CFG["Sources"]["Source3"]["ID"] = "Source3";\n$CFG["Sources"]["Source3"]["Name"] = "RIFT console";\n$CFG["Sources"]["Source3"]["Description"] = "RIFT console log";\n$CFG["Sources"]["Source3"]["SourceType"] = SOURCE_DISK;\n$CFG["Sources"]["Source3"]["LogLineType"] = "syslog23";\n$CFG["Sources"]["Source3"]["MsgParserList"] = "";\n$CFG["Sources"]["Source3"]["MsgNormalize"] = 0;\n$CFG["Sources"]["Source3"]["DiskFile"] = "/var/log/rift/launchpad.log";\n$CFG["Sources"]["Source3"]["ViewID"] = "SYSLOG";\n ' /etc/loganalyzer/config.php
fi


sudo chmod 664 /etc/systemd/system/launchpad.service

if ! systemctl daemon-reload; then
    echo "WARNING: Not able to reload daemons: this must be run in a privileged container: sudo systemctl daemon-reload ; sudo systemctl enable launchpad.service"
else
    # enable launchpad at boot - should always succeed in a privileged container
    systemctl enable launchpad.service
fi
systemctl restart syslog

# start launchpad?
#sudo systemctl start launchpad.service

apachectl restart

echo
echo "RIFT.ware Launchpad is now installed. Run 'sudo systemctl start launchpad.service' to start the service."
echo
