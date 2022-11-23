/*
 Copyright (C) 2017-2022 Fredrik Öhrström (gpl-3.0-or-later)
 Copyright (C) 2018 David Mallon (gpl-3.0-or-later)


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
        di.setName("mkradio4a");
        di.setDefaultFields("name,id,previous_m3,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_HYD, 0x06,  0xfe);
        di.addDetection(MANUFACTURER_WEH, 0x07,  0xfe);
        di.addDetection(MANUFACTURER_TCH, 0x37,  0x95);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

        addNumericFieldWithExtractor(
            "Water_previous",
            "The total water consumption recorded at the end of previous billing period.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            .set(DifVifKey("4315"))
            );

        addStringFieldWithExtractor(
            "Previous_date",
            "Date when previous billing period ended.",
            PrintProperty::JSON | PrintProperty::FIELD ,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(DifVifKey("426C"))
            .set(StorageNr(1))
            );
    }
}
//Test: Techem radio4 66953825 NOKEY  (Warm water)
//telegram=|4B44685036494600953772253895662423FE064E0030052F2F_4315A10000426CBF2C0F542CF2DD8BEC869511B2DB8301C3ABA390FB4FDB6F1144DA1F3897DD55F2AD0D194F68510FF8FADFB9|+38
//{"media":"warm water","meter":"mkradio4a","name":"www3","id":"66953825","Water_previous_m3":16.1,"Previous_date":"2021-12-31","timestamp":"2022-11-21T12:07:44Z","device":"rtlwmbus[00000001]","rssi_dbm":82}"

//Test: Techem radio4 01770002 NOKEY  (Cold water)
//telegram=|4B4468508644710095377202007701A85CFE078A0030052F2F_4315F00200426CBF2C0FEE456BF6F802216503E25EB73E9377D54F672681B76C469696E4C7BCCC9072CC79F712360FC3F57D85|+72
//{"media":"water","meter":"mkradio4a","name":"kww6","id":"01770002","Water_previous_m3":75.2,"Previous_date":"2021-12-31","timestamp":"2022-11-21T12:08:18Z","device": "rtlwmbus[00000001]","rssi_dbm":143}

