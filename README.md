
# wmbusmeters
The program receives and decodes C1,T1 or S1 telegrams
(using the wireless mbus protocol) to acquire
utility meter readings. The readings can then be published using
MQTT, curled to a REST api, inserted into a database or stored in a log file.

[FAQ/WIKI/MANUAL pages](https://weetmuts.github.io/wmbusmeterswiki/)

The program runs on GNU/Linux, MacOSX and Raspberry Pi.

| OS           | Status           |
| ------------ |:-------------:|
|GNU/Linux & MacOSX| [![Build Status](https://travis-ci.org/weetmuts/wmbusmeters.svg?branch=master)](https://travis-ci.org/weetmuts/wmbusmeters) |

| Static Scan        | Status           |
| ------------- |:-------------:|
|Linux G++| [![Build Status](https://scan.coverity.com/projects/14774/badge.svg)](https://scan.coverity.com/projects/weetmuts-wmbusmeters) |


# Run as a daemon

Remove the wmbus dongle (im871a,amb8465,rfmrx2,cul,d1tc) or the generic rtlsdr dongle (RTL2838) from your computer.

`make; sudo make install` will install wmbusmeters as a daemon that starts
automatically when an appropriate wmbus usb dongle is inserted in the computer.
(Note! make install only works for GNU/Linux. For MacOSX try to start
`wmbusmetersd /tmp/thepidfile` from a script instead. Here you can also override the device:
`wmbusmetersd --device=/dev/ttyXXY --listento=t1 /tmp/thepidfile`)

Check the config file /etc/wmbusmeters.conf:
```
loglevel=normal
device=auto
logtelegrams=false
format=json
meterfiles=/var/log/wmbusmeters/meter_readings
meterfilesaction=overwrite
meterfilesnaming=name
meterfilestimestamp=day
logfile=/var/log/wmbusmeters/wmbusmeters.log
shell=/usr/bin/mosquitto_pub -h localhost -t wmbusmeters/$METER_ID -m "$METER_JSON"
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
    --logfile=<file> use this file instead of stdout
    --logtelegrams log the contents of the telegrams for easy replay
    --meterfiles=<dir> store meter readings in dir
    --meterfilesaction=(overwrite|append) overwrite or append to the meter readings file
    --meterfilesnaming=(name|id|name-id) the meter file is the meter's: name, id or name-id
    --meterfilestimestamp=(never|day|hour|minute|micros) the meter file is suffixed with a
                          timestamp (localtime) with the given resolution.
    --oneshot wait for an update from each meter, then quit
    --reopenafter=<time> close/reopen dongle connection repeatedly every <time> seconds, eg 60s, 60m, 24h
    --separator=<c> change field separator to c
    --shell=<cmdline> invokes cmdline with env variables containing the latest reading
    --shellenvs list the env variables available for the meter
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

rtlwmbus, to spawn the background process: "rtl_sdr -f 868.95M -s 1600000 - | rtl_wmbus"

rtlwmbus:868.9M, to tune to this fq instead.

rtlwmbus:<commandline>, to specify the entire background process command line.

stdin, to read raw binary telegrams from stdin.

telegrams.txt, to read raw wmbus telegrams from this file.

stdin:rtlwmbus, to read telegrams formatted using the rtlwmbus format from stdin.

telegrams.msg:rtlwmbus, to read rtlwmbus formatted telegrams from this file.

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
IMST 871a
Amber 8465
BMeters RFM-RX2
rtl_sdr|rtl_wmbus
CUL family

Supported water meters:
Kamstrup Multical 21 (multical21)
Kamstrup flowIQ 3100 (flowiq3100)
Sontex Supercom 587 (supercom587)
Sensus iPERL (iperl)
Apator at-wmbus-16-2 (apator162) (non-standard protocol)
Techem MK Radio 3 (mkradio3) (non-standard protocol)
Diehl/Sappel IZAR RC 868 I R4 PL (izar) (non-standard protocol)
Diehl HYDRUS (hydrus)
Bmeters Hydrodigit (hydrodigit) (partly non-standard protocol)
Honeywell Q400 (q400)

Supported heat cost allocators:
Qundis Q caloric (qcaloric)
Innotas EurisII  (eurisii)
Techem FHKV data II/III (fhkvdataiii)

Supported heat meter:
Heat meter Techem Vario 4 (vario451) (non-standard protocol)
Heat meter Kamstrup Multical 302 (multical302) (in C1 mode, please open issue for T1 mode)

Supported room sensors:
Lansen Thermometer/Hygrometer (lansenth)
Bmeters RFM-AMB Thermometer/Hygrometer (rfmamb)
Elvaco CMa12w Thermometer (cma12w)

Supported electricity meters:
Tauron Amiplus (amiplus) (includes vendor apator and echelon)
EMH Metering (ehzp)
Easy Meter ESYS-WM20 (esyswm)
eBZ wMB-E01 (ebzwmbe)
```

The wmbus dongles imst871a can listen to one type of wmbus telegrams
at a time, ie either C1 or T1 telegrams. Thus you can listen to
multiple meters as long as they all require the same radio mode C1 or
T1.

However if you use amb8465 or rtlwmbus, then you can listen to both C1
and T1 telegrams at the same time.

# Usage examples

```
wmbusmeters --listento=t1 /dev/ttyUSB1:amb8465
```

Simply runs a scan with mode T1 to search for meters and print the IDs

```
wmbusmeters /dev/ttyUSB0:im871a MyTapWater multical21:c1 12345678 00112233445566778899AABBCCDDEEFF
```

If you have performed `make install` or added the udev rules yourself, then you can use
auto instead of the exact usb device.

(The :c1 can be left out, since multical21 only transmits c1 telegrams. The suffix
with the expected link mode might be necessary for other meters, like apator162 for example.
The Multical21 uses compressed telegrams, which means that you might have to wait up to 8 telegrams
(8*16 seconds) until you receive a full length telegram which gives all the information needed
to decode the compressed telegrams.)

Example output:

`MyTapWater   12345678     6.388 m3     6.377 m3    0.000 m3/h     8°C    23°C   DRY(dry 22-31 days)     2018-03-05 12:02.50`

(Here the multical21 itself, is configured to send target volume, therefore the max flow is 0.000 m3/h.)

Example format json output:

`wmbusmeters --format=json auto MyTapWater multical21 12345678 00112233445566778899AABBCCDDEEFF MyHeater multical302 22222222 00112233445566778899AABBCCDDEEFF`

`{"media":"cold water","meter":"multical21","name":"MyTapWater","id":"12345678","total_m3":6.388,"target_m3":6.377,"max_flow_m3h":0.000,"flow_temperature":8,"external_temperature":23,"current_status":"DRY","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"2018-02-08T09:07:22Z"}`

`{"media":"heat","meter":"multical302","name":"MyHeater","id":"22222222","total_kwh":0.000,"total_volume_m3":0.000,"current_kw":"0.000","timestamp":"2018-02-08T09:07:22Z"}`

Example format fields output and use rtlsdr dongle with rtlwmbus tuned to 868.9MHz instead of the
default 868.95MHz.

`wmbusmeters --format=fields rtlwmbus:868.9M GreenhouseWater multical21 33333333 NOKEY`

`GreenhouseTapWater;33333333;9999.099;77.712;0.000;11;31;;2018-03-05 12:10.24`

Eaxmple of using the shell command to publish to MQTT:

`wmbusmeters --shell='HOME=/home/you mosquitto_pub -h localhost -t water -m "$METER_JSON"' auto GreenhouseWater multical21 33333333 NOKEY`

Eaxmple of using the shell command to inject data into postgresql database:

`wmbusmeters --shell="psql waterreadings -c \"insert into readings values ('\$METER_ID',\$METER_TOTAL_M3,'\$METER_TIMESTAMP') \" " auto MyColdWater multical21 12345678 NOKEY`

You can have multiple shell commands and they will be executed in the order you gave them on the commandline.
Note that to single quotes around the command is necessary to pass the env variable names into wmbusmeters.
To list the shell env variables available for your meter, add --shellenvs to the commandline:
`wmbusmeters --shellenvs auto Water iperl 12345678 NOKEY`
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

`wmbusmeters --format=json --meterfiles auto MyTapWater multical21 12345678 NOKEY`

If you have a Kamstrup meters and you have received a KEM file and its password from your supplier, then you can use [utils/kem-import.py](utils/kem-import.py) utility to extract meter information from that file (including the AES key) and to create corresponding meter files in wmbusmetrs' config directory.

You can run wmbusmeters with --logtelegrams to get log output that can be placed in a simulation.txt
file. You can then run wmbusmeter and instead of auto (or an usb device) provide the simulationt.xt
file as argument. See test.sh for more info.

If you do not specify any meters on the command line, then wmbusmeters
will listen and print the header information of any telegram it hears.
You must specify the listening mode.

With an rtlwmbus or amb8465 dongle: `wmbusmeters --listento=c1,t1 auto`

With an imst871a dongle: `wmbusmeters --listento=c1 auto`

# Builds and runs on GNU/Linux and MacOSX (with recent XCode)

`make && make test`

Binary generated: `./build/wmbusmeters`

`make HOST=arm` to cross compile from GNU/Linux to Raspberry PI.

Binary generated: `./build_arm/wmbusmeters`

`make DEBUG=true`

Binary generated: `./build_debug/wmbusmeters`

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

If you do not want the daemon to start automatically, simply edit
/dev/udev/rules.d/99-wmbus-usb-serial.rules and remove
`,TAG+="systemd",ENV{SYSTEMD_WANTS}="wmbusmeters.@/dev/im871a_%n.service"` from each
line.

You can start/stop the daemon with `sudo systemctl stop wmbusmeters@-dev-im871a_0.service`
or `sudo systemctl stop wmbusmeters@-dev-amb8465_1.service` etc. Alas
rtl_sdr does not play nice right now with wmbusmeters daemon. If you first do `sudo systemctl stop wmbusmeters@-dev-rtlsdr_3.service`
it will hang, in a separate window do `sudo killall -9 rtl_sdr` to get rid of the spinning rtl_sdr process.

You can trigger a reload of the config files with `sudo killall -HUP wmbusmetersd`

If you add more dongles, then more daemons gets started, each with a unique name/nr.

# Common problems

If the daemon has started then the wmbus device will be taken and you cannot start wmbusmeters manually.

To run manually, first make sure the daemon is stopped `sudo stop wmbusmeters@-dev-im871a_0.server`
if this hangs, then do `sudo killall -9 wmbusmetersd` and/or `sudo killall -9 wmbusmeters`.

If you are using rtl_sdr/rtl_wmbus and you want to stop the daemon, do
`sudo stop wmbusmeters@-dev-rtlsdr_3.server` followed by `sudo killall -9 rtl_sdr`.

If you are using auto, then start manually with --debug to see how wmbusmeters goes looking for devices.

# Source code

The source code is modular and it should be relatively straightforward to add more receivers and meters.

Read for example the text file: HowToAddaNewMeter.txt

# Caveat

If you do not get proper readings from the meters with non-standard protocols. apator162, mkradio3, vario451
then you have to open an issue here and help out by logging a lot of messages and reverse engineer them
even more..... :-/

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

There is also a lot of wmbus protocol implementation details that
probably are missing. They will be added to the program
as we figure out how the meters send their data.
