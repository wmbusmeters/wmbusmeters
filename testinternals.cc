/*
 Copyright (C) 2018 Fredrik Öhrström

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

#include"cmdline.h"
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

int main(int argc, char **argv)
{
    if (argc > 1) {
        // Supply --debug (oh well, anything works) to enable debug mode.
        debugEnabled(true);
    }
    test_crc();
    test_dvparser();
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

int test_parse(const char *data, std::map<std::string,std::pair<int,std::string>> *values, int testnr)
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

void test_double(map<string,pair<int,string>> &values, const char *key, double v, int testnr)
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

void test_string(map<string,pair<int,string>> &values, const char *key, const char *v, int testnr)
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

void test_date(map<string,pair<int,string>> &values, const char *key, time_t v, int testnr)
{
    int offset;
    time_t value;
    bool b = extractDVdate(&values,
                             key,
                             &offset,
                             &value);
    if (!b || value != v) {
        string date_expected = asctime(localtime(&v));
        string date_got = asctime(localtime(&value));
        fprintf(stderr, "Error in dvparser testnr %d:\ngot (%zu) %sbut expected (%zu) %sfor key %s\n\n", testnr, value, date_got.c_str(), v, date_expected.c_str(), key);
    }
}

int test_dvparser()
{
    map<string,pair<int,string>> values;

    int testnr = 1;
    test_parse("2F 2F 0B 13 56 34 12 8B 82 00 93 3E 67 45 23 0D FD 10 0A 30 31 32 33 34 35 36 37 38 39 0F 88 2F", &values, testnr);
    test_double(values, "0B13", 123.456, testnr);
    test_double(values, "8B8200933E", 234.567, testnr);
    test_string(values, "0DFD10", "30313233343536373839", testnr);

    testnr++;
    values.clear();
    test_parse("0C1348550000426CE1F14C130000000082046C21298C0413330000008D04931E3A3CFE3300000033000000330000003300000033000000330000003300000033000000330000003300000033000000330000004300000034180000046D0D0B5C2B03FD6C5E150082206C5C290BFD0F0200018C4079678885238310FD3100000082106C01018110FD610002FD66020002FD170000", &values, testnr);
    test_double(values, "0C13", 5.548, testnr);
    test_date(values, "426C", 4954431600, testnr);
    test_date(values, "82106C", 946681200, testnr);
    return 0;
}
