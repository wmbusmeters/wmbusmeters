/*
 Copyright (C) 2019 Fredrik Öhrström

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

#include"dvparser.h"
#include"meters.h"
#include"meters_common_implementation.h"
#include"wmbus.h"
#include"wmbus_utils.h"

struct MeterLansenTH : public virtual TempHygroMeter, public virtual MeterCommonImplementation {
    MeterLansenTH(WMBus *bus, MeterInfo &mi);

    double currentTemperature(Unit u);
    double currentRelativeHumidity();

private:

    void processContent(Telegram *t);

    double current_temperature_c_ {};
    double temperature_at_set_date_c_[2]; // storage nr 1 and 2 stored at index 0 and 1.
    double current_relative_humidity_rh_ {};
    double relative_humidity_at_set_date_rh_[2]; // storage nr 1 and 2 stored at index 0 and 1.
};

MeterLansenTH::MeterLansenTH(WMBus *bus, MeterInfo &mi) :
    MeterCommonImplementation(bus, mi, MeterType::LANSENTH, MANUFACTURER_LAS)
{
    setEncryptionMode(EncryptionMode::AES_CBC);

    addMedia(0x1b);

    addLinkMode(LinkMode::T1);

    setExpectedVersion(0x07);

    addPrint("current_temperature", Quantity::Temperature,
             [&](Unit u){ return currentTemperature(u); },
             "The current temperature.",
             true, true);

    addPrint("current_relative_humidity", Quantity::RelativeHumidity,
             [&](Unit u){ return currentRelativeHumidity(); },
             "The current relative humidity.",
             true, true);

    for (int i=1; i<=2; ++i)
    {
        string msg, info;
        strprintf(msg, "temperature_at_set_date_%d", i);
        strprintf(info, "Temperature at the %d billing period date change.", i);
        addPrint(msg, Quantity::Temperature,
                 [this,i](Unit u){ return temperature_at_set_date_c_[i-1]; },
                 info,
                 false, true);

        strprintf(msg, "relative_humidity_at_set_date_%d", i);
        strprintf(info, "Relative humidity at the %d billing period date change.", i);
        addPrint(msg, Quantity::RelativeHumidity,
                 [this,i](Unit u){ return relative_humidity_at_set_date_rh_[i-1]; },
                 info,
                 false, true);
    }

    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

unique_ptr<TempHygroMeter> createLansenTH(WMBus *bus, MeterInfo &mi)
{
    return unique_ptr<TempHygroMeter>(new MeterLansenTH(bus, mi));
}

double MeterLansenTH::currentTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(current_temperature_c_, Unit::C, u);
}

double MeterLansenTH::currentRelativeHumidity()
{
    return current_relative_humidity_rh_;
}

void MeterLansenTH::processContent(Telegram *t)
{
    /*
    (lansenth) 0f: 2F skip
    (lansenth) 10: 2F skip
    (lansenth) 11: 02 dif (16 Bit Integer/Binary Instantaneous value)
    (lansenth) 12: 65 vif (External temperature 10⁻² °C)
    (lansenth) 13: * 8408 current temperature (21.800000 C)
    (lansenth) 15: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    (lansenth) 16: 65 vif (External temperature 10⁻² °C)
    (lansenth) 17: 8308
    (lansenth) 19: 82 dif (16 Bit Integer/Binary Instantaneous value)
    (lansenth) 1a: 01 dife (subunit=0 tariff=0 storagenr=2)
    (lansenth) 1b: 65 vif (External temperature 10⁻² °C)
    (lansenth) 1c: 9508
    (lansenth) 1e: 02 dif (16 Bit Integer/Binary Instantaneous value)
    (lansenth) 1f: FB vif (First extension of VIF-codes)
    (lansenth) 20: 1A vife (?)
    (lansenth) 21: AE01
    (lansenth) 23: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    (lansenth) 24: FB vif (First extension of VIF-codes)
    (lansenth) 25: 1A vife (?)
    (lansenth) 26: AE01
    (lansenth) 28: 82 dif (16 Bit Integer/Binary Instantaneous value)
    (lansenth) 29: 01 dife (subunit=0 tariff=0 storagenr=2)
    (lansenth) 2a: FB vif (First extension of VIF-codes)
    (lansenth) 2b: 1A vife (?)
    (lansenth) 2c: A901
    (lansenth) 2e: 2F skip
    */
    map<string,pair<int,DVEntry>> values;
    parseDV(t, t->content, t->content.begin(), t->content.size(), &values);

    int offset;
    string key;

    if (findKey(ValueInformation::ExternalTemperature, 0, &key, &values))
    {
        extractDVdouble(&values, key, &offset, &current_temperature_c_);
        t->addMoreExplanation(offset, " current temperature (%f C)", current_temperature_c_);
    }

    for (int i=1; i<=2; ++i)
    {
        if (findKey(ValueInformation::ExternalTemperature, i, &key, &values))
        {
            string info;
            strprintf(info, " temperature at set date %d (%%f c)", i);
            extractDVdouble(&values, key, &offset, &temperature_at_set_date_c_[i-1]);
            t->addMoreExplanation(offset, info.c_str(), temperature_at_set_date_c_[i-1]);
        }
    }

    // Temporarily silly solution until the dvparser is upgraded with support for extension

    key = "02FB1A"; // 1A = 0001 1010 = First extension vif code Relative Humidity 10^-1
    if (hasKey(&values, key))
    {
        double tmp;
        extractDVdouble(&values, key, &offset, &tmp, false);
        current_relative_humidity_rh_ = tmp / (double)10.0;
        t->addMoreExplanation(offset, " current relative humidity (%f RH)", current_relative_humidity_rh_);
    }
    key = "42FB1A"; // 1A = 0001 1010 = First extension vif code Relative Humidity 10^-1
    if (hasKey(&values, key))
    {
        double tmp;
        extractDVdouble(&values, key, &offset, &tmp, false);
        relative_humidity_at_set_date_rh_[0] = tmp / (double)10.0;
        t->addMoreExplanation(offset, " relative humidity at set date 1 (%f RH)", relative_humidity_at_set_date_rh_[0]);
    }
    key = "8201FB1A"; // 1A = 0001 1010 = First extension vif code Relative Humidity 10^-1
    if (hasKey(&values, key))
    {
        double tmp;
        extractDVdouble(&values, key, &offset, &tmp, false);
        relative_humidity_at_set_date_rh_[1] = tmp / (double)10.0;
        t->addMoreExplanation(offset, " relative humidity at set date 2 (%f RH)", relative_humidity_at_set_date_rh_[1]);
    }
}
