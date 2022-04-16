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

using namespace std;

struct MeterUltraHeat : public virtual MeterCommonImplementation
{
    MeterUltraHeat(MeterInfo &mi, DriverInfo &di);

private:

    double heat_mj_ {};
    double volume_m3_ {};
    double power_kw_ {};
    double flow_m3h_ {};
    double flow_c_ {};
    double return_c_ {};
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("ultraheat");
    di.setMeterType(MeterType::HeatMeter);
    di.addDetection(MANUFACTURER_LUG, 0x04,  0x04);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterUltraHeat(mi, di)); });
});

MeterUltraHeat::MeterUltraHeat(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
{
    addFieldWithExtractor(
        "heat",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::EnergyMJ,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
        "The total heat energy consumption recorded by this meter.",
        SET_FUNC(heat_mj_, Unit::MJ),
        GET_FUNC(heat_mj_, Unit::MJ));

    addFieldWithExtractor(
        "volume",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::Volume,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON ,
        "The total heating media volume recorded by this meter.",
        SET_FUNC(volume_m3_, Unit::M3),
        GET_FUNC(volume_m3_, Unit::M3));

    addFieldWithExtractor(
        "power",
        Quantity::Power,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::PowerW,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON ,
        "The current power consumption.",
        SET_FUNC(power_kw_, Unit::KW),
        GET_FUNC(power_kw_, Unit::KW));

    addFieldWithExtractor(
        "flow",
        Quantity::Flow,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::VolumeFlow,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON ,
        "The current heat media volume flow.",
        SET_FUNC(flow_m3h_, Unit::M3H),
        GET_FUNC(flow_m3h_, Unit::M3H));

    addFieldWithExtractor(
        "flow",
        Quantity::Temperature,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::FlowTemperature,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON ,
        "The current forward heat media temperature.",
        SET_FUNC(flow_c_, Unit::C),
        GET_FUNC(flow_c_, Unit::C));

    addFieldWithExtractor(
        "return",
        Quantity::Temperature,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::ReturnTemperature,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON ,
        "The current return heat media temperature.",
        SET_FUNC(return_c_, Unit::C),
        GET_FUNC(return_c_, Unit::C));
}

// Test: MyUltra ultraheat 70444600 NOKEY
// telegram=|68F8F86808007200464470A7320404270000000974040970040C0E082303000C14079519000B2D0500000B3B0808000A5B52000A5F51000A6206004C14061818004C0E490603000C7800464470891071609B102D020100DB102D0201009B103B6009009A105B78009A105F74000C22726701003C22000000007C2200000000426C01018C2006000000008C3006000000008C80100600000000CC200600000000CC300600000000CC801006000000009A115B64009A115F63009B113B5208009B112D020100BC0122000000008C010E490603008C2106000000008C3106000000008C811006000000008C011406181800046D310ACA210F21040010A0C116|
// {"media":"heat","meter":"ultraheat","name":"MyUltra","id":"70444600","heat_kwh":8974.444444,"volume_m3":1995.07,"power_kw":0.5,"flow_m3h":0.808,"flow_c":52,"return_c":51,"timestamp":"1111-11-11T11:11:11Z"}
// |MyUltra;70444600;8974.444444;1111-11-11 11:11.11
