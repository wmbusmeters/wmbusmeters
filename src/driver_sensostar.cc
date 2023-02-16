/*
 Copyright (C) 2020-2022 Patrick Schwarz (gpl-3.0-or-later)
 Copyright (C)      2022 Fredrik Öhrström (gpl-3.0-or-later)

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
    struct Driver : public virtual MeterCommonImplementation {
        Driver(MeterInfo &mi, DriverInfo &di);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("sensostar");
        di.setDefaultFields("name,id,total_kwh,total_water_m3,current_status,reporting_date,energy_consumption_at_reporting_date_kwh,timestamp");
        di.setMeterType(MeterType::HeatMeter);
        di.addLinkMode(LinkMode::C1);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_EFE, 0x04,  0x00);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractor(
            "meter_timestamp",
            "Date time for this reading.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        addNumericFieldWithExtractor(
            "total",
            "The total energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            );

        addNumericFieldWithExtractor(
            "power",
            "The active power consumption.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            );

        addNumericFieldWithExtractor(
            "power_max",
            "The maximum power consumption over ?period?.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::AnyPowerVIF)
            );

        addNumericFieldWithExtractor(
            "flow_water",
            "The flow of water.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)
            );

        addNumericFieldWithExtractor(
            "flow_water_max",
            "The maximum forward flow of water over a ?period?.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::VolumeFlow)
            );

        addNumericFieldWithExtractor(
            "forward",
            "The forward temperature of the water.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
            );

        addNumericFieldWithExtractor(
            "return",
            "The return temperature of the water.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ReturnTemperature)
            );

        addNumericFieldWithExtractor(
            "difference",
            "The temperature difference forward-return for the water.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::AutoSigned,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::TemperatureDifference)
            );

        addNumericFieldWithExtractor(
            "total_water",
            "The total amount of water that has passed through this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addStringFieldWithExtractorAndLookup(
            "current_status",
            "Status and error flags.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(VIFRange::ErrorFlags),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::Type::BitToString,
                        AlwaysTrigger, MaskBits(0xff),
                        "OK",
                        {
                            // based on information published here: https://www.engelmann.de/wp-content/uploads/2022/10/1080621004_2022-10-12_BA_S3_ES_Comm_en.pdf
                            { 0x01 , "ERROR_TEMP_SENSOR_1_CABLE_BREAK" },
                            { 0x02 , "ERROR_TEMP_SENSOR_1_SHORT_CIRCUIT" },
                            { 0x04 , "ERROR_TEMP_SENSOR_2_CABLE_BREAK" },
                            { 0x08 , "ERROR_TEMP_SENSOR_2_SHORT_CIRCUIT" },
                            { 0x10 , "ERROR_FLOW_MEASUREMENT_SYSTEM_ERROR" },
                            { 0x20 , "ERROR_ELECTRONICS_DEFECT" },
                            { 0x40 , "OK_INSTRUMENT_RESET" },
                            { 0x80 , "OK_BATTERY_LOW" }
                        }
                    },
                },
            });

        addStringFieldWithExtractor(
            "reporting_date",
                "The reporting date of the last billing period.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "energy_consumption_at_reporting_date",
            "The energy consumption at the last billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(1))
            );

        for (int i=1; i<=30; ++i)
        {
            string name, info;
            strprintf(&name, "enegry_consumption_semi_monthly_%02d", i);
            strprintf(&info, "Energy consumption at semi-monthly position %02d.", i);
            addNumericFieldWithExtractor(
                name,
                info,
                DEFAULT_PRINT_PROPERTIES,
                Quantity::Energy,
                VifScaling::Auto,
                FieldMatcher::build()
                .set(MeasurementType::Instantaneous)
                .set(VIFRange::AnyEnergyVIF)
                .set(StorageNr(i+1)));
        }
    }
}

// Test: Heat sensostar 20480057 NOKEY
// Comment:
// telegram=|68B3B36808007257004820c51400046c100000047839803801040600000000041300000000042B00000000142B00000000043B00000000143B00000000025B1400025f15000261daff0>
// {"media":"heat","meter":"sensostar","name":"Heat","id":"20480057","meter_timestamp":"2022-04-28 13:44","total_kwh":0,"power_kw":0,"power_max_kw":0,"flow_wate>
// |Heat;20480057;0;0;ERROR_FLOW_MEASUREMENT_SYSTEM_ERROR;2000-00-00;0;1111-11-11 11:11.11

// Test: WMZ sensostar 02752560 NOKEY
// Comment: from "Sensostar U"
// telegram=a444c5146025750200047ac20000202f2f046d2e26c62a040643160000041310f0050001fd1700426cbf2c4406570e00008401061f160000840206f6150000840306f5150000840406f31>
// {"media":"heat","meter":"sensostar","name":"WMZ","id":"02752560","meter_timestamp":"2022-10-06 06:46","total_kwh":5699,"total_water_m3":389.136,"current_statu>
// WMZ;02752560;5699;389.136000;OK;1111-11-11 11:11.11

// Test: FBH1OG sensostar 24659380 NOKEY
// Comment: Engelmann Sensostar U, Monthly storage, logging in first semi-month
// Telegram: |BE44C5148093652400048C20BF900F002C25E51E0000AB9F24F74DB68ACA7ABF009007102F2F_046D152CEF220406970200000413FAC6000001FD1700426CDF2C44060000000084010>
// {"media":"heat","meter":"sensostar","name":"FBH1OG","id":"24659380","meter_timestamp":"2023-02-15 12:21","total_kwh":663,"total_water_m3":50.938,"current_sta>

// Test: FBH1OG sensostar 24659380 NOKEY
// Comment: Engelmann Sensostar U, Monthly storage, logging in second semi-month
// Telegram: |BE44C5148093652400048C201E900F002C25612000002D7355D0545637677A1E009007102F2F_046D022DF0220406AA02000004134CCA000001FD1700426CDF2C440600000000C4010>
// {"media":"heat","meter":"sensostar","name":"FBH1OG","id":"24659380","meter_timestamp":"2023-02-16 13:02","total_kwh":682,"total_water_m3":51.788,"current_sta>
