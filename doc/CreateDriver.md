# Creating a new driver

To create a new driver, it is very convenient, to start with an existing driver,
that mostly matches the one you want to integrate. I will use the EurisII as a base for
Integration of Aventies HCA.

In fact, most of the manufacturers already exist in the source code
with their respective shortcodes, e.g. Aventies with "AAA".

Create the `/etc/wmbusmeters.conf` with content adapted to your installation, e.g.:
```
loglevel=debug
device=/dev/ttyUSB0:t1
logtelegrams=true
format=json
meterfiles=/var/lib/wmbusmeters/meter_readings
meterfilesaction=overwrite
meterfilesnaming=name-id
logfile=/var/log/wmbusmeters/wmbusmeters.log
```

Create a meter config in `/etc/wmbusmeters.d/` with content like this, using the existing type, you selected as a base for your class (adjust also name, id and key to your needs):

```
name=ABOGGLKZ
driver=eurisii
id=60900126
key=xxxxxxxxx
```

If you now start wmbusmeters with `wmbusmeters --debug --verbose --useconfig=/etc` and your meter transmits a message,
you will see a log like this:

```
[2021-07-05_21:07:30] (meter) ABOGGLKZ: meter detection did not match the selected driver eurisii! correct driver is: unknown!
(meter) Not printing this warning again for id: 60900126 mfct: (AAA) Aventies, Germany (0x421) type: Heat Cost Allocator (0x08) ver: 0x55
[2021-07-05_21:07:30] (meter) please consider opening an issue at https://github.com/wmbusmeters/wmbusmeters/
[2021-07-05_21:07:30] (meter) to add support for this unknown mfct,media,version combination
(meter) ABOGGLKZ: yes for me
(meter) ABOGGLKZ eurisii handling telegram from 60900126
(meter) ABOGGLKZ 60900126 "7644210426019060550872260190602104550806006005CA4269D455F02AE4A475AD546F7FF1EDF3C959E5480AA0A2341B6B6EA28884FA1E0EC355A23E66D055E3C790298553C3870727149DF88612ABF2EA184AEF0821B16BC11DA5BAABEFB34E4E68C6F2728D935011EAB98FCAA29274CC685B8079F7"
(wmbus) parseDLL @0 119
(telegram) DLL L=76 C=44 (from meter SND_NR) M=0421 (AAA) A=60900126 VER=55 TYPE=08 (Heat Cost Allocator) (driver unknown!) DEV=im871a[00101387] RSSI=-77
(wmbus) parseELL @10 109
(wmbus) parseAFL @10 109
(wmbus) parseTPL @10 109
(TPL) decrypting "CA4269D455F02AE4A475AD546F7FF1EDF3C959E5480AA0A2341B6B6EA28884FA1E0EC355A23E66D055E3C790298553C3870727149DF88612ABF2EA184AEF0821B16BC11DA5BAABEFB34E4E68C6F2728D935011EAB98FCAA29274CC685B8079F7"
(TPL) num encrypted blocks 6 (96 bytes and remaining unencrypted 0 bytes)
(TPL) IV 21042601906055080606060606060606
(TPL) decrypted  "2F2F0B6E660100426EA60082016EA600C2016E9E0082026E7E00C2026E5B0082036E4200C2036E770182046E5B01C2046E4C0182056E4701C2056E3E0182066E3B01C2066E3B0182076E3B01C2076E3B0182086E1301C2086E9C0002FD170000"
(telegram) TPL CI=72 ACC=06 STS=00 CFG=0560 (AES_CBC_IV nb=6 cntn=0 ra=0 hc=0) ID=26019060 MFT=2104 VER=55 TYPE=08 (Heat Cost Allocator)
telegram=|76442104260190605508722601906021045508060060052F2F|0B6E660100426EA60082016EA600C2016E9E0082026E7E00C2026E5B0082036E4200C2036E770182046E5B01C2046E4C0182056E4701C2056E3E0182066E3B01C2066E3B0182076E3B01C2076E3B0182086E1301C2086E9C0002FD170000|+6647
(eurisii) 00: 76 length (118 bytes)
(eurisii) 01: 44 dll-c (from meter SND_NR)
(eurisii) 02: 2104 dll-mfct (AAA)
(eurisii) 04: 26019060 dll-id (60900126)
(eurisii) 08: 55 dll-version
(eurisii) 09: 08 dll-type (Heat Cost Allocator)
(eurisii) 0a: 72 tpl-ci-field (EN 13757-3 Application Layer (long tplh))
(eurisii) 0b: 26019060 tpl-id (60900126)
(eurisii) 0f: 2104 tpl-mfct (AAA)
(eurisii) 11: 55 tpl-version
(eurisii) 12: 08 tpl-type (Heat Cost Allocator)
(eurisii) 13: 06 tpl-acc-field
(eurisii) 14: 00 tpl-sts-field (OK)
(eurisii) 15: 6005 tpl-cfg 0560 (AES_CBC_IV nb=6 cntn=0 ra=0 hc=0 )
(eurisii) 17: 2f2f decrypt check bytes
(eurisii) 19: 0B dif (6 digit BCD Instantaneous value)
(eurisii) 1a: 6E vif (Units for H.C.A.)
(eurisii) 1b: * 660100 current consumption (166.000000 hca)
(eurisii) 1e: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(eurisii) 1f: 6E vif (Units for H.C.A.)
(eurisii) 20: * A600 consumption at set date 1 (166.000000 hca)
(eurisii) 22: 82 dif (16 Bit Integer/Binary Instantaneous value)
(eurisii) 23: 01 dife (subunit=0 tariff=0 storagenr=2)
(eurisii) 24: 6E vif (Units for H.C.A.)
(eurisii) 25: * A600 consumption at set date 2 (166.000000 hca)
(eurisii) 27: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(eurisii) 28: 01 dife (subunit=0 tariff=0 storagenr=3)
(eurisii) 29: 6E vif (Units for H.C.A.)
(eurisii) 2a: * 9E00 consumption at set date 3 (158.000000 hca)
(eurisii) 2c: 82 dif (16 Bit Integer/Binary Instantaneous value)
(eurisii) 2d: 02 dife (subunit=0 tariff=0 storagenr=4)
(eurisii) 2e: 6E vif (Units for H.C.A.)
(eurisii) 2f: * 7E00 consumption at set date 4 (126.000000 hca)
(eurisii) 31: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(eurisii) 32: 02 dife (subunit=0 tariff=0 storagenr=5)
(eurisii) 33: 6E vif (Units for H.C.A.)
(eurisii) 34: * 5B00 consumption at set date 5 (91.000000 hca)
(eurisii) 36: 82 dif (16 Bit Integer/Binary Instantaneous value)
(eurisii) 37: 03 dife (subunit=0 tariff=0 storagenr=6)
(eurisii) 38: 6E vif (Units for H.C.A.)
(eurisii) 39: * 4200 consumption at set date 6 (66.000000 hca)
(eurisii) 3b: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(eurisii) 3c: 03 dife (subunit=0 tariff=0 storagenr=7)
(eurisii) 3d: 6E vif (Units for H.C.A.)
(eurisii) 3e: * 7701 consumption at set date 7 (375.000000 hca)
(eurisii) 40: 82 dif (16 Bit Integer/Binary Instantaneous value)
(eurisii) 41: 04 dife (subunit=0 tariff=0 storagenr=8)
(eurisii) 42: 6E vif (Units for H.C.A.)
(eurisii) 43: * 5B01 consumption at set date 8 (347.000000 hca)
(eurisii) 45: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(eurisii) 46: 04 dife (subunit=0 tariff=0 storagenr=9)
(eurisii) 47: 6E vif (Units for H.C.A.)
(eurisii) 48: * 4C01 consumption at set date 9 (332.000000 hca)
(eurisii) 4a: 82 dif (16 Bit Integer/Binary Instantaneous value)
(eurisii) 4b: 05 dife (subunit=0 tariff=0 storagenr=10)
(eurisii) 4c: 6E vif (Units for H.C.A.)
(eurisii) 4d: * 4701 consumption at set date 10 (327.000000 hca)
(eurisii) 4f: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(eurisii) 50: 05 dife (subunit=0 tariff=0 storagenr=11)
(eurisii) 51: 6E vif (Units for H.C.A.)
(eurisii) 52: * 3E01 consumption at set date 11 (318.000000 hca)
(eurisii) 54: 82 dif (16 Bit Integer/Binary Instantaneous value)
(eurisii) 55: 06 dife (subunit=0 tariff=0 storagenr=12)
(eurisii) 56: 6E vif (Units for H.C.A.)
(eurisii) 57: * 3B01 consumption at set date 12 (315.000000 hca)
(eurisii) 59: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(eurisii) 5a: 06 dife (subunit=0 tariff=0 storagenr=13)
(eurisii) 5b: 6E vif (Units for H.C.A.)
(eurisii) 5c: * 3B01 consumption at set date 13 (315.000000 hca)
(eurisii) 5e: 82 dif (16 Bit Integer/Binary Instantaneous value)
(eurisii) 5f: 07 dife (subunit=0 tariff=0 storagenr=14)
(eurisii) 60: 6E vif (Units for H.C.A.)
(eurisii) 61: * 3B01 consumption at set date 14 (315.000000 hca)
(eurisii) 63: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(eurisii) 64: 07 dife (subunit=0 tariff=0 storagenr=15)
(eurisii) 65: 6E vif (Units for H.C.A.)
(eurisii) 66: * 3B01 consumption at set date 15 (315.000000 hca)
(eurisii) 68: 82 dif (16 Bit Integer/Binary Instantaneous value)
(eurisii) 69: 08 dife (subunit=0 tariff=0 storagenr=16)
(eurisii) 6a: 6E vif (Units for H.C.A.)
(eurisii) 6b: * 1301 consumption at set date 16 (275.000000 hca)
(eurisii) 6d: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(eurisii) 6e: 08 dife (subunit=0 tariff=0 storagenr=17)
(eurisii) 6f: 6E vif (Units for H.C.A.)
(eurisii) 70: * 9C00 consumption at set date 17 (156.000000 hca)
(eurisii) 72: 02 dif (16 Bit Integer/Binary Instantaneous value)
(eurisii) 73: FD vif (Second extension FD of VIF-codes)
(eurisii) 74: 17 vife (Error flags (binary))
(eurisii) 75: * 0000 error flags (0000)
```

Now keep especially the following lines in mind for integation into the meter definitions:
```
[2021-07-05_21:07:30] (meter) ABOGGLKZ: meter detection did not match the selected driver eurisii! correct driver is: unknown!
(meter) Not printing this warning again for id: 60900126 mfct: (AAA) Aventies, Germany (0x421) type: Heat Cost Allocator (0x08) ver: 0x55
```

and the telegram line with the decrypted content.
```
telegram=|76442104260190605508722601906021045508060060052F2F#0B6E660100426EA60082016EA600C2016E9E0082026E7E00C2026E5B0082036E4200C2036E770182046E5B01C2046E4C0182056E4701C2056E3E0182066E3B01C2066E3B0182076E3B01C2076E3B0182086E1301C2086E9C0002FD170000|+6647
```

You can now analyze the telegram with this command:
```
wmbusmeters --analyze 76442104260190605508722601906021045508060060052F2F0B6E660100426EA60082016EA600C2016E9E0082026E7E00C2026E5B0082036E4200C2036E770182046E5B01C2046E4C0182056E4701C2056E3E0182066E3B01C2066E3B0182076E3B01C2076E3B0182086E1301C2086E9C0002FD170000
```

This will test all existing drivers to see which drivers seems to be matching the best. (This method can be improved so do some sanity checks as well.) If the driver already decodes 100% of the content then perhaps you should only add
```
di.addDetection(MANUFACTURER_AAA, 0x08,  0x55);
```
to the existing driver_eursii.cc file. But in this case we chose to create a new driver.
(Remember to add a new test case at the end of the cc file.)

## Creating your new driver

As we used the eurisii meter as a template, you need to copy the driver_eurisii.cc to driver_xyz.cc replacing xyz
with a name created by you. In my case it is driver_aventieshca.cc

As long as you do not need any new (not-yet-implemented) wmbus protocol features, then the
entire driver is self contained within a single cc file, including tests.

Now replace all occurences of class name MeterEurisII by the class name you created (e.g. MeterAventiesHCA in my case).

Adjust the test cases at the end of the cc file! These test cases are picked up automatically
by the testing system. Such regression tests are very important for the future stability of wmbusmeters.

Media/type 0x08 is the code for a Heat Cost allocator. You can find other codes here [here](https://m-bus.com/assets/downloads/MBDOC48.PDF) (on page 76). At least you should check for plausibility...

### Further adjustments

If you need to do processing that is not support by the current addFieldWithExtractor code, then
you will have to implement the processContent function.

To collect the releavant data correctly, you need to adjust the code
for processing and printing the data from the meter. For this you
should edit the `MeterXXX::processContent` method to fit your
needs. Adhere to the debug log output of the processing function and
have a look at other meter's classes for examples how to process
multiple values with different sorage numbers or types.

If you get errors, it is quite probable that you missed the correct value information (e.g. ValueInformation::Volume):
```
    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }
```

Possible ValueInformation types are:

    - Volume (0x10,0x17)
    - OperatingTime (0x24,0x27)
    - VolumeFlow (0x38,0x3F)
    - FlowTemperature (0x58,0x5B)
    - ReturnTemperature (0x5C,0x5F)
    - TemperatureDifference (0x60,0x63)
    - ExternalTemperature (0x64,0x67)
    - HeatCostAllocation (0x6E,0x6E)
    - Date (0x6C,0x6C)
    - DateTime (0x6D,0x6D)
    - EnergyMJ (0x0E,0x0F)
    - EnergyWh (0x00,0x07)
    - PowerW (0x28,0x2f)
    - ActualityDuration (0x74,0x77)

You can add also special properties and getters for special data of your meter, like it is handled with the error codes for EurisII.

## Compiling and testing

If everything was right, you can try compiling the project:

```
./configure
make -j4
```

Test your new version with the analyze command: `./build/wmbusmeters --analyze ...`

Now test your code:
./build/wmbusmeters --format=json <hex> MyHCA aventieshca 12345678 NOKEY

It should print:
```
{"media":"heat cost allocation","meter":"aventieshca","name":"MyHCA","id":"60900126","current_consumption_hca":166,"consumption_at_set_date_hca":166,"consumption_at_set_date_2_hca":166,"consumption_at_set_date_3_hca":158,"consumption_at_set_date_4_hca":126,"consumption_at_set_date_5_hca":91,"consumption_at_set_date_6_hca":66,"consumption_at_set_date_7_hca":375,"consumption_at_set_date_8_hca":347,"consumption_at_set_date_9_hca":332,"consumption_at_set_date_10_hca":327,"consumption_at_set_date_11_hca":318,"consumption_at_set_date_12_hca":315,"consumption_at_set_date_13_hca":315,"consumption_at_set_date_14_hca":315,"consumption_at_set_date_15_hca":315,"consumption_at_set_date_16_hca":275,"consumption_at_set_date_17_hca":156,"error_flags":"","timestamp":"2022-01-18T10:04:08Z"}
```

And now you can run it for real:
./build_debug/wmbusmeters --format=json auto:t1 Water MyMeter 12345678 <key>

or using the config that you setup before:
`./build/wmbusmeters --debug --verbose useconfig=/`

When it looks good, now run the tests:
```
make test
```

If you have the time please try to build with debug information as well: `make DEBUG=true` and `make testd`
This will test the code and check that there are no memory leaks or buffer overflows etc.

Finally try the daemon: make; sudo make install
(Do the daemon reload command if such is requested.)
sudo systemctl start wmbusmeters.service

Update the regression tests in the end of driver_aventieshca.cc

If again everything was ok, `sudo make install` the new wmbusmeters and start it.

## Provide some Information (for decumentation and debugging)

Now edit your meter config in `/etc/wmbusmeters.d/` to use the new meter type (mine is aventieshca).

The log should show the correct recognition (aventieshca)-tags at the beginning of each decoding line for your meter.
```
(aventieshca) 17: 2f2f decrypt check bytes
(aventieshca) 19: 0B dif (6 digit BCD Instantaneous value)
(aventieshca) 1a: 6E vif (Units for H.C.A.)
(aventieshca) 1b: * 660100 current consumption (166.000000 hca)
(aventieshca) 1e: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(aventieshca) 1f: 6E vif (Units for H.C.A.)
(aventieshca) 20: * A600 consumption at set date 1 (166.000000 hca)
(aventieshca) 22: 82 dif (16 Bit Integer/Binary Instantaneous value)
(aventieshca) 23: 01 dife (subunit=0 tariff=0 storagenr=2)
(aventieshca) 24: 6E vif (Units for H.C.A.)
(aventieshca) 25: * A600 consumption at set date 2 (166.000000 hca)
(aventieshca) 27: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(aventieshca) 28: 01 dife (subunit=0 tariff=0 storagenr=3)
(aventieshca) 29: 6E vif (Units for H.C.A.)
(aventieshca) 2a: * 9E00 consumption at set date 3 (158.000000 hca)
(aventieshca) 2c: 82 dif (16 Bit Integer/Binary Instantaneous value)
(aventieshca) 2d: 02 dife (subunit=0 tariff=0 storagenr=4)
(aventieshca) 2e: 6E vif (Units for H.C.A.)
(aventieshca) 2f: * 7E00 consumption at set date 4 (126.000000 hca)
(aventieshca) 31: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(aventieshca) 32: 02 dife (subunit=0 tariff=0 storagenr=5)
(aventieshca) 33: 6E vif (Units for H.C.A.)
(aventieshca) 34: * 5B00 consumption at set date 5 (91.000000 hca)
(aventieshca) 36: 82 dif (16 Bit Integer/Binary Instantaneous value)
(aventieshca) 37: 03 dife (subunit=0 tariff=0 storagenr=6)
(aventieshca) 38: 6E vif (Units for H.C.A.)
(aventieshca) 39: * 4200 consumption at set date 6 (66.000000 hca)
(aventieshca) 3b: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(aventieshca) 3c: 03 dife (subunit=0 tariff=0 storagenr=7)
(aventieshca) 3d: 6E vif (Units for H.C.A.)
(aventieshca) 3e: * 7701 consumption at set date 7 (375.000000 hca)
(aventieshca) 40: 82 dif (16 Bit Integer/Binary Instantaneous value)
(aventieshca) 41: 04 dife (subunit=0 tariff=0 storagenr=8)
(aventieshca) 42: 6E vif (Units for H.C.A.)
(aventieshca) 43: * 5B01 consumption at set date 8 (347.000000 hca)
(aventieshca) 45: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(aventieshca) 46: 04 dife (subunit=0 tariff=0 storagenr=9)
(aventieshca) 47: 6E vif (Units for H.C.A.)
(aventieshca) 48: * 4C01 consumption at set date 9 (332.000000 hca)
(aventieshca) 4a: 82 dif (16 Bit Integer/Binary Instantaneous value)
(aventieshca) 4b: 05 dife (subunit=0 tariff=0 storagenr=10)
(aventieshca) 4c: 6E vif (Units for H.C.A.)
(aventieshca) 4d: * 4701 consumption at set date 10 (327.000000 hca)
(aventieshca) 4f: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(aventieshca) 50: 05 dife (subunit=0 tariff=0 storagenr=11)
(aventieshca) 51: 6E vif (Units for H.C.A.)
(aventieshca) 52: * 3E01 consumption at set date 11 (318.000000 hca)
(aventieshca) 54: 82 dif (16 Bit Integer/Binary Instantaneous value)
(aventieshca) 55: 06 dife (subunit=0 tariff=0 storagenr=12)
(aventieshca) 56: 6E vif (Units for H.C.A.)
(aventieshca) 57: * 3B01 consumption at set date 12 (315.000000 hca)
(aventieshca) 59: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(aventieshca) 5a: 06 dife (subunit=0 tariff=0 storagenr=13)
(aventieshca) 5b: 6E vif (Units for H.C.A.)
(aventieshca) 5c: * 3B01 consumption at set date 13 (315.000000 hca)
(aventieshca) 5e: 82 dif (16 Bit Integer/Binary Instantaneous value)
(aventieshca) 5f: 07 dife (subunit=0 tariff=0 storagenr=14)
(aventieshca) 60: 6E vif (Units for H.C.A.)
(aventieshca) 61: * 3B01 consumption at set date 14 (315.000000 hca)
(aventieshca) 63: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(aventieshca) 64: 07 dife (subunit=0 tariff=0 storagenr=15)
(aventieshca) 65: 6E vif (Units for H.C.A.)
(aventieshca) 66: * 3B01 consumption at set date 15 (315.000000 hca)
(aventieshca) 68: 82 dif (16 Bit Integer/Binary Instantaneous value)
(aventieshca) 69: 08 dife (subunit=0 tariff=0 storagenr=16)
(aventieshca) 6a: 6E vif (Units for H.C.A.)
(aventieshca) 6b: * 1301 consumption at set date 16 (275.000000 hca)
(aventieshca) 6d: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(aventieshca) 6e: 08 dife (subunit=0 tariff=0 storagenr=17)
(aventieshca) 6f: 6E vif (Units for H.C.A.)
(aventieshca) 70: * 9C00 consumption at set date 17 (156.000000 hca)
(aventieshca) 72: 02 dif (16 Bit Integer/Binary Instantaneous value)
(aventieshca) 73: FD vif (Second extension FD of VIF-codes)
(aventieshca) 74: 17 vife (Error flags (binary))
(aventieshca) 75: * 0000 error flags (0000)
```

You can also provide an encrypted telegram, the AES key for the telegram and the decoded data in your pull request for Fredrik to add it to the regression tests. The information is part of the log and looks like this:
```
(serial) received binary "A5C203764421042601906055087226019060210455080A00600537A7B807E3BA027FE98D75848595628733C29F2D3262F23BA4D2C01D37084E784691E115674D6D8CB874698D4D2C9DB3832A38A39021457A46F151FCBC86947EE7E35CF7AFC049381E74FB2B19E0F835B867B40D22E61129D395263441F4DED061B541"
(im871a) checkIM871AFrame "A5C203764421042601906055087226019060210455080A00600537A7B807E3BA027FE98D75848595628733C29F2D3262F23BA4D2C01D37084E784691E115674D6D8CB874698D4D2C9DB3832A38A39021457A46F151FCBC86947EE7E35CF7AFC049381E74FB2B19E0F835B867B40D22E61129D395263441F4DED061B541"
(im871a) has_timestamp=0 has_rssi=1 has_crc16=1
[...]
(TPL) decrypting "37A7B807E3BA027FE98D75848595628733C29F2D3262F23BA4D2C01D37084E784691E115674D6D8CB874698D4D2C9DB3832A38A39021457A46F151FCBC86947EE7E35CF7AFC049381E74FB2B19E0F835B867B40D22E61129D395263441F4DED0"
(TPL) num encrypted blocks 6 (96 bytes and remaining unencrypted 0 bytes)
(TPL) IV 21042601906055080A0A0A0A0A0A0A0A
(TPL) decrypted  "2F2F0B6E660100426EA60082016EA600C2016E9E0082026E7E00C2026E5B0082036E4200C2036E770182046E5B01C2046E4C0182056E4701C2056E3E0182066E3B01C2066E3B0182076E3B01C2076E3B0182086E1301C2086E9C0002FD170000"
(telegram) TPL CI=72 ACC=0a STS=00 CFG=0560 (AES_CBC_IV nb=6 cntn=0 ra=0 hc=0) ID=26019060 MFT=2104 VER=55 TYPE=08 (Heat Cost Allocator)
telegram=|764421042601906055087226019060210455080A0060052F2F|0B6E660100426EA60082016EA600C2016E9E0082026E7E00C2026E5B0082036E4200C2036E770182046E5B01C2046E4C0182056E4701C2056E3E0182066E3B01C2066E3B0182076E3B01C2076E3B0182086E1301C2086E9C0002FD170000|+7599
```

It would also be handy to provide a datasheet of the sensor within the pull request.

That's all :-)
Have fun!
