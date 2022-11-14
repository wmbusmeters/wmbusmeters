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

using namespace std;

struct MeterAventiesWM : public virtual MeterCommonImplementation
{
    MeterAventiesWM(MeterInfo &mi, DriverInfo &di);

private:

    double total_water_consumption_m3_ {};
    double consumption_at_set_date_m3_[14];
    string error_flags_;
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("aventieswm");
    di.setMeterType(MeterType::WaterMeter);
    di.addLinkMode(LinkMode::T1);
    di.addDetection(MANUFACTURER_AAA,  0x07,  0x25);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterAventiesWM(mi, di)); });
});

MeterAventiesWM::MeterAventiesWM(MeterInfo &mi, DriverInfo &di) :
    MeterCommonImplementation(mi, di)
{
    addNumericFieldWithExtractor(
        "total",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::Volume,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
        "The total water consumption recorded by this meter.",
        SET_FUNC(total_water_consumption_m3_, Unit::M3),
        GET_FUNC(total_water_consumption_m3_, Unit::M3));

    for (int i=1; i<=14; ++i)
    {
        string msg, info;
        strprintf(&msg, "consumption_at_set_date_%d", i);
        strprintf(&info, "Water consumption at the %d billing period date.", i);
        addNumericFieldWithExtractor(
            msg,
            Quantity::Volume,
            NoDifVifKey,
            VifScaling::Auto,
            MeasurementType::Instantaneous,
            VIFRange::Volume,
            StorageNr(i),
            TariffNr(0),
            IndexNr(1),
            PrintProperty::JSON,
            info,
            SET_FUNC(consumption_at_set_date_m3_[i-1], Unit::M3),
            GET_FUNC(consumption_at_set_date_m3_[i-1], Unit::M3));
    }

    addStringFieldWithExtractorAndLookup(
        "error_flags",
        Quantity::Text,
        DifVifKey("02FD17"),
        MeasurementType::Instantaneous,
        VIFRange::Any,
        AnyStorageNr,
        AnyTariffNr,
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "Error flags.",
        SET_STRING_FUNC(error_flags_),
        GET_STRING_FUNC(error_flags_),
         {
            {
                {
                    "ERROR_FLAGS",
                    Translate::Type::BitToString,
                    0xffff,
                    "",
                    {
                        { 0x01, "MEASUREMENT" },
                        { 0x02, "SABOTAGE" },
                        { 0x04, "BATTERY" },
                        { 0x08, "CS" },
                        { 0x10, "HF" },
                        { 0x20, "RESET" }
                    }
                },
            },
         });
}

// Test: Votten aventieswm 61070071 A004EB23329A477F1DD2D7820B56EB3D
// telegram=76442104710007612507727100076121042507B5006005E2E95A3C2A1279A5415E6732679B43369FD5FDDDD783EEEBB48236D34E7C94AF0A18A5FDA5F7D64111EB42D4D891622139F2952F9D12A20088DFA4CF8123871123EE1F6C1DCEA414879DDB4E05E508F1826D7EFBA6964DF804C9261EA23BBF03
// {"media":"water","meter":"aventieswm","name":"Votten","id":"61070071","total_m3":466.472,"consumption_at_set_date_1_m3":465.96,"consumption_at_set_date_2_m3":458.88,"consumption_at_set_date_3_m3":449.65,"consumption_at_set_date_4_m3":442.35,"consumption_at_set_date_5_m3":431.07,"consumption_at_set_date_6_m3":423.98,"consumption_at_set_date_7_m3":415.23,"consumption_at_set_date_8_m3":409.03,"consumption_at_set_date_9_m3":400.79,"consumption_at_set_date_10_m3":393.2,"consumption_at_set_date_11_m3":388.63,"consumption_at_set_date_12_m3":379.26,"consumption_at_set_date_13_m3":371.26,"consumption_at_set_date_14_m3":357.84,"error_flags":"","timestamp":"1111-11-11T11:11:11Z"}
// |Votten;61070071;466.472000;;1111-11-11 11:11.11


// Test: Vatten aventieswm 61070072 NOKEY
// telegram=76442104720007612507727200076121042507B50060052F2F0413281E0700431404B60083011440B300C30114A5AF00830214CBAC00C3021463A8008303149EA500C3031433A200830414C79F00C304148F9C00830514989900C30514CF9700830614269400C30614069100830714C88B0002FD171111
// {"media":"water","meter":"aventieswm","name":"Vatten","id":"61070072","total_m3":466.472,"consumption_at_set_date_1_m3":465.96,"consumption_at_set_date_2_m3":458.88,"consumption_at_set_date_3_m3":449.65,"consumption_at_set_date_4_m3":442.35,"consumption_at_set_date_5_m3":431.07,"consumption_at_set_date_6_m3":423.98,"consumption_at_set_date_7_m3":415.23,"consumption_at_set_date_8_m3":409.03,"consumption_at_set_date_9_m3":400.79,"consumption_at_set_date_10_m3":393.2,"consumption_at_set_date_11_m3":388.63,"consumption_at_set_date_12_m3":379.26,"consumption_at_set_date_13_m3":371.26,"consumption_at_set_date_14_m3":357.84,"error_flags":"MEASUREMENT HF ERROR_FLAGS_1100","timestamp":"1111-11-11T11:11:11Z"}
// |Vatten;61070072;466.472000;MEASUREMENT HF ERROR_FLAGS_1100;1111-11-11 11:11.11
