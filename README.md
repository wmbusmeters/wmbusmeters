# wmbusmeters
The program receives and decodes C1 telegrams
(using the wireless mbus protocol) to acquire
utility meter readings. You configure multiple
meters for reading by supplying triplets
on the command line.

wmbusmeters [usbdevice] [meter_name] [meter_id] [meter_ke]

If you want to listen to more than one meter, simply add more triples.

Builds and runs on GNU/Linux:

make

./build/wmbusmeters /dev/ttyUSB0 MyTapWater 12345678 00112233445566778899AABBCCDDEEFF

./build/wmbusmeters --verbose /dev/ttyUSB0 MyTapWater 12345678 00112233445566778899AABBCCDDEEFF

make HOST=arm

./build_arm/wmbusmeters /dev/ttyUSB0 MyTapWater 12345678 00112233445566778899AABBCCDDEEFF

make DEBUG=true

./build_debug/wmbusmeters /dev/ttyUSB0 MyTapWater 12345678 00112233445566778899AABBCCDDEEFF

make DEBUG=true HOST=arm

./build_arm_debug/wmbusmeters /dev/ttyUSB0 MyTapWater 12345678 00112233445566778899AABBCCDDEEFF

(After you insert the im871A USB stick, do:

chown me:me /dev/ttyUSB0

to avoid having to run the program as root.)

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


Code can print total water consumption! But everything else is
missing. CRC checks anyone? :-) Don't rely on these measurements
for anything really important!
