/*
 Copyright (C) 2018-2022 Fredrik Öhrström (gpl-3.0-or-later)

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include"aes.h"
#include"aescmac.h"
#include"cmdline.h"
#include"config.h"
#include"meters.h"
#include"printer.h"
#include"serial.h"
#include"translatebits.h"
#include"util.h"
#include"wmbus.h"
#include"dvparser.h"

#include<string.h>

using namespace std;

int test_crc();
int test_dvparser();
int test_test();
int test_linkmodes();
void test_ids();
void test_kdf();
void test_periods();
void test_devices();
void test_meters();
void test_months();
void test_aes();
void test_sbc();
void test_hex();
void test_translate();
void test_slip();
void test_dvs();

int main(int argc, char **argv)
{
    if (argc > 1) {
        if (!strcmp(argv[1], "--debug"))
        {
            debugEnabled(true);
        }
        if (!strcmp(argv[1], "--trace"))
        {
            debugEnabled(true);
            traceEnabled(true);
        }
    }
    onExit([](){});

    test_crc();
    test_dvparser();
    test_test();
    test_devices();
    test_meters();
    /*
      test_linkmodes();*/
    test_ids();
    test_kdf();
    test_periods();
    test_months();
    test_aes();
    test_sbc();
    test_hex();
    test_translate();
    test_slip();
    test_dvs();

    return 0;
}

int test_crc()
{
    int rc = 0;

    unsigned char data[4];
    data[0] = 0x01;
    data[1] = 0xfd;
    data[2] = 0x1f;
    data[3] = 0x01;

    uint16_t crc = crc16_EN13757(data, 4);
    if (crc != 0xcc22) {
        printf("ERROR! %4x should be cc22\n", crc);
        rc = -1;
    }
    data[3] = 0x00;

    crc = crc16_EN13757(data, 4);
    if (crc != 0xf147) {
        printf("ERROR! %4x should be f147\n", crc);
        rc = -1;
    }

    uchar block[10];
    block[0]=0xEE;
    block[1]=0x44;
    block[2]=0x9A;
    block[3]=0xCE;
    block[4]=0x01;
    block[5]=0x00;
    block[6]=0x00;
    block[7]=0x80;
    block[8]=0x23;
    block[9]=0x07;

    crc = crc16_EN13757(block, 10);

    if (crc != 0xaabc) {
        printf("ERROR! %4x should be aabc\n", crc);
        rc = -1;
    }

    block[0]='1';
    block[1]='2';
    block[2]='3';
    block[3]='4';
    block[4]='5';
    block[5]='6';
    block[6]='7';
    block[7]='8';
    block[8]='9';

    crc = crc16_EN13757(block, 9);

    if (crc != 0xc2b7) {
        printf("ERROR! %4x should be c2b7\n", crc);
        rc = -1;
    }
    return rc;
}

int test_parse(const char *data, std::map<std::string,std::pair<int,DVEntry>> *dv_entries, int testnr)
{
    debug("\n\nTest nr %d......\n\n", testnr);
    bool b;
    Telegram t;
    vector<uchar> databytes;
    hex2bin(data, &databytes);
    vector<uchar>::iterator i = databytes.begin();

    b = parseDV(&t, databytes, i, databytes.size(), dv_entries);

    return b;
}

void test_double(map<string,pair<int,DVEntry>> &values, const char *key, double v, int testnr)
{
    int offset;
    double value;
    bool b =  extractDVdouble(&values,
                              key,
                              &offset,
                              &value);

    if (!b || value != v) {
        fprintf(stderr, "Error in dvparser testnr %d: got %lf but expected value %lf for key %s\n", testnr, value, v, key);
    }
}

void test_string(map<string,pair<int,DVEntry>> &values, const char *key, const char *v, int testnr)
{
    int offset;
    string value;
    bool b = extractDVHexString(&values,
                                key,
                                &offset,
                                &value);
    if (!b || value != v) {
        fprintf(stderr, "Error in dvparser testnr %d: got \"%s\" but expected value \"%s\" for key %s\n", testnr, value.c_str(), v, key);
    }
}

void test_date(map<string,pair<int,DVEntry>> &values, const char *key, string date_expected, int testnr)
{
    int offset;
    struct tm value;
    bool b = extractDVdate(&values,
                           key,
                           &offset,
                           &value);

    char buf[256];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &value);
    string date_got = buf;
    if (!b || date_got != date_expected) {
        fprintf(stderr, "Error in dvparser testnr %d:\ngot >%s< but expected >%s< for key %s\n\n", testnr, date_got.c_str(), date_expected.c_str(), key);
    }
}

int test_dvparser()
{
    map<string,pair<int,DVEntry>> dv_entries;

    int testnr = 1;
    test_parse("2F 2F 0B 13 56 34 12 8B 82 00 93 3E 67 45 23 0D FD 10 0A 30 31 32 33 34 35 36 37 38 39 0F 88 2F", &dv_entries, testnr);
    test_double(dv_entries, "0B13", 123.456, testnr);
    test_double(dv_entries, "8B8200933E", 234.567, testnr);
    test_string(dv_entries, "0DFD10", "30313233343536373839", testnr);

    testnr++;
    dv_entries.clear();
    test_parse("82046C 5f1C", &dv_entries, testnr);
    test_date(dv_entries, "82046C", "2010-12-31 00:00:00", testnr); // 2010-dec-31

    testnr++;
    dv_entries.clear();
    test_parse("0C1348550000426CE1F14C130000000082046C21298C0413330000008D04931E3A3CFE3300000033000000330000003300000033000000330000003300000033000000330000003300000033000000330000004300000034180000046D0D0B5C2B03FD6C5E150082206C5C290BFD0F0200018C4079678885238310FD3100000082106C01018110FD610002FD66020002FD170000", &dv_entries, testnr);
    test_double(dv_entries, "0C13", 5.548, testnr);
    test_date(dv_entries, "426C", "2127-01-01 00:00:00", testnr); // 2127-jan-1
    test_date(dv_entries, "82106C", "2000-01-01 00:00:00", testnr); // 2000-jan-1

    testnr++;
    dv_entries.clear();
    test_parse("426C FE04", &dv_entries, testnr);
    test_date(dv_entries, "426C", "2007-04-30 00:00:00", testnr); // 2010-dec-31
    return 0;
}

int test_test()
{
    shared_ptr<SerialCommunicationManager> manager = createSerialCommunicationManager(0, false);

    shared_ptr<SerialDevice> serial1 = manager->createSerialDeviceSimulator();

    /*
    shared_ptr<WMBus> wmbus_im871a = openIM871A("", manager, serial1);
    manager->stop();*/
    return 0;
}

int test_linkmodes()
{
    LinkModeCalculationResult lmcr;
    auto manager = createSerialCommunicationManager(0, false);

    auto serial1 = manager->createSerialDeviceSimulator();
    auto serial2 = manager->createSerialDeviceSimulator();
    auto serial3 = manager->createSerialDeviceSimulator();
    auto serial4 = manager->createSerialDeviceSimulator();

    vector<string> no_meter_shells, no_meter_jsons;
    Detected de;
    SpecifiedDevice sd;
    shared_ptr<WMBus> wmbus_im871a = openIM871A(de, manager, serial1);
    shared_ptr<WMBus> wmbus_amb8465 = openAMB8465(de, manager, serial2);
    shared_ptr<WMBus> wmbus_rtlwmbus = openRTLWMBUS(de, "", false, manager, serial3);
    shared_ptr<WMBus> wmbus_rawtty = openRawTTY(de, manager, serial4);

    Configuration nometers_config;
    // Check that if no meters are supplied then you must set a link mode.
    lmcr = calculateLinkModes(&nometers_config, wmbus_im871a.get());
    if (lmcr.type != LinkModeCalculationResultType::NoMetersMustSupplyModes)
    {
        printf("ERROR! Expected failure due to automatic deduction! Got instead:\n%s\n", lmcr.msg.c_str());
    }
    debug("test0 OK\n\n");

    nometers_config.default_device_linkmodes.addLinkMode(LinkMode::T1);
    lmcr = calculateLinkModes(&nometers_config, wmbus_im871a.get());
    if (lmcr.type != LinkModeCalculationResultType::Success)
    {
        printf("ERROR! Expected succcess! Got instead:\n%s\n", lmcr.msg.c_str());
    }
    debug("test0.0 OK\n\n");

    Configuration apator_config;
    string apator162 = "apator162";
    vector<string> ids = { "12345678" };
    apator_config.meters.push_back(MeterInfo("", // bus
                                             "m1", // name
                                             MeterDriver::APATOR162, // driver/type
                                             "", // extras
                                             ids, // ids
                                             "", // Key
                                             toMeterLinkModeSet(apator162), // link mode set
                                             0, // baud
                                             no_meter_shells, // shells
                                             no_meter_jsons)); // jsons

    // Check that if no explicit link modes are provided to apator162, then
    // automatic deduction will fail, since apator162 can be configured to transmit
    // either C1 or T1 telegrams.
    lmcr = calculateLinkModes(&apator_config, wmbus_im871a.get());
    if (lmcr.type != LinkModeCalculationResultType::AutomaticDeductionFailed)
    {
        printf("ERROR! Expected failure due to automatic deduction! Got instead:\n%s\n", lmcr.msg.c_str());
    }
    debug("test1 OK\n\n");

    // Check that if we supply the link mode T1 when using an apator162, then
    // automatic deduction will succeeed.
    apator_config.default_device_linkmodes = LinkModeSet();
    apator_config.default_device_linkmodes.addLinkMode(LinkMode::T1);
    apator_config.default_device_linkmodes.addLinkMode(LinkMode::C1);
    lmcr = calculateLinkModes(&apator_config, wmbus_im871a.get());
    if (lmcr.type != LinkModeCalculationResultType::DongleCannotListenTo)
    {
        printf("ERROR! Expected dongle cannot listen to! Got instead:\n%s\n", lmcr.msg.c_str());
    }
    debug("test2 OK\n\n");

    lmcr = calculateLinkModes(&apator_config, wmbus_rtlwmbus.get());
    if (lmcr.type != LinkModeCalculationResultType::Success)
    {
        printf("ERROR! Expected success! Got instead:\n%s\n", lmcr.msg.c_str());
    }
    debug("test3 OK\n\n");

    Configuration multical21_and_supercom587_config;
    string multical21 = "multical21";
    string supercom587 = "supercom587";

    multical21_and_supercom587_config.meters.push_back(MeterInfo("", "m1", MeterDriver::MULTICAL21, "", ids, "",
                                                                 toMeterLinkModeSet(multical21),
                                                                 0,
                                                                 no_meter_shells,
                                                                 no_meter_jsons));
    multical21_and_supercom587_config.meters.push_back(MeterInfo("", "m2", MeterDriver::UNKNOWN, "supercom587", ids, "",
                                                                 toMeterLinkModeSet(supercom587),
                                                                 0,
                                                                 no_meter_shells,
                                                                 no_meter_jsons));

    // Check that meters that transmit on two different link modes cannot be listened to
    // at the same time using im871a.
    lmcr = calculateLinkModes(&multical21_and_supercom587_config, wmbus_im871a.get());
    if (lmcr.type != LinkModeCalculationResultType::AutomaticDeductionFailed)
    {
        printf("ERROR! Expected failure due to automatic deduction! Got instead:\n%s\n", lmcr.msg.c_str());
    }
    debug("test4 OK\n\n");

    // Explicitly set T1
    multical21_and_supercom587_config.default_device_linkmodes = LinkModeSet();
    multical21_and_supercom587_config.default_device_linkmodes.addLinkMode(LinkMode::T1);
    lmcr = calculateLinkModes(&multical21_and_supercom587_config, wmbus_im871a.get());
    if (lmcr.type != LinkModeCalculationResultType::MightMissTelegrams)
    {
        printf("ERROR! Expected might miss telegrams! Got instead:\n%s\n", lmcr.msg.c_str());
    }
    debug("test5 OK\n\n");

    // Explicitly set N1a, but the meters transmit on C1 and T1.
    multical21_and_supercom587_config.default_device_linkmodes = LinkModeSet();
    multical21_and_supercom587_config.default_device_linkmodes.addLinkMode(LinkMode::N1a);
    lmcr = calculateLinkModes(&multical21_and_supercom587_config, wmbus_im871a.get());
    if (lmcr.type != LinkModeCalculationResultType::MightMissTelegrams)
    {
        printf("ERROR! Expected no meter can be heard! Got instead:\n%s\n", lmcr.msg.c_str());
    }
    debug("test6 OK\n\n");

    // Explicitly set N1a, but it is an amber dongle.
    multical21_and_supercom587_config.default_device_linkmodes = LinkModeSet();
    multical21_and_supercom587_config.default_device_linkmodes.addLinkMode(LinkMode::N1a);
    lmcr = calculateLinkModes(&multical21_and_supercom587_config, wmbus_amb8465.get());
    if (lmcr.type != LinkModeCalculationResultType::DongleCannotListenTo)
    {
        printf("ERROR! Expected dongle cannot listen to! Got instead:\n%s\n", lmcr.msg.c_str());
    }
    debug("test7 OK\n\n");

    manager->stop();
    return 0;
}

void test_valid_match_expression(string s, bool expected)
{
    bool b = isValidMatchExpressions(s, false);
    if (b == expected) return;
    if (expected == true)
    {
        printf("ERROR! Expected \"%s\" to be valid! But it was not!\n", s.c_str());
    }
    else
    {
        printf("ERROR! Expected \"%s\" to be invalid! But it was reported as valid!\n", s.c_str());
    }
}

void test_does_id_match_expression(string id, string mes, bool expected, bool expected_uw)
{
    vector<string> expressions = splitMatchExpressions(mes);
    bool uw = false;
    bool b = doesIdMatchExpressions(id, expressions, &uw);
    if (b != expected)
    {
        if (expected == true)
        {
            printf("ERROR! Expected \"%s\" to match \"%s\" ! But it did not!\n", id.c_str(), mes.c_str());
        }
        else
        {
            printf("ERROR! Expected \"%s\" to NOT match \"%s\" ! But it did!\n", id.c_str(), mes.c_str());
        }
    }
    if (expected_uw != uw)
    {
        printf("ERROR! Matching \"%s\" \"%s\" and expecte used_wildcard %d but got %d!\n",
               id.c_str(), mes.c_str(), expected_uw, uw);
    }
}

void test_ids()
{
    test_valid_match_expression("12345678", true);
    test_valid_match_expression("*", true);
    test_valid_match_expression("!12345678", true);
    test_valid_match_expression("12345*", true);
    test_valid_match_expression("!123456*", true);

    test_valid_match_expression("1234567", false);
    test_valid_match_expression("", false);
    test_valid_match_expression("z1234567", false);
    test_valid_match_expression("123456789", false);
    test_valid_match_expression("!!12345678", false);
    test_valid_match_expression("12345678*", false);
    test_valid_match_expression("**", false);
    test_valid_match_expression("123**", false);

    test_valid_match_expression("2222*,!22224444", true);

    test_does_id_match_expression("12345678", "12345678", true, false);
    test_does_id_match_expression("12345678", "*", true, true);
    test_does_id_match_expression("12345678", "2*", false, false);

    test_does_id_match_expression("12345678", "*,!2*", true, true);

    test_does_id_match_expression("22222222", "22*,!22222222", false, false);
    test_does_id_match_expression("22222223", "22*,!22222222", true, true);
    test_does_id_match_expression("22222223", "*,!22*", false, false);
    test_does_id_match_expression("12333333", "123*,!1234*,!1235*,!1236*", true, true);
    test_does_id_match_expression("12366666", "123*,!1234*,!1235*,!1236*", false, false);
    test_does_id_match_expression("11223344", "22*,33*,44*,55*", false, false);
    test_does_id_match_expression("55223344", "22*,33*,44*,55*", true, true);

    test_does_id_match_expression("78563413", "78563412,78563413", true, false);
    test_does_id_match_expression("78563413", "*,!00156327,!00048713", true, true);
}

void eq(string a, string b, const char *tn)
{
    if (a != b)
    {
        printf("ERROR in test %s expected \"%s\" to be equal to \"%s\"\n", tn, a.c_str(), b.c_str());
    }
}

void eqn(int a, int b, const char *tn)
{
    if (a != b)
    {
        printf("ERROR in test %s expected %d to be equal to %d\n", tn, a, b);
    }
}

void test_kdf()
{
    vector<uchar> key;
    vector<uchar> input;
    vector<uchar> mac;

    hex2bin("2b7e151628aed2a6abf7158809cf4f3c", &key);
    mac.resize(16);

    AES_CMAC(&key[0], &input[0], 0, &mac[0]);
    string s = bin2hex(mac);
    string ex = "BB1D6929E95937287FA37D129B756746";
    if (s != ex)
    {
        printf("ERROR in aes-cmac expected \"%s\" but got \"%s\"\n", ex.c_str(), s.c_str());
    }


    input.clear();
    hex2bin("6bc1bee22e409f96e93d7e117393172a", &input);
    AES_CMAC(&key[0], &input[0], 16, &mac[0]);
    s = bin2hex(mac);
    ex = "070A16B46B4D4144F79BDD9DD04A287C";

    if (s != ex)
    {
        printf("ERROR in aes-cmac expected \"%s\" but got \"%s\"\n", ex.c_str(), s.c_str());
    }
}

void testp(time_t now, string period, bool expected)
{
    bool rc = isInsideTimePeriod(now, period);

    char buf[256];
    struct tm now_tm {};
    localtime_r(&now, &now_tm);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M %A", &now_tm);
    string nows = buf;

    if (expected == true && rc == false)
    {
        printf("ERROR in period test is \"%s\" in period \"%s\"? Expected true but got false!\n", nows.c_str(), period.c_str());
    }
    if (expected == false && rc == true)
    {
        printf("ERROR in period test is \"%s\" in period \"%s\"? Expected false but got true!\n", nows.c_str(), period.c_str());
    }
}

void test_periods()
{
    // 3600*24*7+3600 means 1970-01-08 01:00 Thursday in Greenwich.
    // However your local time is adjusted with your timezone.
    // Get your timezone offset tm_gmtoff into the value.
    time_t t = 3600*24*7+3600;
    struct tm value {};
    localtime_r(&t, &value);

    // if tm_gmtoff is zero, then we are in Greenwich!
    // if tm_gmtoff is 3600 then we are in Stockholm!

    t -= value.tm_gmtoff;

    // We have now adjusted the t so that it should be thursday at 1 am.
    // The following test should therefore work inependently on
    // where in the world this test is run.
    testp(t, "mon-sun(00-23)", true);
    testp(t, "mon(00-23)", false);
    testp(t, "thu-fri(01-01)", true);
    testp(t, "mon-wed(00-23),thu(02-23),fri-sun(00-23)", false);
    testp(t, "mon-wed(00-23),thu(01-23),fri-sun(00-23)", true);
    testp(t, "thu(00-00)", false);
    testp(t, "thu(01-01)", true);
}

void testd(string arg, bool xok, string xalias, string xfile, string xtype, string xid, string xextras,
           string xfq, string xbps, string xlm, string xcmd)
{
    SpecifiedDevice d;
    bool ok = d.parse(arg);
    if (ok != xok)
    {
        printf("ERROR in device parse test \"%s\" expected %s but got %s\n", arg.c_str(), xok?"OK":"FALSE", ok?"OK":"FALSE");
        return;
    }
    if (ok == false) return;

    if (d.bus_alias != xalias ||
        d.file != xfile ||
        toString(d.type) != xtype ||
        d.id != xid ||
        d.extras != xextras ||
        d.fq != xfq ||
        d.bps != xbps ||
        d.linkmodes.hr() != xlm ||
        d.command != xcmd)
    {
        printf("ERROR in device parsing parts \"%s\"\n", arg.c_str());
    }
}

void test_devices()
{
    testd("Bus_4711=/dev/ttyUSB0:im871a[12345678]:9600:868.95M:c1,t1", true,
          "Bus_4711", // alias
          "/dev/ttyUSB0", // file
          "im871a", // type
          "12345678", // id
          "", // extras
          "868.95M", // fq
          "9600", // bps
          "c1,t1", // linkmodes
          ""); // command

    testd("/dev/ttyUSB0:im871a:c1", true,
          "", // alias
          "/dev/ttyUSB0", // file
          "im871a", // type
          "", // id
          "", // extras
          "", // fq
          "", // bps
          "c1", // linkmodes
          ""); // command

    testd("im871a[12345678]:c1", true,
          "", // alias
          "", // file
          "im871a", // type
          "12345678", // id
          "", // extras
          "", // fq
          "", // bps
          "c1", // linkmodes
          ""); // command

    testd("im871a(track=7,pi=3.14):c1", true,
          "", // alias
          "", // file
          "im871a", // type
          "", // id
          "track=7,pi=3.14", // extras
          "", // fq
          "", // bps
          "c1", // linkmodes
          ""); // command

    testd("rtlwmbus:c1,t1:CMD(gurka)", true,
          "", // alias
          "", // file
          "rtlwmbus", // type
          "", // id
          "", // extras
          "", // fq
          "", // bps
          "c1,t1", // linkmodes
          "gurka"); // command

    testd("rtlwmbus[plast]:c1,t1", true,
          "", // alias
          "", // file
          "rtlwmbus", // type
          "plast", // id
          "", // extras
          "", // fq
          "", // bps
          "c1,t1", // linkmodes
          ""); // command

    testd("ANTENNA1=rtlwmbus[plast](ppm=5):c1,t1", true,
          "ANTENNA1", // alias
          "", // file
          "rtlwmbus", // type
          "plast", // id
          "ppm=5", // extras
          "", // fq
          "", // bps
          "c1,t1", // linkmodes
          ""); // command

    testd("stdin:rtlwmbus", true,
          "", // alias
          "stdin", // file
          "rtlwmbus", // type
          "", // id
          "", // extras
          "", // fq
          "", // bps
          "none", // linkmodes
          ""); // command

    testd("/dev/ttyUSB0:rawtty:9600", true,
          "", // alias
          "/dev/ttyUSB0", // file
          "rawtty", // type
          "", // id
          "", // extras
          "", // fq
          "9600", // bps
          "none", // linkmodes
          ""); // command

    // testinternals must be run from a location where
    // there is a Makefile.
    testd("Makefile:simulation", true,
          "", // alias
          "Makefile", // file
          "simulation", // type
          "", // id
          "", // extras
          "", // fq
          "", // bps
          "none", // linkmodes
          ""); // command

    testd("auto:c1,t1", true,
          "", // alias
          "", // file
          "auto", // type
          "", // id
          "", // extras
          "", // fq
          "", // bps
          "c1,t1", // linkmodes
          ""); // command

    testd("auto:Makefile:c1,t1", false,
          "", // alias
          "", // file
          "", // type
          "", // id
          "", // extras
          "", // fq
          "", // bps
          "none", // linkmodes
          ""); // command

    testd("Vatten", false,
          "", // alias
          "", // file
          "", // type
          "", // id
          "", // extras
          "", // fq
          "", // bps
          "none", // linkmodes
          ""); // command

    testd("main=/dev/ttyUSB0:mbus:2400", true,
          "main", // alias
          "/dev/ttyUSB0", // file
          "mbus", // type
          "", // id
          "", // extras
          "", // fq
          "2400", // bps
          "none", // linkmodes
          ""); // command

    // Support : inside the command.
    testd("cul:c1:CMD(socat TCP:CUNO:2323 STDIO)", true,
          "", // alias
          "", // file
          "cul", // type
          "", // id
          "", // extras
          "", // fq
          "", // bps
          "c1", // linkmodes
          "socat TCP:CUNO:2323 STDIO"); // command
}

void test_month(int y, int m, int day, int mdiff, string from, string to)
{
    struct tm date;
    date.tm_year  = y-1900;
    date.tm_mon   = m-1;
    date.tm_mday  = day;

    string s = strdate(&date);

    struct tm d;
    d = date;
    addMonths(&d, mdiff);

    string os = strdate(&d);

    if (s != from ||
        os != to)
    {
        printf("ERROR! Expected %s + %d months should be %s\n"
               "But got %s - 11 = %s\n",
               from.c_str(), mdiff, to.c_str(),
               s.c_str(), os.c_str());
    }
}

void test_months()
{
    test_month(2020,12,31, 2, "2020-12-31", "2021-02-28");
    test_month(2020,12,31,-10, "2020-12-31", "2020-02-29");
    test_month(2021,01,31,-1,  "2021-01-31", "2020-12-31");
    test_month(2021,01,31,-2,  "2021-01-31", "2020-11-30");
    test_month(2021,01,31,-24, "2021-01-31", "2019-01-31");
    test_month(2021,01,31, 24, "2021-01-31", "2023-01-31");
    test_month(2021,01,31, 22, "2021-01-31", "2022-11-30");

    // 2020 was a leap year.
    test_month(2021,02,28, -12, "2021-02-28", "2020-02-29");
    // 2000 was a leap year %100=0 but %400=0 overrides.
    test_month(2001,02,28, -12, "2001-02-28", "2000-02-29");
    // 2100 is not a leap year since %100=0 and not overriden %400 != 0.
    test_month(2000,02,29, 12*100, "2000-02-29", "2100-02-28");
}

// Vatten    multical21:BUS1:c1 12345678 KEY
// Tempmeter piigth(info=123):BUS2:2400   0        NOKEY

void testm(string arg, bool xok,
           string xdriver, string xextras, string xbus, string xbps, string xlm)
{
    MeterInfo mi;
    bool ok = mi.parse("", arg, "12345678", "");
    if (ok != xok)
    {
        printf("ERROR in meter parse test \"%s\" expected %s but got %s\n", arg.c_str(), xok?"OK":"FALSE", ok?"OK":"FALSE");
        return;
    }
    if (ok == false) return;

    bool driver_ok = toString(mi.driver) == xdriver || mi.driverName().str() == xdriver;
    bool extras_ok = mi.extras == xextras;
    bool bus_ok = mi.bus == xbus;
    bool bps_ok = to_string(mi.bps) == xbps;
    bool link_modes_ok =  mi.link_modes.hr() == xlm;

    if (!driver_ok || !extras_ok || !bus_ok || !bps_ok || !link_modes_ok)
    {
        printf("ERROR in meterm parsing parts \"%s\" - got (driver: \"%s/%s\" (%d), extras: \"%s\" (%d), bus: \"%s\" (%d), bbps: \"%s\" (%d), linkmodes: \"%s\" (%d))\n",
               arg.c_str(),
               toString(mi.driver).c_str(), mi.driverName().str().c_str(), driver_ok,
               mi.extras.c_str(), extras_ok,
               mi.bus.c_str(), bus_ok,
               to_string(mi.bps).c_str(), bps_ok,
               mi.link_modes.hr().c_str(), link_modes_ok);
    }
}

void testc(string file, string file_content,
    string xdriver, string xextras, string xbus, string xbps, string xlm)
{
    MeterInfo mi;
    Configuration c;

    vector<char> meter_conf(file_content.begin(), file_content.end());
    meter_conf.push_back('\n');

    parseMeterConfig(&c, meter_conf, file);

    mi = c.meters.back();
    if ((toString(mi.driver) != xdriver && mi.driverName().str() != xdriver) ||
        mi.extras != xextras ||
        mi.bus != xbus ||
        to_string(mi.bps) != xbps ||
        mi.link_modes.hr() != xlm)
    {
        printf("ERROR in meterc parsing parts \"%s\" - got (driver: %s/%s, extras: %s, bus: %s, bbps: %s, linkmodes: %s)\n",
               file.c_str(),
               toString(mi.driver).c_str(), mi.driverName().str().c_str(),
               mi.extras.c_str(), mi.bus.c_str(), to_string(mi.bps).c_str(), mi.link_modes.hr().c_str());
    }
}

void test_meters()
{
    string config_content;

    testm("piigth:BUS1:2400", true,
          "piigth", // driver
          "", // extras
          "BUS1", // bus
          "2400", // bps
          "none"); // linkmodes

    testm("c5isf:MAINO:9600:mbus", true,
          "c5isf", // driver
          "", // extras
          "MAINO", // bus
          "9600", // bps
          "mbus"); // linkmodes

    testm("c5isf:DONGLE:t1", true,
          "c5isf", // driver
          "", // extras
          "DONGLE", // bus
          "0", // bps
          "t1"); // linkmodes

    testm("c5isf:t1,c1,mbus", true,
          "c5isf", // driver
          "", // extras
          "", // bus
          "0", // bps
          "c1,t1,mbus"); // linkmodes

    /*
    config_content =
        "name=test\n"
        "driver=piigth:BUS1:2400:mbus\n"
        "id=01234567\n";

    testc("meter/piigth:BUS1:2400", config_content,
          "piigth", // driver
          "", // extras
          "BUS1", // bus
          "2400", // bps
          "mbus"); // linkmodes)
    */

    testm("multical21:c1", true,
          "multical21", // driver
          "", // extras
          "", // bus
          "0", // bps
          "c1"); // linkmodes

    config_content =
        "name=test\n"
        "driver=multical21:c1\n"
        "id=01234567\n";
    testc("meter/multical21:c1", config_content,
          "multical21", // driver
          "", // extras
          "", // bus
          "0", // bps
          "c1"); // linkmodes)


    testm("apator162(offset=162)", true,
          "apator162", // driver
          "offset=162", // extras
          "", // bus
          "0", // bps
          "c1,t1"); // linkmodes

    config_content =
        "name=test\n"
        "driver=apator162(offset=162)\n"
        "id=01234567\n"
        "key=00000000000000000000000000000000\n";
    testc("meter/apator162(offset=162)", config_content,
          "apator162", // driver
          "offset=162", // extras
          "", // bus
          "0", // bps
          "c1,t1"); // linkmodes)

}

void tests(string arg, bool expect, ContentStartsWith sw, string bus, string content)
{
    SendBusContent sbc;
    bool rc = sbc.parse(arg);
    if (rc != expect && rc == false)
    {
        printf("ERROR could not parse send bus content \"%s\"\n", arg.c_str());
        return;
    }
    if (rc != expect && rc == true)
    {
        printf("ERROR could parse send bus content \"%s\" but expected failure!\n", arg.c_str());
        return;
    }

    if (expect == false && rc == false) return; // It failed, which was expected.

    if (sbc.starts_with != sw ||
        sbc.bus != bus ||
        sbc.content != content)
    {
        printf("ERROR in parsing send bus content \"%s\"\n"
               "got      (sw: %s bus: %s, data: %s)\n"
               "expected (sw: %s bus: %s, data: %s)\n", arg.c_str(),
               toString(sbc.starts_with), sbc.bus.c_str(), sbc.content.c_str(),
               toString(sw), bus.c_str(), content.c_str());
    }
}

void test_sbc()
{
    tests("sendc:BUS1:11223344", true,
          ContentStartsWith::C_FIELD,
          "BUS1", // bus
          "11223344"); // content

    tests("sendci:alfa:11", true,
          ContentStartsWith::CI_FIELD,
          "alfa", // bus
          "11"); // content

    tests("alfa:t1", false, ContentStartsWith::C_FIELD, "", "");
    tests("send", false, ContentStartsWith::C_FIELD, "", "");
    tests("sendc:out", false, ContentStartsWith::C_FIELD, "", "");
    tests("sendc:out:", false, ContentStartsWith::C_FIELD, "", "x");

    tests("sends:out:5b00", true,
          ContentStartsWith::SHORT_FRAME,
          "out", // bus
          "5b00"); // content

    tests("sendl:mbus2:1122334455", true,
          ContentStartsWith::LONG_FRAME,
          "mbus2", // bus
          "1122334455"); // content
}

void test_aes()
{
    vector<uchar> key;

    hex2bin("0123456789abcdef0123456789abcdef", &key);
    string poe =
        "Once upon a midnight dreary, while I pondered, weak and weary,\n"
        "Over many a quaint and curious volume of forgotten lore\n";

    while (poe.length() % 16 != 0)
    {
        poe += ".";
    }

    uchar iv[16];
    memset(iv, 0xaa, 16);

    uchar in[poe.length()];
    memcpy(in, &poe[0], poe.size());

    debug("(aes) input: \"%s\"\n", poe.c_str());

    uchar out[sizeof(in)];
    AES_CBC_encrypt_buffer(out, in, sizeof(in), &key[0], iv);

    vector<uchar> outv(out, out+sizeof(out));
    string s = bin2hex(outv);
    debug("(aes) encrypted: \"%s\"\n", s.c_str());

    uchar back[sizeof(in)];
    AES_CBC_decrypt_buffer(back, out, sizeof(in), &key[0], iv);

    string b = string(back, back+sizeof(back));
    debug("(aes) decrypted: \"%s\"\n", b.c_str());

    if (poe != b)
    {
        printf("ERROR! aes with IV encrypt decrypt failed!\n");
    }

    AES_ECB_encrypt(in, &key[0], out, sizeof(in));
    AES_ECB_decrypt(out, &key[0], back, sizeof(in));

    if (memcmp(back, in, sizeof(in)))
    {
        printf("ERROR! aes encrypt decrypt (no iv) failed!\n");
    }
}

void test_is_hex(const char *hex, bool expected_ok, bool expected_invalid, bool strict)
{
    bool got_invalid;
    bool got_ok;
    if (strict) got_ok = isHexStringStrict(hex, &got_invalid);
    else got_ok = isHexStringFlex(hex, &got_invalid);

    if (got_ok != expected_ok || got_invalid != expected_invalid)
    {
        printf("ERROR! hex string %s was expected to be %d (invalid %d) but got %d (invalid %d)\n",
               hex,
               expected_ok, expected_invalid, got_ok, got_invalid);
    }
}

void test_hex()
{
    test_is_hex("00112233445566778899aabbccddeeff", true, false, true);
    test_is_hex("00112233445566778899AABBCCDDEEFF", true, false, true);
    test_is_hex("00112233445566778899AABBCCDDEEF", true, true, true);
    test_is_hex("00112233445566778899AABBCCDDEEFG", false, false, true);

    test_is_hex("00 11 22 33#44|55#66 778899aabbccddeeff", true, false, false);
    test_is_hex("00 11 22 33#4|55#66 778899aabbccddeeff", true, true, false);
}

void test_translate()
{
    Translate::Lookup lookup1 =
        {
            {
                {
                    "ACCESS_BITS",
                    Translate::Type::BitToString,
                    0xf0,
                    "",
                    {
                        { 0x10, "NO_ACCESS" },
                        { 0x20, "ALL_ACCESS" },
                        { 0x40, "TEMP_ACCESS" },
                    }
                },
                {
                    "ACCESSOR_TYPE",
                    Translate::Type::IndexToString,
                    0x0f,
                    "",
                    {
                        { 0x00, "ACCESSOR_RED" },
                        { 0x07, "ACCESSOR_GREEN" },
                    },
                },
            },
        };

   Translate::Lookup lookup2 =
        {
            {
                {
                    "ERROR_FLAGS",
                    Translate::Type::BitToString,
                    0x0f,
                    "",
                    {
                        { 0x01, "BACKWARD_FLOW" },
                        { 0x02, "DRY" },
                        { 0x12, "TRIG" },
                    }
                },
            },
        };

    string s, e;
    s = lookup1.translate(0xA0);
    e = "ALL_ACCESS UNKNOWN_ACCESS_BITS(0x80) ACCESSOR_RED";
    if (s != e)
    {
        printf("ERROR expected \"%s\" but got \"%s\"\n", e.c_str(), s.c_str());
    }

    s = lookup1.translate(0x35);
    e = "NO_ACCESS ALL_ACCESS UNKNOWN_ACCESSOR_TYPE(0x5)";
    if (s != e)
    {
        printf("ERROR expected \"%s\" but got \"%s\"\n", e.c_str(), s.c_str());
    }

    s = lookup2.translate(0x02);
    e = "DRY BAD_RULE_ERROR_FLAGS(from=0x12 mask=0xf)";
    if (s != e)
    {
        printf("ERROR expected \"%s\" but got \"%s\"\n", e.c_str(), s.c_str());
    }
}

void test_slip()
{
    vector<uchar> from = { 1, 0xc0, 3, 4, 5, 0xdb };
    vector<uchar> expected_to = { 0xc0, 1, 0xdb, 0xdc, 3, 4, 5, 0xdb, 0xdd, 0xc0 };
    vector<uchar> to;
    vector<uchar> back;

    addSlipFraming(from, to);

    if (expected_to != to)
    {
        printf("ERROR slip 1\n");
    }

    size_t frame_length = 0;
    removeSlipFraming(to, &frame_length, back);

    if (back != from)
    {
        printf("ERROR slip 2\n");
    }

    if (to.size() != frame_length)
    {
        printf("ERROR slip 3\n");
    }

    vector<uchar> more = { 0xc0, 0xc0, 0xc0, 1, 2, 3, 4, 5, 6, 7, 8 };
    addSlipFraming(more, to);

    frame_length = 0;
    removeSlipFraming(to, &frame_length, back);

    if (back != from)
    {
        printf("ERROR slip 4\n");
    }

    to.erase(to.begin(), to.begin()+frame_length);
    removeSlipFraming(to, &frame_length, back);

    if (back != more)
    {
        printf("ERROR slip 5\n");
    }

    vector<uchar> again = { 0xc0 };
    removeSlipFraming(again, &frame_length, back);

    if (frame_length != 0)
    {
        printf("ERROR slip 6\n");
    }

    vector<uchar> againn = { 0xc0, 1, 2, 3, 4, 5 };
    removeSlipFraming(againn, &frame_length, back);

    if (frame_length != 0)
    {
        printf("ERROR slip 7\n");
    }

}

void test_dvs()
{
    DifVifKey dvk("0B2B");

    if (dvk.dif() != 0x0b || dvk.vif() != 0x2b || dvk.hasDifes() || dvk.hasVifes())
    {
        printf("ERROR test_dvs 1\n");
    }
}
