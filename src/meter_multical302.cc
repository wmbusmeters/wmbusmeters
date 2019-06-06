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
#include"wmbus.h"
#include"wmbus_utils.h"
#include"util.h"

struct MeterMultical302 : public virtual HeatMeter, public virtual MeterCommonImplementation {
    MeterMultical302(WMBus *bus, MeterInfo &mi);

    double totalEnergyConsumption(Unit u);
    double currentPowerConsumption(Unit u);
    double totalVolume(Unit u);

private:
    void processContent(Telegram *t);

    double total_energy_kwh_ {};
    double current_power_kw_ {};
    double total_volume_m3_ {};
};

MeterMultical302::MeterMultical302(WMBus *bus, MeterInfo &mi) :
    MeterCommonImplementation(bus, mi, MeterType::MULTICAL302, MANUFACTURER_KAM)
{
    setEncryptionMode(EncryptionMode::AES_CTR);

    addMedia(0x04); // Heat media

    addLinkMode(LinkMode::C1);

    addPrint("total", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);

    addPrint("total_volume", Quantity::Volume,
             [&](Unit u){ return totalVolume(u); },
             "Total volume of heat media.",
             true, true);

   addPrint("current", Quantity::Power,
             [&](Unit u){ return currentPowerConsumption(u); },
             "Current power consumption.",
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
    vector<uchar>::iterator bytes = t->content.begin();

    int crc0 = t->content[0];
    int crc1 = t->content[1];
    t->addExplanation(bytes, 2, "%02x%02x payload crc", crc0, crc1);
    int frame_type = t->content[2];
    t->addExplanation(bytes, 1, "%02x frame type (%s)", frame_type, frameTypeKamstrupC1(frame_type).c_str());

    if (frame_type == 0x79) {
        // This code should be rewritten to use parseDV see the Multical21 code.
        // But I cannot do this without more examples of 302 telegrams.
        t->addExplanation(bytes, 4, "%02x%02x%02x%02x unknown", t->content[3], t->content[4], t->content[5], t->content[6]);

        int rec1val0 = t->content[7];
        int rec1val1 = t->content[8];
        int rec1val2 = t->content[9];

        t->addExplanation(bytes, 4, "%02x%02x%02x unknown", t->content[10], t->content[11], t->content[12]);

        int total_energy_raw  = rec1val2*256*256 + rec1val1*256 + rec1val0;
        total_energy_kwh_ = total_energy_raw;
        t->addExplanation(bytes, 3, "%02x%02x%02x total power (%d)",
                          rec1val0, rec1val1, rec1val2, total_energy_raw);

	int rec2val0 = t->content[13];
        int rec2val1 = t->content[14];
	int rec2val2 = t->content[15];

        int total_volume_raw = rec2val2*256*256 + rec2val1*256 + rec2val0;
        total_volume_m3_ = total_volume_raw;
        t->addExplanation(bytes, 3, "%02x%02x%02x total volume (%d)",
                          rec2val0, rec2val1, rec2val2, total_volume_raw);
    }
    else if (frame_type == 0x78)
    {
        // This code should be rewritten to use parseDV see the Multical21 code.
        // But I cannot do this without more examples of 302 telegrams.
        vector<uchar> unknowns;
        unknowns.insert(unknowns.end(), t->content.begin()+3, t->content.begin()+24);
        string hex = bin2hex(unknowns);
        t->addExplanation(bytes, 23-2, "%s unknown", hex.c_str());

        int rec1val0 = t->content[24];
        int rec1val1 = t->content[25];

        int current_power_raw = (rec1val1*256 + rec1val0)*100;
        current_power_kw_ = current_power_raw;
        t->addExplanation(bytes, 2, "%02x%02x current power (%d)",
                          rec1val0, rec1val1, current_power_raw);
    }
    else {
        warning("(multical302) warning: unknown frame %02x (did you use the correct encryption key?)\n", frame_type);
    }
}
