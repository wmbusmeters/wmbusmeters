
/* CRC experiments. Some day, the crc checks will actually work. */

#include<stdio.h>
#include<stdint.h>

typedef unsigned char uchar;

#define CRC16_DNP 0x3D65

uint16_t crc16_dnp_per_byte(uint16_t crc, uchar b)
{
    unsigned char i;

    for (i = 0; i < 8; i++) {

        if (((crc & 0x8000) >> 8) ^ (b & 0x80)){
            crc = (crc << 1)  ^ CRC16_DNP;
        } else {
            crc = (crc << 1);
        }

        b <<= 1;
    }

    return crc;
}

uint16_t crc16_dnp(uchar *data, size_t len)
{
    uint16_t crc = 0x0000;

    for (size_t i=0; i<len; ++i) {
        crc = crc16_dnp_per_byte(crc, data[i]);
    }

    return (~crc);
}


int main() {
    unsigned char data[4];
    data[0] = 0x01;
    data[1] = 0xfd;
    data[2] = 0x1f;
    data[3] = 0x01;

    // ./reveng/reveng -m CRC-16/EN-13757 -c 01FD1F01
    // cc22
    int crc = crc16_dnp(data, 4);
    int crc_msb = (crc >> 8)&0xff;
    int crc_lsb = crc&0xff;
    printf("%02x %02x\n", crc_msb, crc_lsb);

    if (crc != 0xcc22) {
        printf("ERROR!\n");
    }
    data[3] = 0x00;

    // ./reveng/reveng -m CRC-16/EN-13757 -c 01FD1F00
    // f147
    crc = crc16_dnp(data, 4);
    crc_msb = (crc >> 8)&0xff;
    crc_lsb = crc&0xff;
    printf("%02x %02x\n", crc_msb, crc_lsb);

    if (crc != 0xf147) {
        printf("ERROR!\n");
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

    // ./reveng/reveng -m CRC-16/EN-13757 -c EE449ACE010000802307
    // aabc
    crc = crc16_dnp(block, 10);
    crc_msb = (crc >> 8)&0xff;
    crc_lsb = crc&0xff;
    printf("%02x %02x\n", crc_msb, crc_lsb);

    if (crc != 0xaabc) {
        printf("ERROR!\n");
    }

    // width=16 poly=0x3d65 init=0x0000 refin=false refout=false xorout=0xffff check=0xc2b7 residue=0xa366 name="CRC-16/EN-13757"
    block[0]='1';
    block[1]='2';
    block[2]='3';
    block[3]='4';
    block[4]='5';
    block[5]='6';
    block[6]='7';
    block[7]='8';
    block[8]='9';

    crc = crc16_dnp(block, 9);
    crc_msb = (crc >> 8)&0xff;
    crc_lsb = crc&0xff;
    printf("%02x %02x\n", crc_msb, crc_lsb);

    if (crc != 0xc2b7) {
        printf("ERROR!\n");
    }

}
