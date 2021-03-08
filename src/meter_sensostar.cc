/*
 Copyright (C) 2020 Patrick Schwarz

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
#include"util.h"

using namespace std;

struct MeterSensostar : public virtual HeatMeter, public virtual MeterCommonImplementation {
    MeterSensostar(MeterInfo &mi);

    double totalEnergyConsumption(Unit u);
    double totalWater(Unit u);

private:
    void processContent(Telegram *t);
    string status();

    string meter_timestamp_;
    double total_energy_consumption_kwh_ {};
    uchar info_codes_ {};
    double total_water_m3_ {};
};

shared_ptr<HeatMeter> createSensostar(MeterInfo &mi)
{
    return shared_ptr<HeatMeter>(new MeterSensostar(mi));
}

MeterSensostar::MeterSensostar(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::SENSOSTAR)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);
    addLinkMode(LinkMode::C1);

    addPrint("meter_timestamp", Quantity::Text,
             [&](){ return meter_timestamp_; },
             "Date time for this reading.",
             false, true);

    addPrint("total", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);

    addPrint("total_water", Quantity::Volume,
             [&](Unit u){ return totalWater(u); },
             "The total amount of water running through meter.",
             true, true);

    addPrint("current_status", Quantity::Text,
             [&](){ return status(); },
             "Status of meter.",
             true, true);
}

void MeterSensostar::processContent(Telegram *t)
{
    /*
    (sensostar) 11: 04 dif (32 Bit Integer/Binary Instantaneous value)
    (sensostar) 12: 6D vif (Date and time type)
    (sensostar) 13: 17248A2B
    (sensostar) 17: 04 dif (32 Bit Integer/Binary Instantaneous value)
    (sensostar) 18: 06 vif (Energy kWh)
    (sensostar) 19: F8200000
    (sensostar) 1d: 01 dif (8 Bit Integer/Binary Instantaneous value)
    (sensostar) 1e: FD vif (Second extension of VIF-codes)
    (sensostar) 1f: 17 vife (Error flags (binary))
    (sensostar) 20: 00
    (sensostar) 21: 04 dif (32 Bit Integer/Binary Instantaneous value)
    (sensostar) 22: 15 vif (Volume 10⁻¹ m³)
    (sensostar) 23: * 8F1D0000 total consumption (756.700000 m3)
    */
    int offset;
    string key;

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        meter_timestamp_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " at date (%s)", meter_timestamp_.c_str());
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::EnergyWh, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_energy_consumption_kwh_);
        t->addMoreExplanation(offset, " total energy consumption (%f kWh)", total_energy_consumption_kwh_);
    }

    extractDVuint8(&t->values, "01FD17", &offset, &info_codes_);
    t->addMoreExplanation(offset, " info codes (%s)", status().c_str());

    if(findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_m3_);
        t->addMoreExplanation(offset, " total water consumption (%f m3)", total_water_m3_);
    }

}

double MeterSensostar::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_consumption_kwh_, Unit::KWH, u);
}

double MeterSensostar::totalWater(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_m3_, Unit::M3, u);
}

string MeterSensostar::status()
{
    if (info_codes_ == 0) return "OK";
    string s;
    // We would like to know what the bits mean, but currently we do not!
    strprintf(s, "ERROR(%04x)", info_codes_);
    return s;
}
