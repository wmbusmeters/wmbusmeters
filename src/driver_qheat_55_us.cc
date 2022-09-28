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

#include "meters_common_implementation.h"

namespace
{
    struct Driver : public virtual MeterCommonImplementation
    {
        Driver(MeterInfo &mi, DriverInfo &di);

        // Telegrams contain the following values:

        // device_date_time - current device date and time
        // total_energy_consumption_kwh - current reading

        // key_date - billing date, storagenr 1
        // key_date_kwh - billing date reading, storagenr 1

        // prev_month - date of the previous month in storagenr 2
        // prev_month_kwh[13] - 13 past month readings, storagenr 2 to 14
    };

    static bool ok = registerDriver([](DriverInfo &di)
        {
            di.setName("qheat_55_us");
            di.setDefaultFields("name,id,total_energy_consumption_kwh,key_date_kwh,timestamp");
            di.setMeterType(MeterType::HeatMeter);

            di.addLinkMode(LinkMode::C1);
            di.addDetection(MANUFACTURER_LUG, 0x04,  0x0a);
            di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
        });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
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
            "The total energy consumption recorded by this meter.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
                .set(MeasurementType::Instantaneous)
                .set(VIFRange::AnyEnergyVIF)
                .set(StorageNr(0))
                .set(TariffNr(0))
                .set(IndexNr(1))
            );

        addStringFieldWithExtractor(
            "key_date",
            "The key (billing) date",
            PrintProperty::JSON,
            FieldMatcher::build()
                .set(MeasurementType::Instantaneous)
                .set(VIFRange::DateTime)
                .set(StorageNr(1))
                .set(TariffNr(0))
                .set(IndexNr(1))
            );

        addNumericFieldWithExtractor(
            "key_date",
            "The total energy consumption recorded at key (billing) date",
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
                .set(MeasurementType::Instantaneous)
                .set(VIFRange::AnyEnergyVIF)
                .set(StorageNr(1))
                .set(TariffNr(0))
                .set(IndexNr(1))
            );

        addStringFieldWithExtractor(
            "prev_month",
            "The date of end of last month.",
            PrintProperty::JSON,
            FieldMatcher::build()
                .set(MeasurementType::Instantaneous)
                .set(VIFRange::DateTime)
                .set(StorageNr(2))
                .set(TariffNr(0))
                .set(IndexNr(1))
            );

        for (int i = 1; i <= 13; ++i)
        {
            string key, info;
            strprintf(key, "prev_%d_month", i);
            strprintf(info, "Energy consumption %d months back.", i);

            addNumericFieldWithExtractor(
                key,
                info,
                PrintProperty::JSON,
                Quantity::Energy,
                VifScaling::Auto,
                FieldMatcher::build()
                    .set(MeasurementType::Instantaneous)
                    .set(VIFRange::AnyEnergyVIF)
                    .set(StorageNr(i + 1))
                    .set(TariffNr(0))
                    .set(IndexNr(1))
                );
        }

        addNumericFieldWithExtractor(
            "actuality_duration",
            "The time between the measurement and the sending of this telegram.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Time,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ActualityDuration)
            );

        addNumericFieldWithExtractor(
            "time_without_measurement",
            "How long the meter has been in an error state and unable to measure values, while powered up.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Time,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::AtError)
            .set(VIFRange::OnTime)
            );

    }
}
// Test: Heat qheat_55_us 70835484 NOKEY
// telegram=9644a732845483700a047ae70000200274fc00046d230Bd3250c0605920000446d3B17Bf2c4c068251000084016d3B17de248c010605900000cc0106988500008c020629770000cc0206226600008c030682510000cc0306933600008c040602260000cc0406691800008c050633140000cc0506900900008c060618020000cc0606000000008c0706000000003c22000000000f001000
// {"media":"heat","meter":"qheat_55_us","name":"Heat","id":"70835484","device_date_time":"2022-05-19 11:35","total_energy_consumption_kwh":9205,"key_date":"2021-12-31 23:59","key_date_kwh":5182,"prev_month":"2022-04-30 23:59","prev_1_month_kwh":9005,"prev_2_month_kwh":8598,"prev_3_month_kwh":7729,"prev_4_month_kwh":6622,"prev_5_month_kwh":5182,"prev_6_month_kwh":3693,"prev_7_month_kwh":2602,"prev_8_month_kwh":1869,"prev_9_month_kwh":1433,"prev_10_month_kwh":990,"prev_11_month_kwh":218,"prev_12_month_kwh":0,"prev_13_month_kwh":0,"actuality_duration_h":0.07,"time_without_measurement_h":0,"timestamp":"1111-11-11T11:11:11Z"}
// |Heat;70835484;9205;5182;1111-11-11 11:11.11
