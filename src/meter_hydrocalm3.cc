/*
 Copyright (C) 2018-2020 Fredrik Öhrström
                    2020 Eric Bus

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
#include"dvparser.h"
#include"wmbus.h"
#include"wmbus_utils.h"
#include"util.h"


struct MeterHydrocalM3 : public virtual HeatMeter, public virtual MeterCommonImplementation {
    MeterHydrocalM3(MeterInfo &mi);

    double totalEnergyConsumption(Unit u);
    double totalVolume(Unit u);
    double t1Temperature(Unit u);
    bool  hasT1Temperature();
    double t2Temperature(Unit u);
    bool  hasT2Temperature();

private:
    void processContent(Telegram *t);

    double total_energy_kwh_ {};
    double total_volume_m3_ {};
    double t1_temperature_c_ { 127 };
    bool has_t1_temperature_ {};
    double t2_temperature_c_ { 127 };
    bool has_t2_temperature_ {};
    string target_date_ {};

};

MeterHydrocalM3::MeterHydrocalM3(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterType::HYDROCALM3)
{
    setExpectedELLSecurityMode(ELLSecurityMode::AES_CTR);

    addLinkMode(LinkMode::T1);

    addPrint("total_energy_consumption", Quantity::Energy, Unit(Unit::GJ), //this unit converts the JSON output
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);

    addPrint("total_volume", Quantity::Volume,
             [&](Unit u){ return totalVolume(u); },
             "Total volume of media.",
             true, true);

    addPrint("t1_temperature", Quantity::Temperature,
             [&](Unit u){ return t1Temperature(u); },
             "The T1 temperature.",
             true, true);

    addPrint("t2_temperature", Quantity::Temperature,
             [&](Unit u){ return t2Temperature(u); },
             "The T2 temperature.",
             true, true);

    addPrint("at_date", Quantity::Text,
             [&](){ return target_date_; },
             "Date when total energy consumption was recorded.",
             true, true);

}

shared_ptr<HeatMeter> createHydrocalM3(MeterInfo &mi) {
    return shared_ptr<HeatMeter>(new MeterHydrocalM3(mi));
}

double MeterHydrocalM3::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_kwh_, Unit::MJ, u); //this unit identifies the data received in telegram
}

double MeterHydrocalM3::totalVolume(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_volume_m3_, Unit::M3, u);
}

double MeterHydrocalM3::t1Temperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(t1_temperature_c_, Unit::C, u);
}

bool MeterHydrocalM3::hasT1Temperature()
{
    return has_t1_temperature_;
}

double MeterHydrocalM3::t2Temperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(t2_temperature_c_, Unit::C, u);
}

bool MeterHydrocalM3::hasT2Temperature()
{
    return has_t2_temperature_;
}


/*
double MeterHydrocalM3::volumeFlow(Unit u)
{
    assertQuantity(u, Quantity::Flow);
    return convert(volume_flow_m3h_, Unit::M3H, u);
}

*/

void MeterHydrocalM3::processContent(Telegram *t)
{
    /*
      (supercom587) 00: 8e length (142 bytes)
      (supercom587) 01: 44 dll-c (from meter SND_NR)
      (supercom587) 02: b409 dll-mfct (BMT)
      (supercom587) 04: 71493602 dll-id (02364971)
      (supercom587) 08: 0b dll-version
      (supercom587) 09: 0d dll-type (Heat/Cooling load meter)
      (supercom587) 0a: 7a tpl-ci-field (EN 13757-3 Application Layer (short tplh))
      (supercom587) 0b: b6 tpl-acc-field
      (supercom587) 0c: 00 tpl-sts-field
      (supercom587) 0d: 8005 tpl-cfg 0580 (AES_CBC_IV nb=8 cntn=0 ra=0 hc=0 )
      (supercom587) 0f: 2f2f decrypt check bytes
      (supercom587) 11: 0C dif (8 digit BCD Instantaneous value)
      (supercom587) 12: 0E vif (Energy MJ)
      (supercom587) 13: 00000000
      (supercom587) 17: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (supercom587) 18: 6D vif (Date and time type)
      (supercom587) 19: 112D872C
      (supercom587) 1d: 0C dif (8 digit BCD Instantaneous value)
      (supercom587) 1e: 13 vif (Volume l)
      (supercom587) 1f: * 00000000 total consumption (0.000000 m3)
      (supercom587) 23: 0C dif (8 digit BCD Instantaneous value)
      (supercom587) 24: 0E vif (Energy MJ)
      (supercom587) 25: 00000000
      (supercom587) 29: 0C dif (8 digit BCD Instantaneous value)
      (supercom587) 2a: 13 vif (Volume l)
      (supercom587) 2b: 00000000
      (supercom587) 2f: 0C dif (8 digit BCD Instantaneous value)
      (supercom587) 30: 13 vif (Volume l)
      (supercom587) 31: 00000000
      (supercom587) 35: 0C dif (8 digit BCD Instantaneous value)
      (supercom587) 36: 13 vif (Volume l)
      (supercom587) 37: 00000000
      (supercom587) 3b: 0A dif (4 digit BCD Instantaneous value)
      (supercom587) 3c: 5A vif (Flow temperature 10⁻¹ °C)
      (supercom587) 3d: 9400
      (supercom587) 3f: 0A dif (4 digit BCD Instantaneous value)
      (supercom587) 40: 5E vif (Return temperature 10⁻¹ °C)
      (supercom587) 41: 9500
      (supercom587) 43: 0F manufacturer specific data

*/
    int offset;
    string key;

    if(findKey(MeasurementType::Instantaneous, ValueInformation::EnergyMJ, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_energy_kwh_);
        t->addMoreExplanation(offset, " total energy consumption (%f MJ)", total_energy_kwh_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_volume_m3_);
        t->addMoreExplanation(offset, " total volume (%f m3)", total_volume_m3_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::FlowTemperature, 0, 0, &key, &t->values)) {
        has_t1_temperature_ = extractDVdouble(&t->values, key, &offset, &t1_temperature_c_);
        t->addMoreExplanation(offset, " T1 flow temperature (%f °C)", t1_temperature_c_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::ReturnTemperature, 0, 0, &key, &t->values)) {
        has_t2_temperature_ = extractDVdouble(&t->values, key, &offset, &t2_temperature_c_);
        t->addMoreExplanation(offset, " T2 flow temperature (%f °C)", t2_temperature_c_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        target_date_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " target date (%s)", target_date_.c_str());
    }

}
