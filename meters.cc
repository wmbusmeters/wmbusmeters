/*
 Copyright (C) 2017-2018 Fredrik Öhrström

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

#include"meters.h"
#include"meters_common_implementation.h"

#include<memory.h>

MeterCommonImplementation::MeterCommonImplementation(WMBus *bus, const char *name, const char *id, const char *key,
                                                     MeterType type, int manufacturer, int media,
                                                     LinkMode required_link_mode) :
    type_(type), manufacturer_(manufacturer), media_(media), name_(name), bus_(bus),
    required_link_mode_(required_link_mode)
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

LinkMode MeterCommonImplementation::requiredLinkMode()
{
    return required_link_mode_;
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
    if (!strcmp(type, "flowiq3100")) return FLOWIQ3100_METER;
    if (!strcmp(type, "multical302")) return MULTICAL302_METER;
    if (!strcmp(type, "omnipower")) return OMNIPOWER_METER;
    if (!strcmp(type, "supercom587")) return SUPERCOM587_METER;
    if (!strcmp(type, "iperl")) return IPERL_METER;
    if (!strcmp(type, "qcaloric")) return QCALORIC_METER;
    return UNKNOWN_METER;
}

LinkMode toMeterLinkMode(const char *type)
{
    if (!strcmp(type, "multical21")) return LinkModeC1;
    if (!strcmp(type, "flowiq3100")) return LinkModeC1;
    if (!strcmp(type, "multical302")) return LinkModeC1;
    if (!strcmp(type, "omnipower")) return LinkModeC1;
    if (!strcmp(type, "supercom587")) return LinkModeT1;
    if (!strcmp(type, "iperl")) return LinkModeT1;

    return UNKNOWN_LINKMODE;
}


bool MeterCommonImplementation::isTelegramForMe(Telegram *t)
{
    return (manufacturer_ == 0 || t->m_field == manufacturer_) &&
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

vector<string> MeterCommonImplementation::getRecords()
{
    vector<string> recs;
    for (auto& p : values_)
    {
        recs.push_back(p.first);
    }
    return recs;
}

double MeterCommonImplementation::getRecordAsDouble(string record)
{
    return 0.0;
}

uint16_t MeterCommonImplementation::getRecordAsUInt16(string record)
{
    return 0;
}

void MeterCommonImplementation::triggerUpdate(Telegram *t)
{
    datetime_of_update_ = time(NULL);
    num_updates_++;
    for (auto &cb : on_update_) if (cb) cb(this);
    t->handled = true;
}

void MeterCommonImplementation::updateMedia(int media)
{
    media_ = media;
}
