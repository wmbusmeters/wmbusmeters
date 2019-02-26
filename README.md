# wmbusmeters
The program receives and decodes C1 or T1 telegrams
(using the wireless mbus protocol) to acquire
utility meter readings. The readings can then be published using
MQTT, curled to a REST api, inserted into a database or stored in a log file.

The program runs on GNU/Linux (standard x86) and Raspberry Pi (arm).

| OS           | Status           |
| ------------ |:-------------:|
|GNU/Linux & MacOSX| [![Build Status](https://travis-ci.org/weetmuts/wmbusmeters.svg?branch=master)](https://travis-ci.org/weetmuts/wmbusmeters) |

| Static Scan        | Status           |
| ------------- |:-------------:|
|Linux G++| [![Build Status](https://scan.coverity.com/projects/14774/badge.svg)](https://scan.coverity.com/projects/weetmuts-wmbusmeters) |


# Run as a daemon

Remove the wmbus dongle (im871a or amb8465) from your computer.

`sudo make install` will install wmbusmeters as a daemon that starts
automatically when an appropriate wmbus usb dongle is inserted in the computer.

Check the config file /etc/wmbusmeters.conf:
```
loglevel=normal
device=auto
logtelegrams=false
meterfilesdir=/var/log/wmbusmeters/meter_readings
logfile=/var/log/wmbusmeters/wmbusmeters.log
shell=/usr/bin/mosquitto_pub -h localhost -t wmbusmeters -m "$METER_JSON"
```

Then add a meter file in /etc/wmbusmeters.d/MyTapWater
```
name=MyTapWater
type=multical21
id=12345678
key=00112233445566778899AABBCCDDEEFF
```

Now plugin your wmbus dongle. Wmbusmeters should start automatically,
check with `tail -f /var/log/syslog` and `tail -f /var/log/wmbusmeters/wmbusmeters.log`

The latest reading of the meter can also be found here: /var/log/wmbusmeters/meter_readings/MyTapWater

# Run using config files

If you cannot install as a daemon, then you can also start
wmbusmeters in your terminal using the config files in /etc/wmbusmeters.
```
wmbusmeters --useconfig
```

Or you can start wmbusmeters with your own config files:
```
wmbusmeters --useconfig=/home/me/.config/wmbusmeters
```

The files/dir should then be located here:
`/home/me/.config/wmbusmeters/etc/wmbusmeters.conf` and
`/home/me/.config/wmbusmeters/etc/wmbusmeters.d`


```
wmbusmeters version: 0.9
Usage: wmbusmeters {options} (auto | /dev/ttyUSBx)] { [meter_name] [meter_type] [meter_id] [meter_key] }*

Add more meter quadruplets to listen to more meters.
Add --verbose for detailed debug information.
    --robot or --robot=json for json output.
    --robot=fields for semicolon separated fields.
    --separator=X change field separator to X.
    --logfile=file
    --meterfiles=dir to create status files below dir,
          named dir/meter_name, containing the latest reading.
    --meterfiles defaults dir to /tmp.
    --c1 or --t1 listen to C1 or T1 messages
    --shell=cmd invokes cmd with env variables containing the latest reading.
    --shellenvs list the env variables available for the meter.
    --oneshot wait for an update from each meter, then quit.
    --useconfig=dir look for configuration file in dir/etc/wmbusmeters.conf and
          dir/etc/wmbusmeters.d
    --useconfig defaults to the root /etc
    --exitafter=20h program exits after running for twenty hours,
          or 10m for ten minutes or 5s for five seconds.

Specifying auto as the device will automatically look for usb
wmbus dongles on /dev/im871a and /dev/amb8465.

You can specify the device rtlwmbus to have wmbusmeters spawn rtl_sdr|rtlwmbus

Supported water meters:
Kamstrup Multical 21 (multical21)
Kamstrup flowIQ 3100 (flowiq3100)
Sontex Supercom 587 (supercom587)
Sensus iPERL (iperl)

Work in progress:
Heat meter Kamstrup Multical 302 (multical302)
Electricity meter Kamstrup Omnipower (omnipower)
Heat cost allocator Qundis Q caloric (qcaloric)
```

No meter quadruplets means listen for telegram traffic and print any id heard,
but you have to specify if you want to listen using radio mode C1 or T1. E.g.

```
wmbusmeters --t1 /dev/ttyUSB0
```

You can listen to multiple meters as long as they all require the same radio mode C1 or T1.
So (currently) you cannot listen to a multical21 and a supercom587 with a single dongle at the same time.

# Usage examples

```
wmbusmeters /dev/ttyUSB0 MyTapWater multical21 12345678 00112233445566778899AABBCCDDEEFF
```

wmbusmeters will detect which kind of dongle is connected to /dev/ttyUSB0. It can be either an IMST 871a dongle or an Amber Wireless AMB8465. If you have setup the udev rules below, then you can use auto instead of /dev/ttyUSB0.

Example output:

`MyTapWater   12345678     6.388 m3     6.377 m3    0.000 m3/h     8°C    23°C   DRY(dry 22-31 days)     2018-03-05 12:02.50`

(Here the multical21 itself, is configured to send target volume, therefore the max flow is 0.000 m3/h.)

Example robot json output:

`wmbusmeters --robot=json auto MyTapWater multical21 12345678 00112233445566778899AABBCCDDEEFF MyHeater multical302 22222222 00112233445566778899AABBCCDDEEFF`

`{"media":"cold water","meter":"multical21","name":"MyTapWater","id":"12345678","total_m3":6.388,"target_m3":6.377,"max_flow_m3h":0.000,"flow_temperature":8,"external_temperature":23,"current_status":"DRY","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"2018-02-08T09:07:22Z"}`

`{"media":"heat","meter":"multical302","name":"MyHeater","id":"22222222","total_kwh":0.000,"total_volume_m3":0.000,"current_kw":"0.000","timestamp":"2018-02-08T09:07:22Z"}`

Example robot fields output:

`wmbusmeters --robot=fields auto GreenhouseWater multical21 33333333 ""`

`GreenhouseTapWater;33333333;9999.099;77.712;0.000;11;31;;2018-03-05 12:10.24`

Eaxmple of using the shell command to publish to MQTT:

`wmbusmeters --shell='HOME=/home/you mosquitto_pub -h localhost -t water -m "$METER_JSON"' auto GreenhouseWater multical21 33333333 ""`

Eaxmple of using the shell command to inject data into postgresql database:

`wmbusmeters --shell="psql waterreadings -c \"insert into readings values ('\$METER_ID',\$METER_TOTAL_M3,'\$METER_TIMESTAMP') \" " auto MyColdWater multical21 12345678 ""`

You can have multiple shell commands and they will be executed in the order you gave them on the commandline.
Note that to single quotes around the command is necessary to pass the env variable names into wmbusmeters.

You can use `--debug` to get both verbose output and the actual data bytes sent back and forth with the wmbus usb dongle.

If the meter does not use encryption of its meter data, then enter an empty key on the command line.
(you must enter "")

`wmbusmeters --robot --meterfiles auto MyTapWater multical21 12345678 ""`

If you have a Multical21 meter and you have received a KEM file and its password,
from your water municipality, then you can use the XMLExtract.java utility to get
the meter key from the KEM file. You need to unzip the the KEM file first though,
if it is zipped.

You can run wmbusmeters with --logtelegrams to get log output that can be placed in a simulation.txt
file. You can then run wmbusmeter and instead of auto (or an usb device) provide the simulationt.xt
file as argument. See test.sh for more info.

# Builds and runs on GNU/Linux:

`make && make test`

Binary generated: `./build/wmbusmeters`

`make HOST=arm`

Binary generated: `./build_arm/wmbusmeters`

`make DEBUG=true`

Binary generated: `./build_debug/wmbusmeters`

`make DEBUG=true HOST=arm`

Binary generated: `./build_arm_debug/wmbusmeters`

`make HOST=arm dist`

(Work in progress...)
Binary generated: `./wmbusmeters_0.8_armhf.deb`

# System configuration

Add yourself to the dialout group to get access to the newly plugged in im871A USB stick.
Or even better, add this udev rule:

Create the file: `/etc/udev/rules.d/99-usb-serial.rules` with the content
```
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", SYMLINK+="im871a",MODE="0660", GROUP="yourowngroup"
SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", SYMLINK+="amb8465",MODE="0660", GROUP="yourowngroup"
```

When you insert the wmbus USB dongle, a properly named symlink will be
created: either `/dev/im871a` or `/dev/amb8465`. These symlinks are
necessary if you want to pass "auto" to wmbusmeters instead of the
exact serial port /dev/ttyUSBx.

# Source code

The source code is modular and it should be relatively straightforward to add more receivers and meters.

Read for example the text file: HowToAddaNewMeter.txt

# Good documents on the wireless mbus protocol:

https://oms-group.org/download4all/

http://www.m-bus.com/files/w4b21021.pdf

https://www.infineon.com/dgdl/TDA5340_AN_WMBus_v1.0.pdf

http://fastforward.ag/downloads/docu/FAST_EnergyCam-Protocol-wirelessMBUS.pdf

http://www.multical.hu/WiredMBus-water.pdf

http://uu.diva-portal.org/smash/get/diva2:847898/FULLTEXT02.pdf

http://projekter.aau.dk/projekter/da/studentthesis/wireless-mbus-based-extremely-low-power-protocol-for-wireless-communication-with-water-meters(6e1139d5-6f24-4b8a-a727-9bc108012bcc).html

The AES source code is copied from:

https://github.com/kokke/tiny-AES128-C

TODO: CRC checks are still missing. If the wrong AES key
is supplied you probably get zero readings and
sometimes warnings about wrong type of frames.

There is also a lot of wmbus protocol implementation details that
probably are missing. They will be added to the program
as we figure out how the meters send their data.
