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

#include"meters.h"
#include"meters_common_implementation.h"

#include<memory.h>

MeterCommonImplementation::MeterCommonImplementation(WMBus *bus, const char *name, const char *id, const char *key,
                                                     MeterType type, int manufacturer, int media) :
    type_(type), manufacturer_(manufacturer), media_(media), name_(name), bus_(bus)
{
    use_aes_ = true;
    hex2bin(id, &id_);
    if (strlen(key) == 0) {
	use_aes_ = false;
    } else {
	hex2bin(key, &key_);
    }
}

MeterType MeterCommonImplementation::type()
{
    return type_;
}

int MeterCommonImplementation::manufacturer()
{
    return manufacturer_;
}

int MeterCommonImplementation::media()
{
    return media_;
}

string MeterCommonImplementation::id()
{
    return bin2hex(id_);
}

string MeterCommonImplementation::name()
{
    return name_;
}

WMBus *MeterCommonImplementation::bus()
{
    return bus_;
}

void MeterCommonImplementation::onUpdate(function<void(Meter*)> cb)
{
    on_update_.push_back(cb);
}

int MeterCommonImplementation::numUpdates()
{
    return num_updates_;
}

string MeterCommonImplementation::datetimeOfUpdateHumanReadable()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));
    strftime(datetime, 20, "%Y-%m-%d %H:%M.%S", localtime(&datetime_of_update_));
    return string(datetime);
}

string MeterCommonImplementation::datetimeOfUpdateRobot()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));
    // This is the date time in the Greenwich timezone (Zulu time), dont get surprised!
    strftime(datetime, sizeof(datetime), "%FT%TZ", gmtime(&datetime_of_update_));
    return string(datetime);
}

MeterType toMeterType(const char *type)
{
    if (!strcmp(type, "multical21")) return MULTICAL21_METER;
    if (!strcmp(type, "multical302")) return MULTICAL302_METER;
    if (!strcmp(type, "omnipower")) return OMNIPOWER_METER;
    if (!strcmp(type, "water")) return MULTICAL21_METER;
    if (!strcmp(type, "heat")) return MULTICAL302_METER;
    if (!strcmp(type, "electricity")) return OMNIPOWER_METER;
    return UNKNOWN_METER;
}


bool MeterCommonImplementation::isTelegramForMe(Telegram *t)
{
    return t->m_field == manufacturer_ &&
        t->a_field_address[3] == id_[3] &&
        t->a_field_address[2] == id_[2] &&
        t->a_field_address[1] == id_[1] &&
        t->a_field_address[0] == id_[0];
}

bool MeterCommonImplementation::useAes()
{
    return use_aes_;
}

vector<uchar> MeterCommonImplementation::key()
{
    return key_;
}

void MeterCommonImplementation::triggerUpdate(Telegram *t)
{
    datetime_of_update_ = time(NULL);
    num_updates_++;
    for (auto &cb : on_update_) if (cb) cb(this);
    t->handled = true;
}
