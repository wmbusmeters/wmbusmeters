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
    // Total water counted through the meter
    float totalWaterConsumption();
    bool  hasTotalWaterConsumption();

    // Meter sends target water consumption or max flow, depending on meter configuration
    // We can see which was sent inside the wmbus message!
    // Target water consumption: The total consumption at the start of the previous 30 day period.
    float targetWaterConsumption();
    bool  hasTargetWaterConsumption();
    // Max flow during last month or last 24 hours depending on meter configuration.
    float maxFlow();
    bool  hasMaxFlow();

    // statusHumanReadable: DRY,REVERSED,LEAK,BURST if that status is detected right now, followed by
    //                      (dry 15-21 days) which means that, even it DRY is not active right now,
    //                      DRY has been active for 15-21 days during the last 30 days.
    string statusHumanReadable();
    string status();
    string timeDry();
    string timeReversed();
    string timeLeaking();
    string timeBursting();

    string datetimeOfUpdateHumanReadable();
    string datetimeOfUpdateRobot();
    void onUpdate(function<void(Meter*)> cb);
    int numUpdates();

private:
    void handleTelegram(Telegram*t);
    void processContent(vector<uchar> &d);
    string decodeTime(int time);

    int info_codes_;
    float total_water_consumption_;
    bool has_total_water_consumption_;
    float target_volume_;
    bool has_target_volume_;
    float max_flow_;
    bool has_max_flow_;

    time_t datetime_of_update_;

    string name_;
    vector<uchar> id_;
    vector<uchar> key_;
    WMBus *bus_;
    vector<function<void(Meter*)>> on_update_;
    int num_updates_;
    bool use_aes_;
};

MeterMultical21::MeterMultical21(WMBus *bus, const char *name, const char *id, const char *key) :
    info_codes_(0), total_water_consumption_(0), has_total_water_consumption_(false),
    target_volume_(0), has_target_volume_(false),
    max_flow_(0), has_max_flow_(false), name_(name), bus_(bus), num_updates_(0), use_aes_(true)
{
    hex2bin(id, &id_);
    if (strlen(key) == 0) {
	use_aes_ = false;
    } else {
	hex2bin(key, &key_);
    }
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
    on_update_.push_back(cb);
}

int MeterMultical21::numUpdates()
{
    return num_updates_;
}

float MeterMultical21::totalWaterConsumption()
{
    return total_water_consumption_;
}

bool MeterMultical21::hasTotalWaterConsumption()
{
    return has_total_water_consumption_;
}

float MeterMultical21::targetWaterConsumption()
{
    return target_volume_;
}

bool MeterMultical21::hasTargetWaterConsumption()
{
    return has_target_volume_;
}

float MeterMultical21::maxFlow()
{
    return max_flow_;
}

bool MeterMultical21::hasMaxFlow()
{
    return has_max_flow_;
}

string MeterMultical21::datetimeOfUpdateHumanReadable()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));
    strftime(datetime, 20, "%Y-%m-%d %H:%M.%S", localtime(&datetime_of_update_));
    return string(datetime);
}

string MeterMultical21::datetimeOfUpdateRobot()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));
    // This is the date time in the Greenwich timezone (Zulu time), dont get surprised!
    strftime(datetime, sizeof(datetime), "%FT%TZ", gmtime(&datetime_of_update_));
    return string(datetime);
}

Meter *createMultical21(WMBus *bus, const char *name, const char *id, const char *key) {
    return new MeterMultical21(bus,name,id,key);
}

void MeterMultical21::handleTelegram(Telegram *t) {

    if (t->m_field != MANUFACTURER_KAM ||
        t->a_field_address[3] != id_[3] ||
        t->a_field_address[2] != id_[2] ||
	    t->a_field_address[1] != id_[1] ||
        t->a_field_address[0] != id_[0])
    {
        // This telegram is not intended for this meter.
        return;
    }

    verbose("(multical21) %s %02x%02x%02x%02x ",
            name_.c_str(),
            t->a_field_address[0], t->a_field_address[1], t->a_field_address[2],
            t->a_field_address[3]);

    // This is part of the wmbus protocol, should be moved to wmbus source files!
    int cc_field = t->payload[0];
    verbose("CC-field=%02x ( ", cc_field);
    if (cc_field & CC_B_BIDIRECTIONAL_BIT) verbose("bidir ");
    if (cc_field & CC_RD_RESPONSE_DELAY_BIT) verbose("fast_res ");
    else verbose("slow_res ");
    if (cc_field & CC_S_SYNCH_FRAME_BIT) verbose("synch ");
    if (cc_field & CC_R_RELAYED_BIT) verbose("relayed "); // Relayed by a repeater
    if (cc_field & CC_P_HIGH_PRIO_BIT) verbose("prio ");
    verbose(") ");

    int acc = t->payload[1];
    verbose("ACC-field=%02x ", acc);

    uchar sn[4];
    sn[0] = t->payload[2];
    sn[1] = t->payload[3];
    sn[2] = t->payload[4];
    sn[3] = t->payload[5];

    verbose("SN=%02x%02x%02x%02x encrypted=", sn[3], sn[2], sn[1], sn[0]);
    // Here is a bug, since it always reports no encryption, but the multicals21
    // so far have all have encryption enabled.
    if ((sn[3] & SN_ENC_BITS) == 0) verbose("no");
    else if ((sn[3] & SN_ENC_BITS) == 0x40) verbose("yes");
    else verbose("? %d\n", sn[3] & SN_ENC_BITS);
    verbose("\n");

    // The content begins with the Payload CRC at offset 6.
    vector<uchar> content;
    content.insert(content.end(), t->payload.begin()+6, t->payload.end());
    size_t remaining = content.size();
    if (remaining > 16) remaining = 16;

    uchar iv[16];
    int i=0;
    // M-field
    iv[i++] = t->m_field&255; iv[i++] = t->m_field>>8;
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

    if (use_aes_) {
	vector<uchar> ivv(iv, iv+16);
	string s = bin2hex(ivv);
        debug("(multical21) IV   %s\n", s.c_str());

	uchar xordata[16];
	AES_ECB_encrypt(iv, &key_[0], xordata, 16);

	uchar decrypt[16];
	xorit(xordata, &content[0], decrypt, remaining);

	vector<uchar> dec(decrypt, decrypt+remaining);
	debugPayload("(multical21) decrypted", dec);

	if (content.size() > 22) {
	    warning("(multical21) warning: Received too many bytes of content! "
		    "Got %zu bytes, expected at most 22.\n", content.size());
	}
	if (content.size() > 16) {
	    // Yay! Lets decrypt a second block. Full frame content is 22 bytes.
	    // So a second block should enough for everyone!
            remaining = content.size()-16;
	    if (remaining > 16) remaining = 16; // Should not happen.

	    incrementIV(iv, sizeof(iv));
	    vector<uchar> ivv2(iv, iv+16);
	    string s2 = bin2hex(ivv2);
	    debug("(multical21) IV+1 %s\n", s2.c_str());

	    AES_ECB_encrypt(iv, &key_[0], xordata, 16);

	    xorit(xordata, &content[16], decrypt, remaining);

	    vector<uchar> dec2(decrypt, decrypt+remaining);
            debugPayload("(multical21) decrypted", dec2);

	    // Append the second decrypted block to the first.
	    dec.insert(dec.end(), dec2.begin(), dec2.end());
	}
	content.clear();
	content.insert(content.end(), dec.begin(), dec.end());
    }

    processContent(content);
    datetime_of_update_ = time(NULL);

    num_updates_++;
    for (auto &cb : on_update_) if (cb) cb(this);
}

float getScaleFactor(int vif) {
    switch (vif) {
    case 0x13: return 1000.0;
    case 0x14: return 100.0;
    case 0x15: return 10.0;
    case 0x16: return 10.0;
    }
    warning("(multical21) warning: Unknown vif code %d for scale factor, using 1000.0 instead.\n", vif);
    return 1000.0;
}

void MeterMultical21::processContent(vector<uchar> &c) {
    int crc0 = c[0];
    int crc1 = c[1];
    int frame_type = c[2];
    verbose("(multical21) CRC16:      %02x%02x\n", crc1, crc0);
    /*
    uint16_t crc = crc16(&(c[2]), c.size()-2);
    verbose("(multical21) CRC16 calc: %04x\n", crc);
    */
    if (frame_type == 0x79) {
        verbose("(multical21) Short frame %d bytes\n", c.size());
        if (c.size() != 15) {
            warning("(multical21) warning: Unexpected length of frame %zu. Expected 15 bytes!\n", c.size());
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

        info_codes_ = rec1val1*256+rec1val0;
        verbose("(multical21) short rec1 %02x %02x info codes\n", rec1val1, rec1val0);

        int consumption_raw = rec2val3*256*256*256 + rec2val2*256*256 + rec2val1*256 + rec2val0;
        verbose("(multical21) short rec2 %02x %02x %02x %02x = %d total consumption\n", rec2val3, rec2val2, rec2val1, rec2val0, consumption_raw);

        // The dif=0x04 vif=0x13 means current volume with scale factor .001
        total_water_consumption_ = ((float)consumption_raw) / ((float)1000);
	has_total_water_consumption_ = true;

        // The short frame target volume supplies two low bytes,
        // the remaining two hi bytes are >>probably<< picked from rec2!
        int target_volume_raw = rec2val3*256*256*256 + rec2val2*256*256 + rec3val1*256 + rec3val0;
        verbose("(multical21) short rec3 (%02x %02x) %02x %02x = %d target volume\n", rec2val3, rec2val2, rec3val1, rec3val0, target_volume_raw);
        target_volume_ = ((float)target_volume_raw) / ((float)1000);
	has_target_volume_ = true;

    } else
    if (frame_type == 0x78) {
        verbose("(multical21) Full frame %d bytes\n", c.size());
        if (c.size() != 22) {
            warning("(multical21) warning: Unexpected length of frame %zu. Expected 22 bytes!\n", c.size());
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
            warning("(multical21) warning: Unexpected field! Expected info codes\n"
                    "with dif=0x02 vif=0xff vife=0x20 but got dif=%02x vif=%02x vife=%02x\n", rec1dif, rec1vif, rec1vife);
        }

        info_codes_ = rec1val1*256+rec1val0;
        verbose("(multical21) full rec1 dif=%02x vif=%02x vife=%02x\n", rec1dif, rec1vif, rec1vife);
        verbose("(multical21) full rec1 %02x %02x info codes\n", rec1val1, rec1val0);

        if (rec2dif != 0x04 || rec2vif != 0x13) {
            warning("(multical21) warning: Unexpected field! Expected current volume\n"
                    "with dif=0x04 vif=0x13 but got dif=%02x vif=%02x\n", rec2dif, rec2vif);
        }

        int consumption_raw = rec2val3*256*256*256 + rec2val2*256*256 + rec2val1*256 + rec2val0;
        verbose("(multical21) full rec2 dif=%02x vif=%02x\n", rec2dif, rec2vif);
        verbose("(multical21) full rec2 %02x %02x %02x %02x = %d total consumption\n", rec2val3, rec2val2, rec2val1, rec2val0, consumption_raw);
        // The dif=0x04 vif=0x13 means current volume with scale factor .001
        total_water_consumption_ = ((float)consumption_raw) / ((float)1000);
	has_total_water_consumption_ = true;

        if (rec3dif != 0x44 || rec3vif != 0x13) {
            warning("(multical21) warning: Unexpected field! Expected target volume (ie volume recorded on first day of month)\n"
                    "with dif=0x44 vif=0x13 but got dif=%02x vif=%02x\n", rec3dif, rec3vif);
        }
        int target_volume_raw = rec3val3*256*256*256 + rec3val2*256*256 + rec3val1*256 + rec3val0;
        verbose("(multical21) full rec3 dif=%02x vif=%02x\n", rec3dif, rec3vif);
        verbose("(multical21) full rec3 %02x %02x %02x %02x = %d target volume\n", rec3val3, rec3val2, rec3val1, rec3val0, target_volume_raw);

        target_volume_ = ((float)target_volume_raw) / ((float)1000);
	has_target_volume_ = true;

        // To unknown bytes, seems to be very constant.
        verbose("(multical21) full rec4 %02x %02x = unknown\n", rec4val1, rec4val0);
    } else {
        warning("(multical21) warning: Unknown frame %02x\n", frame_type);
    }
}

string MeterMultical21::status() {
    string s;
    if (info_codes_ & INFO_CODE_DRY) s.append("DRY ");
    if (info_codes_ & INFO_CODE_REVERSE) s.append("REVERSED ");
    if (info_codes_ & INFO_CODE_LEAK) s.append("LEAK ");
    if (info_codes_ & INFO_CODE_BURST) s.append("BURST ");
    if (s.length() > 0) {
        s.pop_back(); // Remove final space
        return s;
    }
    return s;
}

string MeterMultical21::timeDry() {
    int time_dry = (info_codes_ >> INFO_CODE_DRY_SHIFT) & 7;
    if (time_dry) {
        return decodeTime(time_dry);
    }
    return "";
}

string MeterMultical21::timeReversed() {
    int time_reversed = (info_codes_ >> INFO_CODE_REVERSE_SHIFT) & 7;
    if (time_reversed) {
        return decodeTime(time_reversed);
    }
    return "";
}

string MeterMultical21::timeLeaking() {
    int time_leaking = (info_codes_ >> INFO_CODE_LEAK_SHIFT) & 7;
    if (time_leaking) {
        return decodeTime(time_leaking);
    }
    return "";
}

string MeterMultical21::timeBursting() {
    int time_bursting = (info_codes_ >> INFO_CODE_BURST_SHIFT) & 7;
    if (time_bursting) {
        return decodeTime(time_bursting);
    }
    return "";
}

string MeterMultical21::statusHumanReadable() {
    string s;
    bool dry = info_codes_ & INFO_CODE_DRY;
    int time_dry = (info_codes_ >> INFO_CODE_DRY_SHIFT) & 7;
    if (dry || time_dry) {
        if (dry) s.append("DRY");
        s.append("(dry ");
        s.append(decodeTime(time_dry));
        s.append(") ");
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
    if (s.length() > 0) {
        s.pop_back();
        return s;
    }
    return "OK";
}

string MeterMultical21::decodeTime(int time) {
    if (time>7) {
        warning("(multical21) warning: Cannot decode time %d should be 0-7.\n", time);
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
