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
    --robot=fields for semicolon separated fields
    --separator=X change field separator to X
    --meterfiles to create status files below tmp,
          named /tmp/meter_name, containing the latest reading.
    --oneshot wait for an update from each meter, then quit.

Specifying auto as the device will automatically look for usb
wmbus dongles on /dev/im871a and /dev/amb8465.

Two meter types are supported: multical21 and multical302 (multical302 is still work in progress).
```

Currently the meters are hardcoded for the European default setting that specifies what extra data
is sent in the telegrams. If someone has a non-default meter that sends other extra data, then this
will show up as a warning when a long telegram is received (but not in the short telegrams!).
If this should happen, then we need to implement a way to pass the meter configuration as a parameter.

No meter quadruplets means listen for telegram traffic and print any id heard.

# Builds and runs on GNU/Linux:

```
make
./build/wmbusmeters /dev/ttyUSB0 MyTapWater multical21 12345678 00112233445566778899AABBCCDDEEFF
```

wmbusmeters will detect which kind of dongle is connected to /dev/ttyUSB0. It can be either an IMST 871a dongle or an Amber Wireless AMB8465. If you have setup the udev rules below, then you can use auto instead of /dev/ttyUSB0.

Example output:
`MyTapWater     12345678         6.375 m3       2017-08-31 09:09.08      3.040 m3      DRY(dry 22-31 days)`

`./build/wmbusmeters --verbose /dev/ttyUSB0 MyTapWater multical21 12345678 00112233445566778899AABBCCDDEEFF`

`./build/wmbusmeters --robot auto MyElectricity multical302 12345678 00112233445566778899AABBCCDDEEFF MyTapWater multical21 12345678 00112233445566778899AABBCCDDEEFF`

Robot output:
`{"name":"MyTapWater","id":"12345678","total_m3":6.375,"target_m3":3.040,"current_status":"","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"2017-08-31T09:07:18Z"}`

You can use `--debug` to get both verbose output and the actual data bytes sent back and forth with the wmbus usb dongle.

`make HOST=arm`

Binary generated: `./build_arm/wmbusmeters`

`make DEBUG=true`

Binary generated: `./build_debug/wmbusmeters`

`make DEBUG=true HOST=arm`

Binary generated: `./build_arm_debug/wmbusmeters`

If the meter does not use encryption of its meter data, then enter an empty key on the command line.
(you must enter "")

`./build/wmbusmeters --robot --meterfiles /dev/ttyUSB0 MyTapWater multical21 12345678 ""`

You can run wmbusmeters with --logtelegrams to get log output that can be placed in a simulation.txt
file. You can then run wmbusmeter and instead of auto (or an usb device) provide the simulationt.xt
file as argument. See test.sh for more info.

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

Two supported meters: Multical21 (water meter) and Multical302 (power meter, work in progress).

The source code is modular and it should be relatively straightforward to add more receivers and meters.

# Good documents on the wireless mbus protocol:

http://www.m-bus.com/files/w4b21021.pdf

https://www.infineon.com/dgdl/TDA5340_AN_WMBus_v1.0.pdf

http://fastforward.ag/downloads/docu/FAST_EnergyCam-Protocol-wirelessMBUS.pdf

http://www.multical.hu/WiredMBus-water.pdf

http://uu.diva-portal.org/smash/get/diva2:847898/FULLTEXT02.pdf

The AES source code is copied from:

https://github.com/kokke/tiny-AES128-C

The following other github projects were of great help:

https://github.com/ffcrg/ecpiww

https://github.com/tobiasrask/wmbus-client

https://github.com/CBrunsch/scambus/

TODO: CRC checks are still missing. If the wrong AES key
is supplied you probably get zero readings and
sometimes warnings about wrong type of frames.
