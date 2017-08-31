# wmbusmeters
The program receives and decodes C1 telegrams
(using the wireless mbus protocol) to acquire
utility meter readings. 

wmbusmeters [usbdevice] { [meter_name] [meter_id] [meter_key] }*

If you want to listen to more than one meter, simply add more meter triplets.
Add --robot to get JSON output.

Builds and runs on GNU/Linux:

make

./build/wmbusmeters /dev/ttyUSB0 MyTapWater 12345678 00112233445566778899AABBCCDDEEFF

./build/wmbusmeters --verbose /dev/ttyUSB0 MyTapWater 12345678 00112233445566778899AABBCCDDEEFF

./build/wmbusmeters --robot /dev/ttyUSB0 MyTapWater 12345678 00112233445566778899AABBCCDDEEFF

make HOST=arm

Binary generated: ./build_arm/wmbusmeters

make DEBUG=true

Binary generated: ./build_debug/wmbusmeters

make DEBUG=true HOST=arm

Binary generated: ./build_arm_debug/wmbusmeters

Add yourself to the dialout group to get access to the newly plugged in im87A USB stick.

Currently only supports the USB stick receiver im871A
and the water meter Multical21. The source code is modular
and it should be relatively straightforward to add
more receivers (Amber anyone?) and meters.

Good documents on the wireless mbus protocol:

https://www.infineon.com/dgdl/TDA5340_AN_WMBus_v1.0.pdf

http://fastforward.ag/downloads/docu/FAST_EnergyCam-Protocol-wirelessMBUS.pdf

http://www.multical.hu/WiredMBus-water.pdf

The AES source code is copied from:

https://github.com/kokke/tiny-AES128-C

The following two other github projects were of great help:

https://github.com/ffcrg/ecpiww

https://github.com/tobiasrask/wmbus-client

TODO: CRC checks are still missing. If the wrong AES key
is supplied you probably get zero readings and
sometimes warnings about wrong type of frames.
