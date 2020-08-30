/*
 Copyright (C) 2020 Fredrik Öhrström

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

struct MeterCMa12w : public virtual TempHygroMeter, public virtual MeterCommonImplementation {
    MeterCMa12w(MeterInfo &mi);

    double currentTemperature(Unit u);
    double currentRelativeHumidity();

private:

    void processContent(Telegram *t);

    double current_temperature_c_ {};
    double average_temperature_1h_c_;
};

MeterCMa12w::MeterCMa12w(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterType::CMA12W, MANUFACTURER_ELV)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addMedia(0x1b);

    addLinkMode(LinkMode::T1);

    addExpectedVersion(0x20);

    addPrint("current_temperature", Quantity::Temperature,
             [&](Unit u){ return currentTemperature(u); },
             "The current temperature.",
             true, true);

    addPrint("average_temperature_1h", Quantity::Temperature,
             [&](Unit u){ return convert(average_temperature_1h_c_, Unit::C, u); },
             "The average temperature over the last hour.",
             false, true);
}

unique_ptr<TempHygroMeter> createCMa12w(MeterInfo &mi)
{
    return unique_ptr<TempHygroMeter>(new MeterCMa12w(mi));
}

double MeterCMa12w::currentTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(current_temperature_c_, Unit::C, u);
}

double MeterCMa12w::currentRelativeHumidity()
{
    return 0.0;
}

void MeterCMa12w::processContent(Telegram *t)
{
    /*
      (cma12w) 0f: 2F skip
      (cma12w) 10: 2F skip
      (cma12w) 11: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (cma12w) 12: 65 vif (External temperature 10⁻² °C)
      (cma12w) 13: * 1E09 current temperature (23.340000 C)
      (cma12w) 15: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
      (cma12w) 16: 65 vif (External temperature 10⁻² °C)
      (cma12w) 17: * 1809 average temperature 1h (23.280000 C))
      (cma12w) 19: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (cma12w) 1a: FD vif (Second extension of VIF-codes)
      (cma12w) 1b: 1B vife (Digital Input (binary))
      (cma12w) 1c: 3003
      (cma12w) 1e: 0D dif (variable length Instantaneous value)
      (cma12w) 1f: FD vif (Second extension of VIF-codes)
      (cma12w) 20: 0F vife (Software version #)
      (cma12w) 21: 05 varlen=5
      (cma12w) 22: 302E302E34
      A single 0f at the end!?
    */

    int offset;
    string key;

    if (findKey(MeasurementType::Unknown, ValueInformation::ExternalTemperature, 0, 0, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &current_temperature_c_);
        t->addMoreExplanation(offset, " current temperature (%f C)", current_temperature_c_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::ExternalTemperature, 1, 0, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &average_temperature_1h_c_);
        t->addMoreExplanation(offset, " average temperature 1h (%f C))", average_temperature_1h_c_);
    }

}
