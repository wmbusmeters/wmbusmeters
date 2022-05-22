/*
 Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"meters_common_implementation.h"

struct MeterQheat55US : public virtual MeterCommonImplementation
{
    MeterQheat55US(MeterInfo &mi, DriverInfo &di);

private:

    double total_energy_consumption_kwh_ {};
    double energy_at_set_date_kwh_ {};
    string set_date_txt_;
    double energy_at_old_date_kwh_ {};
    string old_date_txt_;
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("qheat_55_us");
    di.setMeterType(MeterType::HeatMeter);
    
    di.addLinkMode(LinkMode::C1);
    di.addDetection(MANUFACTURER_LUG, 0x04,  0x0a);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterQheat55US(mi, di)); });
});

MeterQheat55US::MeterQheat55US(MeterInfo &mi, DriverInfo &di) :  MeterCommonImplementation(mi, di)
{
    addStringFieldWithExtractor(
        "device_date_time",
        "Device date time.",
        PrintProperty::JSON,
        FieldMatcher::build()
        .set(MeasurementType::Instantaneous)
        .set(VIFRange::DateTime)
        );

    addNumericFieldWithExtractor(
        "total_energy_consumption",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::AnyEnergyVIF,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
        "The total energy consumption recorded by this meter.",
        SET_FUNC(total_energy_consumption_kwh_, Unit::KWH),
        GET_FUNC(total_energy_consumption_kwh_, Unit::KWH));

    addNumericFieldWithExtractor(
        "energy_at_old_date",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::AnyEnergyVIF,
        StorageNr(1),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The total energy consumption recorded when?",
        SET_FUNC(energy_at_old_date_kwh_, Unit::KWH),
        GET_FUNC(energy_at_old_date_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "old_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        VIFRange::DateTime,
        StorageNr(1),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The last billing old date?",
        SET_STRING_FUNC(old_date_txt_),
        GET_STRING_FUNC(old_date_txt_));

    addNumericFieldWithExtractor(
        "energy_at_set_date",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::AnyEnergyVIF,
        StorageNr(2),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The total energy consumption recorded by this meter at the due date.",
        SET_FUNC(energy_at_set_date_kwh_, Unit::KWH),
        GET_FUNC(energy_at_set_date_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "set_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        VIFRange::DateTime,
        StorageNr(2),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The last billing set date.",
        SET_STRING_FUNC(set_date_txt_),
        GET_STRING_FUNC(set_date_txt_));

}

// Test: Heat qheat_55_us 70835484 NOKEY
// telegram=9644a732845483700a047ae70000200274fc00046d230Bd3250c0605920000446d3B17Bf2c4c068251000084016d3B17de248c010605900000cc0106988500008c020629770000cc0206226600008c030682510000cc0306933600008c040602260000cc0406691800008c050633140000cc0506900900008c060618020000cc0606000000008c0706000000003c22000000000f001000
// {"media":"heat","meter":"qheat_55_us","name":"Heat","id":"70835484","total_energy_consumption_kwh":9205,"energy_at_old_date_kwh":5182,"old_date":"2021-12-31 23:59","energy_at_set_date_kwh":9005,"set_date":"2022-04-30 23:59","timestamp":"2022-05-19T10:49:42Z","device":"rtlwmbus[00000001]","rssi_dbm":140}
// |Heat;70835484;9205.000000;5182.000000;9005.000000;2022-05-22 09:48.11
