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

struct MeterMultical21 : public Meter {
    MeterMultical21(WMBus *bus, const char *name, const char *id, const char *key);

    string id();
    string name();
    float totalWaterConsumption();
    string datetimeOfUpdate();
    void onUpdate(function<void()> cb);
    
private:
    void handleTelegram(Telegram*t);
    float processContent(vector<uchar> &d);
    
    float total_water_consumption_;
    time_t datetime_of_update_;
    
    string name_;
    vector<uchar> id_;
    vector<uchar> key_;
    WMBus *bus_;
    function<void()> on_update_;
};

MeterMultical21::MeterMultical21(WMBus *bus, const char *name, const char *id, const char *key) :
    total_water_consumption_(0), name_(name), bus_(bus)
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

void MeterMultical21::onUpdate(function<void()> cb)
{
    on_update_ = cb;
}

float MeterMultical21::totalWaterConsumption()
{
    return total_water_consumption_;
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
        
        verbose("Ignoring meter %02x %02x %02x %02x \n",
                t->a_field_address[3], t->a_field_address[2], t->a_field_address[1], t->a_field_address[0]);
        return;
    } else {
        verbose("Update from meter %02x %02x %02x %02x!\n",
               t->a_field_address[3], t->a_field_address[2], t->a_field_address[1], t->a_field_address[0]);
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

    uchar back[16];
    AES_ECB_encrypt(iv, &key_[0], back, 16);

    uchar decrypt[16];
    xorit(back, &content[0], decrypt, 16);

    vector<uchar> dec(decrypt, decrypt+16);
    string answer = bin2hex(dec);

    // The total water consumption data is inside the first 16 bytes for
    // both short and full frames. So this code does not even bother
    // to decrypt later bytes! Its aes-ctr mode, so add 1 to IV to
    // decrypt next 16 bytes, add 2 to IV to decrypt the next next 16 bytes
    // and so on and so forth...
    //
    // There is supposedly more interesting data, like the water temperature
    // and warning flags, like leak, burst, dry etc.
    verbose("Decrypted >%s<\n", answer.c_str());

    total_water_consumption_ = processContent(dec);
    datetime_of_update_ = time(NULL);

    if (on_update_) on_update_();
}

float MeterMultical21::processContent(vector<uchar> &c) {
    float consumption = 0.0;
    /* int crc0 = c[0];
       int crc1 = c[1]; */
    int frame_type = c[2];

    if (frame_type == 0x79) {
        verbose("Short frame\n");
        /*        int ecrc0 = c[3];
        int ecrc1 = c[4];
        int ecrc2 = c[5];
        int ecrc3 = c[6];
        int rec1val0 = c[7];
        int rec1val1 = c[8];*/
        int rec2val0 = c[9];
        int rec2val1 = c[10];
        int rec2val2 = c[11];
        int rec2val3 = c[12];
        /*        int rec3val0 = c[13];
        int rec3val1 = c[14];
        int rec3val2 = c[15];
        int rec3val3 = c[16];*/
        int tmp = rec2val3*256*256*256 + rec2val2*256*256 + rec2val1*256 + rec2val0;
        // This scale factor 1000, is dependent on the meter device.
        // The full frame has info on this, here it is hardcoded to
        // the domestic meter setting.
        consumption = ((float)tmp) / ((float)1000);
    } else
    if (frame_type == 0x78) {
        verbose("Full frame\n");
        /*        int rec1dif = c[3]; 
        int rec1vif = c[4];
        int rec1vife = c[5];
        int rec1val0 = c[6];
        int rec1val1 = c[7];
        */
        /*int rec2dif = c[8]; 
          int rec2vif = c[9];*/
        int rec2val0 = c[10];
        int rec2val1 = c[11];
        int rec2val2 = c[12];
        int rec2val3 = c[13];

        /*
        int rec3dif = c[14]; 
        int rec3vif = c[15];
        int rec3val0 = c[16];
        int rec3val1 = c[17];
        int rec3val2 = c[18];
        int rec3val3 = c[19];
        */
        int tmp = rec2val3*256*256*256 + rec2val2*256*256 + rec2val1*256 + rec2val0;
        consumption = ((float)tmp) / ((float)1000);        
    } else {
        printf("Unknown frame %02x\n", frame_type);
    }
     
    return consumption;
}
