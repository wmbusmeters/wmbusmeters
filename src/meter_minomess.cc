/*
 Copyright (C) 2021 Olli Salonen

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

struct MeterMinomess : public virtual MeterCommonImplementation {
    MeterMinomess(MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

    double targetWaterConsumption(Unit u);
    bool  hasTargetWaterConsumption();

private:

    string status();
    uint16_t info_codes_ {};
    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};
    string meter_date_ {};
    double target_water_consumption_m3_ {};
    string target_water_consumption_date_ {};
    bool has_target_water_consumption_ {};

};

MeterMinomess::MeterMinomess(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::MINOMESS)
{
    setMeterType(MeterType::WaterMeter);

    setExpectedELLSecurityMode(ELLSecurityMode::AES_CTR);

    addLinkMode(LinkMode::C1);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);

    addPrint("meter_date", Quantity::Text,
             [&](){ return meter_date_; },
             "Date when measurement was recorded.",
             false, true);

    addPrint("target", Quantity::Volume,
             [&](Unit u){ return targetWaterConsumption(u); },
             "The total water consumption recorded at the beginning of this month.",
             true, true);

    addPrint("target_date", Quantity::Text,
             [&](){ return target_water_consumption_date_; },
             "Date when target water consumption was recorded.",
             false, true);

    addPrint("status", Quantity::Text,
             [&](){ return status(); },
             "Status of meter.",
             true, true);

}

shared_ptr<Meter> createMinomess(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterMinomess(mi));
}

double MeterMinomess::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MeterMinomess::hasTotalWaterConsumption()
{
    return true;
}

double MeterMinomess::targetWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(target_water_consumption_m3_, Unit::M3, u);
}

bool MeterMinomess::hasTargetWaterConsumption()
{
    return has_target_water_consumption_;
}

void MeterMinomess::processContent(Telegram *t)
{
    // 00: 66 length (102 bytes)
    // 01: 44 dll-c (from meter SND_NR)
    // 02: 496a dll-mfct (ZRI)
    // 04: 10640355 dll-id (55036410)
    // 08: 14 dll-version
    // 09: 37 dll-type (Radio converter (meter side))
    // 0a: 72 tpl-ci-field (EN 13757-3 Application Layer (long tplh))
    // 0b: 51345015 tpl-id (15503451)
    // 0f: 496a tpl-mfct (ZRI)
    // 11: 00 tpl-version
    // 12: 07 tpl-type (Water meter)
    // 13: 76 tpl-acc-field
    // 14: 00 tpl-sts-field (OK)
    // 15: 5005 tpl-cfg 0550 (AES_CBC_IV nb=5 cntn=0 ra=0 hc=0 )
    // 17: 2f2f decrypt check bytes

    // 19: 0C dif (8 digit BCD Instantaneous value)
    // 1a: 13 vif (Volume l)
    // 1b: * 55140000 total consumption (1.455000 m3)
    // 1f: 02 dif (16 Bit Integer/Binary Instantaneous value)
    // 20: 6C vif (Date type G)
    // 21: * A92B meter date (2021-11-09)
    // 23: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // 24: 04 dife (subunit=0 tariff=0 storagenr=8)
    // 25: 6C vif (Date type G)
    // 26: * A12B target consumption reading date (2021-11-01)
    // 28: 8C dif (8 digit BCD Instantaneous value)
    // 29: 04 dife (subunit=0 tariff=0 storagenr=8)
    // 2a: 13 vif (Volume l)
    // 2b: * 71000000 target consumption (0.071000 m3)
    //
    // 2f: 8D dif (variable length Instantaneous value)
    // 30: 04 dife (subunit=0 tariff=0 storagenr=8)
    // 31: 93 vif (Volume l)
    // 32: 13 vife (Reverse compact profile without register)
    // 33: 2C varlen=44
    //  This register has 24-bit integers for the consumption of the past months n-2 until n-15.
    //  If the meter is commissioned less than 15 months ago, you will see FFFFFF as the value.
    //          n-2    n-3    n-4    n-5    n-6    n-7    n-8    n-9    n-10   n-11   n-12   n-13   n-14   n-15
    // 34: FBFE 000000 FFFFFF FFFFFF FFFFFF FFFFFF FFFFFF FFFFFF FFFFFF FFFFFF FFFFFF FFFFFF FFFFFF FFFFFF FFFFFF
    //
    // 60: 02 dif (16 Bit Integer/Binary Instantaneous value)
    // 61: FD vif (Second extension FD of VIF-codes)
    // 62: 17 vife (Error flags (binary))
    // 63: * 0000 info codes (OK)

    int offset;
    string key;

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Date, 0, 0, &key, &t->values)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        meter_date_ = strdate(&date);
        t->addMoreExplanation(offset, " meter date (%s)", meter_date_.c_str());
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 8, 0, &key, &t->values)) {
        uint32_t i;
        extractDVdouble(&t->values, key, &offset, &target_water_consumption_m3_);
        extractDVuint32(&t->values, key, &offset, &i);
        /* if the meter is recently commissioned, the target water consumption value is bogus */
        if (i != 0xffffffff) {
           has_target_water_consumption_ = true;
	} else {
           target_water_consumption_m3_ = 0;
	}
        t->addMoreExplanation(offset, " target consumption (%f m3)", target_water_consumption_m3_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Date, 8, 0, &key, &t->values)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        target_water_consumption_date_ = strdate(&date);
        t->addMoreExplanation(offset, " target consumption reading date (%s)", target_water_consumption_date_.c_str());
    }

    if (extractDVuint16(&t->values, "02FD17", &offset, &info_codes_)) {
        t->addMoreExplanation(offset, " info codes (%s)", status().c_str());
    }

}

string MeterMinomess::status()
{
    /*
    To-Do: Implement status handling
    According to data sheet, there are two status/info bytes, byte A and byte B.

    Byte A:
    bit 7 removal active in the past
    bit 6 tamper active in the past
    bit 5 leak active in the past
    bit 4 temporary error (in connection with smart functions)
    bit 3 permanent error (meter value might be lost)
    bit 2 battery EOL (measured)
    bit 1 abnormal error
    bit 0 unused

    Byte B:
    bit 7 burst
    bit 6 removal
    bit 5 leak
    bit 4 backflow in the past
    bit 3 backflow
    bit 2 meter blocked in the past
    bit 1 meter undersized
    bit 0 meter oversized
    */

    string s;

    if (info_codes_ == 0) return "OK";

    s = tostrprintf("%x", info_codes_);
    if (s.length() > 0) {
        s.pop_back(); // Remove final space
        return s;
    }
    return s;
}
