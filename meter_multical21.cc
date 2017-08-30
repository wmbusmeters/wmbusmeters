// Copyright (c) 2017 Fredrik Öhrström
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include"aes.h"
#include"meters.h"
#include"wmbus.h"
#include"util.h"

#include<memory.h>
#include<stdio.h>
#include<string>
#include<time.h>
#include<vector>

using namespace std;

#define INFO_CODE_DRY 0x01
#define INFO_CODE_DRY_SHIFT (4+0)

#define INFO_CODE_REVERSE 0x02
#define INFO_CODE_REVERSE_SHIFT (4+3)

#define INFO_CODE_LEAK 0x04
#define INFO_CODE_LEAK_SHIFT (4+6)

#define INFO_CODE_BURST 0x08
#define INFO_CODE_BURST_SHIFT (4+9) 

struct MeterMultical21 : public Meter {
    MeterMultical21(WMBus *bus, const char *name, const char *id, const char *key);

    string id();
    string name();
    float totalWaterConsumption();
    float targetWaterConsumption();
    string statusHumanReadable();
    
    string datetimeOfUpdate();
    void onUpdate(function<void(Meter*)> cb);
    
private:
    void handleTelegram(Telegram*t);
    void processContent(vector<uchar> &d);
    string decodeTime(int time);

    int info_codes_; 
    float total_water_consumption_;
    float target_volume_; 
    time_t datetime_of_update_;
    
    string name_;
    vector<uchar> id_;
    vector<uchar> key_;
    WMBus *bus_;
    function<void(Meter*)> on_update_;

};

MeterMultical21::MeterMultical21(WMBus *bus, const char *name, const char *id, const char *key) :
    info_codes_(0), total_water_consumption_(0), target_volume_(0), name_(name), bus_(bus)
{
    hex2bin(id, &id_);
    hex2bin(key, &key_);
    bus_->onTelegram(calll(this,handleTelegram,Telegram*));
}

string MeterMultical21::id()
{
    return bin2hex(id_);
}

string MeterMultical21::name()
{
    return name_;
}

void MeterMultical21::onUpdate(function<void(Meter*)> cb)
{
    on_update_ = cb;
}

float MeterMultical21::totalWaterConsumption()
{
    return total_water_consumption_;
}

float MeterMultical21::targetWaterConsumption()
{
    return target_volume_;
}

string MeterMultical21::datetimeOfUpdate()
{
    char datetime[20];
    memset(datetime, 0, sizeof(datetime));
    strftime(datetime, 20, "%Y-%m-%d %H:%M.%S", localtime(&datetime_of_update_));
    return string(datetime);
}

Meter *createMultical21(WMBus *bus, const char *name, const char *id, const char *key) {
    return new MeterMultical21(bus,name,id,key);
}

void MeterMultical21::handleTelegram(Telegram *t) {

    if (t->a_field_address[3] != id_[3] ||
        t->a_field_address[2] != id_[2] ||
        t->a_field_address[1] != id_[1] ||
        t->a_field_address[0] != id_[0]) {
        
        verbose("Meter %s ignores message with id %02x%02x%02x%02x \n",
                name_.c_str(),
                t->a_field_address[0], t->a_field_address[1], t->a_field_address[2], t->a_field_address[3]);
        return;
    } else {
        verbose("Meter %s receives update with id %02x%02x%02x%02x!\n",
                name_.c_str(),
               t->a_field_address[0], t->a_field_address[1], t->a_field_address[2], t->a_field_address[3]);
    }
       
    int cc_field = t->payload[0];
    //int acc = t->payload[1];

    uchar sn[4];
    sn[0] = t->payload[2];
    sn[1] = t->payload[3];
    sn[2] = t->payload[4];
    sn[3] = t->payload[5];

    vector<uchar> content;
    content.insert(content.end(), t->payload.begin()+6, t->payload.end());
    while (content.size() < 16) { content.push_back(0); }
    
    uchar iv[16];
    int i=0;
    // M-field
    iv[i++] = t->m_field>>8; iv[i++] = t->m_field&255;
    // A-field
    for (int j=0; j<6; ++j) { iv[i++] = t->a_field[j]; }
    // CC-field
    iv[i++] = cc_field;
    // SN-field
    for (int j=0; j<4; ++j) { iv[i++] = sn[j]; }
    // FN
    iv[i++] = 0; iv[i++] = 0;
    // BC
    iv[i++] = 0;

    vector<uchar> ivv(iv, iv+16);
    string s = bin2hex(ivv);
    verbose("IV %s\n", s.c_str());

    uchar xordata[16];
    AES_ECB_encrypt(iv, &key_[0], xordata, 16);

    uchar decrypt[16];
    xorit(xordata, &content[0], decrypt, 16);

    vector<uchar> dec(decrypt, decrypt+16);
    string answer = bin2hex(dec);
    verbose("Decrypted >%s<\n", answer.c_str());

    if (content.size() > 22) {
        fprintf(stderr, "Received too many bytes of content from a Multical21 meter!\n"
                "Got %zu bytes, expected at most 22.\n", content.size());
    }
    if (content.size() > 16) {
        // Yay! Lets decrypt a second block. Full frame content is 22 bytes.
        // So a second block should enough for everyone!
        size_t remaining = content.size()-16;
        if (remaining > 16) remaining = 16; // Should not happen.
        
        incrementIV(iv, sizeof(iv));
        vector<uchar> ivv2(iv, iv+16);
        string s2 = bin2hex(ivv2);
        verbose("IV+1 %s\n", s2.c_str());
        
        AES_ECB_encrypt(iv, &key_[0], xordata, 16);
        
        xorit(xordata, &content[16], decrypt, remaining);
        
        vector<uchar> dec2(decrypt, decrypt+remaining);
        string answer2 = bin2hex(dec2);
        verbose("Decrypted second block >%s<\n", answer2.c_str());

        // Append the second decrypted block to the first.
        dec.insert(dec.end(), dec2.begin(), dec2.end());
    }

    processContent(dec);
    datetime_of_update_ = time(NULL);

    if (on_update_) on_update_(this);
}

void MeterMultical21::processContent(vector<uchar> &c) {
    /* int crc0 = c[0];
       int crc1 = c[1]; */
    int frame_type = c[2];

    if (frame_type == 0x79) {
        verbose("Short frame %d bytes\n", c.size());
        if (c.size() != 16) {
            fprintf(stderr, "Warning! Unexpected length of frame %zu. Expected 16 bytes!\n", c.size());
        }
        /*int ecrc0 = c[3];
        int ecrc1 = c[4];
        int ecrc2 = c[5];
        int ecrc3 = c[6];*/
        int rec1val0 = c[7];
        int rec1val1 = c[8];
        int rec2val0 = c[9];
        int rec2val1 = c[10];
        int rec2val2 = c[11];
        int rec2val3 = c[12];
        int rec3val0 = c[13];
        int rec3val1 = c[14];
        int rec3val2 = c[15];

        info_codes_ = rec1val1*256+rec1val0;
        verbose("short rec1 %02x %02x info codes\n", rec1val1, rec1val0);

        int consumption_raw = rec2val3*256*256*256 + rec2val2*256*256 + rec2val1*256 + rec2val0;
        verbose("short rec2 %02x %02x %02x %02x = %d total consumption\n", rec2val3, rec2val2, rec2val1, rec2val0, consumption_raw);
        // The dif=0x04 vif=0x13 means current volume with scale factor .001
        total_water_consumption_ = ((float)consumption_raw) / ((float)1000);

        // The short frame target volume supplies two low bytes, the remaining two hi bytes are picked from rec2.
        int target_volume_raw = rec2val3*256*256*256 + rec2val2*256*256 + rec3val1*256 + rec3val0;
        verbose("short rec3 (%02x %02x) %02x %02x = %d target volume\n", rec2val3, rec2val2, rec3val1, rec3val0, target_volume_raw); 
        target_volume_ = ((float)target_volume_raw) / ((float)1000);

        // Which leaves one unknown byte.
        verbose("short rec3 %02x = unknown\n", rec3val2);
    } else
    if (frame_type == 0x78) {
        verbose("Full frame %d bytes\n", c.size());
        if (c.size() != 22) {
            fprintf(stderr, "Warning! Unexpected length of frame %zu. Expected 22 bytes!\n", c.size());
        }
        int rec1dif = c[3]; 
        int rec1vif = c[4];
        int rec1vife = c[5];

        int rec1val0 = c[6];
        int rec1val1 = c[7];

        int rec2dif = c[8]; 
        int rec2vif = c[9];
        int rec2val0 = c[10];
        int rec2val1 = c[11];
        int rec2val2 = c[12];
        int rec2val3 = c[13];

        int rec3dif = c[14]; 
        int rec3vif = c[15];
        int rec3val0 = c[16];
        int rec3val1 = c[17];
        int rec3val2 = c[18];
        int rec3val3 = c[19];

        // There are two more bytes in the data. Unknown purpose.
        int rec4val0 = c[20];
        int rec4val1 = c[21];

        if (rec1dif != 0x02 || rec1vif != 0xff || rec1vife != 0x20 ) {
            fprintf(stderr, "Warning unexpected field! Expected info codes\n"
                    "with dif=0x02 vif=0xff vife=0x20 but got dif=%02x vif=%02x vife=%02x\n", rec1dif, rec1vif, rec1vife);
        }

        info_codes_ = rec1val1*256+rec1val0;
        verbose("full rec1 dif=%02x vif=%02x vife=%02x\n", rec1dif, rec1vif, rec1vife);                
        verbose("full rec1 %02x %02x info codes\n", rec1val1, rec1val0);

        if (rec2dif != 0x04 || rec2vif != 0x13) {
            fprintf(stderr, "Warning unexpected field! Expected current volume\n"
                    "with dif=0x04 vif=0x13 but got dif=%02x vif=%02x\n", rec2dif, rec2vif);
        }

        int consumption_raw = rec2val3*256*256*256 + rec2val2*256*256 + rec2val1*256 + rec2val0;
        verbose("full rec2 dif=%02x vif=%02x\n", rec2dif, rec2vif);
        verbose("full rec2 %02x %02x %02x %02x = %d total consumption\n", rec2val3, rec2val2, rec2val1, rec2val0, consumption_raw);
        // The dif=0x04 vif=0x13 means current volume with scale factor .001
        total_water_consumption_ = ((float)consumption_raw) / ((float)1000);        
        
        if (rec3dif != 0x44 || rec3vif != 0x13) {
            fprintf(stderr, "Warning unexpecte field! Expected target volume (ie volume recorded on first day of month)\n"
                    "with dif=0x44 vif=0x13 but got dif=%02x vif=%02x\n", rec3dif, rec3vif);
        }
        int target_volume_raw = rec3val3*256*256*256 + rec3val2*256*256 + rec3val1*256 + rec3val0;
        verbose("full rec3 dif=%02x vif=%02x\n", rec3dif, rec3vif);        
        verbose("full rec3 %02x %02x %02x %02x = %d target volume\n", rec3val3, rec3val2, rec3val1, rec3val0, target_volume_raw); 

        target_volume_ = ((float)target_volume_raw) / ((float)1000);

        verbose("full rec4 %02x %02x = unknown\n", rec4val1, rec4val0);
    } else {
        fprintf(stderr, "Unknown frame %02x\n", frame_type);
    }
}

string MeterMultical21::statusHumanReadable() {
    string s;
    bool dry = info_codes_ & INFO_CODE_DRY;
    int time_dry = (info_codes_ >> INFO_CODE_DRY_SHIFT) & 7;
    if (dry || time_dry) {
        if (dry) s.append("DRY");
        s.append("(dry ");
        s.append(decodeTime(time_dry));
        s.append(")");
    }

    bool reversed = info_codes_ & INFO_CODE_REVERSE;
    int time_reversed = (info_codes_ >> INFO_CODE_REVERSE_SHIFT) & 7;
    if (reversed || time_reversed) {
        if (dry) s.append("REVERSED");
        s.append("(rev ");
        s.append(decodeTime(time_reversed));
        s.append(") ");
    }

    bool leak = info_codes_ & INFO_CODE_LEAK;
    int time_leak = (info_codes_ >> INFO_CODE_LEAK_SHIFT) & 7;
    if (leak || time_leak) {
        if (dry) s.append("LEAK");
        s.append("(leak ");
        s.append(decodeTime(time_leak));
        s.append(") ");
    }

    bool burst = info_codes_ & INFO_CODE_BURST;
    int time_burst = (info_codes_ >> INFO_CODE_BURST_SHIFT) & 7;
    if (burst || time_burst) {
        if (dry) s.append("BURST");
        s.append("(burst ");
        s.append(decodeTime(time_burst));
        s.append(") ");
    }
    return s;
}

string MeterMultical21::decodeTime(int time) {
    if (time>7) {
        fprintf(stderr, "Error when decoding time %d\n", time);
    }
    switch (time) {
    case 0:
        return "0 hours";
    case 1:
        return "1-8 hours";
    case 2:
        return "9-24 hours";
    case 3:
        return "2-3 days";
    case 4:
        return "4-7 days";
    case 5:
        return "8-14 days";
    case 6:
        return "15-21 days";
    case 7:
        return "22-31 days";
    default:
        return "?";
    }
}
