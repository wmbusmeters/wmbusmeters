# Copyright (C) 2021 Fredrik Öhrström (gpl-3.0-or-later)
#
# Create /lib/systemd/system/wmbusmeters.service
#

if [ -f /etc/systemd/system/wmbusmeters.service ]
then
    echo "Oups! Found an old wmbusmeters.service here /etc/systemd/system/wmbusmeters.service"
    echo "Removing this! Keeping a backup here though $HOME/ancient.wmbusmeters.service"
    mv /etc/systemd/system/wmbusmeters.service $HOME/ancient.wmbusmeters.service
fi

if [ -f /etc/udev/rules.d/99-wmbus-usb-serial.rules ]
then
    echo "Oups! Found an old udev rule here /etc/udev/rules.d/99-wmbus-usb-serial.rules"
    echo "Removing this! Keeping a backup here though $HOME/ancient.99-wmbus-usb-serial.rules"
    mv /etc/udev/rules.d/99-wmbus-usb-serial.rules $HOME/ancient.99-wmbus-usb-serial.rules
fi

SYSTEMD_NEEDS_RELOAD=false
mkdir -p "$ROOT"/lib/systemd/system/

OLD_WMBS=~/old.wmbusmeters.service.backup
CURR_WMBS="$ROOT"/lib/systemd/system/wmbusmeters.service
if [ -f $CURR_WMBS ]
then
    echo "systemd: backing up $CURR_WMBS to here: $OLD_WMBS"
    cp $CURR_WMBS $OLD_WMBS 2>/dev/null
fi

# Create service file for starting daemon without explicit device.
# This means that wmbusmeters will rely on the conf file device setting.
cat <<'EOF' > $CURR_WMBS
[Unit]
Description="wmbusmeters service"
Documentation=https://github.com/weetmuts/wmbusmeters
Documentation=man:wmbusmeters(1)
After=network.target
StopWhenUnneeded=false
StartLimitIntervalSec=10
StartLimitInterval=10
StartLimitBurst=3

[Service]
Type=forking
PrivateTmp=yes
User=wmbusmeters
Group=wmbusmeters
Restart=always
RestartSec=1

# Run ExecStartPre with root-permissions

PermissionsStartOnly=true
ExecStartPre=-/bin/mkdir -p /var/lib/wmbusmeters/meter_readings
ExecStartPre=/bin/chown -R wmbusmeters:wmbusmeters /var/log/wmbusmeters
ExecStartPre=-/bin/mkdir -p /run/wmbusmeters
ExecStartPre=/bin/chown -R wmbusmeters:wmbusmeters /run/wmbusmeters

ExecStart=/usr/sbin/wmbusmetersd /run/wmbusmeters/wmbusmeters.pid
ExecReload=/bin/kill -HUP $MAINPID
PIDFile=/run/wmbusmeters/wmbusmeters.pid

[Install]
WantedBy=multi-user.target
EOF

if diff $OLD_WMBS $CURR_WMBS 1>/dev/null
then
    echo "systemd: no changes to $CURR_WMBS"
else
    echo "systemd: updated $CURR_WMBS"
    SYSTEMD_NEEDS_RELOAD=true
fi

OLD_WMBAS=~/old.wmbusmeters@.service.backup
CURR_WMBAS="$ROOT"/lib/systemd/system/wmbusmeters@.service
if [ -f $CURR_WMBAS ]
then
    echo "systemd: removing $CURR_WMBAS"
    echo "systemd: backing up $CURR_WMBAS to here: $OLD_WMBAS"
    cp $CURR_WMBAS $OLD_WMBAS 2>/dev/null
    rm $CURR_WMBAS
    SYSTEMD_NEEDS_RELOAD=true
fi

if [ "$SYSTEMD_NEEDS_RELOAD" = "true" ]
then
    echo
    echo
    echo "You need to reload systemd configuration! Please do:"
    echo "sudo systemctl daemon-reload"
fi
