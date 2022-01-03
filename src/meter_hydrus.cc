/*
 Copyright (C) 2019-2020 Fredrik Öhrström
 Copyright (C)      2021 Vincent Privat

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

using namespace std;

struct MeterHydrus : public virtual MeterCommonImplementation {
    MeterHydrus(MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    double totalWaterConsumptionTariff1(Unit u);
    double totalWaterConsumptionTariff2(Unit u);
    double totalWaterConsumptionAtDate(Unit u);
    double totalWaterConsumptionTariff1AtDate(Unit u);
    double totalWaterConsumptionTariff2AtDate(Unit u);
    bool   hasTotalWaterConsumption();
    double maxFlow(Unit u);
    bool   hasMaxFlow();
    double flowTemperature(Unit u);
    double externalTemperature(Unit u);

private:
    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};
    double total_water_consumption_tariff1_m3_ {};
    double total_water_consumption_tariff2_m3_ {};
    string current_date_;
    double total_water_consumption_at_date_m3_ {};
    double total_water_consumption_tariff1_at_date_m3_ {};
    double total_water_consumption_tariff2_at_date_m3_ {};
    string at_date_;
    double max_flow_m3h_ {};
    double flow_temperature_c_ { 127 };
    double external_temperature_c_ {};
    uint32_t actuality_duration_s_ {};
    double operating_time_h_ {};
    double remaining_battery_life_year_ {};
    string status_; // TPL STS

    map<int,string> error_codes_;
};

MeterHydrus::MeterHydrus(MeterInfo &mi) :
    MeterCommonImplementation(mi, "hydrus")
{
    setMeterType(MeterType::WaterMeter);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    error_codes_ = { { 0x10, "TEMPERATURE_MEASUREMENT_ERROR" },
                     { 0x30, "AIR_IN_PIPE" },
                     { 0x70, "MEASUREMENT_ERROR" },
                     { 0x90, "LEAKAGE_OR_NO_USAGE" },
                     { 0xb0, "REVERSE_FLOW" },
                     { 0xd0, "LOW_TEMPERATURE" },
                     { 0xf0, "AIR_IN_PIPE" } };

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);

    addPrint("total_tariff1", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumptionTariff1(u); },
             "The total water consumption recorded by this meter at tariff 1.",
             false, true);

    addPrint("total_tariff2", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumptionTariff2(u); },
             "The total water consumption recorded by this meter at tariff 2.",
             false, true);

    addPrint("max_flow", Quantity::Flow,
             [&](Unit u){ return maxFlow(u); },
             "The maximum flow recorded during previous period.",
             true, true);

    addPrint("flow_temperature", Quantity::Temperature,
             [&](Unit u){ return flowTemperature(u); },
             "The water temperature.",
             false, true);

    addPrint("external_temperature", Quantity::Temperature,
             [&](Unit u){ return externalTemperature(u); },
             "The external temperature.",
             false, true);

    addPrint("current_date", Quantity::Text,
             [&](){ return current_date_; },
             "Current date of measurement.",
             false, true);

    addPrint("total_at_date", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumptionAtDate(u); },
             "The total water consumption recorded at date.",
             false, true);

    addPrint("total_tariff1_at_date", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumptionTariff1AtDate(u); },
             "The total water consumption recorded at tariff 1 at date.",
             false, true);

    addPrint("total_tariff2_at_date", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumptionTariff2AtDate(u); },
             "The total water consumption recorded at tariff 2 at date.",
             false, true);

    addPrint("at_date", Quantity::Text,
             [&](){ return at_date_; },
             "Date when total water consumption was recorded.",
             false, true);

    addPrint("actuality_duration", Quantity::Time, Unit::Second,
             [&](Unit u){ return convert(actuality_duration_s_, Unit::Second, u); },
             "Elapsed time between measurement and transmission",
             false, true);

    addPrint("operating_time", Quantity::Time, Unit::Hour,
             [&](Unit u){ return convert(operating_time_h_, Unit::Hour, u); },
             "How long the meter is operating",
             false, true);

    addPrint("remaining_battery_life", Quantity::Time, Unit::Year,
             [&](Unit u){ return convert(remaining_battery_life_year_, Unit::Year, u); },
             "How many more years the battery is expected to last",
             false, true);

    addPrint("status", Quantity::Text,
             [&](){ return status_; },
             "The status is OK or some error condition.",
             true, true);
}

shared_ptr<Meter> createHydrus(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterHydrus(mi));
}

void MeterHydrus::processContent(Telegram *t)
{
    // There are two distinctintly different Hydrus telegrams.
    // Unfortunately there seems to be no markings on the
    // physical meter that tells us which version it is.
    //
    // Fortunately the mfct,media,version bits does distinguish
    // the meters!
    //
    // This driver currently only decodes parts of both telegrams.
    // Eventually we should either split this driver into two.
    // Or make more generic capabilities in drivers to switch
    // between different telegrams formats, that are similar
    // but not quite.

    /*
     New style telegram:
     (hydrus) 11: 0C dif (8 digit BCD Instantaneous value)
     (hydrus) 12: 13 vif (Volume l)
     (hydrus) 13: * 50340000 total consumption (3.450000 m3)
     (hydrus) 17: 0D dif (variable length Instantaneous value)
     (hydrus) 18: FD vif (Second extension of VIF-codes)
     (hydrus) 19: 11 vife (Customer)
     (hydrus) 1a: 0A varlen=10
     (hydrus) 1b: 38373130313442303241
     (hydrus) 25: 0B dif (6 digit BCD Instantaneous value)
     (hydrus) 26: 3B vif (Volume flow l/h)
     (hydrus) 27: * 000000 max flow (0.000000 m3/h)
     (hydrus) 2a: 02 dif (16 Bit Integer/Binary Instantaneous value)
     (hydrus) 2b: FD vif (Second extension of VIF-codes)
     (hydrus) 2c: 74 vife (Reserved)
     (hydrus) 2d: * DC15 battery life days (5596)
     (hydrus) 2f: C4 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
     (hydrus) 30: 01 dife (subunit=0 tariff=0 storagenr=3)
     (hydrus) 31: 6D vif (Date and time type)
     (hydrus) 32: * 3B178D29 at date (2020-09-13 23:59)
     (hydrus) 36: CC dif (8 digit BCD Instantaneous value storagenr=1)
     (hydrus) 37: 01 dife (subunit=0 tariff=0 storagenr=3)
     (hydrus) 38: 13 vif (Volume l)
     (hydrus) 39: * 31340000 total consumption at date (3.431000 m3)
     (hydrus) 3d: 2F skip
     (hydrus) 3e: 2F skip

     {"media":"warm water","meter":"hydrus","name":"Vatten","id":"65656565","total_m3":3.45,"max_flow_m3h":0,"flow_temperature_c":127,"total_at_date_m3":3.431,"at_date":"2020-09-13 23:59","battery_life_days":"5596","status":"OK","timestamp":"2020-09-27T08:54:42Z"}

     Old style telegram

     (hydrus) 11: 01 dif (8 Bit Integer/Binary Instantaneous value)
     (hydrus) 12: FD vif (Second extension of VIF-codes)
     (hydrus) 13: 08 vife (Access Number (transmission count))
     (hydrus) 14: 30
     (hydrus) 15: 0C dif (8 digit BCD Instantaneous value)
     (hydrus) 16: 13 vif (Volume l)
     (hydrus) 17: * 74110000 total consumption (1.174000 m3)
     (hydrus) 1b: 7C dif (8 digit BCD Value during error state storagenr=1)
     (hydrus) 1c: 13 vif (Volume l)
     (hydrus) 1d: 00000000
     (hydrus) 21: FC dif (8 digit BCD Value during error state storagenr=1)
     (hydrus) 22: 10 dife (subunit=0 tariff=1 storagenr=1)
     (hydrus) 23: 13 vif (Volume l)
     (hydrus) 24: 00000000
     (hydrus) 28: FC dif (8 digit BCD Value during error state storagenr=1)
     (hydrus) 29: 20 dife (subunit=0 tariff=2 storagenr=1)
     (hydrus) 2a: 13 vif (Volume l)
     (hydrus) 2b: 00000000
     (hydrus) 2f: 72 dif (16 Bit Integer/Binary Value during error state storagenr=1)
     (hydrus) 30: 6C vif (Date type G)
     (hydrus) 31: 0000
     (hydrus) 33: 0B dif (6 digit BCD Instantaneous value)
     (hydrus) 34: 3B vif (Volume flow l/h)
     (hydrus) 35: * 000000 max flow (0.000000 m3/h)
     (hydrus) 38: 02 dif (16 Bit Integer/Binary Instantaneous value)
     (hydrus) 39: FD vif (Second extension of VIF-codes)
     (hydrus) 3a: 74 vife (Reserved)
     (hydrus) 3b: * 8713 battery life days (4999)
     (hydrus) 3d: 02 dif (16 Bit Integer/Binary Instantaneous value)
     (hydrus) 3e: 5A vif (Flow temperature 10⁻¹ °C)
     (hydrus) 3f: * 6800 flow temperature (10.400000 °C)
     (hydrus) 41: C4 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
     (hydrus) 42: 01 dife (subunit=0 tariff=0 storagenr=3)
     (hydrus) 43: 6D vif (Date and time type)
     (hydrus) 44: * 3B177F2A at date (2019-10-31 23:59)
     (hydrus) 48: CC dif (8 digit BCD Instantaneous value storagenr=1)
     (hydrus) 49: 01 dife (subunit=0 tariff=0 storagenr=3)
     (hydrus) 4a: 13 vif (Volume l)
     (hydrus) 4b: * 00020000 total consumption at date (0.200000 m3)

     {"media":"water","meter":"hydrus","name":"Votten","id":"64646464","total_m3":1.174,"max_flow_m3h":0,"flow_temperature_c":10.4,"total_at_date_m3":0.2,"at_date":"2019-10-31 23:59","battery_life_days":"4999","status":"OK","timestamp":"2020-09-27T08:54:42Z"}

    */
    /*
      Yet another version telegram:
      (hydrus) 0f: 2f2f decrypt check bytes
      (hydrus) 11: 01 dif (8 Bit Integer/Binary Instantaneous value)
      (hydrus) 12: FD vif (Second extension of VIF-codes)
      (hydrus) 13: 08 vife (Access Number (transmission count))
      (hydrus) 14: 88
      (hydrus) 15: 0C dif (8 digit BCD Instantaneous value)
      (hydrus) 16: 13 vif (Volume l)
      (hydrus) 17: * 45911600 total consumption (169.145000 m3)
      (hydrus) 1b: 8C dif (8 digit BCD Instantaneous value)
      (hydrus) 1c: 10 dife (subunit=0 tariff=1 storagenr=0)
      (hydrus) 1d: 05 vif (Energy 10² Wh)
      (hydrus) 1e: 77900200
      (hydrus) 22: 0B dif (6 digit BCD Instantaneous value)
      (hydrus) 23: 3B vif (Volume flow l/h)
      (hydrus) 24: * 000000 max flow (0.000000 m3/h)
      (hydrus) 27: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (hydrus) 28: 5A vif (Flow temperature 10⁻¹ °C)
      (hydrus) 29: * DD00 flow temperature (22.100000 °C)
      (hydrus) 2b: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (hydrus) 2c: FD vif (Second extension of VIF-codes)
      (hydrus) 2d: 17 vife (Error flags (binary))
      (hydrus) 2e: 0000
      (hydrus) 30: 0A dif (4 digit BCD Instantaneous value)
      (hydrus) 31: A6 vif (Operating time hours)
      (hydrus) 32: 18 vife (?)
      (hydrus) 33: 0000
      (hydrus) 35: 0B dif (6 digit BCD Instantaneous value)
      (hydrus) 36: 26 vif (Operating time hours)
      (hydrus) 37: 255802
      (hydrus) 3a: 2B dif (6 digit BCD Minimum value)
      (hydrus) 3b: 3B vif (Volume flow l/h)
      (hydrus) 3c: 000000
      (hydrus) 3f: C4 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
      (hydrus) 40: 01 dife (subunit=0 tariff=0 storagenr=3)
      (hydrus) 41: 6D vif (Date and time type)
      (hydrus) 42: * 3B179F2A at date (2020-10-31 23:59)
      (hydrus) 46: CC dif (8 digit BCD Instantaneous value storagenr=1)
      (hydrus) 47: 01 dife (subunit=0 tariff=0 storagenr=3)
      (hydrus) 48: 13 vif (Volume l)
      (hydrus) 49: * 14811600 total consumption at date (168.114000 m3)
      (hydrus) 4d: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (hydrus) 4e: 66 vif (External temperature 10⁻¹ °C)
      (hydrus) 4f: D400
      (hydrus) 51: 2F skip
      (hydrus) 52: 2F skip
      (hydrus) 53: 2F skip
      (hydrus) 54: 2F skip
      (hydrus) 55: 2F skip
      (hydrus) 56: 2F skip
      (hydrus) 57: 2F skip
      (hydrus) 58: 2F skip
      (hydrus) 59: 2F skip
      (hydrus) 5a: 2F skip
      (hydrus) 5b: 2F skip
      (hydrus) 5c: 2F skip
      (hydrus) 5d: 2F skip
      (hydrus) 5e: 2F skip
    */

    /* Another one for #216 / #223

	(hydrus) 00: 66 length (102 bytes)
	(hydrus) 01: 44 dll-c (from meter SND_NR)
	(hydrus) 02: 2423 dll-mfct (HYD)
	(hydrus) 04: 28001081 dll-id (81100028)
	(hydrus) 08: 64 dll-version
	(hydrus) 09: 0e dll-type (Bus/System component)
	(hydrus) 0a: 72 tpl-ci-field (EN 13757-3 Application Layer (long tplh))
	(hydrus) 0b: 66567464 tpl-id (64745666)
	(hydrus) 0f: a511 tpl-mfct (DME)
	(hydrus) 11: 70 tpl-version
	(hydrus) 12: 07 tpl-type (Water meter)
	(hydrus) 13: 1f tpl-acc-field
	(hydrus) 14: 00 tpl-sts-field
	(hydrus) 15: 5005 tpl-cfg 0550 (AES_CBC_IV nb=5 cntn=0 ra=0 hc=0 )
	(hydrus) 17: 2f2f decrypt check bytes
	(hydrus) 19: 03 dif (24 Bit Integer/Binary Instantaneous value)
	(hydrus) 1a: 74 vif (Actuality duration seconds)
	(hydrus) 1b: * 111A00 actuality duration (-0.000029 s)
	(hydrus) 1e: 0C dif (8 digit BCD Instantaneous value)
	(hydrus) 1f: 13 vif (Volume l)
	(hydrus) 20: * 91721300 total consumption (137.291000 m3)
	(hydrus) 24: 8C dif (8 digit BCD Instantaneous value)
	(hydrus) 25: 10 dife (subunit=0 tariff=1 storagenr=0)
	(hydrus) 26: 13 vif (Volume l)
	(hydrus) 27: * 00000000 total consumption at tariff 1 (0.000000 m3)
	(hydrus) 2b: 8C dif (8 digit BCD Instantaneous value)
	(hydrus) 2c: 20 dife (subunit=0 tariff=2 storagenr=0)
	(hydrus) 2d: 13 vif (Volume l)
	(hydrus) 2e: * 91721300 total consumption at tariff 2 (137.291000 m3)
	(hydrus) 32: 0B dif (6 digit BCD Instantaneous value)
	(hydrus) 33: 3B vif (Volume flow l/h)
	(hydrus) 34: * 000000 max flow (0.000000 m3/h)
	(hydrus) 37: 0B dif (6 digit BCD Instantaneous value)
	(hydrus) 38: 26 vif (Operating time hours)
	(hydrus) 39: * 784601 operating time (14678.000000 h)
	(hydrus) 3c: 02 dif (16 Bit Integer/Binary Instantaneous value)
	(hydrus) 3d: 5A vif (Flow temperature 10⁻¹ °C)
	(hydrus) 3e: * F500 flow temperature (24.500000 °C)
	(hydrus) 40: 02 dif (16 Bit Integer/Binary Instantaneous value)
	(hydrus) 41: 66 vif (External temperature 10⁻¹ °C)
	(hydrus) 42: * EF00 external temperature (23.900000 °C)
	(hydrus) 44: 04 dif (32 Bit Integer/Binary Instantaneous value)
	(hydrus) 45: 6D vif (Date and time type)
	(hydrus) 46: * 1B08B721 current date (2021-01-23 08:27)
	(hydrus) 4a: 4C dif (8 digit BCD Instantaneous value storagenr=1)
	(hydrus) 4b: 13 vif (Volume l)
	(hydrus) 4c: * 38861200 total consumption at date (128.638000 m3)
	(hydrus) 50: CC dif (8 digit BCD Instantaneous value storagenr=1)
	(hydrus) 51: 10 dife (subunit=0 tariff=1 storagenr=1)
	(hydrus) 52: 13 vif (Volume l)
	(hydrus) 53: * 00000000 total consumption at tariff 1 at date (0.000000 m3)
	(hydrus) 57: CC dif (8 digit BCD Instantaneous value storagenr=1)
	(hydrus) 58: 20 dife (subunit=0 tariff=2 storagenr=1)
	(hydrus) 59: 13 vif (Volume l)
	(hydrus) 5a: * 38861200 total consumption at tariff 1 at date (128.638000 m3)
	(hydrus) 5e: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
	(hydrus) 5f: 6C vif (Date type G)
	(hydrus) 60: * 9F2C at date (2020-12-31 00:00)
	(hydrus) 62: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
	(hydrus) 63: EC vif (Date type G)
	(hydrus) 64: 7E vife (additive correction constant: unit of VIF * 10^-1)
	(hydrus) 65: BF2C
    */

    /* And another two more that seem to be branded IZAR RS 868 */
    /*
      (hydrus) 00: 1e length (30 bytes)
      (hydrus) 01: 44 dll-c (from meter SND_NR)
      (hydrus) 02: 2423 dll-mfct (HYD)
      (hydrus) 04: 79738960 dll-id (60897379)
      (hydrus) 08: 8b dll-version
      (hydrus) 09: 07 dll-type (Water meter)
      (hydrus) 0a: 7a tpl-ci-field (EN 13757-3 Application Layer (short tplh))
      (hydrus) 0b: 8f tpl-acc-field
      (hydrus) 0c: 00 tpl-sts-field (OK)
      (hydrus) 0d: 107d tpl-cfg 7d10 (accessibility synchronous SPECIFIC_16_31 )
      (hydrus) 0f: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (hydrus) 10: 13 vif (Volume l)
      (hydrus) 11: * 12170100 total consumption (71.442000 m3)
      (hydrus) 15: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
      (hydrus) 16: 6C vif (Date type G)
      (hydrus) 17: * BF23 at date (2021-03-31 00:00)
      (hydrus) 19: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
      (hydrus) 1a: 13 vif (Volume l)
      (hydrus) 1b: * 44100100 total consumption at date (69.700000 m3)

      (hydrus) 01: 44 dll-c (from meter SND_NR)
      (hydrus) 02: 2423 dll-mfct (HYD)
      (hydrus) 04: 20479060 dll-id (60904720)
      (hydrus) 08: 8b dll-version
      (hydrus) 09: 06 dll-type (Warm Water (30°C-90°C) meter)
      (hydrus) 0a: 7a tpl-ci-field (EN 13757-3 Application Layer (short tplh))
      (hydrus) 0b: 2a tpl-acc-field
      (hydrus) 0c: 00 tpl-sts-field (OK)
      (hydrus) 0d: 10d8 tpl-cfg d810 (bidirectional accessibility SPECIFIC_16_31 )
      (hydrus) 0f: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (hydrus) 10: 13 vif (Volume l)
      (hydrus) 11: * DDC00000 total consumption (49.373000 m3)
      (hydrus) 15: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
      (hydrus) 16: 6C vif (Date type G)
      (hydrus) 17: * BF23 at date (2021-03-31 00:00)
      (hydrus) 19: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
      (hydrus) 1a: 13 vif (Volume l)
      (hydrus) 1b: * 82BB0000 total consumption at date (48.002000 m3)
    */

    int offset;
    string key;
    struct tm datetime;

    // Container 0 : current / total

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 1, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_tariff1_m3_);
        t->addMoreExplanation(offset, " total consumption at tariff 1 (%f m3)", total_water_consumption_tariff1_m3_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 2, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_tariff2_m3_);
        t->addMoreExplanation(offset, " total consumption at tariff 2 (%f m3)", total_water_consumption_tariff2_m3_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::VolumeFlow, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &max_flow_m3h_);
        t->addMoreExplanation(offset, " max flow (%f m3/h)", max_flow_m3h_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::FlowTemperature, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &flow_temperature_c_);
        t->addMoreExplanation(offset, " flow temperature (%f °C)", flow_temperature_c_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::ExternalTemperature, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &external_temperature_c_);
        t->addMoreExplanation(offset, " external temperature (%f °C)", external_temperature_c_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
        extractDVdate(&t->values, key, &offset, &datetime);
        current_date_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " current date (%s)", current_date_.c_str());
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::ActualityDuration, 0, 0, &key, &t->values)) {
        extractDVuint24(&t->values, key, &offset, &actuality_duration_s_);
        t->addMoreExplanation(offset, " actuality duration (%f s)", actuality_duration_s_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::OperatingTime, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &operating_time_h_);
        t->addMoreExplanation(offset, " operating time (%f h)", operating_time_h_);
    }

    // Container 1/3 : past/future records

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 1, 0, &key, &t->values)
     || findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 3, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_at_date_m3_);
        t->addMoreExplanation(offset, " total consumption at date (%f m3)", total_water_consumption_at_date_m3_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 1, 1, &key, &t->values)
     || findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 3, 1, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_tariff1_at_date_m3_);
        t->addMoreExplanation(offset, " total consumption at tariff 1 at date (%f m3)", total_water_consumption_tariff1_at_date_m3_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 1, 2, &key, &t->values)
     || findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 3, 2, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_tariff2_at_date_m3_);
        t->addMoreExplanation(offset, " total consumption at tariff 1 at date (%f m3)", total_water_consumption_tariff2_at_date_m3_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Date    , 1, 0, &key, &t->values)
     || findKey(MeasurementType::Instantaneous, ValueInformation::DateTime, 3, 0, &key, &t->values)) {
        extractDVdate(&t->values, key, &offset, &datetime);
        at_date_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " at date (%s)", at_date_.c_str());
    }

    // TODO: a date in the future is also transmitted with VIFE 7E in container 1

    // custom

    uint16_t days {};
    if (hasKey(&t->values, "02FD74") && extractDVuint16(&t->values, "02FD74", &offset, &days))
    {
        remaining_battery_life_year_ = ((double)days) / 365.25;
        t->addMoreExplanation(offset, " battery life (%d days %f years)", days, remaining_battery_life_year_);
    }

    status_ = decodeTPLStatusByte(t->tpl_sts, &error_codes_);
}

double MeterHydrus::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

double MeterHydrus::totalWaterConsumptionTariff1(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_tariff1_m3_, Unit::M3, u);
}

double MeterHydrus::totalWaterConsumptionTariff2(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_tariff2_m3_, Unit::M3, u);
}

double MeterHydrus::totalWaterConsumptionAtDate(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_at_date_m3_, Unit::M3, u);
}

double MeterHydrus::totalWaterConsumptionTariff1AtDate(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_tariff1_at_date_m3_, Unit::M3, u);
}

double MeterHydrus::totalWaterConsumptionTariff2AtDate(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_tariff2_at_date_m3_, Unit::M3, u);
}

bool MeterHydrus::hasTotalWaterConsumption()
{
    return true;
}

double MeterHydrus::maxFlow(Unit u)
{
    assertQuantity(u, Quantity::Flow);
    return convert(max_flow_m3h_, Unit::M3H, u);
}

bool MeterHydrus::hasMaxFlow()
{
    return true;
}

double MeterHydrus::flowTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(flow_temperature_c_, Unit::C, u);
}

double MeterHydrus::externalTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(external_temperature_c_, Unit::C, u);
}
