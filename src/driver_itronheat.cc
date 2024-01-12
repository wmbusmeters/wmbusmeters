/*
 Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)
               2023 Andreas Horrer (gpl-3.0-or-later)

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

namespace
{
    struct Driver : public virtual MeterCommonImplementation
    {
        Driver(MeterInfo &mi, DriverInfo &di);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("itronheat");
        di.addNameAlias("ultramaxx");
        di.setDefaultFields("name,id,status,total_energy_consumption_kwh,total_volume_m3,timestamp");
        di.setMeterType(MeterType::HeatMeter);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_ITW, 0x04, 0x00);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalCommonFields("meter_datetime");

        addStringFieldWithExtractorAndLookup(
            "status",
            "Meter status from error flags and tpl status field.",
            DEFAULT_PRINT_PROPERTIES  |
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags),
            Translate::Lookup(
                {
                    {
                        {
                            "ERROR_FLAGS",
                            Translate::Type::BitToString,
                            AlwaysTrigger, MaskBits(0xffff),
                            "OK",
                            {
                            }
                        },
                    },
                }));

        addNumericFieldWithExtractor(
            "total_energy_consumption",
            "The total heat energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            );

        addNumericFieldWithExtractor(
            "total_volume",
            "The total heating media volume recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addStringFieldWithExtractor(
            "last_year_date",
             "Last day previous year when total energy consumption was recorded.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(StorageNr(1))
            .set(VIFRange::Date)
            );

        addNumericFieldWithExtractor(
            "last_year_energy_consumption",
            "The total energy consumption recorded at the last day of the previous year.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(StorageNr(1))
            .set(VIFRange::AnyEnergyVIF)
            );
    }
}

// Test: Heat itronheat 23340485 NOKEY
// Comment: Allmess UltraMaXX with ITRON EquaScan hMIU RF Module
// telegram=|444497268504342300047AD00030A52F2F_04062C0100000C1429270000066D2D130AE12B007406FEFEFEFE426C1F010D7FEB0E00000006040C995500372F2F0C7951622223|
// {"id":"23340485","media":"heat","meter":"itronheat","meter_datetime":"2023-11-01 10:19:45","name":"Heat","status":"OK","timestamp":"1111-11-11T11:11:11Z","total_energy_consumption_kwh":300,"total_volume_m3":27.29,"last_year_date":"2000-01-31"}
// |Heat;23340485;OK;300;27.29;1111-11-11 11:11.11

// Test: Heat_lower_case itronheat 11755757 NOKEY
// Comment: Itron Heatmeter with strange MFCT iTW instead of ITW
// telegram=|_404497A65757751100047A040000A00406494C0000041348318000066D2B0ED7EE2A0044069A360000426CDF2C0D7FEB0700000006040C5C7E034E0C7942502321|
// {"id":"11755757","media":"heat","meter":"itronheat","meter_datetime":"2023-10-14 23:14:43","name":"Heat_lower_case","status":"OK","timestamp":"1111-11-11T11:11:11Z","total_energy_consumption_kwh":19529,"total_volume_m3":8401.224,"last_year_date":"2022-12-31","last_year_energy_consumption_kwh": 13978}
// |Heat_lower_case;11755757;OK;19529;8401.224;1111-11-11 11:11.11

