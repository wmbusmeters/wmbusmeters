/*
 Copyright (C) 2019-2020 Fredrik Öhrström (gpl-3.0-or-later)

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

struct MeterMunia : public virtual MeterCommonImplementation {
    MeterMunia(MeterInfo &mi);

    double currentTemperature(Unit u);
    double currentRelativeHumidity();

private:

    void processContent(Telegram *t);

    double current_temperature_c_ {};
    double current_relative_humidity_rh_ {};
};

MeterMunia::MeterMunia(MeterInfo &mi) :
    MeterCommonImplementation(mi, "munia")
{
    setMeterType(MeterType::TempHygroMeter);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    addPrint("current_temperature", Quantity::Temperature,
             [&](Unit u){ return currentTemperature(u); },
             "The current temperature.",
             true, true);

    addPrint("current_relative_humidity", Quantity::RelativeHumidity,
             [&](Unit u){ return currentRelativeHumidity(); },
             "The current relative humidity.",
             true, true);

}

shared_ptr<Meter> createMunia(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterMunia(mi));
}

double MeterMunia::currentTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(current_temperature_c_, Unit::C, u);
}

double MeterMunia::currentRelativeHumidity()
{
    return current_relative_humidity_rh_;
}

void MeterMunia::processContent(Telegram *t)
{
    /*
      (munia) 11: 0A dif (16 Bit Integer/Binary Instantaneous value)
      (munia) 12: 66 vif (External temperature 10⁻² °C)
      (munia) 13: * 0102 current temperature (20.100000 C)

      (munia) 1e: 0A dif (16 Bit Integer/Binary Instantaneous value)
      (munia) 1f: FB vif (First extension of VIF-codes)
      (munia) 20: 1A vife (Relative humidity * 10^(-1)%)
      (munia) 21: * 5706 current relative humidity (60.570000 RH)
    */

    int offset;
    string key;

    if (findKey(MeasurementType::Unknown, ValueInformation::ExternalTemperature, 0, 0, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &current_temperature_c_);
        t->addMoreExplanation(offset, " current temperature (%f C)", current_temperature_c_);
    }

    // Temporarily silly solution until the dvparser is upgraded with support for extension
    key = "0AFB1A"; // 1A = 0001 1010 = First extension vif code Relative Humidity 10^-1
    if (hasKey(&t->values, key))
    {
        double tmp;
        extractDVdouble(&t->values, key, &offset, &tmp, false);
        current_relative_humidity_rh_ = tmp / (double)10.0;
        t->addMoreExplanation(offset, " current relative humidity (%f RH)", current_relative_humidity_rh_);
    }
}
