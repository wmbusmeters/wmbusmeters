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
                        "TPL_BITS",
                        Translate::Type::BitToString,
                        0xe0, // Always 0xe0 for tpl status bits. The 0x1f are standard defined.
                        "OK",
                        {
//                            { 0x20, "?"}
//                            { 0x40, "?"}
//                            { 0x80, "?"}
                        }
                    },
                },
            });
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
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
            "smoke_alarm",
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

        addStringFieldWithExtractorAndLookup(
            "status",
            "Status and error flags.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT | PrintProperty::JOIN_TPL_STATUS,
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
                        }
                    },
                },
            }));

    }
}

// Test: Smokey ei6500 00012811 NOKEY
// telegram=|5E442515112801000C1A7A370050252F2F_0BFD0F060101046D300CAB2202FD17000082206CAB22426C01018440FF2C000F11008250FD61000082506C01018260FD6100008360FD3100000082606C01018270FD61010082706CAB222F2F2F2F|
// {"media":"smoke detector","meter":"ei6500","name":"Smokey","id":"00012811","software_version":"010106","message_datetime":"2021-02-11 12:48","last_alarm_date":"2000-01-01","smoke_alarm_counter":0,"duration_removed_h":0,"last_remove_date":"2000-01-01","removed_counter":0,"test_button_last_date":"2021-02-11","test_button_counter":1,"status":"NOT_INSTALLED","timestamp":"1111-11-11T11:11:11Z"}
// |Smokey;00012811;2000-01-01;0.000000;NOT_INSTALLED;1111-11-11 11:11.11

// telegram=|5E442515112801000C1A7A370f50252F2F_0BFD0F060101046D300CAB2202FD17030182206CAB22426C01018440FF2C000F11008250FD61000282506C01018260FD6100008360FD3171000082606C01018270FD61010082706CAB222F2F2F2F|
// {"media":"smoke detector","meter":"ei6500","name":"Smokey","id":"00012811","software_version":"010106","message_datetime":"2021-02-11 12:48","last_alarm_date":"2000-01-01","smoke_alarm_counter":512,"duration_removed_h":1.883333,"last_remove_date":"2000-01-01","removed_counter":0,"test_button_last_date":"2021-02-11","test_button_counter":1,"status":"ENVIRONMENT_CHANGED OBSTACLE_DETECTED ALARM POWER_LOW PERMANENT_ERROR","timestamp":"1111-11-11T11:11:11Z"}
// |Smokey;00012811;2000-01-01;512.000000;ENVIRONMENT_CHANGED OBSTACLE_DETECTED ALARM POWER_LOW PERMANENT_ERROR;1111-11-11 11:11.11
