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
    double energy_past_month_date_kwh_[13] {}; // 13 past month readings, storagenr 2 to 14
    string last_month_date_txt_;
    double energy_at_key_date_kwh_ {}; // billing date reading, storagenr 1
    string key_date_txt_;
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
        "energy_at_key_date",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::AnyEnergyVIF,
        StorageNr(1),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The total energy consumption recorded at key (billing) date",
        SET_FUNC(energy_at_key_date_kwh_, Unit::KWH),
        GET_FUNC(energy_at_key_date_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "key_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        VIFRange::DateTime,
        StorageNr(1),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The key (billing) date",
        SET_STRING_FUNC(key_date_txt_),
        GET_STRING_FUNC(key_date_txt_));

    addNumericFieldWithExtractor(
        "energy_last_month",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::AnyEnergyVIF,
        StorageNr(2),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The total energy consumption recorded by this meter at end of last month.",
        SET_FUNC(energy_past_month_date_kwh_[0], Unit::KWH),
        GET_FUNC(energy_past_month_date_kwh_[0], Unit::KWH));

        for (int i=3; i<=14; ++i)
        {
            string key, info;
            strprintf(key, "energy_%d_months_back", i-1);
            strprintf(info, "Energy consumption %d months back.", i-1);

            addNumericFieldWithExtractor(
                key,
                Quantity::Energy,
                NoDifVifKey,
                VifScaling::Auto,
                MeasurementType::Instantaneous,
                VIFRange::AnyEnergyVIF,
                StorageNr(i),
                TariffNr(0),
                IndexNr(1),
                PrintProperty::JSON,
                info,
                SET_FUNC(energy_past_month_date_kwh_[i-2], Unit::KWH),
                GET_FUNC(energy_past_month_date_kwh_[i-2], Unit::KWH));
        }

    addStringFieldWithExtractor(
        "last_month_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        VIFRange::DateTime,
        StorageNr(2),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The date of end of last month.",
        SET_STRING_FUNC(last_month_date_txt_),
        GET_STRING_FUNC(last_month_date_txt_));

}

// Test: Heat qheat_55_us 70835484 NOKEY
// telegram=9644a732845483700a047ae70000200274fc00046d230Bd3250c0605920000446d3B17Bf2c4c068251000084016d3B17de248c010605900000cc0106988500008c020629770000cc0206226600008c030682510000cc0306933600008c040602260000cc0406691800008c050633140000cc0506900900008c060618020000cc0606000000008c0706000000003c22000000000f001000
// {"media":"heat","meter":"qheat_55_us","name":"Heat","id":"70835484","device_date_time":"2022-05-19 11:35","total_energy_consumption_kwh":9205,"energy_at_key_date_kwh":5182,"key_date":"2021-12-31 23:59","energy_last_month_kwh":9005,"energy_2_months_back_kwh":8598,"energy_3_months_back_kwh":7729,"energy_4_months_back_kwh":6622,"energy_5_months_back_kwh":5182,"energy_6_months_back_kwh":3693,"energy_7_months_back_kwh":2602,"energy_8_months_back_kwh":1869,"energy_9_months_back_kwh":1433,"energy_10_months_back_kwh":990,"energy_11_months_back_kwh":218,"energy_12_months_back_kwh":0,"energy_13_months_back_kwh":0,"last_month_date":"2022-04-30 23:59","timestamp":"2022-05-22T13:34:58Z"}
// |Heat;70835484;9205.000000;5182.000000;9005.000000;2022-05-22 13:34.36
