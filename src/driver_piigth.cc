/*
 Copyright (C) 2021-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
#include"bus.h"

struct MeterPIIGTH : public virtual MeterCommonImplementation
{
    MeterPIIGTH(MeterInfo &mi, DriverInfo &di);

    void poll(shared_ptr<BusManager> bus_manager);

    double temperature_c_ {};
    double average_temperature_1h_c_ {};
    double average_temperature_24h_c_ {};
    double relative_humidity_rh_ {};
    double average_relative_humidity_1h_rh_ {};
    double average_relative_humidity_24h_rh_ {};
    string fabrication_no_txt_ {};
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("piigth");
    di.setMeterType(MeterType::TempHygroMeter);
    di.addLinkMode(LinkMode::MBUS);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterPIIGTH(mi, di)); });
    di.addDetection(MANUFACTURER_PII,  0x1b,  0x01);
});

MeterPIIGTH::MeterPIIGTH(MeterInfo &mi, DriverInfo &di) :
    MeterCommonImplementation(mi, di)
{
    addFieldWithExtractor(
        "temperature",
        Quantity::Temperature,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::ExternalTemperature,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The current temperature.",
        SET_FUNC(temperature_c_, Unit::C),
        GET_FUNC(temperature_c_, Unit::C));

    addFieldWithExtractor(
        "average_temperature_1h",
        Quantity::Temperature,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::ExternalTemperature,
        StorageNr(1),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The average temperature over the last hour.",
        SET_FUNC(average_temperature_1h_c_, Unit::C),
        GET_FUNC(average_temperature_1h_c_, Unit::C));

    addFieldWithExtractor(
        "average_temperature_24h",
        Quantity::Temperature,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::ExternalTemperature,
        StorageNr(2),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The average temperature over the last 24 hours.",
        SET_FUNC(average_temperature_24h_c_, Unit::C),
        GET_FUNC(average_temperature_24h_c_, Unit::C));

    /*
    addFieldWithExtractor(
        "relative_humidity",
        Quantity::Temperature,
        DifVifKey("02FB1A"),
        VifScaling::Auto,
        MeasurementType::Unknown,
        ValueInformation::RelativeHumidity,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The current relative humidity.",
        SET_FUNC(relative_humidity_rh_, Unit::RH),
        GET_FUNC(relative_humidity_rh_, Unit::RH));
    */
    addStringFieldWithExtractor(
        "fabrication_no",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::FabricationNo,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "Fabrication number.",
        SET_STRING_FUNC(fabrication_no_txt_),
        GET_STRING_FUNC(fabrication_no_txt_));

}
/*

    addPrint("current_relative_humidity", Quantity::RelativeHumidity,
             [&](Unit u){ return currentRelativeHumidity(); },
             "The current relative humidity.",
             true, true);

    addPrint("average_temperature_1h", Quantity::Temperature,
             [&](Unit u){ return convert(average_temperature_1h_c_, Unit::C, u); },
             "The average temperature over the last hour.",
             false, true);

    addPrint("average_relative_humidity_1h", Quantity::RelativeHumidity,
             [&](Unit u){ return average_relative_humidity_1h_rh_; },
             "The average relative humidity over the last hour.",
             false, true);

    addPrint("average_temperature_24h", Quantity::Temperature,
             [&](Unit u){ return convert(average_temperature_24h_c_, Unit::C, u); },
             "The average temperature over the last 24 hours.",
             false, true);

    addPrint("average_relative_humidity_24h", Quantity::RelativeHumidity,
             [&](Unit u){ return average_relative_humidity_24h_rh_; },
             "The average relative humidity over the last 24 hours.",
             false, true);
}
*/

void MeterPIIGTH::poll(shared_ptr<BusManager> bus_manager)
{
    WMBus *dev = bus_manager->findBus(bus());

    if (!dev)
    {
        fprintf(stderr, "Could not find bus from name %s\n", bus().c_str());
        return;
    }

    vector<uchar> buf;

    /*
    buf[0] = 0x10; // Start
    buf[1] = 0x40; // SND_NKE
    buf[2] = 0x00; // address 0
    uchar cs = 0;
    for (int i=1; i<3; ++i) cs += buf[i];
    buf[3] = cs; // checksum
    buf[4] = 0x16; // Stop

    dev->serial()->send(buf);

    sleep(2);
    */
    // 10000284
/*
       10000284 2941 01 1B
*/
    buf.resize(17);
    buf[0] = 0x68;
    buf[1] = 0x0b;
    buf[2] = 0x0b;
    buf[3] = 0x68;
    buf[4] = 0x73; // SND_UD
    buf[5] = 0xfd; // address 253
    buf[6] = 0x52; // ci 52
    buf[7] = 0x84; // id 78
    buf[8] = 0x02; // id 56
    buf[9] = 0x00; // id 34
    buf[10] = 0x10; // id 12
    buf[11] = 0x29; // mfct 29
    buf[12] = 0x41; // mfct 41   2941 == PII
    buf[13] = 0x01; // version/generation
    buf[14] = 0x1b; // type/media/device

    uchar cs = 0;
    for (int i=4; i<15; ++i) cs += buf[i];
    buf[15] = cs; // checksum
    buf[16] = 0x16; // Stop

    dev->serial()->send(buf);

    sleep(1);
    // 10 5B FD 58 16
    buf.resize(5);
    buf[0] = 0x10; // Start
    buf[1] = 0x5b; // REQ_UD2
    buf[2] = 0xfd; // 00 or address 253 previously setup
    cs = 0;
    for (int i=1; i<3; ++i) cs += buf[i];
    buf[3] = cs; // checksum
    buf[4] = 0x16; // Stop

    dev->serial()->send(buf);
}

/*
void MeterPIIGTH::processContent(Telegram *t)
{
    int offset;
    string key;

    if (findKey(MeasurementType::Unknown, ValueInformation::ExternalTemperature, 1, 0, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &average_temperature_1h_c_);
        t->addMoreExplanation(offset, " average temperature 1h (%f C))", average_temperature_1h_c_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::ExternalTemperature, 2, 0, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &average_temperature_24h_c_);
        t->addMoreExplanation(offset, " average temperature 24h (%f C))", average_temperature_24h_c_);
    }

    // Temporarily silly solution until the dvparser is upgraded with support for extension

    key = "02FB1A"; // 1A = 0001 1010 = First extension vif code Relative Humidity 10^-1
    if (hasKey(&t->values, key))
    {
        double tmp;
        extractDVdouble(&t->values, key, &offset, &tmp, false);
        current_relative_humidity_rh_ = tmp / (double)10.0;
        t->addMoreExplanation(offset, " current relative humidity (%f RH)", current_relative_humidity_rh_);
    }
    key = "42FB1A"; // 1A = 0001 1010 = First extension vif code Relative Humidity 10^-1
    if (hasKey(&t->values, key))
    {
        double tmp;
        extractDVdouble(&t->values, key, &offset, &tmp, false);
        average_relative_humidity_1h_rh_ = tmp / (double)10.0;
        t->addMoreExplanation(offset, " average relative humidity 1h (%f RH)", average_relative_humidity_1h_rh_);
    }
    key = "8201FB1A"; // 1A = 0001 1010 = First extension vif code Relative Humidity 10^-1
    if (hasKey(&t->values, key))
    {
        double tmp;
        extractDVdouble(&t->values, key, &offset, &tmp, false);
        average_relative_humidity_24h_rh_ = tmp / (double)10.0;
        t->addMoreExplanation(offset, " average relative humidity 24h (%f RH)", average_relative_humidity_24h_rh_);
    }
}
*/
