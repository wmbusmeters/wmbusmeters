/*
 Copyright (C) 2017-2020 Fredrik Öhrström

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

#include<cmath>

using namespace std;


constexpr uint8_t ERROR_CODE_VOLTAGE_PHASE_1_OVERFLOW=0x01;
constexpr uint8_t ERROR_CODE_VOLTAGE_PHASE_2_OVERFLOW=0x02;
constexpr uint8_t ERROR_CODE_VOLTAGE_PHASE_3_OVERFLOW=0x04;

constexpr uint8_t ERROR_CODE_CURRENT_PHASE_1_OVERFLOW=0x08;
constexpr uint8_t ERROR_CODE_CURRENT_PHASE_2_OVERFLOW=0x10;
constexpr uint8_t ERROR_CODE_CURRENT_PHASE_3_OVERFLOW=0x20;

constexpr uint8_t ERROR_CODE_FREQUENCY_OUT_OF_RANGE=0x40;


struct MeterEM24 : public virtual MeterCommonImplementation {
    MeterEM24(MeterInfo &mi);

    double totalEnergyConsumption(Unit u);
    double totalEnergyProduction(Unit u);

    double totalReactiveEnergyConsumption(Unit u);
    double totalReactiveEnergyProduction(Unit u);

    double totalApparentEnergyConsumption(Unit u);
    double totalApparentEnergyProduction(Unit u);

    string status();

private:
    void processContent(Telegram *t);

    double total_true_energy_consumption_kwh_ {};
    double total_true_energy_production_kwh_ {};

    double total_reactive_energy_consumption_kvarh_ {};
    double total_reactive_energy_production_kvarh_ {};

    uint8_t error_codes_ {};
};

shared_ptr<Meter> createEM24(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterEM24(mi));
}

MeterEM24::MeterEM24(MeterInfo &mi) :
    MeterCommonImplementation(mi, "em24")
{
    setMeterType(MeterType::ElectricityMeter);

    setExpectedELLSecurityMode(ELLSecurityMode::AES_CTR);

    addLinkMode(LinkMode::C1);

    addPrint("total_energy_consumption", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);

    addPrint("total_energy_production", Quantity::Energy,
             [&](Unit u){ return totalEnergyProduction(u); },
             "The total energy production recorded by this meter.",
             true, true);

    addPrint("total_reactive_energy_consumption", Quantity::Reactive_Energy,
             [&](Unit u){ return totalReactiveEnergyConsumption(u); },
             "The total reactive energy consumption recorded by this meter.",
             true, true);

    addPrint("total_reactive_energy_production", Quantity::Reactive_Energy,
             [&](Unit u){ return totalReactiveEnergyProduction(u); },
             "The total reactive energy production recorded by this meter.",
             true, true);

    addPrint("total_apparent_energy_consumption", Quantity::Apparent_Energy,
             [&](Unit u){ return totalApparentEnergyConsumption(u); },
             "The total apparent energy consumption by calculation.",
             true, true);

    addPrint("total_apparent_energy_production", Quantity::Apparent_Energy,
             [&](Unit u){ return totalApparentEnergyProduction(u); },
             "The total apparent energy production by calculation.",
             true, true);

    addPrint("errors", Quantity::Text,
             [&](){ return status(); },
             "Any errors currently being reported.",
             false, true);
}

double MeterEM24::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_true_energy_consumption_kwh_, Unit::KWH, u);
}

double MeterEM24::totalEnergyProduction(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_true_energy_production_kwh_, Unit::KWH, u);
}

double MeterEM24::totalReactiveEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Reactive_Energy);
    return convert(total_reactive_energy_consumption_kvarh_, Unit::KVARH, u);
}

double MeterEM24::totalReactiveEnergyProduction(Unit u)
{
    assertQuantity(u, Quantity::Reactive_Energy);
    return convert(total_reactive_energy_production_kvarh_, Unit::KVARH, u);
}

double MeterEM24::totalApparentEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Apparent_Energy);
    return convert(
        sqrt(
            pow(total_true_energy_consumption_kwh_, 2) +
            pow(total_reactive_energy_consumption_kvarh_, 2)
        )
    , Unit::KVAH, u);
}

double MeterEM24::totalApparentEnergyProduction(Unit u)
{
    assertQuantity(u, Quantity::Apparent_Energy);
    return convert(
        sqrt(
            pow(total_true_energy_production_kwh_, 2) +
            pow(total_reactive_energy_production_kvarh_, 2)
        )
    , Unit::KVAH, u);
}

void MeterEM24::processContent(Telegram *t)
{
    int offset;

    // 04 dif (32 Bit Integer/Binary Instantaneous value)
    // 05 vif (Energy 10² Wh)
    extractDVdouble(&t->values, "0405", &offset, &total_true_energy_consumption_kwh_);
    t->addMoreExplanation(offset, " total power (%f kwh)", total_true_energy_consumption_kwh_);

    // 04 dif (32 Bit Integer/Binary Instantaneous value)
    // FB vif (First extension of VIF-codes)
    // 82 vife (Reserved)
    // 75 vife (Cold / Warm Temperature Limit 10^-2 Celsius)
    extractDVdouble(&t->values, "04FB8275", &offset, &total_reactive_energy_consumption_kvarh_);
    t->addMoreExplanation(offset, " total reactive power (%f kvarh)", total_reactive_energy_consumption_kvarh_);

    // 04 dif (32 Bit Integer/Binary Instantaneous value)
    // 85 vif (Energy 10² Wh)
    // 3C vife (backward flow)
    extractDVdouble(&t->values, "04853C", &offset, &total_true_energy_production_kwh_);
    t->addMoreExplanation(offset, " total power (%f kwh)", total_true_energy_production_kwh_);

    // 04 dif (32 Bit Integer/Binary Instantaneous value)
    // FB vif (First extension of VIF-codes)
    // 82 vife (Reserved)
    // F5 vife (Cold / Warm Temperature Limit 10^-2 Celsius)
    // 3C vife (Reserved)
    extractDVdouble(&t->values, "04FB82F53C", &offset, &total_reactive_energy_production_kvarh_);
    t->addMoreExplanation(offset, " total reactive power (%f kvarh)", total_reactive_energy_production_kvarh_);

    // 01 dif (8 Bit Integer/Binary Instantaneous value)
    // FD vif (Second extension of VIF-codes)
    // 17 vife (Error flags (binary))
    extractDVuint8(&t->values, "01FD17", &offset, &error_codes_);
    t->addMoreExplanation(offset, " error codes (%s)", status().c_str());
}

string MeterEM24::status()
{
    string s;
    if (error_codes_ & ERROR_CODE_VOLTAGE_PHASE_1_OVERFLOW) s.append("V 1 OVERFLOW ");
    if (error_codes_ & ERROR_CODE_VOLTAGE_PHASE_2_OVERFLOW) s.append("V 2 OVERFLOW ");
    if (error_codes_ & ERROR_CODE_VOLTAGE_PHASE_3_OVERFLOW) s.append("V 3 OVERFLOW ");
    if (error_codes_ & ERROR_CODE_CURRENT_PHASE_1_OVERFLOW) s.append("I 1 OVERFLOW ");
    if (error_codes_ & ERROR_CODE_CURRENT_PHASE_2_OVERFLOW) s.append("I 2 OVERFLOW ");
    if (error_codes_ & ERROR_CODE_CURRENT_PHASE_3_OVERFLOW) s.append("I 3 OVERFLOW ");
    if (error_codes_ & ERROR_CODE_FREQUENCY_OUT_OF_RANGE) s.append("FREQUENCY ");
    if (s.length() > 0) {
        s.pop_back(); // Remove final space
        return s;
    }
    return s;
}
