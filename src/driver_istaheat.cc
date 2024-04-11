/*
 Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)
 Copyright (C) 2024 Arthur van Dorp (gpl-3.0-or-later)

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

// ista sensonic 3 heat meter sending on c1, product number 4030020.
// (Can also be used for cold metering, I don't think the device I own is configured for that use case.)
// (Attention device owners: Do not click randomly on the single button of the meter. If you are in the
// wireless service loop and click wrongly, you will activate istas proprietary wireless protocol. Only ista
// will be able to reset the device and activate the wireless m-bus. To activate the wireless m-bus you have
// to long click until 2A is shown, then wait shortly, click *once", wait until 2B is shown, wait again,
// single click until 2C is shown, then double click. Do not double click in 2A or 2B.)

// The device measures every 8s, but sends wireless m-bus telegrams every 4 minutes.
// AES key has to be obtained from your contractor or directly from ista.


#include"meters_common_implementation.h"

namespace
{
    struct Driver : public virtual MeterCommonImplementation
    {
        Driver(MeterInfo &mi, DriverInfo &di);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("istaheat");
        di.setDefaultFields("name,id,status,total_energy_consumption_kwh,total_volume_at_end_last_month_m3,"
        "consumption_previous_month_period_kwh,meter_month_period_end_date,"
        "consumption_previous_year_period_kwh,meter_year_period_end_date,"
        "timestamp");
        di.setMeterType(MeterType::HeatMeter);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_IST, 0x04, 0xa9);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("meter_datetime,model_version,parameter_set");
        addOptionalLibraryFields("flow_temperature_c,return_temperature_c");

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
                            Translate::MapType::BitToString,
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
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            );

        addNumericFieldWithExtractor(
            "total_volume_at_end_last_month",
            "The total heating media volume recorded by this meter at the end of last month.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "consumption_previous_month_period",
            "The total heat energy for the previous month period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractor(
            "meter_month_period_end_date",
            "Meter date for month period end.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(2))
            );
        
        addNumericFieldWithExtractor(
            "consumption_previous_year_period",
            "The total heat energy for the previous year period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(2))
            );
        
        addStringFieldWithExtractor(
            "meter_year_period_end_date",
            "Meter date for year period end.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );
    }
}
// Test: HeatItUp istaheat 33503169 NOKEY
// telegram=|5344742669315033A9048C2070900F002C25961200009D6949E80EB1E2707A96003007102F2F_0C0500000000426C00004C050000000082016CFE298C0105000000008C0115000000002F2F2F2F2F2F2F2F2F2F2F|
// {"consumption_previous_month_period_kwh":0,"consumption_previous_year_period_kwh":0,"id":"33503169","media":"heat","meter":"istaheat","meter_month_period_end_date":"2023-09-30","meter_year_period_end_date":"2000-00-00","name":"HeatItUp","total_energy_consumption_kwh":0,"total_volume_at_end_last_month_m3":0,"status":"OK","timestamp":"1111-11-11T11:11:11Z"}
// |HeatItUp;33503169;OK;0;0;0;2023-09-30;0;2000-00-00;1111-11-11 11:11.11

// Test: FeelTheHeat istaheat 44503169 NOKEY
// telegram=|5344742669315044A9048C2017900F002C253DCD0000CE827C98B4346AB67A3D003007102F2F_0C0514980400426CFF2C4C052061020082016C1F318C0105581604008C0115142800002F2F2F2F2F2F2F2F2F2F2F|
// {"consumption_previous_month_period_kwh":2612,"consumption_previous_year_period_kwh":4165.8,"id":"44503169","media":"heat","meter":"istaheat","meter_month_period_end_date":"2024-01-31","meter_year_period_end_date":"2023-12-31","name":"FeelTheHeat","total_energy_consumption_kwh":4981.4,"total_volume_at_end_last_month_m3":281.4,"status":"OK","timestamp":"1111-11-11T11:11:11Z"}
// |FeelTheHeat;44503169;OK;4981.4;281.4;2612;2024-01-31;4165.8;2023-12-31;1111-11-11 11:11.11
