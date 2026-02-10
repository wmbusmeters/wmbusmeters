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

namespace
{
    struct Driver : public virtual MeterCommonImplementation
    {
        Driver(MeterInfo &mi, DriverInfo &di);
        void processContent(Telegram *t);
        void decodeRF_RKN0(Telegram *t);
        void decodeRF_RKN9(Telegram *t);
    };

    static bool ok = staticRegisterDriver([](DriverInfo&di)
    {
        di.setName("hydroclima");
        di.setDefaultFields("name,id,current_consumption_hca,average_ambient_temperature_c,timestamp");
        di.setMeterType(MeterType::HeatCostAllocationMeter);
        di.addLinkMode(LinkMode::T1);
        di.addMVT(MANUFACTURER_BMP, 0x08,  0x53);
        di.addMVT(MANUFACTURER_BMP, 0x08,  0x85);
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "current_consumption",
            "The current heat cost allocation.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            );

        addStringFieldWithExtractor(
            "set_date",
            "The most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1)));

        addNumericFieldWithExtractor(
            "consumption_at_set_date",
            "Heat cost allocation at the most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            .set(StorageNr(1))
            );

        addNumericField("average_ambient_temperature",
                        Quantity::Temperature,
                        DEFAULT_PRINT_PROPERTIES,
                        "Average ambient temperature since this beginning of this month.");

        addNumericField("max_ambient_temperature",
                        Quantity::Temperature,
                        DEFAULT_PRINT_PROPERTIES,
                        "Max ambient temperature  since the beginning of this month.");

        addNumericField("average_ambient_temperature_last_month",
                        Quantity::Temperature,
                        DEFAULT_PRINT_PROPERTIES,
                        "Average ambient temperature last month.");

        addNumericField("average_heater_temperature_last_month",
                        Quantity::Temperature,
                        DEFAULT_PRINT_PROPERTIES,
                        "Average heater temperature last month.");

        // Fields specific to HydroClima 2 ITN (version 0x85, frame 0x11)
        addNumericField("average_heater_temperature",
                        Quantity::Temperature,
                        DEFAULT_PRINT_PROPERTIES,
                        "Average heater temperature since beginning of this month.");

        addNumericField("consumption_at_set_date_1",
                        Quantity::HCA,
                        DEFAULT_PRINT_PROPERTIES,
                        "Heat cost allocation at set date 1 (most recent billing period).");

        addNumericField("consumption_at_set_date_2",
                        Quantity::HCA,
                        DEFAULT_PRINT_PROPERTIES,
                        "Heat cost allocation at set date 2.");

        addNumericField("consumption_at_set_date_3",
                        Quantity::HCA,
                        DEFAULT_PRINT_PROPERTIES,
                        "Heat cost allocation at set date 3.");

        addNumericField("consumption_at_set_date_4",
                        Quantity::HCA,
                        DEFAULT_PRINT_PROPERTIES,
                        "Heat cost allocation at set date 4.");

        addNumericField("ambient_temperature_at_set_date_1",
                        Quantity::Temperature,
                        DEFAULT_PRINT_PROPERTIES,
                        "Ambient temperature at set date 1.");

        addNumericField("ambient_temperature_at_set_date_2",
                        Quantity::Temperature,
                        DEFAULT_PRINT_PROPERTIES,
                        "Ambient temperature at set date 2.");

        addNumericField("ambient_temperature_at_set_date_3",
                        Quantity::Temperature,
                        DEFAULT_PRINT_PROPERTIES,
                        "Ambient temperature at set date 3.");

        addNumericField("total_consumption",
                        Quantity::HCA,
                        DEFAULT_PRINT_PROPERTIES,
                        "Total heat cost allocation across all billing periods.");
    }

    double toTemperature(uchar hi, uchar lo)
    {
        return ((double)((hi<<8) | lo))/100.0;
    }

    double toIndicationU(uchar hi, uchar lo)
    {
        return ((double)((hi<<8) | lo))/10.0;
    }

    double toTotalIndicationU(uchar hihi, uchar hi, uchar lo)
    {
        int x = (hihi << 16) | (hi<<8) | lo;
        return ((double)x)/10.0;
    }

    void Driver::processContent(Telegram *t)
    {
        if (t->mfct_0f_index == -1) return; // Check that there is mfct data.

        if (t->dv_entries.count("036E") != 0)
        {
            decodeRF_RKN0(t);
        }
        else
        {
            decodeRF_RKN9(t);
        }
    }

    void Driver::decodeRF_RKN0(Telegram *t)
    {
        int offset = t->header_size+t->mfct_0f_index;

        // Mark the 0F DIF byte itself as protocol (not content data)
        t->addSpecialExplanation(offset-1, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                 "*** 0F manufacturer specific data");

        vector<uchar> bytes;
        t->extractMfctData(&bytes); // Extract raw frame data after the DIF 0x0F.

        debugPayload("(hydroclima mfct)", bytes);

        int i = 0;
        int len = bytes.size();
        string info;

        // [0] Frame identifier (0x10 = v0x53, 0x11 = v0x85 HydroClima 2 ITN)
        if (i >= len) return;
        uchar frame_identifier = bytes[i];
        t->addSpecialExplanation(i+offset, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                 "*** %02X frame identifier %s", frame_identifier,
                                 (frame_identifier == 0x10 || frame_identifier == 0x11) ? "OK" : "UNKNOWN");
        i++;

        // [1-2] STS - status word
        if (i+1 >= len) return;
        uint16_t status = bytes[i+1]<<8 | bytes[i];
        t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                 "*** %02X%02X status %04x", bytes[i], bytes[i+1], status);
        i+=2;

        // [3-4] TIM - current time
        if (i+1 >= len) return;
        uint16_t time = bytes[i+1]<<8 | bytes[i];
        t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                 "*** %02X%02X time", bytes[i], bytes[i+1], time);
        i+=2;

        // [5-6] DAT - current date
        if (i+1 >= len) return;
        uint16_t date = bytes[i+1]<<8 | bytes[i];
        t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                 "*** %02X%02X date %x", bytes[i], bytes[i+1], date);
        i+=2;

        if (frame_identifier == 0x11)
        {
            // === Frame 0x11 (version 0x85, HydroClima 2 ITN KA1 structure, 47 bytes total) ===
            // Per BMeters PAPP-HARF2 specification.

            // [7-8] DOP - housing open event date
            if (i+1 >= len) return;
            uint16_t dop = bytes[i+1]<<8 | bytes[i];
            t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X housing open date %04x", bytes[i], bytes[i+1], dop);
            i+=2;

            // [9-10] TKA - average heater/radiator temperature (current period)
            if (i+1 >= len) return;
            double avg_heater_temp = toTemperature(bytes[i+1], bytes[i]);
            setNumericValue("average_heater_temperature", Unit::C, avg_heater_temp);
            info = renderJsonOnlyDefaultUnit("average_heater_temperature", Quantity::Temperature);
            t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X (%s)", bytes[i], bytes[i+1], info.c_str());
            i+=2;

            // [11-12] TOA - average ambient temperature (current period)
            if (i+1 >= len) return;
            double avg_ambient_temp = toTemperature(bytes[i+1], bytes[i]);
            setNumericValue("average_ambient_temperature", Unit::C, avg_ambient_temp);
            info = renderJsonOnlyDefaultUnit("average_ambient_temperature", Quantity::Temperature);
            t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X (%s)", bytes[i], bytes[i+1], info.c_str());
            i+=2;

            // [13-14] TMH1 - max temperature (previous period)
            if (i+1 >= len) return;
            double max_temp = toTemperature(bytes[i+1], bytes[i]);
            setNumericValue("max_ambient_temperature", Unit::C, max_temp);
            info = renderJsonOnlyDefaultUnit("max_ambient_temperature", Quantity::Temperature);
            t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X (%s)", bytes[i], bytes[i+1], info.c_str());
            i+=2;

            // [15-16] TMHD1 - date of max temperature (previous period)
            if (i+1 >= len) return;
            uint16_t max_date = bytes[i+1]<<8 | bytes[i];
            t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X max date %04x", bytes[i], bytes[i+1], max_date);
            i+=2;

            // [17-18] TKA1 - average heater/radiator temperature (previous period)
            if (i+1 >= len) return;
            double avg_heater_temp_last = toTemperature(bytes[i+1], bytes[i]);
            setNumericValue("average_heater_temperature_last_month", Unit::C, avg_heater_temp_last);
            info = renderJsonOnlyDefaultUnit("average_heater_temperature_last_month", Quantity::Temperature);
            t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X (%s)", bytes[i], bytes[i+1], info.c_str());
            i+=2;

            // [19-20] TOA1 - average ambient temperature (previous period)
            if (i+1 >= len) return;
            double avg_ambient_temp_last = toTemperature(bytes[i+1], bytes[i]);
            setNumericValue("average_ambient_temperature_last_month", Unit::C, avg_ambient_temp_last);
            info = renderJsonOnlyDefaultUnit("average_ambient_temperature_last_month", Quantity::Temperature);
            t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X (%s)", bytes[i], bytes[i+1], info.c_str());
            i+=2;

            // [21-28] CNI1-4 - consumption history (4 x 2B, in HCA units)
            const char *cni_names[] = {
                "consumption_at_set_date_1", "consumption_at_set_date_2",
                "consumption_at_set_date_3", "consumption_at_set_date_4"
            };
            for (int n = 0; n < 4; n++)
            {
                if (i+1 >= len) return;
                uint16_t cni = (bytes[i+1]<<8) | bytes[i];
                setNumericValue(cni_names[n], Unit::HCA, (double)cni);
                info = renderJsonOnlyDefaultUnit(cni_names[n], Quantity::HCA);
                t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                         "*** %02X%02X (%s)", bytes[i], bytes[i+1], info.c_str());
                i+=2;
            }

            // [29-34] TONI1-3 - ambient temperature history (3 x 2B, 0x8000 = no data)
            const char *toni_names[] = {
                "ambient_temperature_at_set_date_1",
                "ambient_temperature_at_set_date_2",
                "ambient_temperature_at_set_date_3"
            };
            for (int n = 0; n < 3; n++)
            {
                if (i+1 >= len) return;
                uint16_t raw = (bytes[i+1]<<8) | bytes[i];
                if (raw != 0x8000)
                {
                    double toni = ((double)raw) / 100.0;
                    setNumericValue(toni_names[n], Unit::C, toni);
                    info = renderJsonOnlyDefaultUnit(toni_names[n], Quantity::Temperature);
                    t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                             "*** %02X%02X (%s)", bytes[i], bytes[i+1], info.c_str());
                }
                else
                {
                    t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                             "*** %02X%02X no data", bytes[i], bytes[i+1]);
                }
                i+=2;
            }

            // [35-37] TK22LAR1 - count TK < 22.5 C (3B)
            if (i+2 >= len) return;
            int tk_below = (bytes[i+2]<<16) | (bytes[i+1]<<8) | bytes[i];
            t->addSpecialExplanation(i+offset, 3, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X%02X count TK<22.5 %d", bytes[i], bytes[i+1], bytes[i+2], tk_below);
            i+=3;

            // [38-40] TK22AR1 - count 22.5 <= TK < 35 C (3B)
            if (i+2 >= len) return;
            int tk_mid = (bytes[i+2]<<16) | (bytes[i+1]<<8) | bytes[i];
            t->addSpecialExplanation(i+offset, 3, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X%02X count 22.5<=TK<35 %d", bytes[i], bytes[i+1], bytes[i+2], tk_mid);
            i+=3;

            // [41-43] TK35AR1 - count TK >= 35 C (3B)
            if (i+2 >= len) return;
            int tk_above = (bytes[i+2]<<16) | (bytes[i+1]<<8) | bytes[i];
            t->addSpecialExplanation(i+offset, 3, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X%02X count TK>=35 %d", bytes[i], bytes[i+1], bytes[i+2], tk_above);
            i+=3;

            // [44-46] U - total consumption all periods (3B, /10 = HCA)
            if (i+2 >= len) return;
            double total = toTotalIndicationU(bytes[i+2], bytes[i+1], bytes[i]);
            setNumericValue("total_consumption", Unit::HCA, total);
            info = renderJsonOnlyDefaultUnit("total_consumption", Quantity::HCA);
            t->addSpecialExplanation(i+offset, 3, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X%02X (%s)", bytes[i], bytes[i+1], bytes[i+2], info.c_str());
            i+=3;
        }
        else
        {
            // === Frame 0x10 (version 0x53, original HydroClima format, 24 bytes total) ===

            // [7-8] TOA - average ambient temperature (current period)
            if (i+1 >= len) return;
            double average_ambient_temperature_c = toTemperature(bytes[i+1], bytes[i]);
            setNumericValue("average_ambient_temperature", Unit::C, average_ambient_temperature_c);
            info = renderJsonOnlyDefaultUnit("average_ambient_temperature", Quantity::Temperature);
            t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X (%s)", bytes[i], bytes[i+1], info.c_str());
            i+=2;

            // [9-10] TMH - max temperature
            if (i+1 >= len) return;
            double max_ambient_temperature_c = toTemperature(bytes[i+1], bytes[i]);
            setNumericValue("max_ambient_temperature", Unit::C, max_ambient_temperature_c);
            info = renderJsonOnlyDefaultUnit("max_ambient_temperature", Quantity::Temperature);
            t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X (%s)", bytes[i], bytes[i+1], info.c_str());
            i+=2;

            // [11-12] TMHD - date of max temperature
            if (i+1 >= len) return;
            uint16_t max_date = bytes[i+1]<<8 | bytes[i];
            t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X max date %x", bytes[i], bytes[i+1], max_date);
            i+=2;

            // [13-14] num measurements
            if (i+1 >= len) return;
            uint16_t num_measurements = bytes[i+1]<<8 | bytes[i];
            t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X num measurements %d", bytes[i], bytes[i+1], num_measurements);
            i+=2;

            // [15-16] TOA1 - average ambient temperature (previous period)
            if (i+1 >= len) return;
            double average_ambient_temperature_last_month_c = toTemperature(bytes[i+1], bytes[i]);
            setNumericValue("average_ambient_temperature_last_month", Unit::C,
                            average_ambient_temperature_last_month_c);
            info = renderJsonOnlyDefaultUnit("average_ambient_temperature_last_month", Quantity::Temperature);
            t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X (%s)", bytes[i], bytes[i+1], info.c_str());
            i+=2;

            // [17-18] TKA1 - average heater temperature (previous period)
            if (i+1 >= len) return;
            double average_heater_temperature_last_month_c = toTemperature(bytes[i+1], bytes[i]);
            setNumericValue("average_heater_temperature_last_month", Unit::C,
                            average_heater_temperature_last_month_c);
            info = renderJsonOnlyDefaultUnit("average_heater_temperature_last_month", Quantity::Temperature);
            t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X (%s)", bytes[i], bytes[i+1], info.c_str());
            i+=2;

            // [19-20] U - indication (2B, /10 = HCA)
            if (i+1 >= len) return;
            double indication_u = toIndicationU(bytes[i+1], bytes[i]);
            t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X indication u %f", bytes[i], bytes[i+1], indication_u);
            i+=2;

            // [21-23] UC - total indication (3B, /10 = HCA)
            if (i+2 >= len) return;
            double total_indication_u = toTotalIndicationU(bytes[i+2], bytes[i+1], bytes[i]);
            t->addSpecialExplanation(i+offset, 3, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X%02X total indication u %f", bytes[i], bytes[i+1], bytes[i+2], total_indication_u);
            i+=3;
        }
    }

    void Driver::decodeRF_RKN9(Telegram *t)
    {
        int offset = t->header_size+t->mfct_0f_index;

        // Mark the 0F DIF byte itself as protocol (not content data)
        t->addSpecialExplanation(offset-1, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                 "*** 0F manufacturer specific data");

        vector<uchar> bytes;
        t->extractMfctData(&bytes); // Extract raw frame data after the DIF 0x0F.

        debugPayload("(hydroclima mfct)", bytes);

        int i = 0;
        int len = bytes.size();
        string info;

        double last_x_month_uc {};
        for (int m = 1; m <= 12; ++m)
        {
            if (i+1 >= len) return;
            last_x_month_uc = toIndicationU(bytes[i+1], bytes[i]);
            t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X last %d month uc %f", bytes[i], bytes[i+1], m, last_x_month_uc);
            i+=2;
        }

        if (i+1 >= len) return;
        uint16_t date_case_opened = bytes[i+1]<<8 | bytes[i];
        t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                 "*** %02X%02X date case opened %x", bytes[i], bytes[i+1], date_case_opened);
        i+=2;

        if (i+1 >= len) return;
        uint16_t date_start_month = bytes[i+1]<<8 | bytes[i];
        t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                 "*** %02X%02X date start month %x", bytes[i], bytes[i+1], date_start_month);
        i+=2;

        // The test telegram I have has more data, but the specification I have ends here!?!
    }

}


// Test: HCA hydroclima 68036198 NOKEY
// Comment:
// telegram=|2e44b0099861036853087a000020002F2F036E0000000F100043106A7D2C4A078F12202CB1242A06D3062100210000|
// {"_":"telegram","media":"heat cost allocation","meter":"hydroclima","name":"HCA","id":"68036198","current_consumption_hca":0,"average_ambient_temperature_c":18.66,"max_ambient_temperature_c":47.51,"average_ambient_temperature_last_month_c":15.78,"average_heater_temperature_last_month_c":17.47,"timestamp":"1111-11-11T11:11:11Z"}
// |HCA;68036198;0;18.66;1111-11-11 11:11.11


// Test: HCAA hydroclima 74393723 NOKEY
// Comment:
// telegram=|2D44B009233739743308780F9D1300023ED97AEC7BC5908A32C15D8A32C126915AC15AC126912691269187912689|
// {"_":"telegram","media":"heat cost allocation","meter":"hydroclima","name":"HCAA","id":"74393723","timestamp":"1111-11-11T11:11:11Z"}
// |HCAA;74393723;null;null;1111-11-11 11:11.11


// Test: HCA85 hydroclima 93000952 06006500000000000000000000000000
// Comment: Version 0x85 with frame identifier 0x11 (HydroClima 2 ITN KA1 structure)
// telegram=|5144b0095209009385088c20807a80004025e1643fee024fea668b79a2eb98e9068aecebd8f0a92d6da9cda2675cfaeddd9cdece8d1639be8a953d0ec284dd5447305a68fc6a2fe69b89574e54fa76b0b348|
// {"_":"telegram","media":"heat cost allocation","meter":"hydroclima","name":"HCA85","id":"93000952","current_consumption_hca":596,"average_ambient_temperature_c":22.71,"max_ambient_temperature_c":58.2,"average_ambient_temperature_last_month_c":21.78,"average_heater_temperature_last_month_c":37.34,"average_heater_temperature_c":49.19,"consumption_at_set_date_hca":2265,"consumption_at_set_date_1_hca":468,"consumption_at_set_date_2_hca":2265,"consumption_at_set_date_3_hca":1913,"consumption_at_set_date_4_hca":1632,"ambient_temperature_at_set_date_1_c":22.66,"ambient_temperature_at_set_date_2_c":23.12,"ambient_temperature_at_set_date_3_c":22.66,"total_consumption_hca":243.5,"set_date":"2025-12-31","timestamp":"1111-11-11T11:11:11Z"}
// |HCA85;93000952;596;22.71;1111-11-11 11:11.11

// Test: HCA85B hydroclima 93001021 06006500000000000000000000000000
// Comment: Version 0x85 second device with no-data markers in TONI fields
// telegram=|5144b0092110009385088c20ee7aee404025ae46448c6081f085cf46cd634ec47179e92024e0bcff8e6449fa81767def444bcf1e734c4f17d67b6bc738bdd004422c156abfe9be2c4abcba41dac5668d29e9|
// {"_":"telegram","media":"heat cost allocation","meter":"hydroclima","name":"HCA85B","id":"93001021","current_consumption_hca":0,"average_ambient_temperature_c":20.61,"max_ambient_temperature_c":20.75,"average_ambient_temperature_last_month_c":18.94,"average_heater_temperature_last_month_c":19.4,"average_heater_temperature_c":21.05,"consumption_at_set_date_hca":0,"consumption_at_set_date_1_hca":0,"consumption_at_set_date_2_hca":0,"consumption_at_set_date_3_hca":0,"consumption_at_set_date_4_hca":0,"ambient_temperature_at_set_date_1_c":20.86,"total_consumption_hca":3.4,"set_date":"2025-05-31","timestamp":"1111-11-11T11:11:11Z"}
// |HCA85B;93001021;0;20.61;1111-11-11 11:11.11
