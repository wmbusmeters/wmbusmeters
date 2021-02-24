/*
 Copyright (C) 2018-2020 Fredrik Öhrström

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
    int index();
    void setIndex(int i);
    vector<string>& ids();
    string idsc();
    vector<string>  fields();
    vector<Print>   prints();
    string name();
    MeterType type();

    ELLSecurityMode expectedELLSecurityMode();
    TPLSecurityMode expectedTPLSecurityMode();

    string datetimeOfUpdateHumanReadable();
    string datetimeOfUpdateRobot();

    void onUpdate(function<void(Telegram*,Meter*)> cb);
    int numUpdates();

    static bool isTelegramForMeter(Telegram *t, Meter *meter, MeterInfo *mi);
    MeterKeys *meterKeys();

    std::vector<std::string> getRecords();
    double getRecordAsDouble(std::string record);
    uint16_t getRecordAsUInt16(std::string record);

    MeterCommonImplementation(MeterInfo &mi, MeterType type);

    ~MeterCommonImplementation() = default;

    string meterDriver() { return toMeterDriver(type_); }

protected:

    void triggerUpdate(Telegram *t);
    void setExpectedELLSecurityMode(ELLSecurityMode dsm);
    void setExpectedTPLSecurityMode(TPLSecurityMode tsm);
    void addConversions(std::vector<Unit> cs);
    void addShell(std::string cmdline);
    void addJson(std::string json);
    std::vector<std::string> &shellCmdlines();
    std::vector<std::string> &additionalJsons();
    void addLinkMode(LinkMode lm);
    // Print with the default unit for this quantity.
    void addPrint(string vname, Quantity vquantity,
                  function<double(Unit)> getValueFunc, string help, bool field, bool json);
    // Print with exactly this unit for this quantity.
    void addPrint(string vname, Quantity vquantity, Unit unit,
                  function<double(Unit)> getValueFunc, string help, bool field, bool json);
    // Print the dimensionless Text quantity, no unit is needed.
    void addPrint(string vname, Quantity vquantity,
                  function<std::string()> getValueFunc, string help, bool field, bool json);
    bool handleTelegram(AboutTelegram &about, vector<uchar> frame, bool simulated, string *id, bool *id_match);
    void printMeter(Telegram *t,
                    string *human_readable,
                    string *fields, char separator,
                    string *json,
                    vector<string> *envs,
                    vector<string> *more_json, // Add this json "key"="value" strings.
                    vector<string> *selected_fields); // Only print these fields. Json always everything.

    virtual void processContent(Telegram *t) = 0;

private:

    int index_ {};
    MeterType type_ {};
    MeterKeys meter_keys_ {};
    ELLSecurityMode expected_ell_sec_mode_ {};
    TPLSecurityMode expected_tpl_sec_mode_ {};
    string name_;
    vector<string> ids_;
    string idsc_;
    vector<function<void(Telegram*,Meter*)>> on_update_;
    int num_updates_ {};
    time_t datetime_of_update_ {};
    LinkModeSet link_modes_ {};
    vector<string> shell_cmdlines_;
    vector<string> jsons_;

protected:
    std::map<std::string,std::pair<int,std::string>> values_;
    vector<Unit> conversions_;
    vector<Print> prints_;
    vector<string> fields_;
};

#endif
