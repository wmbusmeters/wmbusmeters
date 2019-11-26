/*
 Copyright (C) 2018-2019 Fredrik Öhrström

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

#include"meters.h"
#include"meters_common_implementation.h"
#include"dvparser.h"
#include"wmbus.h"
#include"wmbus_utils.h"
#include"util.h"

#define INFO_CODE_VOLTAGE_INTERRUPTED 1
#define INFO_CODE_WRONG_FLOW_DIRECTION 2
#define INFO_CODE_SENSOR_T2_OUT_OF_RANGE 4
#define INFO_CODE_SENSOR_T1_OUT_OF_RANGE 8
#define INFO_CODE_FLOW_SENSOR_WEAK_OR_AIR 16
#define INFO_CODE_TEMP_DIFF_WRONG_POLARITY 32
#define INFO_CODE_VOLTAGE_TOO_LOW 128

struct MeterMultical302 : public virtual HeatMeter, public virtual MeterCommonImplementation {
    MeterMultical302(WMBus *bus, MeterInfo &mi);

    double totalEnergyConsumption(Unit u);
    double targetEnergyConsumption(Unit u);
    double currentPowerConsumption(Unit u);
    string status();
    double totalVolume(Unit u);
    double targetVolume(Unit u);

private:
    void processContent(Telegram *t);

    uchar info_codes_ {};
    double total_energy_kwh_ {};
    double target_energy_kwh_ {};
    double current_power_kw_ {};
    double total_volume_m3_ {};
    string target_date_ {};
};

MeterMultical302::MeterMultical302(WMBus *bus, MeterInfo &mi) :
    MeterCommonImplementation(bus, mi, MeterType::MULTICAL302, MANUFACTURER_KAM)
{
    setEncryptionMode(EncryptionMode::AES_CTR);

    addMedia(0x04); // Heat media

    addLinkMode(LinkMode::C1);

    addPrint("total_energy_consumption", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);

   addPrint("current_power_consumption", Quantity::Power,
             [&](Unit u){ return currentPowerConsumption(u); },
             "Current power consumption.",
             true, true);

    addPrint("total_volume", Quantity::Volume,
             [&](Unit u){ return totalVolume(u); },
             "Total volume of heat media.",
             true, true);

    addPrint("at_date", Quantity::Text,
             [&](){ return target_date_; },
             "Date when total energy consumption was recorded.",
             false, true);

    addPrint("total_energy_consumption_at_date", Quantity::Energy,
             [&](Unit u){ return targetEnergyConsumption(u); },
             "The total energy consumption recorded at the target date.",
             false, true);

    addPrint("current_status", Quantity::Text,
             [&](){ return status(); },
             "Status of meter.",
             true, true);

    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

unique_ptr<HeatMeter> createMultical302(WMBus *bus, MeterInfo &mi) {
    return unique_ptr<HeatMeter>(new MeterMultical302(bus, mi));
}

double MeterMultical302::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_kwh_, Unit::KWH, u);
}

double MeterMultical302::targetEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(target_energy_kwh_, Unit::KWH, u);
}

double MeterMultical302::totalVolume(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_volume_m3_, Unit::M3, u);
}

double MeterMultical302::currentPowerConsumption(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(current_power_kw_, Unit::KW, u);
}

void MeterMultical302::processContent(Telegram *t)
{
    /*
      (multical302) 11: bcdb payload crc
      (multical302) 13: 78 frame type (long frame)
      (multical302) 14: 03 dif (24 Bit Integer/Binary Instantaneous value)
      (multical302) 15: 06 vif (Energy kWh)
      (multical302) 16: * 2C0000 total energy consumption (44.000000 kWh)
      (multical302) 19: 43 dif (24 Bit Integer/Binary Instantaneous value storagenr=1)
      (multical302) 1a: 06 vif (Energy kWh)
      (multical302) 1b: * 000000 target energy consumption (0.000000 kWh)
      (multical302) 1e: 03 dif (24 Bit Integer/Binary Instantaneous value)
      (multical302) 1f: 14 vif (Volume 10⁻² m³)
      (multical302) 20: * 630000 total volume (0.990000 m3)
      (multical302) 23: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
      (multical302) 24: 6C vif (Date type G)
      (multical302) 25: * 7F2A target date (2019-10-31 00:00)
      (multical302) 27: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (multical302) 28: 2D vif (Power 10² W)
      (multical302) 29: * 1300 current power consumption (1.900000 kW)
      (multical302) 2b: 01 dif (8 Bit Integer/Binary Instantaneous value)
      (multical302) 2c: FF vif (Vendor extension)
      (multical302) 2d: 21 vife (per minute)
      (multical302) 2e: * 00 info codes (00)
    */
    map<string,pair<int,DVEntry>> values;

    vector<uchar>::iterator bytes = t->content.begin();

    int crc0 = t->content[0];
    int crc1 = t->content[1];
    t->addExplanation(bytes, 2, "%02x%02x payload crc", crc0, crc1);

    int frame_type = t->content[2];
    t->addExplanation(bytes, 1, "%02x frame type (%s)", frame_type, frameTypeKamstrupC1(frame_type).c_str());

    if (frame_type == 0x79) {
        // This is a "compact frame" in wmbus lingo.
        // (Other such frame_types are Ci=0x69, 0x6a, 0x6b and Ci=0x79, 0x7b, compact frames and format frames)
        // 0,1 = crc for format signature = hash over DRH (Data Record Header)
        // The DRH is the dif(difes)vif(vifes) bytes for all the records...
        // This hash is used to find the suitable format string, that has been previously
        // seen in a long frame telegram.
        uchar ecrc0 = t->content[3];
        uchar ecrc1 = t->content[4];
        t->addExplanation(bytes, 2, "%02x%02x format signature", ecrc0, ecrc1);
        uint16_t format_signature = ecrc1<<8 | ecrc0;

        vector<uchar> format_bytes;
        bool ok = loadFormatBytesFromSignature(format_signature, &format_bytes);
        if (!ok) {
            verbose("(%s) ignoring telegram since format signature hash 0x%02x is yet unknown.\n",
                    "multical302",  format_signature);
            return;
        }
        vector<uchar>::iterator format = format_bytes.begin();

        // 2,3 = crc for payload = hash over both DRH and data bytes. Or is it only over the data bytes?
        int ecrc2 = t->content[5];
        int ecrc3 = t->content[6];
        t->addExplanation(bytes, 2, "%02x%02x data crc", ecrc2, ecrc3);
        parseDV(t, t->content, t->content.begin()+7, t->content.size()-7, &values, &format, format_bytes.size());
    }
    else if (frame_type == 0x78)
    {
        parseDV(t, t->content, t->content.begin()+3, t->content.size()-3, &values);
    }
    else {
        warning("(multical302) warning: unknown frame %02x (did you use the correct encryption key?)\n", frame_type);
        return;
    }

    int offset;
    string key;

    extractDVuint8(&values, "01FF21", &offset, &info_codes_);
    t->addMoreExplanation(offset, " info codes (%s)", status().c_str());

    if(findKey(MeasurementType::Instantaneous, ValueInformation::EnergyWh, 0, &key, &values)) {
        extractDVdouble(&values, key, &offset, &total_energy_kwh_);
        t->addMoreExplanation(offset, " total energy consumption (%f kWh)", total_energy_kwh_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, &key, &values)) {
        extractDVdouble(&values, key, &offset, &total_volume_m3_);
        t->addMoreExplanation(offset, " total volume (%f m3)", total_volume_m3_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::EnergyWh, 1, &key, &values)) {
        extractDVdouble(&values, key, &offset, &target_energy_kwh_);
        t->addMoreExplanation(offset, " target energy consumption (%f kWh)", target_energy_kwh_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::PowerW, 0, &key, &values)) {
        extractDVdouble(&values, key, &offset, &current_power_kw_);
        t->addMoreExplanation(offset, " current power consumption (%f kW)", current_power_kw_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::Date, 1, &key, &values)) {
        struct tm datetime;
        extractDVdate(&values, key, &offset, &datetime);
        target_date_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " target date (%s)", target_date_.c_str());
    }

}

string MeterMultical302::status()
{
    string s;
    if (info_codes_ & INFO_CODE_VOLTAGE_INTERRUPTED) s.append("VOLTAGE_INTERRUPTED ");
    if (info_codes_ & INFO_CODE_WRONG_FLOW_DIRECTION) s.append("WRONG_FLOW_DIRECTION ");
    if (info_codes_ & INFO_CODE_SENSOR_T2_OUT_OF_RANGE) s.append("SENSOR_T2_OUT_OF_RANGE ");
    if (info_codes_ & INFO_CODE_SENSOR_T1_OUT_OF_RANGE) s.append("SENSOR_T1_OUT_OF_RANGE ");
    if (info_codes_ & INFO_CODE_FLOW_SENSOR_WEAK_OR_AIR) s.append("FLOW_SENSOR_WEAK_OR_AIR ");
    if (info_codes_ & INFO_CODE_TEMP_DIFF_WRONG_POLARITY) s.append("TEMP_DIFF_WRONG_POLARITY ");
    if (info_codes_ & 64) s.append("UNKNOWN_64 ");
    if (info_codes_ & INFO_CODE_VOLTAGE_TOO_LOW) s.append("VOLTAGE_TOO_LOW ");
    if (s.length() > 0) {
        s.pop_back(); // Remove final space
        return s;
    }
    return s;
}
