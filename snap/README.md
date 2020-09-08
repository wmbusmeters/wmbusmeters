Installation of snapd
===============================

Please refer to official documentation for snapd installation - https://snapcraft.io/docs/installing-snapd

Installation of wmbusmeters Snap
===============================

Installing a snap is straightforward:

    sudo snap install wmbusmeters
    
The stable channel is used by default, but opting to install from a edge channel is easily accomplished:

    sudo snap install --channel=edge wmbusmeters
    
In stable channel latest releases of wmbusmeters will be available, but in edge channel wmbusmeters snap will contain all latest code changes.

Using the wmbusmeters Snap
===============================

After installing the Snap, the privileged access to USB interfaces need to be configured:

    sudo snap connect wmbusmeters:raw-usb core:raw-usb

Using wmbusmeters Daemon
-------------------------------------------

For all actions with wmbusmeters daemon, like enable/disable/start/stop/restart/status
systemctl commands should be used

For example:

    systemctl start snap.wmbusmeters.wmbusmeters.service
    systemctl status snap.wmbusmeters.wmbusmeters.service

Configuration for wmbusmeters
-----------------------------

Configuration of wmbusmeters is located in
   `/var/snap/wmbusmeters/common/etc/`

Logs are in 
   `/var/snap/wmbusmeters/common/logs/`
   
When changes are made to configuration, service must be restarted.

Those directories are persistent across updates - files will not be deleted or changed when updating snap. 
Files and directories are being deleted only when snap is removed.

When using rtlwmbus, rtl433 or shell commands then LD_LIBRARY_PATH and full path to binary inside snap should be used.

In case of rtlwmbus following configuration should be used:

    device=rtlwmbus:LD_LIBRARY_PATH=/var/lib/snapd/lib/gl:/var/lib/snapd/lib/gl32:/var/lib/snapd/void:/snap/wmbusmeters/current/lib/x86_64-linux-gnu:/snap/wmbusmeters/current/usr/lib/x86_64-linux-gnu::/snap/wmbusmeters/current/lib:/snap/wmbusmeters/current/usr/lib:/snap/wmbusmeters/current/lib/x86_64-linux-gnu:/snap/wmbusmeters/current/usr/lib/x86_64-linux-gnu /snap/wmbusmeters/current/usr/bin/rtl_sdr -f 868.95M -s 1600000 - 2>/dev/null | /snap/wmbusmeters/current/usr/bin/rtl_wmbus
    
 or rtl433
 
     device=rtl433:LD_LIBRARY_PATH=/var/lib/snapd/lib/gl:/var/lib/snapd/lib/gl32:/var/lib/snapd/void:/snap/wmbusmeters/current/lib/x86_64-linux-gnu:/snap/wmbusmeters/current/usr/lib/x86_64-linux-gnu::/snap/wmbusmeters/current/lib:/snap/wmbusmeters/current/usr/lib:/snap/wmbusmeters/current/lib/x86_64-linux-gnu:/snap/wmbusmeters/current/usr/lib/x86_64-linux-gnu /snap/wmbusmeters/current/usr/bin/rtl_433 -F csv -f 868.95M
     
 or when shell command is being used 
 
    shell=LD_LIBRARY_PATH=/var/lib/snapd/lib/gl:/var/lib/snapd/lib/gl32:/var/lib/snapd/void:/snap/wmbusmeters/current/lib/x86_64-linux-gnu:/snap/wmbusmeters/current/usr/lib/x86_64-linux-gnu::/snap/wmbusmeters/current/lib:/snap/wmbusmeters/current/usr/lib:/snap/wmbusmeters/current/lib/x86_64-linux-gnu:/snap/wmbusmeters/current/usr/lib/x86_64-linux-gnu /snap/wmbusmeters/current/usr/bin/mosquitto_pub -h localhost -t water -m '$METER_JSON'
 
