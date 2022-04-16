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

struct MeterVario451Mid : public virtual MeterCommonImplementation
{
    MeterVario451Mid(MeterInfo &mi, DriverInfo &di);

private:

    double total_energy_consumption_kwh_ {};
    double energy_at_set_date_kwh_ {};
    string set_date_txt_;
    double energy_at_old_date_kwh_ {};
    string old_date_txt_;
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("vario451mid");
    di.setMeterType(MeterType::HeatMeter);
    // Mode 7
    di.addLinkMode(LinkMode::T1);
    di.addDetection(MANUFACTURER_TCH, 0x04,  0x17);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterVario451Mid(mi, di)); });
});

MeterVario451Mid::MeterVario451Mid(MeterInfo &mi, DriverInfo &di) :  MeterCommonImplementation(mi, di)
{
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
        VIFRange::Date,
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
        StorageNr(8),
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
        VIFRange::Date,
        StorageNr(8),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The last billing set date.",
        SET_STRING_FUNC(set_date_txt_),
        GET_STRING_FUNC(set_date_txt_));

}

// Test: Heato vario451mid 94430412 NOKEY
// telegram=734468501204439417048c0084900f002c2536700000B767B64527c50ac67a33005007102f2f8404062846000082046c9f2c8d04861f1e72fe00000000000000000000000000000000000000000000000000000000440600000000426cffff0406c94700002f2f2f2f2f2f2f2f2f2f2f2f2f2f2f
// {"media":"heat","meter":"vario451mid","name":"Heato","id":"94430412","total_energy_consumption_kwh":18377,"energy_at_old_date_kwh":0,"old_date":"2127-15-31","energy_at_set_date_kwh":17960,"set_date":"2020-12-31","timestamp":"1111-11-11T11:11:11Z"}
// |Heato;94430412;18377.000000;0.000000;17960.000000;1111-11-11 11:11.11
