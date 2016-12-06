# install the launchpad systemd service
# these files should work on both ub16 and fc20

cat <<EOF | sudo tee /etc/systemd/system/launchpad.service
[Unit]
Description=RIFT.ware Launchpad
After=network-online.target

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/sh -c 'nohup sudo -b -H /usr/rift/rift-shell -r -i /usr/rift -a /usr/rift/.artifacts -- ./demos/launchpad.py'
ExecStop=/bin/sh -c 'killall rwmain'

[Install]
WantedBy=default.target
EOF

sudo chmod 664 /etc/systemd/system/launchpad.service

if ! sudo systemctl daemon-reload; then
    echo "WARNING: Not able to reload daemons: this must be run in a privileged container: sudo systemctl daemon-reload ; sudo systemctl enable launchpad.service"
else
    # enable launchpad at boot - should always succeed in a privileged container
    sudo systemctl enable launchpad.service
fi

# start launchpad?
#sudo systemctl start launchpad.service

echo
echo "RIFT.ware Launchpad is now installed. Run 'sudo systemctl start launchpad.service' to start the service."
echo
