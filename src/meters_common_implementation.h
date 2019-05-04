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

struct Print
{
    string vname; // Value name, like: total current previous target
    Quantity quantity; // Quantity: Energy, Volume
    Unit default_unit; // Default unit for above quantity: KWH, M3
    function<double(Unit)> getValueDouble; // Callback to fetch the value from the meter.
    function<string()> getValueString; // Callback to fetch the value from the meter.
    string help; // Helpful information on this meters use of this value.
    bool field; // If true, print in hr/fields output.
    bool json; // If true, print in json and shell env variables.
};

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

    EncryptionMode encMode();
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

    string meterName() { return toMeterName(type_); }

protected:

    void triggerUpdate(Telegram *t);
    void setExpectedVersion(int version);
    int expectedVersion();
    void addConversions(std::vector<Unit> cs);
    void addMedia(int media);
    void addManufacturer(int m);
    void addPrint(string vname, Quantity vquantity,
                  function<double(Unit)> getValueFunc, string help, bool field, bool json);
    void addPrint(string vname, Quantity vquantity,
                  function<std::string()> getValueFunc, string help, bool field, bool json);
    void setEncryptionMode(EncryptionMode em);
    EncryptionMode encryptionMode();
    void handleTelegram(Telegram *t);
    void printMeter(Telegram *t,
                    string *human_readable,
                    string *fields, char separator,
                    string *json,
                    vector<string> *envs);

    virtual void processContent(Telegram *t) = 0;

private:

    MeterType type_ {};
    int expected_meter_version_ {};
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
    EncryptionMode enc_mode_ {};

protected:
    std::map<std::string,std::pair<int,std::string>> values_;
    vector<Unit> conversions_;
    vector<Print> prints_;
};

#endif
