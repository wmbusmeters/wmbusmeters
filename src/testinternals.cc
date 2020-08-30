/*
 Copyright (C) 2018-2019 Fredrik Öhrström

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

#include"aescmac.h"
#include"cmdline.h"
#include"config.h"
#include"meters.h"
#include"printer.h"
#include"serial.h"
#include"util.h"
#include"wmbus.h"
#include"dvparser.h"

#include<string.h>

using namespace std;

int test_crc();
int test_dvparser();
int test_linkmodes();
void test_ids();
void test_kdf();
void test_periods();
void test_devices();

int main(int argc, char **argv)
{
    if (argc > 1) {
        // Supply --debug (oh well, anything works) to enable debug mode.
        debugEnabled(true);
    }
    onExit([](){});

    test_crc();
    test_dvparser();
    test_linkmodes();
    test_ids();
    test_kdf();
    test_periods();
    test_devices();
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

int test_parse(const char *data, std::map<std::string,std::pair<int,DVEntry>> *values, int testnr)
{
    debug("\n\nTest nr %d......\n\n", testnr);
    bool b;
    Telegram t;
    vector<uchar> databytes;
    hex2bin(data, &databytes);
    vector<uchar>::iterator i = databytes.begin();

    b = parseDV(&t, databytes, i, databytes.size(), values);

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
    bool b = extractDVstring(&values,
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
    map<string,pair<int,DVEntry>> values;

    int testnr = 1;
    test_parse("2F 2F 0B 13 56 34 12 8B 82 00 93 3E 67 45 23 0D FD 10 0A 30 31 32 33 34 35 36 37 38 39 0F 88 2F", &values, testnr);
    test_double(values, "0B13", 123.456, testnr);
    test_double(values, "8B8200933E", 234.567, testnr);
    test_string(values, "0DFD10", "30313233343536373839", testnr);

    testnr++;
    values.clear();
    test_parse("82046C 5f1C", &values, testnr);
    test_date(values, "82046C", "2010-12-31 00:00:00", testnr); // 2010-dec-31

    testnr++;
    values.clear();
    test_parse("0C1348550000426CE1F14C130000000082046C21298C0413330000008D04931E3A3CFE3300000033000000330000003300000033000000330000003300000033000000330000003300000033000000330000004300000034180000046D0D0B5C2B03FD6C5E150082206C5C290BFD0F0200018C4079678885238310FD3100000082106C01018110FD610002FD66020002FD170000", &values, testnr);
    test_double(values, "0C13", 5.548, testnr);
    test_date(values, "426C", "2127-01-01 00:00:00", testnr); // 2127-jan-1
    test_date(values, "82106C", "2000-01-01 00:00:00", testnr); // 2000-jan-1

    testnr++;
    values.clear();
    test_parse("426C FE04", &values, testnr);
    test_date(values, "426C", "2007-04-30 00:00:00", testnr); // 2010-dec-31
    return 0;
}


int test_linkmodes()
{
    LinkModeCalculationResult lmcr;
    auto manager = createSerialCommunicationManager(0, 0, false);

    auto serial1 = manager->createSerialDeviceSimulator();
    auto serial2 = manager->createSerialDeviceSimulator();
    auto serial3 = manager->createSerialDeviceSimulator();
    auto serial4 = manager->createSerialDeviceSimulator();

    vector<string> no_meter_shells, no_meter_jsons;

    unique_ptr<WMBus> wmbus_im871a = openIM871A("", manager.get(), std::move(serial1));
    unique_ptr<WMBus> wmbus_amb8465 = openAMB8465("", manager.get(), std::move(serial2));
    unique_ptr<WMBus> wmbus_rtlwmbus = openRTLWMBUS("", manager.get(), [](){}, std::move(serial3));
    unique_ptr<WMBus> wmbus_rawtty = openRawTTY("", 0, manager.get(), std::move(serial4));

    Configuration nometers_config;
    // Check that if no meters are supplied then you must set a link mode.
    nometers_config.link_mode_configured = false;
    lmcr = calculateLinkModes(&nometers_config, wmbus_im871a.get());
    if (lmcr.type != LinkModeCalculationResultType::NoMetersMustSupplyModes)
    {
        printf("ERROR! Expected failure due to automatic deduction! Got instead:\n%s\n", lmcr.msg.c_str());
    }
    debug("test0 OK\n\n");

    nometers_config.link_mode_configured = true;
    nometers_config.listen_to_link_modes.addLinkMode(LinkMode::T1);
    lmcr = calculateLinkModes(&nometers_config, wmbus_im871a.get());
    if (lmcr.type != LinkModeCalculationResultType::Success)
    {
        printf("ERROR! Expected succcess! Got instead:\n%s\n", lmcr.msg.c_str());
    }
    debug("test0.0 OK\n\n");

    Configuration apator_config;
    string apator162 = "apator162";
    apator_config.meters.push_back(MeterInfo("m1", apator162, "12345678", "",
                                             toMeterLinkModeSet(apator162),
                                             no_meter_shells,
                                             no_meter_jsons));

    // Check that if no explicit link modes are provided to apator162, then
    // automatic deduction will fail, since apator162 can be configured to transmit
    // either C1 or T1 telegrams.
    apator_config.link_mode_configured = false;
    lmcr = calculateLinkModes(&apator_config, wmbus_im871a.get());
    if (lmcr.type != LinkModeCalculationResultType::AutomaticDeductionFailed)
    {
        printf("ERROR! Expected failure due to automatic deduction! Got instead:\n%s\n", lmcr.msg.c_str());
    }
    debug("test1 OK\n\n");

    // Check that if we supply the link mode T1 when using an apator162, then
    // automatic deduction will succeeed.
    apator_config.link_mode_configured = true;
    apator_config.listen_to_link_modes = LinkModeSet();
    apator_config.listen_to_link_modes.addLinkMode(LinkMode::T1);
    apator_config.listen_to_link_modes.addLinkMode(LinkMode::C1);
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
    multical21_and_supercom587_config.meters.push_back(MeterInfo("m1", multical21, "12345678", "",
                                                                 toMeterLinkModeSet(multical21),
                                                                 no_meter_shells,
                                                                 no_meter_jsons));
    multical21_and_supercom587_config.meters.push_back(MeterInfo("m2", supercom587, "12345678", "",
                                                                 toMeterLinkModeSet(supercom587),
                                                                 no_meter_shells,
                                                                 no_meter_jsons));

    // Check that meters that transmit on two different link modes cannot be listened to
    // at the same time using im871a.
    multical21_and_supercom587_config.link_mode_configured = false;
    lmcr = calculateLinkModes(&multical21_and_supercom587_config, wmbus_im871a.get());
    if (lmcr.type != LinkModeCalculationResultType::AutomaticDeductionFailed)
    {
        printf("ERROR! Expected failure due to automatic deduction! Got instead:\n%s\n", lmcr.msg.c_str());
    }
    debug("test4 OK\n\n");

    // Explicitly set T1
    multical21_and_supercom587_config.link_mode_configured = true;
    multical21_and_supercom587_config.listen_to_link_modes = LinkModeSet();
    multical21_and_supercom587_config.listen_to_link_modes.addLinkMode(LinkMode::T1);
    lmcr = calculateLinkModes(&multical21_and_supercom587_config, wmbus_im871a.get());
    if (lmcr.type != LinkModeCalculationResultType::MightMissTelegrams)
    {
        printf("ERROR! Expected might miss telegrams! Got instead:\n%s\n", lmcr.msg.c_str());
    }
    debug("test5 OK\n\n");

    // Explicitly set N1a, but the meters transmit on C1 and T1.
    multical21_and_supercom587_config.link_mode_configured = true;
    multical21_and_supercom587_config.listen_to_link_modes = LinkModeSet();
    multical21_and_supercom587_config.listen_to_link_modes.addLinkMode(LinkMode::N1a);
    lmcr = calculateLinkModes(&multical21_and_supercom587_config, wmbus_im871a.get());
    if (lmcr.type != LinkModeCalculationResultType::MightMissTelegrams)
    {
        printf("ERROR! Expected no meter can be heard! Got instead:\n%s\n", lmcr.msg.c_str());
    }
    debug("test6 OK\n\n");

    // Explicitly set N1a, but it is an amber dongle.
    multical21_and_supercom587_config.link_mode_configured = true;
    multical21_and_supercom587_config.listen_to_link_modes = LinkModeSet();
    multical21_and_supercom587_config.listen_to_link_modes.addLinkMode(LinkMode::N1a);
    lmcr = calculateLinkModes(&multical21_and_supercom587_config, wmbus_amb8465.get());
    if (lmcr.type != LinkModeCalculationResultType::DongleCannotListenTo)
    {
        printf("ERROR! Expected dongle cannot listen to! Got instead:\n%s\n", lmcr.msg.c_str());
    }
    debug("test7 OK\n\n");

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

void test_does_id_match_expression(string id, string mes, bool expected)
{
    vector<string> expressions = splitMatchExpressions(mes);
    bool b = doesIdMatchExpressions(id, expressions);
    if (b == expected) return;
    if (expected == true)
    {
        printf("ERROR! Expected \"%s\" to match \"%s\" ! But it did not!\n", id.c_str(), mes.c_str());
    }
    else
    {
        printf("ERROR! Expected \"%s\" to NOT match \"%s\" ! But it did!\n", id.c_str(), mes.c_str());
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

    test_does_id_match_expression("12345678", "12345678", true);
    test_does_id_match_expression("12345678", "*", true);
    test_does_id_match_expression("12345678", "2*", false);

    test_does_id_match_expression("12345678", "*,!2*", true);

    test_does_id_match_expression("22222222", "22*,!22222222", false);
    test_does_id_match_expression("22222223", "22*,!22222222", true);
    test_does_id_match_expression("22222223", "*,!22*", false);
    test_does_id_match_expression("12333333", "123*,!1234*,!1235*,!1236*", true);
    test_does_id_match_expression("12366666", "123*,!1234*,!1235*,!1236*", false);
    test_does_id_match_expression("11223344", "22*,33*,44*,55*", false);
    test_does_id_match_expression("55223344", "22*,33*,44*,55*", true);

    test_does_id_match_expression("78563413", "78563412,78563413", true);
    test_does_id_match_expression("78563413", "*,!00156327,!00048713", true);
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

void testd(string arg, string xf, string xs, string xl, bool xok)
{
    Device d;
    bool ok = isPossibleDevice(arg, &d);
    if (ok != xok)
    {
        printf("ERROR in device parsing \"%s\"\n", arg.c_str());
        return;
    }
    if (ok == false) return;
    if (xf != d.file ||
        xs != d.suffix ||
        xl != d.linkmodes)
    {
        printf("ERROR in device parsing parts \"%s\"\n", arg.c_str());
    }
}

void test_devices()
{
    testd("auto", "auto", "", "", true);
    testd("/dev/ttyUSB0:9600", "/dev/ttyUSB0", "9600", "", true);
    testd("auto:gurka", "", "", "", false);
}
