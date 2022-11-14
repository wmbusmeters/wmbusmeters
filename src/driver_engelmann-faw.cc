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

struct MeterEngelmannFAW : public virtual MeterCommonImplementation
{
    MeterEngelmannFAW(MeterInfo &mi, DriverInfo &di);

private:

    double total_water_consumption_m3_ {};
    double consumption_at_set_date_m3_[16];
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("engelmann-faw");
    di.setMeterType(MeterType::WaterMeter);
    di.addLinkMode(LinkMode::T1);
    di.addDetection(MANUFACTURER_EFE,  0x07,  0x00);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterEngelmannFAW(mi, di)); });
});

MeterEngelmannFAW::MeterEngelmannFAW(MeterInfo &mi, DriverInfo &di) :
    MeterCommonImplementation(mi, di)
{
    addStringFieldWithExtractor(
        "reporting_date",
            "The reporting date of the last billing period.",
        PrintProperty::JSON | PrintProperty::FIELD,
        FieldMatcher::build()
        .set(MeasurementType::Instantaneous)
        .set(VIFRange::Date)
        .set(StorageNr(1))
        );

    addNumericFieldWithExtractor(
        "consumption_at_reporting_date",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::Volume,
        StorageNr(1),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
        "The water consumption at the last billing period date.",
        SET_FUNC(total_water_consumption_m3_, Unit::M3),
        GET_FUNC(total_water_consumption_m3_, Unit::M3));

    addStringFieldWithExtractorAndLookup(
        "current_status",
        "Status and error flags.",
        PrintProperty::JSON | PrintProperty::FIELD | JOIN_TPL_STATUS,
        FieldMatcher::build()
        .set(VIFRange::ErrorFlags),
        {
            {
                {
                    "ERROR_FLAGS",
                    Translate::Type::BitToString,
                    0xff,
                    "OK",
                    {
                        { 0x01, "VOLUME_DETECTION_COIL(S)_DEFECT" },
                        { 0x02, "RESET" },
                        { 0x04, "CRC_ERROR" },
                        { 0x08, "REMOVAL_DETECTED" },
                        { 0x10, "MAGNETIC_MANIPULATION" },
                        { 0x20, "LEAKAGE" },
                        { 0x40, "BLOCKED" },
                        { 0x80, "REVERSE_FLOW" },
                    }
                },
            },
        });
    
    for (int i=2; i<=16; ++i)
    {
        string msg, info;
        strprintf(msg, "consumption_%d_months_ago", i-1);
        strprintf(info, "Water consumption %d month(s) ago.", i-1);
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
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            info,
            SET_FUNC(consumption_at_set_date_m3_[i-1], Unit::M3),
            GET_FUNC(consumption_at_set_date_m3_[i-1], Unit::M3));
    }


}


// Test: Wasserzaehler engelmann-faw 43000255 NOKEY
// telegram=8f44c5145502004301077260402520c51400076b0000002f2f426cbf2c441322e9000001fd17008401133c340100c40113ae2d010084021303290100c402137e21010084031313180100c403138a0e010084041337060100c40413b2fc00008405139af30000c4051322e90000840613c1df0000c40613cdd5000084071365ce0000c407136dc500008408138dbf0000
// {"media":"water","meter":"engelmann-faw","name":"Wasserzaehler","id":"20254060","reporting_date":"2021-12-31","consumption_at_reporting_date_m3":59.682,"current_status":"OK","consumption_1_months_ago_m3":78.908,"consumption_2_months_ago_m3":77.23,"consumption_3_months_ago_m3":76.035,"consumption_4_months_ago_m3":74.11,"consumption_5_months_ago_m3":71.699,"consumption_6_months_ago_m3":69.258,"consumption_7_months_ago_m3":67.127,"consumption_8_months_ago_m3":64.69,"consumption_9_months_ago_m3":62.362,"consumption_10_months_ago_m3":59.682,"consumption_11_months_ago_m3":57.281,"consumption_12_months_ago_m3":54.733,"consumption_13_months_ago_m3":52.837,"consumption_14_months_ago_m3":50.541,"consumption_15_months_ago_m3":49.037,"timestamp":"1111-11-11T11:11:11Z"}
// |Wasserzaehler;20254060;2021-12-31;59.682000;OK;1111-11-11 11:11.11

