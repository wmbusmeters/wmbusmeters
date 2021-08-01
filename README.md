
# wmbusmeters

The program receives and decodes C1,T1 or S1 telegrams
(using the wireless mbus protocol) to acquire
utility meter readings. The readings can then be published using
MQTT, curled to a REST api, inserted into a database or stored in a log file.

[FAQ/WIKI/MANUAL pages](https://weetmuts.github.io/wmbusmeterswiki/)

The program runs on GNU/Linux, MacOSX, FreeBSD, and Raspberry Pi.

| System       | Status        |
| ------------ |:-------------:|
| Ubuntu | [![Build Ubuntu Status](https://github.com/weetmuts/wmbusmeters/workflows/Build%20Ubuntu/badge.svg)](https://github.com/weetmuts/wmbusmeters/actions)|
| MacOSX | [![Build MacOSX Status](https://github.com/weetmuts/wmbusmeters/workflows/Build%20MacOSX/badge.svg)](https://github.com/weetmuts/wmbusmeters/actions)|
|Docker |[![CircleCI>](https://circleci.com/gh/weetmuts/wmbusmeters.svg?style=shield)](https://circleci.com/gh/weetmuts/wmbusmeters)|
|Snap |[![wmbusmeters](https://snapcraft.io//wmbusmeters/badge.svg)](https://snapcraft.io/wmbusmeters)|

# Distributions

**wmbusmeters** package is available on [Fedora](https://src.fedoraproject.org/rpms/wmbusmeters) _(version 31 or newer)_ and can be simply installed by using:

```shell
dnf install wmbusmeters
```

Availability of **wmbusmeters** for other Linux distributions can be checked on [release-monitoring](https://release-monitoring.org/project/88654/) project page.

# Docker

Experimental docker containers are available here: https://hub.docker.com/r/weetmuts/wmbusmeters

# Snap

Experimental snaps are available here: https://snapcraft.io/wmbusmeters
Read the wiki for more info on how to use the snap: https://weetmuts.github.io/wmbusmeterswiki/SNAP.html

# Build from source and run as a daemon

Building and installing from source is easy and recommended since the
development progresses quickly.  First remove the wmbus dongle
(im871a,amb8465,cul,rc1180) or the generic rtlsdr dongle (RTL2832U)
from your computer. Then do:

`./configure; make; sudo make install` will install wmbusmeters as a daemon.

Check the contents of your `/etc/wmbusmeters.conf` file, assuming it
has `device=auto:t1` and you are using a im871a,amb8465,rc1180 or cul device,
then you can now start the daemon with `sudo systemctl start wmbusmeters`
or you can try it from the command line `wmbusmeters auto:t1`

Wmbusmeters will scan for wmbus devices every few seconds and detect whenever
a device is plugged in or removed.

To have the wmbusmeters daemon start automatically when the computer boots do:
`sudo systemctl enable wmbusmeters`

You can trigger a reload of the config files with `sudo killall -HUP wmbusmetersd`

(Note! make install only works for GNU/Linux. For MacOSX try to start
`wmbusmetersd /tmp/thepidfile` from a script instead.)

Check the config file /etc/wmbusmeters.conf and edit the device. For example:
`auto:c1` or `im871a:c1,t1` or `im871a[457200101056]:t1` or `/dev/ttyUSB2:amb8465:c1,t1`

Adding a device like auto or im871a will trigger an automatic probe of all serial ttys
to auto find or to find on which tty the im871a resides.

If you specify a full device path like `/dev/ttyUSB0:im871a:c1` or `rtlwmbus` or `rtl433`
then it will not probe the serial devices. If you must be really sure that it will not probe something
you can add `donotprobe=/dev/ttyUSB0` or `donotprobe=all`.

You can specify combinations like: `device=rc1180:t1` `device=auto:c1`
to set the rc1180 dongle to t1 but any other auto-detected dongle to c1.

```ini
loglevel=normal
# Search for a wmbus device and set it to c1.
device=auto:c1
# But do not probe this serial tty.
donotprobe=/dev/ttyACM2
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
ignoreduplicates=true
```

Then add a meter file in /etc/wmbusmeters.d/MyTapWater

```ini
name=MyTapWater
id=12345678
key=00112233445566778899AABBCCDDEEFF
```

Meter driver detection will be automatic. You can also provide an
explicit driver name with: `driver=multical21:c1` or explicitly state
that driver detection is automatic: `driver=auto`.

Now plugin your wmbus dongle.
Wmbusmeters should start automatically, check with `tail -f /var/log/syslog` and `tail -f /var/log/wmbusmeters/wmbusmeters.log`
(If you are using an rtlsdr dongle, then make sure that either the binaries `/usr/bin/rtl_sdr` and
`/usr/bin/rtl_wmbus` exists and are executable. Or `rtl_sdr/rtl_wmbus` exists inside the same directory
as the wmbusmeters directory is located. If not you will see the
error message `(rtlwmbus) error: when starting as daemon, wmbusmeters looked for .../rtl_wmbus and /usr/bin/rtl_wmbus, but found neither!`
and the daemon will refuse to start.)

The latest reading of the meter can also be found here: `/var/log/wmbusmeters/meter_readings/MyTapWater`

You can use several ids using `id=1111111,2222222,3333333` or you can listen to all
meters of a certain type `id=*` or you can suffix with star `id=8765*` to match
all meters with a given prefix. If you supply at least one positive match rule, then you
can add negative match rules as well. For example `id=*,!2222*`
which will match all meter ids, except those that begin with 2222.

You can add the static json data `"address":"RoadenRd 456","city":"Stockholm"` to every json message with the
wmbusmeters.conf setting:

```ini
field_address=RoadenRd 456
field_city=Stockholm
```

If you add `field_floor=5` to the meter file `MyTapWater`, then you can have the meter tailored static json `"floor":"5"` added to telegrams handled by that particular meter. (The old prefix json_ still works.)

If you are running on a Raspberry PI with flash storage and you relay the data to
another computer using a shell command (`mosquitto_pub` or `curl` or similar) then you might want to remove `meterfiles` and `meterfilesaction` to minimize the writes to the local flash file system.

If you specify `--meterfilesaction=append --meterfilestimestamp=day` then wmbusmeters will append all todays received telegrams in for example the file `Water_2019-12-11`, the day after the telegrams will be recorded in `Water_2019-12-12`. You can change the resolution to day,hour,minute and micros. Micros means that every telegram gets their own file.

The purpose of the alarm shell and timeout is to notify you about
problems within wmbusmeters and the wmbus dongles, not the meters
themselves. Thus the timeout is for a dongle to receive some telegram
at all. It does not matter from which meter.

# Run using config files

If you cannot install as a daemon, then you can also start
wmbusmeters in your terminal using the config files in `/etc/wmbusmeters`.

```shell
wmbusmeters --useconfig=/
```

Or you can start wmbusmeters with your own config files:

```shell
wmbusmeters --useconfig=/home/me/.config/wmbusmeters
```

If you already have config with a device specified, and you want to use
the config with another device. You might have multiple meters in the config
that you want to listen to. Then you can add `--device` to override the settings
in the config. Like this:

```shell
wmbusmeters --useconfig=/home/me/.config/wmbusmeters --device=rtlwmbus
```

You must have both `--useconfig=` and `--device=` for it to work.

The files/dir should then be located here:
`/home/me/.config/wmbusmeters/etc/wmbusmeters.conf` and
`/home/me/.config/wmbusmeters/etc/wmbusmeters.d`

When running using config files then you can trigger a reload of the config files
using `sudo killall -HUP wmbusmetersd` or `killall -HUP wmbusmeters`
depending on if you are running as a daemon or not.

# Running without config files, good for experimentation and test.

```
wmbusmeters version: 1.3.0
Usage: wmbusmeters {options} <device> ( [meter_name] [meter_driver]{:<modes>} [meter_id] [meter_key] )*

As <options> you can use:

    --addconversions=<unit>+ add conversion to these units to json and meter env variables (GJ)
    --alarmexpectedactivity=mon-fri(08-17),sat-sun(09-12) Specify when the timeout is tested, default is mon-sun(00-23)
    --alarmshell=<cmdline> invokes cmdline when an alarm triggers
    --alarmtimeout=<time> Expect a telegram to arrive within <time> seconds, eg 60s, 60m, 24h during expected activity.
    --debug for a lot of information
    --device=<device> override device in config files. Use only in combination with --useconfig= option
    --donotprobe=<tty> do not auto-probe this tty. Use multiple times for several ttys or specify "all" for all ttys.
    --exitafter=<time> exit program after time, eg 20h, 10m 5s
    --format=<hr/json/fields> for human readable, json or semicolon separated fields
    --help list all options
    --ignoreduplicates=<bool> ignore duplicate telegrams, remember the last 10 telegrams
    --json_xxx=yyy always add "xxx"="yyy" to the json output and add shell env METER_xxx=yyy
    --license print GPLv3+ license
    --listento=<mode> listen to one of the c1,t1,s1,s1m,n1a-n1f link modes
    --listento=<mode>,<mode> listen to more than one link mode at the same time, assuming the dongle supports it
    --listenvs=<meter_driver> list the env variables available for the given meter driver
    --listfields=<meter_driver> list the fields selectable for the given meter driver
    --listmeters list all meter drivers
    --listmeters=<search> list all meter drivers containing the text <search>
    --logfile=<file> use this file for logging
    --logtelegrams log the contents of the telegrams for easy replay
    --logtimestamps=<when> add log timestamps: always never important
    --meterfiles=<dir> store meter readings in dir
    --meterfilesaction=(overwrite|append) overwrite or append to the meter readings file
    --meterfilesnaming=(name|id|name-id) the meter file is the meter's: name, id or name-id
    --meterfilestimestamp=(never|day|hour|minute|micros) the meter file is suffixed with a
                          timestamp (localtime) with the given resolution.
    --nodeviceexit if no wmbus devices are found, then exit immediately
    --oneshot wait for an update from each meter, then quit
    --resetafter=<time> reset the wmbus dongle regularly, default is 23h
    --addfields=timestamp_ut add the timestamp_ut field to be printed (--listfields=<meter> to list available fields)
    --selectfields=id,timestamp,total_m3 select only these fields to be printed (--listfields=<meter> to list available fields)
      addfields and selectfields are mutually exclusive.
    --separator=<c> change field separator to c
    --shell=<cmdline> invokes cmdline with env variables containing the latest reading
    --silent do not print informational messages nor warnings
    --trace for tons of information
    --useconfig=<dir> load config files from dir/etc
    --usestderr write notices/debug/verbose and other logging output to stderr (the default)
    --usestdoutforlogging write debug/verbose and logging output to stdout
    --verbose for more information
    --version print version
```

As device you can use:

`auto:c1`, to have wmbusmeters probe for devices: im871a, amb8465, cul, rc1180 or rtlsdr (spawns rtlwmbus).

`im871a:c1` to start all connected *im871a* devices in *c1* mode, ignore all other devices.

`/dev/ttyUSB1:amb8465:c1` to start only this device on this tty. Do not probe for other devices.

If you have two im871a you can supply both of them with their unique id:s and set different listening modes:
`im871a[12345678]:c1` `im871a[11223344]:t1`

You can also specify rtlwmbus and if you set the serial in the rtlsdr
dongle using `rtl_eeprom -s 1234` you can also refer to a specific
rtlsdr dongle like this `rtlwmbus[1234]`.

`/dev/ttyUSB0:amb8465`, if you have an amb8465 dongle assigned to ttyUSB0. Other suffixes are im871a,cul.

`/dev/ttyUSB0`, to have wmbusmeters auto-detect amb8465, im871a, rc1180 or cul device.

`/dev/ttyUSB0:38400`, to have wmbusmeters set the baud rate to 38400 and listen for raw wmbus telegrams.
These telegrams are expected to have the data link layer crc bytes removed already!

`rtlwmbus`, to spawn the background process: `rtl_sdr -f 868.625M -s 1600000 - 2>/dev/null | rtl_wmbus -s`
for each attached rtlsdr dongle. This will listen to S1,T1 and C1 meters in parallel.

Note that this uses a noticeable amount of CPU time by rtl_wmbus.
You can therefore use a tailored rtl_wmbus command that is more suitable for your needs.

`rtlwmbus:CMD(<command line>)`, to specify the entire background
process command line that is expected to produce rtlwbus compatible
output.
The command line cannot contain parentheses.
Likewise for rtl433.

Here is an example command line that reduces the rtl_wmbus CPU usage if you only need T1/C1 telegrams.
It disable S1 decoding (`-p s`) and trades lower cpu usage for reception performance (`-a`):

`rtlwmbus:CMD(rtl_sdr -f 868.95M -s 1600000 - 2>/dev/null | rtl_wmbus -p s -a)`

`rtlwmbus(ppm=17)`, to tune your rtlsdr dongle accordingly.
Use this to tune your dongle and at the same time listen to S1,T1 and C1.

`rtlwmbus:433M`, to tune to this fq instead.
This will listen to exactly to what is on this frequency.

`rtl433`, to spawn the background process: `rtl_433 -F csv -f 868.95M`

`rtl433(ppm=17)`, to tune your rtlsdr dongle accordingly.

`rtl433:433M`, to tune to this fq instead.

`stdin`, to read raw binary telegrams from stdin.
These telegrams are expected to have the data link layer crc bytes removed already!

`telegrams.bin`, to read raw wmbus telegrams from this file.
These telegrams are expected to have the data link layer crc bytes removed already!

`stdin:rtlwmbus`, to read telegrams formatted using the rtlwmbus format from stdin. Works for rtl433 as well.

`telegrams.msg:rtlwmbus`, to read rtlwmbus formatted telegrams from this file. Works for rtl433 as well.

`simulation_abc.txt`, to read telegrams from the file (the file must have a name beginning with simulation_....)
expecting the same format that is the output from `--logtelegrams`. This format also supports replay with timing.

As meter quadruples you specify:

* `<meter_name>`: a mnemonic for this particular meter (!Must not contain a colon ':' character!)
* `<meter_driver>`: use `auto` or one of the supported meters (can be suffixed with `:<mode>` to specify which mode you expect the meter to use when transmitting)
* `<meter_id>`: an 8 digit mbus id, usually printed on the meter
* `<meter_key>`: an encryption key unique for the meter
  if the meter uses no encryption, then supply `NOKEY`

```
Supported wmbus dongles:
IMST 871a (im871a)
Amber 8465 (amb8465)
CUL family (cul)
Radiocraft (RC1180)
rtl_wmbus (rtlwmbus)
rtl_433 (rtl433)

Supported water meters:
Aventies (aventieswm)
Apator at-wmbus-08   (apator08) (non-standard protocol)
Apator at-wmbus-16-2 (apator162) (non-standard protocol, spurious decoding errors)
Apator Ultrimis (ultrimis)
Aquametro/Integra Topas Es Kr (topaseskr)
Bmeters Hydrodigit (hydrodigit) (partly non-standard protocol)
Diehl/Sappel IZAR RC 868 I R4 PL (izar) (non-standard protocol)
Diehl HYDRUS (hydrus)
Diehl IZAR RC I G4 (dme_07)
Elster Merlin 868 (emerlin868)
Elster V200H (ev200)
Maddalena EVO 868 (evo868)
Honeywell Q400 (q400)
Kamstrup Multical 21 (multical21)
Kamstrup flowIQ 2200 (flowiq2200)
Kamstrup flowIQ 3100 (flowiq3100)
Qundis QWater5.5 (lse_07_17)
Sontex Supercom 587 (supercom587)
Sensus iPERL (iperl)
Techem MK Radio 3 and 4 (mkradio3,mkradio4) (non-standard protocols)
Waterstar M (waterstarm)

Currently not supported, please help!
Diehl/Sappel ACQUARIUS/IZAR R3 (izar3)

Supported heat cost allocators:
Innotas EurisII  (eurisii)
Qundis Q caloric (qcaloric)
Sontex 868 (sontex868)
Techem FHKV data II/III (fhkvdataiii)
Siemens WHE542 (whe5x)

Supported heat meters:
Heat meter Techem Compact V / Compact Ve (compact5) (non-standard protocol)
Heat meter Techem Vario 4 (vario451) (non-standard protocol)
Heat meter Kamstrup Multical 302 (multical302) (in C1 mode, please open issue for T1 mode)
Heat and Cooling meter Kamstrup Multical 403 (multical403) (in C1 mode)
Heat and Cooling meter Kamstrup Multical 603 (multical603) (in C1 mode)
Heat and Cooling meter Kamstrup Multical 803 (multical803) (in C1 mode)
Heat meter Apator Elf (elf)
Heat meter Diehl Sharky 775 (sharky)

Supported room sensors:
Bmeters RFM-AMB Thermometer/Hygrometer (rfmamb)
Elvaco CMa12w Thermometer (cma12w)
Lansen Thermometer/Hygrometer (lansenth)

Supported smoke detectors:
Lansen Smoke Detector (lansensm)
Ei Electronics Smoke Detector ei6500-oms (ei6500) (work in progress)

Supported door/window detectors:
Lansen Door/Window Detector (lansendw)

Supported pulse counter:
Lansen Pulse Counter (lansenpu)

Supported electricity meters:
Easy Meter ESYS-WM20 (esyswm)
eBZ wMB-E01 (ebzwmbe)
EMH Metering (ehzp)
Tauron Amiplus (amiplus) (includes vendor apator and echelon)
Gavazzi EM24 (em24)
Gransystems 301 and 303 (gransystems)
Kamstrup Omnipower (omnipower)
```

The wmbus dongle im871a can listen to either s1, c1 or t1.
However with the latest firmware version (0x15) im871a can
also listen to c1 and t1 telegrams at the same time.
(Use `--verbose` to see your dongles firmware version.)
If you have the older firmware you can download the upgrader here:
https://wireless-solutions.de/downloadfile/wireless-m-bus-software/

The amb8465 dongle can listen to either s1, c1 or t1. However it
can also listen to c1 and t1 at the same time.

With the latest rtlwmbus you can listen to s1, c1 and t1 at
the same time.

The cul dongle can listen to either s1, c1 or t1, but only
one at a time.

The rc1180 dongle can listen only to t1.

# Usage examples

```shell
wmbusmeters auto:c1
```

Listens for C1 telegrams using any of your available wmbus dongles:
```
Received telegram from: 12345678
          manufacturer: (KAM) Kamstrup Energi (0x2c2d)
           device type: Cold water meter (0x16)
            device ver: 0x1b
                device: im871a[12345678]
                  rssi: -77 dBm
                driver: multical21
```

Now listen to this specific meter, since the driver is auto-detected, we can use `auto` for the meter driver.

```shell
wmbusmeters auto:c1 MyTapWater auto 12345678 00112233445566778899AABBCCDDEEFF
```

(The Multical21 and other meters use compressed telegrams, which means
that you might have to wait up to 8 telegrams (8*16 seconds) until you
receive a full length telegram which gives all the information needed
to decode the compressed telegrams.)

Example output:

`MyTapWater   12345678     6.388 m3     6.377 m3    0.000 m3/h     8°C    23°C   DRY(dry 22-31 days)     2018-03-05 12:02.50`

(Here the multical21 itself, is configured to send target volume, therefore the max flow is 0.000 m3/h.)

Example format json output:

```shell
wmbusmeters --format=json /dev/ttyUSB0:im871a MyTapWater multical21:c1 12345678 00112233445566778899AABBCCDDEEFF MyHeater multical302 22222222 00112233445566778899AABBCCDDEEFF
```

```json
{"media":"cold water","meter":"multical21","name":"MyTapWater","id":"12345678","total_m3":6.388,"target_m3":6.377,"max_flow_m3h":0.000,"flow_temperature":8,"external_temperature":23,"current_status":"DRY","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"2018-02-08T09:07:22Z","device":"im871a[1234567]","rssi_dbm":-40}
```

```json
{"media":"heat","meter":"multical302","name":"MyHeater","id":"22222222","total_kwh":0.000,"total_volume_m3":0.000,"current_kw":"0.000","timestamp":"2018-02-08T09:07:22Z"}
```

Example format fields output and use tuned rtlsdr dongle with rtlwmbus.

```shell
wmbusmeters --format=fields 'rtlwmbus(ppm=72)' GreenhouseWater multical21:c1 33333333 NOKEY
```

```
GreenhouseTapWater;33333333;9999.099;77.712;0.000;11;31;;2018-03-05 12:10.24
```

You can select a subset of all available fields:

```shell
wmbusmeters --format=fields --selectfields=id,total_m3 /dev/ttyUSB0:im871a GreenhouseWater multical21:c1 33333333 NOKEY
```

```
33333333;9999.099
```

Instead of selecting a subset you can add fields, such as: timestamp_ut, timestamp_utc, timestamp_lt
and extra constant fields added using --field_xxx=yyy.

```shell
wmbusmeters --format=fields --field_city=Stockholm --addfields=timestamp_ut,city /dev/ttyUSB0:im871a GreenhouseWater multical21:c1 33333333 NOKEY
```

```
GreenhouseTapWater;33333333;9999.099;77.712;0.000;11;31;;2018-03-05 12:10.24;1627855621,Stockholm
```


You can list all available fields for a meter: `wmbusmeters --listfields=multical21`

You can list all meters: `wmbusmeters --listmeters`

You can search for meters: `wmbusmeters --listmeters=water` or `wmbusmteres --listmeters=q`

Eaxmple of using the shell command to publish to MQTT:

```shell
wmbusmeters --shell='HOME=/home/you mosquitto_pub -h localhost -t water -m "$METER_JSON"' /dev/ttyUSB0:im871a GreenhouseWater multical21:c1 33333333 NOKEY
```

Eaxmple of using the shell command to inject data into postgresql database:

```shell
wmbusmeters --shell="psql waterreadings -c \"insert into readings values ('\$METER_ID',\$METER_TOTAL_M3,'\$METER_TIMESTAMP') \" " /dev/ttyUSB0:amb8465 MyColdWater multical21:c1 12345678 NOKEY
```

(It is much easier to add shell commands in the conf file since you do not need to quote the quotes.)

You can have multiple shell commands and they will be executed in the order you gave them on the commandline.

To list the shell env variables available for a meter, run `wmbusmeters --listenvs=multical21` which outputs:

```
METER_JSON
METER_TYPE
METER_NAME
METER_ID
METER_TOTAL_M3
METER_TARGET_M3
METER_MAX_FLOW_M3H
METER_FLOW_TEMPERATURE_C
METER_EXTERNAL_TEMPERATURE_C
METER_CURRENT_STATUS
METER_TIME_DRY
METER_TIME_REVERSED
METER_TIME_LEAKING
METER_TIME_BURSTING
METER_TIMESTAMP
```

(If you have supplied `--json_floor=5` then you will also see `METER_floor` in the list)

Note that the `METER_TIMESTAMP` and the timestamp in the json output, is in UTC format, this is not your localtime.
However the hr and fields output will print your localtime.

You can add `shell=commandline` to a meter file stored in `wmbusmeters.d`, then this meter will use
this shell command instead of the command stored in `wmbusmeters.conf`.

You can use `--debug` to get both verbose output and the actual data bytes sent back and forth with the wmbus usb dongle.

If the meter does not use encryption of its meter data, then enter NOKEY on the command line.

```shell
wmbusmeters --format=json --meterfiles /dev/ttyUSB0:im871a:c1 MyTapWater multical21:c1 12345678 NOKEY
```

# Using wmbusmeters in a pipe

```shell
rtl_sdr -f 868.625M -s 1600000 - 2>/dev/null | rtl_wmbus -s | wmbusmeters --format=json stdin:rtlwmbus MyMeter auto 12345678 NOKEY | ...more processing...
```

# Additional tools

If you have a Kamstrup meters and you have received a KEM file and its
password from your supplier, then you can use `python2 utils/kem-import.py`
[utils/kem-import.py](utils/kem-import.py) to extract meter
information from that file (including the AES key) and to create
corresponding meter files in wmbusmetrs' config directory.

You can also use the XMLExtract Java program. `javac utils/XMLExtract`
and then `java -cp utils XMLExtract` to print the key on the command line.

You can run wmbusmeters with `--logtelegrams` to get log output that can
be placed in a simulation.txt file. You can then run wmbusmeter and
instead of an usb device, you provide the `simulation.txt` file as
argument. See test.sh for more info.

If you do not specify any meters on the command line, then wmbusmeters
will listen and print the header information of any telegram it hears.

# Builds and runs on GNU/Linux MacOSX (with recent XCode), and FreeBSD

(For MacOSX do `brew install librtlsdr libusb` which takes such a long
time that the MacOSX travis build is disabled for the moment.)

`./configure && make && make test`

Binary generated: `./build/wmbusmeters`

`make install` will install this binary.

`make HOST=arm` to cross compile from GNU/Linux to Raspberry PI.

Binary generated: `./build_arm/wmbusmeters`

`make DEBUG=true`

Binary generated: `./build_debug/wmbusmeters`

`make testd` to run all tests using the debug build.

Debug builds only work on FreeBSD if the compiler is LLVM. If your
system default compiler is gcc, set `CXX=clang++` to the build
environment to force LLVM to be used.

`make DEBUG=true HOST=arm`

Binary generated: `./build_arm_debug/wmbusmeters`

# System configuration

`make install` installs the files:

`/etc/wmbusmeters.conf`
`/usr/bin/wmbusmeters`
`/usr/sbin/wmbusmetersd`
`/etc/systemd/system/wmbusmeters.service`
`/etc/logrotate.d/wmbusmeters`

creates these directories:

`/etc/wmbusmeters.d`
`/var/log/wmbusmeters/meter_readings`

and adds the user `wmbusmeters` with no login account.

# Common problems

If the daemon has started then the wmbus device will be taken and you cannot start wmbusmeters manually.

To run manually, first make sure the daemon is stopped `sudo stop wmbusmeters@-dev-im871a_0.server`
if this hangs, then do `sudo killall -9 wmbusmetersd` and/or `sudo killall -9 wmbusmeters`.

If you are using rtl_sdr/rtl_wmbus and you want to stop the daemon, do
`sudo stop wmbusmeters@-dev-rtlsdr_3.server` followed by `sudo killall -9 rtl_sdr`.

## How to receive telegrams over longer distances

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

# Source code

The source code is modular and it should be relatively straightforward to add more receivers and meters.

Read for example the text file: HowToAddaNewMeter.txt

# Caveat

If you do not get proper readings from the meters with non-standard protocols. apator162, mkradio3, vario451
then you have to open an issue here and help out by logging a lot of messages and reverse engineer them
even more..... :-/

# Good free documents on the wireless mbus protocol standard EN 13757

https://oms-group.org/

There is also a lot of wmbus protocol implementation details that
are missing. They will be added to the program as we figure out
how the meters send their data.
