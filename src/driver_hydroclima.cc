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

        double average_ambient_temperature_ {};
        double max_ambient_temperature_ {};
        double average_ambient_temperature_last_month_ {};
        double average_heater_temperature_last_month_ {};
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("hydroclima");
        di.setMeterType(MeterType::HeatCostAllocationMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_BMP, 0x08,  0x53);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "current_consumption",
            "The current heat cost allocation.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
            Quantity::HCA,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            );

        addPrint("average_ambient_temperature",
                 Quantity::Temperature,
                 GET_FUNC(average_ambient_temperature_, Unit::C),
                 "Average ambient temperature since this beginning of this month.",
                 PrintProperty::JSON | PrintProperty::FIELD);

        addPrint("max_ambient_temperature",
                 Quantity::Temperature,
                 GET_FUNC(max_ambient_temperature_, Unit::C),
                 "Max ambient temperature  since the beginning of this month.",
                 PrintProperty::JSON);

        addPrint("average_ambient_temperature_last_month",
                 Quantity::Temperature,
                 GET_FUNC(average_ambient_temperature_last_month_, Unit::C),
                 "Average ambient temperature last month.",
                 PrintProperty::JSON);

        addPrint("average_heater_temperature_last_month",
                 Quantity::Temperature,
                 GET_FUNC(average_heater_temperature_last_month_, Unit::C),
                 "Average heater temperature last month.",
                 PrintProperty::JSON);

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

        int offset = t->header_size+t->mfct_0f_index;

        vector<uchar> bytes;
        t->extractMfctData(&bytes); // Extract raw frame data after the DIF 0x0F.

        debugPayload("(hydroclima mfct)", bytes);

        int i = 0;
        int len = bytes.size();
        string info;

        // Typicall 25 bytes: 100043106A7D2C4A078F12202CB1242A06D306210021000000

        if (i >= len) return;
        uchar frame_identifier = bytes[i];
        t->addSpecialExplanation(i+offset, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                 "*** %02X frame identifier %s", frame_identifier,
                                 frame_identifier == 0x10 ? "OK" :"UNKNOWN");
        i++;

        if (i+1 >= len) return;
        uint16_t status = bytes[i+1]<<8 | bytes[i];
        t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                 "*** %02X%02X status", bytes[i], bytes[i+1], status);
        i+=2;

        if (i+1 >= len) return;
        uint16_t time = bytes[i+1]<<8 | bytes[i];
        t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                 "*** %02X%02X time", bytes[i], bytes[i+1], time);
        i+=2;

        if (i+1 >= len) return;
        uint16_t date = bytes[i+1]<<8 | bytes[i];
        t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                 "*** %02X%02X date %x", bytes[i], bytes[i+1], date);
        i+=2;

        if (i+1 >= len) return;
        average_ambient_temperature_ = toTemperature(bytes[i+1], bytes[i]);
        info = renderJsonOnlyDefaultUnit("average_ambient_temperature");
        t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                 "*** %02X%02X (%s)", bytes[i], bytes[i+1], info.c_str());
        i+=2;

        if (i+1 >= len) return;
        max_ambient_temperature_ = toTemperature(bytes[i+1], bytes[i]);
        info = renderJsonOnlyDefaultUnit("max_ambient_temperature");
        t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                 "*** %02X%02X (%s)", bytes[i], bytes[i+1], info.c_str());
        i+=2;

        if (i+1 >= len) return;
        uint16_t max_date = bytes[i+1]<<8 | bytes[i];
        t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                 "*** %02X%02X max date %x", bytes[i], bytes[i+1], max_date);
        i+=2;

        if (i+1 >= len) return;
        uint16_t num_measurements = bytes[i+1]<<8 | bytes[i];
        t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                 "*** %02X%02X num measurements %d", bytes[i], bytes[i+1], num_measurements);
        i+=2;

        if (i+1 >= len) return;
        average_ambient_temperature_last_month_ = toTemperature(bytes[i+1], bytes[i]);
        info = renderJsonOnlyDefaultUnit("average_ambient_temperature_last_month");
        t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                 "*** %02X%02X (%s)", bytes[i], bytes[i+1], info.c_str());
        i+=2;

        if (i+1 >= len) return;
        average_heater_temperature_last_month_ = toTemperature(bytes[i+1], bytes[i]);
        info = renderJsonOnlyDefaultUnit("average_temperature_heater_last_month");
        t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                 "*** %02X%02X (%s)", bytes[i], bytes[i+1], info.c_str());
        i+=2;

        if (i+1 >= len) return;
        double indication_u = toIndicationU(bytes[i+1], bytes[i]);
        t->addSpecialExplanation(i+offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                 "*** %02X%02X indication u %f", bytes[i], bytes[i+1], indication_u);
        i+=2;

        if (i+2 >= len) return;
        double total_indication_u = toTotalIndicationU(bytes[i+2], bytes[i+1], bytes[i]);
        t->addSpecialExplanation(i+offset, 3, KindOfData::CONTENT, Understanding::FULL,
                                 "*** %02X%02X%02X total indication u %f", bytes[i], bytes[i+1], bytes[i+2], total_indication_u);
        i+=3;
    }
}


// Test: HCA hydroclima 68036198 NOKEY
// Comment:
// telegram=|2e44b0099861036853087a000020002F2F036E0000000F100043106A7D2C4A078F12202CB1242A06D3062100210000|
// {"media":"heat cost allocation","meter":"hydroclima","name":"HCA","id":"68036198","current_consumption_hca":0,"average_ambient_temperature_c":18.66,"max_ambient_temperature_c":47.51,"average_ambient_temperature_last_month_c":15.78,"average_heater_temperature_last_month_c":17.47,"timestamp":"1111-11-11T11:11:11Z"}
// |HCA;68036198;0.000000;18.660000;1111-11-11 11:11.11
