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

#ifndef METERS_COMMON_IMPLEMENTATION_H_
#define METERS_COMMON_IMPLEMENTATION_H_

#include"meters.h"
#include"units.h"

#include<map>
#include<set>

struct MeterCommonImplementation : public virtual Meter
{
    vector<string> ids();
    string name();
    MeterType type();
    vector<int> media();
    WMBus *bus();
    LinkMode requiredLinkMode();

    string datetimeOfUpdateHumanReadable();
    string datetimeOfUpdateRobot();

    void onUpdate(function<void(Telegram*,Meter*)> cb);
    int numUpdates();

    bool isTelegramForMe(Telegram *t);
    bool useAes();
    vector<uchar> key();

    std::vector<std::string> getRecords();
    double getRecordAsDouble(std::string record);
    uint16_t getRecordAsUInt16(std::string record);

    MeterCommonImplementation(WMBus *bus, string& name, string& id, string& key,
                              MeterType type, int manufacturer,
                              LinkMode required_link_mode);

    ~MeterCommonImplementation() = default;

protected:

    void triggerUpdate(Telegram *t);
    void addMedia(int media);
    void addManufacturer(int m);

private:

    MeterType type_ {};
    vector<int> media_;
    set<int> manufacturers_;
    string name_;
    vector<string> ids_;
    vector<uchar> key_;
    WMBus *bus_ {};
    vector<function<void(Telegram*,Meter*)>> on_update_;
    int num_updates_ {};
    bool use_aes_ {};
    time_t datetime_of_update_ {};
    LinkMode required_link_mode_ {};
    vector<Unit> conversions_ {};

protected:
    std::map<std::string,std::pair<int,std::string>> values_;
};

#endif
