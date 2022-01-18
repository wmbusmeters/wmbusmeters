/*
 Copyright (C) 2019-2022 Fredrik Öhrström

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

#include"dvparser.h"
#include"meters.h"
#include"meters_common_implementation.h"
#include"wmbus.h"
#include"wmbus_utils.h"

struct MeterAventiesHCA : public virtual MeterCommonImplementation
{
    MeterAventiesHCA(MeterInfo &mi, DriverInfo &di);

private:

    double current_consumption_hca_ {};
    double consumption_at_set_date_hca_[17] {}; // storagenr 1 to 17 are stored in index 0 to 16
    string error_flags_;
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("aventieshca");
    di.setMeterType(MeterType::HeatCostAllocationMeter);
    di.setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);
    di.addLinkMode(LinkMode::T1);
    di.addDetection(MANUFACTURER_AAA, 0x08,  0x55);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterAventiesHCA(mi, di)); });
});

MeterAventiesHCA::MeterAventiesHCA(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
{
    addFieldWithExtractor(
        "current_consumption",
        Quantity::HCA,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::HeatCostAllocation,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
        "The current heat cost allocation.",
        SET_FUNC(current_consumption_hca_, Unit::HCA),
        GET_FUNC(current_consumption_hca_, Unit::HCA));

    addFieldWithExtractor(
        "consumption_at_set_date",
        Quantity::HCA,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::HeatCostAllocation,
        StorageNr(1),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "Heat cost allocation at the most recent billing period date.",
        SET_FUNC(consumption_at_set_date_hca_[0], Unit::HCA),
        GET_FUNC(consumption_at_set_date_hca_[0], Unit::HCA));

    for (int i=2; i<=17; ++i)
    {
        string key, info;
        strprintf(key, "consumption_at_set_date_%d", i);
        strprintf(info, "Heat cost allocation at the %d billing period date.", i);

        addFieldWithExtractor(
            key,
            Quantity::HCA,
            NoDifVifKey,
            VifScaling::Auto,
            MeasurementType::Instantaneous,
            ValueInformation::HeatCostAllocation,
            StorageNr(i),
            TariffNr(0),
            IndexNr(1),
            PrintProperty::JSON,
            info,
            SET_FUNC(consumption_at_set_date_hca_[i-1], Unit::HCA),
            GET_FUNC(consumption_at_set_date_hca_[i-1], Unit::HCA));
    }

    addStringFieldWithExtractorAndLookup(
        "error_flags",
        Quantity::Text,
        DifVifKey("02FD17"),
        MeasurementType::Unknown,
        ValueInformation::Any,
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
         }
        );

}

// Test: HCA aventieshca 60900126 NOKEY
// telegram=|76442104260190605508722601906021045508060060052F2F#0B6E660100426EA60082016EA600C2016E9E0082026E7E00C2026E5B0082036E4200C2036E770182046E5B01C2046E4C0182056E4701C2056E3E0182066E3B01C2066E3B0182076E3B01C2076E3B0182086E1301C2086E9C0002FD170000|
// {"media":"heat cost allocation","meter":"aventieshca","name":"HCA","id":"60900126","current_consumption_hca":166,"consumption_at_set_date_hca":166,"consumption_at_set_date_2_hca":166,"consumption_at_set_date_3_hca":158,"consumption_at_set_date_4_hca":126,"consumption_at_set_date_5_hca":91,"consumption_at_set_date_6_hca":66,"consumption_at_set_date_7_hca":375,"consumption_at_set_date_8_hca":347,"consumption_at_set_date_9_hca":332,"consumption_at_set_date_10_hca":327,"consumption_at_set_date_11_hca":318,"consumption_at_set_date_12_hca":315,"consumption_at_set_date_13_hca":315,"consumption_at_set_date_14_hca":315,"consumption_at_set_date_15_hca":315,"consumption_at_set_date_16_hca":275,"consumption_at_set_date_17_hca":156,"error_flags":"","timestamp":"1111-11-11T11:11:11Z"}
// |HCA;60900126;166.000000;;1111-11-11 11:11.11
