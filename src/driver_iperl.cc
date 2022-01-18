/*
 Copyright (C) 2017-2022 Fredrik Öhrström
 Copyright (C) 2018 David Mallon

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

struct MeterIperl : public virtual MeterCommonImplementation
{
    MeterIperl(MeterInfo &mi, DriverInfo &di);

private:

    double total_water_consumption_m3_ {};
    double max_flow_m3h_ {};
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("iperl");
    di.setMeterType(MeterType::WaterMeter);
    di.setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);
    di.addLinkMode(LinkMode::T1);
    di.addDetection(MANUFACTURER_SEN,  0x06,  0x68);
    di.addDetection(MANUFACTURER_SEN,  0x07,  0x68);
    di.addDetection(MANUFACTURER_SEN,  0x07,  0x7c);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterIperl(mi, di)); });
});

MeterIperl::MeterIperl(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
{
    setMeterType(MeterType::WaterMeter);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    // version 0x68
    // version 0x7c Sensus 640

    addFieldWithExtractor(
        "total",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
        "The total water consumption recorded by this meter.",
        SET_FUNC(total_water_consumption_m3_, Unit::M3),
        GET_FUNC(total_water_consumption_m3_, Unit::M3));

    addFieldWithExtractor(
        "max_flow",
        Quantity::Flow,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::VolumeFlow,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
        "The maxium flow recorded during previous period.",
        SET_FUNC(max_flow_m3h_, Unit::M3H),
        GET_FUNC(max_flow_m3h_, Unit::M3H));
}

// Test: MoreWater iperl 12345699 NOKEY
// Comment: Test iPerl T1 telegram, that after decryption, has 2f2f markers.
// telegram=|1E44AE4C9956341268077A36001000#2F2F0413181E0000023B00002F2F2F2F|
// {"media":"water","meter":"iperl","name":"MoreWater","id":"12345699","total_m3":7.704,"max_flow_m3h":0,"timestamp":"1111-11-11T11:11:11Z"}
// |MoreWater;12345699;7.704000;0.000000;1111-11-11 11:11.11

// Test: WaterWater iperl 33225544 NOKEY
// Comment: Test iPerl T1 telegram not encrypted, which has no 2f2f markers.
// telegram=|1844AE4C4455223368077A55000000|041389E20100023B0000|
// {"media":"water","meter":"iperl","name":"WaterWater","id":"33225544","total_m3":123.529,"max_flow_m3h":0,"timestamp":"1111-11-11T11:11:11Z"}
// |WaterWater;33225544;123.529000;0.000000;1111-11-11 11:11.11
