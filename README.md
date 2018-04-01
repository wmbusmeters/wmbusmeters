# wmbusmeters
The program receives and decodes C1 telegrams
(using the wireless mbus protocol) to acquire
utility meter readings.

| OS/Compiler        | Status           |
| ------------- |:-------------:|
|Linux G++| [![Build Status](https://travis-ci.org/weetmuts/wmbusmeters.svg?branch=master)](https://travis-ci.org/weetmuts/wmbusmeters) |

```
wmbusmeters version: 0.4
Usage: wmbusmeters {options} (auto | /dev/ttyUSBx)] { [meter_name] [meter_type] [meter_id] [meter_key] }*

Add more meter quadruplets to listen to more meters.
Add --verbose for detailed debug information.
    --robot or --robot=json for json output.
    --robot=fields for semicolon separated fields.
    --separator=X change field separator to X.
    --meterfiles=dir to create status files below dir,
          named dir/meter_name, containing the latest reading.
    --meterfiles defaults dir to /tmp.
    --oneshot wait for an update from each meter, then quit.
    --exitafter=20h program exits after running for twenty hours,
          or 10m for ten minutes or 5s for five seconds.

Specifying auto as the device will automatically look for usb
wmbus dongles on /dev/im871a and /dev/amb8465.

The meter type: multical21 (a water meter) is supported.
The meter types: multical302 (heat) and omnipower (electricity)
are work in progress.
```

Currently the meters are hardcoded for the European default setting
that specifies what extra data is sent in the telegrams. If someone
has a non-default meter that sends other extra data, then this will
show up as a warning when a long telegram is received (but not in the
short telegrams, where wrong values might be printed instead!). If
this happens to someone, then we need to implement a way to pass the
meter configuration as a parameter.

Actually, the mbus (and consequently the wmbus) protocol is a standard
that is self-describing.  Thus in reality it should not be necessary
to supply exactly which kind of meter we expect for a given id.  This
should be possible to figure out when we receive the first telegram.

Thus, strictly speaking, it should not be necessary to specify the
exact meter type. A more generic meter type might be just "water",
"heat" or electricity. But for the moment, the separation of meter
types will remain in the code. Thus even though the meter type right
now is named multical302, the other heat meters (multical-402 and
multical-602) might be compatible as well. The same is true for the
omnipower meter type, which might include the electricity meters
Kamstrup-162 Kamstrup-382, Kamstrup-351 etc).

No meter quadruplets means listen for telegram traffic and print any id heard.

# Usage examples

```
./build/wmbusmeters /dev/ttyUSB0 MyTapWater multical21 12345678 00112233445566778899AABBCCDDEEFF
```

wmbusmeters will detect which kind of dongle is connected to /dev/ttyUSB0. It can be either an IMST 871a dongle or an Amber Wireless AMB8465. If you have setup the udev rules below, then you can use auto instead of /dev/ttyUSB0.

Example output:

`MyTapWater      12345678         6.388 m3        6.377 m3       DRY(dry 22-31 days)     2018-03-05 12:02.50`

Example robot json output:

`./build/wmbusmeters --robot=json auto MyTapWater multical21 12345678 00112233445566778899AABBCCDDEEFF MyHeater multical302 22222222 00112233445566778899AABBCCDDEEFF`

`{media:"cold water",meter:"multical21","name":"MyTapWater","id":"12345678","total_m3":6.388,"target_m3":6.377,"current_status":"DRY","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"2018-02-08T09:07:22Z"}`

`{media:"heat",meter:"multical302","name":"MyHeater","id":"22222222","total_kwh":0.000,"total_volume_m3":0.000,"current_kw":"0.000","timestamp":"2018-02-08T09:07:22Z"}`

Example robot fields output:

`./build/wmbusmeters --robot=fields auto GreenhouseWater multical21 33333333 ""`

`GreenhouseTapWater;33333333;9999.099;77.712;;2018-03-05 12:10.24`

You can use `--debug` to get both verbose output and the actual data bytes sent back and forth with the wmbus usb dongle.

If the meter does not use encryption of its meter data, then enter an empty key on the command line.
(you must enter "")

`./build/wmbusmeters --robot --meterfiles auto MyTapWater multical21 12345678 ""`

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

# Limitations

Two usb wmbus receivers are supported: IMST im871A and Amber Wireless AMB8465.

Two supported meters: Multical21 (water meter) and Multical302 (heat meter?, work in progress).

The source code is modular and it should be relatively straightforward to add more receivers and meters.

# Good documents on the wireless mbus protocol:

http://www.m-bus.com/files/w4b21021.pdf

https://www.infineon.com/dgdl/TDA5340_AN_WMBus_v1.0.pdf

http://fastforward.ag/downloads/docu/FAST_EnergyCam-Protocol-wirelessMBUS.pdf

http://www.multical.hu/WiredMBus-water.pdf

http://uu.diva-portal.org/smash/get/diva2:847898/FULLTEXT02.pdf

http://projekter.aau.dk/projekter/da/studentthesis/wireless-mbus-based-extremely-low-power-protocol-for-wireless-communication-with-water-meters(6e1139d5-6f24-4b8a-a727-9bc108012bcc).html

The AES source code is copied from:

https://github.com/kokke/tiny-AES128-C

The following other github projects were of great help:

https://github.com/ffcrg/ecpiww

https://github.com/tobiasrask/wmbus-client

https://github.com/CBrunsch/scambus/

TODO: CRC checks are still missing. If the wrong AES key
is supplied you probably get zero readings and
sometimes warnings about wrong type of frames.

There is also a lot of wmbus protocol implementation details that
probably are missing. They will be added to the program
as we figure out how the meters send their data.
