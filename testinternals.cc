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

#include<string.h>

using namespace std;

int test_crc();

int main(int argc, char **argv)
{
    return test_crc();
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
