/*
 Copyright (C) 2020-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("ei6500");
        di.setMeterType(MeterType::SmokeDetector);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_EIE, 0x1a, 0x0c);
        di.addMfctTPLStatusBits(
            {
                {
                    {
                        "TPL_STS",
                        Translate::Type::BitToString,
                        0xe0, // Always use 0xe0 for tpl mfct status bits.
                        "OK",
                        {
                            { 0x20, "FLOW_MEASUREMENT_ERROR"},
                            { 0x40, "TEMPERATURE_MEASUREMENT_ERROR"}
                            /* Or should this be:
                               bit 0x40 RTC invalid
                            */
                        }
                    },
                },
            });
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "status",
            "Status and error flags.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT |
            PrintProperty::STATUS | PrintProperty::JOIN_TPL_STATUS,
            FieldMatcher::build()
            .set(VIFRange::ErrorFlags),
            Translate::Lookup(
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::Type::BitToString,
                        0xffff,
                        "OK",
                        {
                            { 0x0001, "NOT_INSTALLED", TestBit::NotSet },
                            { 0x0002, "ENVIRONMENT_CHANGED" },
                            { 0x0040, "REMOVED" },
                            { 0x0080, "LOW_BATTERY" },
                            { 0x0100, "OBSTACLE_DETECTED" },
                            { 0x0200, "COVERING_DETECTED" }
                            /* Or should this be
                               bit  7-7 Not used, zero by default
                               bit  8 Application error, unknown field C
                               bit  9 Application error, unknown field CI
                               bit 10 Application error, unknown record
                               bit 11 Application error, access right
                               bit 12 Application error, record size
                               bit 13 Application error, record value
                               bit 14 Application error, bad password
                               bit 15 Not used, zero by defaul
                            */
                        }
                    },
                },
            }));

        addStringFieldWithExtractor(
            "last_alarm_date",
            "Date when the smoke alarm last triggered.",
            PrintProperty::FIELD | PrintProperty::JSON | PrintProperty::IMPORTANT,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(SubUnitNr(1))
            .set(TariffNr(1))
            .set(VIFRange::Date)
            );

        addNumericFieldWithExtractor(
            "alarm",
            "Number of times the smoke alarm has triggered.",
            PrintProperty::FIELD | PrintProperty::JSON | PrintProperty::IMPORTANT,
            Quantity::Counter,
            VifScaling::None,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(SubUnitNr(1))
            .set(TariffNr(1))
            .set(VIFRange::CumulationCounter)
            );

        addStringFieldWithExtractor(
            "software_version",
            "Meter software version number.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::SoftwareVersion)
            );

        addStringFieldWithExtractor(
            "message_datetime",
            "Device date time.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        addNumericFieldWithExtractor(
            "duration_removed",
            "Time the smoke alarm has been removed.",
            PrintProperty::JSON,
            Quantity::Time,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(SubUnitNr(1))
            .set(TariffNr(2))
            .set(VIFRange::DurationOfTariff)
            );

        addStringFieldWithExtractor(
            "last_remove_date",
            "Date when the smoke alarm was last removed.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(SubUnitNr(1))
            .set(TariffNr(2))
            .set(VIFRange::Date)
            );

        addNumericFieldWithExtractor(
            "removed",
            "Number of times the smoke alarm has been removed.",
            PrintProperty::JSON,
            Quantity::Counter,
            VifScaling::None,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(SubUnitNr(1))
            .set(TariffNr(2))
            .set(VIFRange::CumulationCounter)
            );

        addStringFieldWithExtractor(
            "test_button_last_date",
            "Date when test button was last pressed.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(SubUnitNr(1))
            .set(TariffNr(3))
            .set(VIFRange::Date)
            );

        addNumericFieldWithExtractor(
            "test_button",
            "Number of times the test button has been pressed.",
            PrintProperty::JSON,
            Quantity::Counter,
            VifScaling::None,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(SubUnitNr(1))
            .set(TariffNr(3))
            .set(VIFRange::CumulationCounter)
            );

        addStringFieldWithExtractor(
            "installation_date",
            "Date when the smoke alarm was installed.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(TariffNr(2))
            .set(VIFRange::Date)
            );

        addStringFieldWithExtractor(
            "last_sound_check_date",
            "Date when the smoke alarm last checked the piezo speaker.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(StorageNr(1))
            .set(VIFRange::Date)
            );

        addStringFieldWithExtractorAndLookup(
            "dust_level",
            "Dust level 0 (best) to 15 (worst).",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(DifVifKey("8440FF2C")),
            Translate::Lookup(
            {
                {
                    {
                        "DUST",
                        Translate::Type::IndexToString,
                        0x1f,
                        "",
                        {
                        }
                    },
                },
            }));

        addStringFieldWithExtractorAndLookup(
            "battery_level",
            "Battery voltage level.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(DifVifKey("8440FF2C")),
            Translate::Lookup(
            {
                {
                    {
                        "BATTERY_VOLTAGE",
                        Translate::Type::IndexToString,
                        0x0f00,
                        "",
                        {
                            { 0x0000, "2.25V" },
                            { 0x0100, "2.30V" },
                            { 0x0200, "2.35V" },
                            { 0x0300, "2.40V" },

                            { 0x0400, "2.45V" },
                            { 0x0500, "2.50V" },
                            { 0x0600, "2.55V" },
                            { 0x0700, "2.60V" },

                            { 0x0800, "2.65V" },
                            { 0x0900, "2.70V" },
                            { 0x0a00, "2.75V" },
                            { 0x0b00, "2.80V" },

                            { 0x0c00, "2.85V" },
                            { 0x0d00, "2.90V" },
                            { 0x0e00, "2.95V" },
                            { 0x0f00, "3.00V" }
                        }
                    },
                },
            }));

        addStringFieldWithExtractorAndLookup(
            "obstacle_distance",
            "The distance to a detected obstacle.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(DifVifKey("8440FF2C")),
            Translate::Lookup(
            {
                {
                    {
                        "OBSTACLE_DISTANCE",
                        Translate::Type::IndexToString,
                        0x700000,
                        "",
                        {
                            { 0x000000, "SEODS_NOT_COMPLETED" },
                            { 0x100000, "" }, // No obstacle detected
                            { 0x200000, "45_TO_60_CM" },
                            { 0x300000, "38_TO_53_CM" },

                            { 0x400000, "33_TO_48_CM" },
                            { 0x500000, "28_TO_40_CM" },
                            { 0x600000, "20_TO_33_CM" },
                            { 0x700000, "0_TO_25_CM" },
                        }
                    },
                },
            }));


        addStringFieldWithExtractorAndLookup(
            "head_status",
            "Status of smoke detector sensors, merged into the status field.",
            PrintProperty::JOIN_INTO_STATUS,
            FieldMatcher::build()
            .set(DifVifKey("8440FF2C")),
            Translate::Lookup(
            {
                {
                    {
                        "HEAD_STATUS",
                        Translate::Type::BitToString,
                        0xff8ff0e0,
                        "OK",
                        {
                           /* 0x00000000-0x000000f dust level*/
                            { 0x00000020, "SOUNDER_FAULT" },
                            { 0x00000040, "TAMPER_WHILE_REMOVED" },
                            { 0x00000080, "EOL_REACHED" },
                           /* 0x00000100-0x000f00 battery evel*/
                            { 0x00001000, "LOW_BATTERY_FAULT" },
                            { 0x00002000, "ALARM_SENSOR_FAULT" },
                            { 0x00004000, "OBSTACLE_DETECTOR_FAULT" },
                            { 0x00008000, "EOL_WITHIN_12_MONTH" },
                            { 0x00010000, "SEODS_NOT_YET_COMPLETED", TestBit::NotSet },
                            { 0x00020000, "ENV_CHANGED_SINCE_INSTALLATION" },
                            { 0x00040000, "COMM_TO_HEAD_FAULT" },
                            { 0x00080000, "INTERFERENCE_PREVENTING_OBSTACLE_DETECTION" },
                           /* 0x00100000-0x0700000 distance */
                           /* 0x00800000 reserved */
                            { 0x01000000, "OBSTACLE_DETECTED" },
                            { 0x02000000, "SMOKE_DETECTOR_FULLY_COVERED" }
                        }
                    },
                },
            }));

    }
}
