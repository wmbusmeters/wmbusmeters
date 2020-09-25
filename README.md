
# wmbusmeters
The program receives and decodes C1,T1 or S1 telegrams
(using the wireless mbus protocol) to acquire
utility meter readings. The readings can then be published using
MQTT, curled to a REST api, inserted into a database or stored in a log file.

[FAQ/WIKI/MANUAL pages](https://weetmuts.github.io/wmbusmeterswiki/)

The program runs on GNU/Linux, MacOSX, FreeBSD, and Raspberry Pi.

| OS           | Status           |
| ------------ |:-------------:|
|GNU/Linux & MacOSX| [![Build Status](https://travis-ci.org/weetmuts/wmbusmeters.svg?branch=master)](https://travis-ci.org/weetmuts/wmbusmeters) |
|Docker build status|[![CircleCI>](https://circleci.com/gh/weetmuts/wmbusmeters.svg?style=shield)](https://circleci.com/gh/weetmuts/wmbusmeters)|
|Snap status|[![wmbusmeters](https://snapcraft.io//wmbusmeters/badge.svg)](https://snapcraft.io/wmbusmeters)|


| Static Scan        | Status           |
| ------------- |:-------------:|
|Linux G++| [![Build Status](https://scan.coverity.com/projects/14774/badge.svg)](https://scan.coverity.com/projects/weetmuts-wmbusmeters) |

# Distributions
**wmbusmeters** package is available on [Fedora](https://src.fedoraproject.org/rpms/wmbusmeters) _(version 31 or newer)_ and can be simply installed by using:
```
# dnf install wmbusmeters
```
Availability of **wmbusmeters** for other Linux distributions can be checked on [release-monitoring](https://release-monitoring.org/project/88654/) project page.

# Run as a daemon

Remove the wmbus dongle (im871a,amb8465,rfmrx2,cul,d1tc) or the generic rtlsdr dongle (RTL2838) from your computer.

`make; sudo make install` will install wmbusmeters as a daemon that starts
automatically when an appropriate wmbus usb dongle is inserted in the computer.
(Note! make install only works for GNU/Linux. For MacOSX try to start
`wmbusmetersd /tmp/thepidfile` from a script instead. Here you can also override the device:
`wmbusmetersd --device=/dev/ttyXXY --listento=t1 /tmp/thepidfile`)

Check the config file /etc/wmbusmeters.conf and edit the device to
point to your dongle.
```
loglevel=normal
device=/dev/ttyUSB0:im871a
logtelegrams=false
format=json
meterfiles=/var/log/wmbusmeters/meter_readings
meterfilesaction=overwrite
meterfilesnaming=name
meterfilestimestamp=day
logfile=/var/log/wmbusmeters/wmbusmeters.log
shell=/usr/bin/mosquitto_pub -h localhost -t wmbusmeters/$METER_ID -m "$METER_JSON"
alarmshell=/usr/bin/mosquitto_pub -h localhost -t wmbusmeters_alarm -m "$ALARM_TYPE $ALARM_MESSAGE"
alarmtimeout=1h
alarmexpectedactivity=mon-sun(00-23)
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
(If you are using an rtlsdr dongle, then make sure the binaries /usr/bin/rtl_sdr and
/usr/bin/rtl_wmbus exists and are executable. If not you will see the
error message `(rtlwmbus) error: when starting as daemon, wmbusmeters expects /usr/bin/rtl_sdr to exist!`
and the daemon will refuse to start.)

The latest reading of the meter can also be found here: /var/log/wmbusmeters/meter_readings/MyTapWater

You can use several ids using `id=1111111,2222222,3333333` or you can listen to all
meters of a certain type `id=*` or you can suffix with star `id=8765*` to match
all meters with a given prefix. If you supply at least one positive match rule, then you
can add negative match rules as well. For example `id=*,!2222*`
which will match all meter ids, except those that begin with 2222.

You can add the static json data "address":"RoadenRd 456","city":"Stockholm" to every json message with the
wmbusmeters.conf setting:
```
json_address=RoadenRd 456
json_city=Stockholm
```
If you add `json_floor=5` to the meter file MyTapWater, then you can have the meter tailored
static json "floor":"5" added to telegrams handled by that particular meter.

If you are running on a Raspberry PI with flash storage and you relay the data to
another computer using a shell command (mosquitto_pub or curl or similar) then you might want to remove
`meterfiles` and `meterfilesaction` to minimize the writes to the local flash file system.

If you specify --metefilesaction=append --meterfilestimestamp=day then wmbusmeters will
append all todays received telegrams in for example the file Water_2019-12-11, the day
after the telegrams will be recorded in Water_2019-12-12. You can change the resolution
to day,hour,minute and micros. Micros means that every telegram gets their own file.

# Run using config files

If you cannot install as a daemon, then you can also start
wmbusmeters in your terminal using the config files in /etc/wmbusmeters.
```
wmbusmeters --useconfig=/
```

Or you can start wmbusmeters with your own config files:
```
wmbusmeters --useconfig=/home/me/.config/wmbusmeters
```

You can add --device and --listento to override the settings in the config. Like this:
```
wmbusmeters --useconfig=/home/me/.config/wmbusmeters --device=/dev/ttyXXY --listento=t1`
```

The files/dir should then be located here:
`/home/me/.config/wmbusmeters/etc/wmbusmeters.conf` and
`/home/me/.config/wmbusmeters/etc/wmbusmeters.d`

When running using config files then you can trigger a reload of the config files
using `sudo killall -HUP wmbusmetersd` or `killall -HUP wmbusmeters`
depending on if you are running as a daemon or not.

# Running without config files, good for experimentation and test.
```
wmbusmeters version: 0.9.15
Usage: wmbusmeters {options} <device>{:suffix} ( [meter_name] [meter_type]{:<modes>} [meter_id] [meter_key] )*

As <options> you can use:

    --addconversions=<unit>+ add conversion to these units to json and meter env variables (GJ)
    --debug for a lot of information
    --exitafter=<time> exit program after time, eg 20h, 10m 5s
    --format=<hr/json/fields> for human readable, json or semicolon separated fields
    --json_xxx=yyy always add "xxx"="yyy" to the json output and add shell env METER_xxx=yyy
    --listento=<mode> tell the wmbus dongle to listen to this single link mode where mode can be
                      c1,t1,s1,s1m,n1a,n1b,n1c,n1d,n1e,n1f
    --listento=c1,t1,s1 tell the wmbus dongle to listen to these link modes
                      different dongles support different combinations of modes
    --c1 --t1 --s1 --s1m ... another way to set the link mode for the dongle
    --listenvs list the env variables available for the meter
    --listfields list the fields selectable for the meter
    --logfile=<file> use this file instead of stdout
    --logtelegrams log the contents of the telegrams for easy replay
    --meterfiles=<dir> store meter readings in dir
    --meterfilesaction=(overwrite|append) overwrite or append to the meter readings file
    --meterfilesnaming=(name|id|name-id) the meter file is the meter's: name, id or name-id
    --meterfilestimestamp=(never|day|hour|minute|micros) the meter file is suffixed with a
                          timestamp (localtime) with the given resolution.
    --oneshot wait for an update from each meter, then quit
    --reopenafter=<time> close/reopen dongle connection repeatedly every <time> seconds, eg 60s, 60m, 24h
    --selectfields=id,timestamp,total_m3 select fields to be printed
    --separator=<c> change field separator to c
    --shell=<cmdline> invokes cmdline with env variables containing the latest reading
    --useconfig=<dir> load config files from dir/etc
    --usestderr write debug/verbose and logging output to stderr
    --verbose for more information

As <device> you can use:

auto, to have wmbusmeters look for the links /dev/im871a, /dev/amb8465, /dev/rfmrx2 and /dev/rtlsdr
(the links are automatically generated by udev if you have run the install scripts)

/dev/ttyUSB0:amb8465, if you have an amb8465 dongle assigned to ttyUSB0. Other suffixes are im871a,rfmrx2,d1tc,cul.

/dev/ttyUSB0, to have wmbusmeters auto-detect amb8465, im871a or CUL device.
(The rfmrx2 and the d1tc device cannot be autodetected right now, you have to specify it as a suffix on the device.)

/dev/ttyUSB0:38400, to have wmbusmeters set the baud rate to 38400 and listen for raw wmbus telegrams.
These telegrams are expected to have the data link layer crc bytes removed already!

rtlwmbus, to spawn the background process: "rtl_sdr -f 868.95M -s 1600000 - 2>/dev/null | rtl_wmbus"

rtlwmbus:868.9M, to tune to this fq instead.

rtl433, to spawn the background process: "rtl_433 -F csv -f 868.95M"

rtl433:868.9M, to tune to this fq instead.

rtlwmbus:<commandline>, to specify the entire background process command line that is expected to produce rtlwbus compatible output.
Likewise for rtl433.

stdin, to read raw binary telegrams from stdin.
These telegrams are expected to have the data link layer crc bytes removed already!

telegrams.bin, to read raw wmbus telegrams from this file.
These telegrams are expected to have the data link layer crc bytes removed already!

stdin:rtlwmbus, to read telegrams formatted using the rtlwmbus format from stdin. Works for rtl433 as well.

telegrams.msg:rtlwmbus, to read rtlwmbus formatted telegrams from this file. Works for rtl433 as well.

simulation_abc.txt, to read telegrams from the file (which has a name beginning with simulation_)
expecting the same format that is the output from --logtelegrams. This format also supports replay with timing.

As meter quadruples you specify:

<meter_name> a mnemonic for this particular meter
<meter_type> one of the supported meters
(can be suffixed with :<mode> to specify which mode you expect the meter to use when transmitting)
<meter_id> an 8 digit mbus id, usually printed on the meter
<meter_key> an encryption key unique for the meter
    if the meter uses no encryption, then supply NOKEY

Supported wmbus dongles:
IMST 871a (im871a)
Amber 8465 (amb8465)
BMeters RFM-RX2 (rfmrx2)
CUL family (cul)
rtl_wmbus (rtlwmbus)
rtl_433 (rtl433)

Supported water meters:
Apator at-wmbus-08   (apator08) (non-standard protocol)
Apator at-wmbus-16-2 (apator162) (non-standard protocol)
Aquametro/Integra Topas Es Kr (topaseskr)
Bmeters Hydrodigit (hydrodigit) (partly non-standard protocol)
Diehl/Sappel IZAR RC 868 I R4 PL (izar) (non-standard protocol)
Diehl HYDRUS (hydrus)
Honeywell Q400 (q400)
Kamstrup Multical 21 (multical21)
Kamstrup flowIQ 3100 (flowiq3100)
Sontex Supercom 587 (supercom587)
Sensus iPERL (iperl)
Techem MK Radio 3 (mkradio3) (non-standard protocol)
Waterstar M (waterstarm)

Currently not supported, please help!
Diehl/Sappel ACQUARIUS/IZAR R3 (izar3)

Supported heat cost allocators:
Innotas EurisII  (eurisii)
Qundis Q caloric (qcaloric)
Techem FHKV data II/III (fhkvdataiii)

Supported heat meter:
Heat meter Techem Compact V (compact5) (non-standard protocol)
(compact5 is unfortunately not quite supported since we lack a test telegram to prevent regressions)
Heat meter Techem Vario 4 (vario451) (non-standard protocol)
Heat meter Kamstrup Multical 302 (multical302) (in C1 mode, please open issue for T1 mode)
Heat and Cooling meter Kamstrup Multical 403 (multical403) (in C1 mode)
Heat and Cooling meter Kamstrup Multical 603 (multical403) (in C1 mode) (work in progress)

Supported room sensors:
Bmeters RFM-AMB Thermometer/Hygrometer (rfmamb)
Elvaco CMa12w Thermometer (cma12w)
Lansen Thermometer/Hygrometer (lansenth)

Supported smoke detectors:
Lansen Smoke Detector (lansensm)

Supported door/window detectors:
Lansen Door/Window Detector (lansendw)

Supported pulse counter:
Lansen Pulse Counter (lansenpu)

Supported electricity meters:
Easy Meter ESYS-WM20 (esyswm)
eBZ wMB-E01 (ebzwmbe)
EMH Metering (ehzp)
Tauron Amiplus (amiplus) (includes vendor apator and echelon)
```

The wmbus dongles imst871a can listen to one type of wmbus telegrams
at a time, ie either C1 or T1 telegrams. Thus you can listen to
multiple meters as long as they all require the same radio mode C1 or
T1.

However if you use amb8465 or rtlwmbus, then you can listen to both C1
and T1 telegrams at the same time.

# Usage examples

```
wmbusmeters --listento=c1 /dev/ttyUSB1:amb8465
```

Simply runs a scan with mode C1 to search for meters and print the IDs and any detected driver,
for example:
```
Received telegram from: 12345678
          manufacturer: (KAM) Kamstrup Energi (0x2c2d)
           device type: Cold water meter (0x16)
            device ver: 0x1b
         device driver: multical21
```

Now listen to this specific meter.

```
wmbusmeters /dev/ttyUSB0:im871a MyTapWater multical21:c1 12345678 00112233445566778899AABBCCDDEEFF
```

(The :c1 can be left out, since multical21 only transmits c1 telegrams. The suffix
with the expected link mode might be necessary for other meters, like apator162 for example.
The Multical21 uses compressed telegrams, which means that you might have to wait up to 8 telegrams
(8*16 seconds) until you receive a full length telegram which gives all the information needed
to decode the compressed telegrams.)

Example output:

`MyTapWater   12345678     6.388 m3     6.377 m3    0.000 m3/h     8°C    23°C   DRY(dry 22-31 days)     2018-03-05 12:02.50`

(Here the multical21 itself, is configured to send target volume, therefore the max flow is 0.000 m3/h.)

Example format json output:

`wmbusmeters --format=json /dev/ttyUSB0:im871a MyTapWater multical21 12345678 00112233445566778899AABBCCDDEEFF MyHeater multical302 22222222 00112233445566778899AABBCCDDEEFF`

`{"media":"cold water","meter":"multical21","name":"MyTapWater","id":"12345678","total_m3":6.388,"target_m3":6.377,"max_flow_m3h":0.000,"flow_temperature":8,"external_temperature":23,"current_status":"DRY","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"2018-02-08T09:07:22Z"}`

`{"media":"heat","meter":"multical302","name":"MyHeater","id":"22222222","total_kwh":0.000,"total_volume_m3":0.000,"current_kw":"0.000","timestamp":"2018-02-08T09:07:22Z"}`

Example format fields output and use rtlsdr dongle with rtlwmbus tuned to 868.9MHz instead of the
default 868.95MHz.

`wmbusmeters --format=fields rtlwmbus:868.9M GreenhouseWater multical21 33333333 NOKEY`

`GreenhouseTapWater;33333333;9999.099;77.712;0.000;11;31;;2018-03-05 12:10.24`

You can select a subset of all available fields:

`wmbusmeters --format=fields --selectfields=id,total_m3 /dev/ttyUSB0:im871a GreenhouseWater multical21 33333333 NOKEY`

`33333333;9999.099`

You can list all available fields for the meter by adding `--listfields` to the command line.

Eaxmple of using the shell command to publish to MQTT:

`wmbusmeters --shell='HOME=/home/you mosquitto_pub -h localhost -t water -m "$METER_JSON"' /dev/ttyUSB0:im871a GreenhouseWater multical21 33333333 NOKEY`

Eaxmple of using the shell command to inject data into postgresql database:

`wmbusmeters --shell="psql waterreadings -c \"insert into readings values ('\$METER_ID',\$METER_TOTAL_M3,'\$METER_TIMESTAMP') \" " /dev/ttyUSB0:amb8465 MyColdWater multical21 12345678 NOKEY`

You can have multiple shell commands and they will be executed in the order you gave them on the commandline.
Note that to single quotes around the command is necessary to pass the env variable names into wmbusmeters.
To list the shell env variables available for your meter, add --listenvs to the commandline:
`wmbusmeters --listenvs /dev/ttyUSB1:cul Water iperl 12345678 NOKEY`
which outputs:
```
Environment variables provided to shell for meter iperl:
METER_JSON
METER_TYPE
METER_ID
METER_TOTAL_M3
METER_MAX_FLOW_M3H
METER_TIMESTAMP
```
(If you have supplied --json_floor=5 then you will also see METER_floor in the list)

Note that the METER_TIMESTAMP and the timestamp in the json output, is in UTC format, this is not your localtime.
However the hr and fields output will print your localtime.

You can add `shell=commandline` to a meter file stored in wmbusmeters.d, then this meter will use
this shell command instead of the command stored in wmbusmeters.conf.

You can use `--debug` to get both verbose output and the actual data bytes sent back and forth with the wmbus usb dongle.

If the meter does not use encryption of its meter data, then enter NOKEY on the command line.

`wmbusmeters --format=json --meterfiles /dev/ttyUSB0:im871a MyTapWater multical21 12345678 NOKEY`

If you have a Kamstrup meters and you have received a KEM file and its password from your supplier, then you can use [utils/kem-import.py](utils/kem-import.py) utility to extract meter information from that file (including the AES key) and to create corresponding meter files in wmbusmetrs' config directory.

You can run wmbusmeters with --logtelegrams to get log output that can be placed in a simulation.txt
file. You can then run wmbusmeter and instead of an usb device, you provide the simulationt.xt
file as argument. See test.sh for more info.

If you do not specify any meters on the command line, then wmbusmeters
will listen and print the header information of any telegram it hears.
You must specify the listening mode.

With an rtlwmbus or amb8465 dongle: `wmbusmeters --listento=c1,t1 /dev/ttyUSB0:amb8465`

With an imst871a dongle: `wmbusmeters --listento=c1 /dev/ttyUSB0:im871a`

# Builds and runs on GNU/Linux MacOSX (with recent XCode), and FreeBSD

`make && make test`

Binary generated: `./build/wmbusmeters`

`make HOST=arm` to cross compile from GNU/Linux to Raspberry PI.

Binary generated: `./build_arm/wmbusmeters`

`make DEBUG=true`

Binary generated: `./build_debug/wmbusmeters`

Debug builds only work on FreeBSD if the compiler is LLVM. If your system default compiler is gcc, set `CXX=clang++` to the build environment to force LLVM to be used.

`make DEBUG=true HOST=arm`

Binary generated: `./build_arm_debug/wmbusmeters`

`make HOST=arm dist`

(Work in progress...)
Binary generated: `./wmbusmeters_0.8_armhf.deb`

# System configuration

`make install` installs the files:

`/etc/wmbusmeters.conf`
`/usr/bin/wmbusmeters`
`/usr/sbin/wmbusmetersd`
`/etc/systemd/system/wmbusmeters.service`
`/etc/udev/rules.d/99-wmbus-usb-serial.rules`
`/etc/logrotate.d/wmbusmeters`

creates these directories:
`/etc/wmbusmeters.d`
`/var/log/wmbusmeters/meter_readings`

and adds the user `wmbusmeters` with no login account.

This means that when a im871a/amb8465 dongle is inserted, then the
appropriate /dev/im871a or /dev/amb8465 link is created. Also the
wmbusmeters daemon will be automatically started/stopped whenever the
im871a/amb8465 dongle is inserted/removed, and the daemon starts when
the computer boots, if the dongle is already inserted.

You can start/stop the daemon with `sudo systemctl stop wmbusmeters@-dev-im871a_0.service`
or `sudo systemctl stop wmbusmeters@-dev-amb8465_1.service` etc.

You can trigger a reload of the config files with `sudo killall -HUP wmbusmetersd`

If you add more dongles, then more daemons gets started, each with a unique name/nr.

# Daemon without udev rules

To start the daemon without the udev rules. Then do:
`make install EXTRA_INSTALL_OPTIONS='--no-udev-rules'`
then no udev rules will be added.

(If you have already installed once before you might have to remove
/dev/udev/rules.d/99-wmbus-usb-serial.rules)

You can now start/stop the daemon with `sudo systemctl stop wmbusmeters`
the device must of course be correct in the /etc/wmbusmeters.conf file.

To have wmbusmeters start automatically when the computer boots do:
`sudo systemctl enable wmbusmeters`

# Common problems

If the daemon has started then the wmbus device will be taken and you cannot start wmbusmeters manually.

To run manually, first make sure the daemon is stopped `sudo stop wmbusmeters@-dev-im871a_0.server`
if this hangs, then do `sudo killall -9 wmbusmetersd` and/or `sudo killall -9 wmbusmeters`.

If you are using rtl_sdr/rtl_wmbus and you want to stop the daemon, do
`sudo stop wmbusmeters@-dev-rtlsdr_3.server` followed by `sudo killall -9 rtl_sdr`.

## How to receive telegrams over longer distances.

I only have personal experience of the im871a,amb8465 and an rtlsdr
compatible dongle.  The commercial dongles (im871a,amb8464) receive
well despite having tiny antennas inside the dongle. However the
reception range is limited by walls and you must definitely get quite
close to the meter if it is mounted underground in a concrete tube.

The rtlsdr/rtl-wmbus solution seems to work for a lot of users, but it
does use more cpu-power since it decodes the radio traffic in
software. Range seems to be similar to the other dongles, despite the
antenna being much larger.

At least one professional collector use the same commercial dongles,
but the versions with an external antenna connector, into which they
attach a radio amplifier for the proper frequency, and then a larger
antennna. This makes it possible to receive telegrams from meters
underground and over larger distances.

## Non-standard baud rate set for AMB8465 USB stick

Wmbusmeters expects the serial baud rate for the AMB8465 USB stick to be 9600 8n1.
If you have used another tool and changed the baud rate to something else
you need to restore the baud rate to 9600 8n1. You can do that with that other tool,
or you can try wmbusmeters-admin and select `Reset wmbus receives`
this command try all potential baud rates and send the factory reset command.
Then you have to unplug and reinsert the dongle.

If you like to send the bytes manually, the correct bytes are:
 * Factory reset of the settings: `0xFF1100EE`
 * Reset the stick to apply the factory defaults: `0xFF0500FA` this is not necessary if you unplug and reinsert the dongle.

## WMB13U-868, WMB14UE-868 USB sticks

These dongles do not seem to work with Linux, perhaps problems with the usb2serial pl2303 driver?

# Docker

Experimental docker containers are available here: https://hub.docker.com/r/weetmuts/wmbusmeters

# Source code

The source code is modular and it should be relatively straightforward to add more receivers and meters.

Read for example the text file: HowToAddaNewMeter.txt

# Caveat

If you do not get proper readings from the meters with non-standard protocols. apator162, mkradio3, vario451
then you have to open an issue here and help out by logging a lot of messages and reverse engineer them
even more..... :-/

# Good free documents on the wireless mbus protocol standard EN 13757:

https://oms-group.org/

There is also a lot of wmbus protocol implementation details that
are missing. They will be added to the program as we figure out
how the meters send their data.
