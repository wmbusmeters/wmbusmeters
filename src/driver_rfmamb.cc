/*
 Copyright (C) 2019-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        void processContent(Telegram *t);
    };

    static bool ok = staticRegisterDriver([](DriverInfo&di)
    {
        di.setName("rfmamb");
        di.setDefaultFields("name,id,current_temperature_c,current_relative_humidity_rh,timestamp");
        di.setMeterType(MeterType::TempHygroMeter);
        di.addLinkMode(LinkMode::T1);
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
        di.addMVT(MANUFACTURER_BMT, 0x1b,  0x10);
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        // BMeters-specific TPL status byte decoding (set in processContent).
        // Standard wMBus bit 3 = PERMANENT_ERROR, but BMeters uses it for MODULE_REMOVED (tamper).
        // All bits decoded with BMeters-specific meanings in processContent.
        addStringField(
            "status",
            "BMeters-specific status flags from TPL status byte.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::STATUS);

        addNumericFieldWithExtractor(
            "current_temperature",
            "The current temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            );

        addNumericFieldWithExtractor(
            "average_temperature_1h",
            "The average temperature over the last hour.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "average_temperature_24h",
            "The average temperature over the last 24 hours.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "maximum_temperature_1h",
            "The maximum temperature over the last hour.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::ExternalTemperature)
            );

        addNumericFieldWithExtractor(
            "maximum_temperature_24h",
            "The maximum temperature over the last 24 hours.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::ExternalTemperature)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "minimum_temperature_1h",
            "The minimum temperature over the last hour.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Minimum)
            .set(VIFRange::ExternalTemperature)
            );

        addNumericFieldWithExtractor(
            "minimum_temperature_24h",
            "The minimum temperature over the last 24 hours.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Minimum)
            .set(VIFRange::ExternalTemperature)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "current_relative_humidity",
            "The current relative humidity.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RelativeHumidity)
            );

        addNumericFieldWithExtractor(
            "average_relative_humidity_1h",
            "The average relative humidity over the last hour.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RelativeHumidity)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "average_relative_humidity_24h",
            "The average relative humidity over the last 24 hours.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RelativeHumidity)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "maximum_relative_humidity_1h",
            "The maximum relative humidity over the last hour.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::RelativeHumidity)
            );

        addNumericFieldWithExtractor(
            "maximum_relative_humidity_24h",
            "The maximum relative humidity over the last 24 hours.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::RelativeHumidity)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "minimum_relative_humidity_1h",
            "The minimum relative humidity over the last hour.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Minimum)
            .set(VIFRange::RelativeHumidity)
            );

        addNumericFieldWithExtractor(
            "minimum_relative_humidity_24h",
            "The minimum relative humidity over the last 24 hours.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::RelativeHumidity,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Minimum)
            .set(VIFRange::RelativeHumidity)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "device",
            "The meters date time.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        // Historical monthly averages from manufacturer-specific 0x0F section.
        // Device configuration (COnf0 bits 7-6) determines data layout:
        //   10 = 12 temperature averages
        //   01 = 12 humidity averages
        //   11 = 6 temperature + 6 humidity averages (default assumption)
        for (int i = 1; i <= 6; i++)
        {
            addNumericField(
                tostrprintf("historical_average_temperature_month_%d", i),
                Quantity::Temperature,
                DEFAULT_PRINT_PROPERTIES,
                tostrprintf("Monthly average temperature %d month(s) ago.", i));
        }

        for (int i = 1; i <= 6; i++)
        {
            addNumericField(
                tostrprintf("historical_average_relative_humidity_month_%d", i),
                Quantity::RelativeHumidity,
                DEFAULT_PRINT_PROPERTIES,
                tostrprintf("Monthly average relative humidity %d month(s) ago.", i));
        }
    }

    void Driver::processContent(Telegram *t)
    {
        // Decode BMeters-specific status from TPL status byte.
        // Standard wMBus maps bit 3 as PERMANENT_ERROR, but BMeters PDF
        // defines: bit 2=LOW_BATTERY, bit 3=MODULE_REMOVED (tamper),
        // bit 4=SENSOR_READ_ERROR, bit 5=TEMP_OUT_OF_RANGE, bit 6=RH_OUT_OF_RANGE.
        {
            int sts = t->tpl_sts;
            string s;

            int app = sts & 0x03;
            if (app == 1) s += "APP_BUSY ";
            else if (app == 2) s += "APP_ERROR ";
            if (sts & 0x04) s += "LOW_BATTERY ";
            if (sts & 0x08) s += "MODULE_REMOVED ";
            if (sts & 0x10) s += "SENSOR_READ_ERROR ";
            if (sts & 0x20) s += "TEMP_OUT_OF_RANGE ";
            if (sts & 0x40) s += "RH_OUT_OF_RANGE ";

            while (s.size() > 0 && s.back() == ' ') s.pop_back();
            if (s.empty()) s = "OK";

            setStringValue("status", s, NULL);
        }

        if (t->mfct_0f_index == -1) return;

        int offset = t->header_size + t->mfct_0f_index;

        vector<uchar> bytes;
        t->extractMfctData(&bytes);

        // Expect 24 bytes of historical data (12 x 2-byte values)
        if (bytes.size() < 24) return;

        // Parse historical monthly averages.
        // In "both" mode (COnf0 bits 7-6 = 11): slots 0-5 = temperature, slots 6-11 = humidity.
        // Values are 16-bit integers: temperature in 1/10 °C, humidity in %RH.
        // Special values: 0xFFFF = not available, high nibble 0xF = out of range.

        for (int i = 0; i < 6; i++)
        {
            int pos = i * 2;
            uchar lo = bytes[pos];
            uchar hi = bytes[pos + 1];
            uint16_t raw = (hi << 8) | lo;

            string vname = tostrprintf("historical_average_temperature_month_%d", i + 1);

            if (raw == 0xFFFF)
            {
                t->addSpecialExplanation(offset + 1 + pos, 2, KindOfData::CONTENT,
                                         Understanding::FULL,
                                         "*** %02X%02X historical temperature month %d: not available",
                                         lo, hi, i + 1);
                continue;
            }

            if ((raw >> 12) == 0xF)
            {
                t->addSpecialExplanation(offset + 1 + pos, 2, KindOfData::CONTENT,
                                         Understanding::FULL,
                                         "*** %02X%02X historical temperature month %d: out of range",
                                         lo, hi, i + 1);
                continue;
            }

            double temp_c = raw / 10.0;
            setNumericValue(vname, Unit::C, temp_c);

            string info = renderJsonOnlyDefaultUnit(vname, Quantity::Temperature);
            t->addSpecialExplanation(offset + 1 + pos, 2, KindOfData::CONTENT,
                                     Understanding::FULL,
                                     "*** %02X%02X (%s)", lo, hi, info.c_str());
        }

        for (int i = 0; i < 6; i++)
        {
            int pos = (i + 6) * 2;
            uchar lo = bytes[pos];
            uchar hi = bytes[pos + 1];
            uint16_t raw = (hi << 8) | lo;

            string vname = tostrprintf("historical_average_relative_humidity_month_%d", i + 1);

            if (raw == 0xFFFF)
            {
                t->addSpecialExplanation(offset + 1 + pos, 2, KindOfData::CONTENT,
                                         Understanding::FULL,
                                         "*** %02X%02X historical humidity month %d: not available",
                                         lo, hi, i + 1);
                continue;
            }

            if ((raw >> 12) == 0xF)
            {
                t->addSpecialExplanation(offset + 1 + pos, 2, KindOfData::CONTENT,
                                         Understanding::FULL,
                                         "*** %02X%02X historical humidity month %d: out of range",
                                         lo, hi, i + 1);
                continue;
            }

            double rh = (double)raw;
            setNumericValue(vname, Unit::RH, rh);

            string info = renderJsonOnlyDefaultUnit(vname, Quantity::RelativeHumidity);
            t->addSpecialExplanation(offset + 1 + pos, 2, KindOfData::CONTENT,
                                     Understanding::FULL,
                                     "*** %02X%02X (%s)", lo, hi, info.c_str());
        }
    }
}

// Test: Rummet rfmamb 11772288 NOKEY
// telegram=|5744b40988227711101b7ab20800000265a00842658f088201659f08226589081265a0086265510852652b0902fb1aba0142fb1ab0018201fb1abd0122fb1aa90112fb1aba0162fb1aa60152fb1af501066d3b3bb36b2a00|
// {"_":"telegram","media":"room sensor","meter":"rfmamb","name":"Rummet","id":"11772288","status":"MODULE_REMOVED","current_temperature_c":22.08,"average_temperature_1h_c":21.91,"average_temperature_24h_c":22.07,"maximum_temperature_1h_c":22.08,"minimum_temperature_1h_c":21.85,"maximum_temperature_24h_c":23.47,"minimum_temperature_24h_c":21.29,"current_relative_humidity_rh":44.2,"average_relative_humidity_1h_rh":43.2,"average_relative_humidity_24h_rh":44.5,"minimum_relative_humidity_1h_rh":42.5,"maximum_relative_humidity_1h_rh":44.2,"maximum_relative_humidity_24h_rh":50.1,"minimum_relative_humidity_24h_rh":42.2,"device_datetime":"2019-10-11 19:59","timestamp":"1111-11-11T11:11:11Z"}
// |Rummet;11772288;22.08;44.2;1111-11-11 11:11.11

// Test: Pokojak rfmamb 23699558 6C649F296476D737CACB75A2D639CE14
// telegram=|5e44b40958956923101b7a7f085005816afb5f6f40ec742610b93c109973edd8c098f505d5dfcfd53dfd72708178e4b81436cb753c6dff5094c48c26607c66419628d424ce41f38f9bd927757d82fa43396aa59a77c694acadab776460d472|
// {"_":"telegram","media":"room sensor","meter":"rfmamb","name":"Pokojak","id":"23699558","status":"MODULE_REMOVED","current_temperature_c":20.91,"average_temperature_1h_c":20.95,"average_temperature_24h_c":19.5,"maximum_temperature_1h_c":20.98,"minimum_temperature_1h_c":20.91,"maximum_temperature_24h_c":19.88,"minimum_temperature_24h_c":19.07,"current_relative_humidity_rh":35.8,"average_relative_humidity_1h_rh":36.1,"average_relative_humidity_24h_rh":36.9,"minimum_relative_humidity_1h_rh":35.8,"maximum_relative_humidity_1h_rh":36.5,"maximum_relative_humidity_24h_rh":37,"minimum_relative_humidity_24h_rh":36.7,"device_datetime":"2026-02-09 08:59","timestamp":"1111-11-11T11:11:11Z"}
// |Pokojak;23699558;20.91;35.8;1111-11-11 11:11.11
