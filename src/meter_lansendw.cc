/*
 Copyright (C) 2020-2021 Fredrik Öhrström

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


#define INFO_CODE_CLOSED 0x0011
#define INFO_CODE_OPEN   0x0055

struct MeterLansenDW : public virtual MeterCommonImplementation
{
    MeterLansenDW(MeterInfo &mi, DriverInfo &di);

private:

    void processContent(Telegram *t);

    string status();

    uint16_t info_codes_ {};
    double pulse_counter_a_ {};
    double pulse_counter_b_ {};
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("lansendw");
    di.setMeterType(MeterType::DoorWindowDetector);
    di.addLinkMode(LinkMode::T1);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterLansenDW(mi, di)); });
    di.addDetection(MANUFACTURER_LAS,  0x1d,  0x07);
});

MeterLansenDW::MeterLansenDW(MeterInfo &mi, DriverInfo &di) :
    MeterCommonImplementation(mi, di)
{
    setMeterType(MeterType::DoorWindowDetector);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    addPrint("status", Quantity::Text,
             [&](){ return status(); },
             "The current status: OPEN or CLOSED.",
             true, true);

    /*
    addPrint("statuss", Quantity::Text,
             [&](){ return status(); },
             "The current status: OPEN or CLOSED.",
             true, true);
    */
    addPrint("counter_a", Quantity::Counter,
             [&](Unit u) { assertQuantity(u, Quantity::Counter); return pulse_counter_a_; },
             "How many times the door/window has been opened or closed.",
             false, true);

    addPrint("counter_b", Quantity::Counter,
             [&](Unit u) { assertQuantity(u, Quantity::Counter); return pulse_counter_b_; },
             "The current number of counted pulses from counter b.",
             false, true);
}


shared_ptr<Meter> createLansenDW(MeterInfo &mi)
{
    DriverInfo di;
    di.setName("lansendw");
    return shared_ptr<Meter>(new MeterLansenDW(mi, di));
}


void MeterLansenDW::processContent(Telegram *t)
{
    /*
      (wmbus) 11: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (wmbus) 12: FD vif (Second extension of VIF-codes)
      (wmbus) 13: 1B vife (Digital Input (binary))
      (wmbus) 14: 1100
      (wmbus) 16: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (wmbus) 17: FD vif (Second extension of VIF-codes)
      (wmbus) 18: 97 vife (Error flags (binary))
      (wmbus) 19: 1D vife (Response delay time [bittimes])
      (wmbus) 1a: 0100
      (wmbus) 1c: 0E dif (12 digit BCD Instantaneous value)
      (wmbus) 1d: FD vif (Second extension of VIF-codes)
      (wmbus) 1e: 3A vife (Dimensionless / no VIF)
      (wmbus) 1f: 220000000000
      (wmbus) 25: 8E dif (12 digit BCD Instantaneous value)
      (wmbus) 26: 40 dife (subunit=1 tariff=0 storagenr=0)
      (wmbus) 27: FD vif (Second extension of VIF-codes)
      (wmbus) 28: 3A vife (Dimensionless / no VIF)
      (wmbus) 29: 000000000000
    */

    /*
      (wmbus) 11: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (wmbus) 12: FD vif (Second extension of VIF-codes)
      (wmbus) 13: 1B vife (Digital Input (binary))
      (wmbus) 14: 5500
      (wmbus) 16: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (wmbus) 17: FD vif (Second extension of VIF-codes)
      (wmbus) 18: 97 vife (Error flags (binary))
      (wmbus) 19: 1D vife (Response delay time [bittimes])
      (wmbus) 1a: 0100
      (wmbus) 1c: 0E dif (12 digit BCD Instantaneous value)
      (wmbus) 1d: FD vif (Second extension of VIF-codes)
      (wmbus) 1e: 3A vife (Dimensionless / no VIF)
      (wmbus) 1f: 230000000000
      (wmbus) 25: 8E dif (12 digit BCD Instantaneous value)
      (wmbus) 26: 40 dife (subunit=1 tariff=0 storagenr=0)
      (wmbus) 27: FD vif (Second extension of VIF-codes)
      (wmbus) 28: 3A vife (Dimensionless / no VIF)
      (wmbus) 29: 000000000000
    */
    int offset;

    if (extractDVuint16(&t->values, "02FD1B", &offset, &info_codes_))
    {
        t->addMoreExplanation(offset, renderJsonOnlyDefaultUnit("status"));
    }

    if (extractDVdouble(&t->values, "0EFD3A", &offset, &pulse_counter_a_, false))
    {
        t->addMoreExplanation(offset, renderJsonOnlyDefaultUnit("counter_a"));
    }

    if (extractDVdouble(&t->values, "8E40FD3A", &offset, &pulse_counter_b_, false))
    {
        t->addMoreExplanation(offset, renderJsonOnlyDefaultUnit("counter_b"));
    }
}

string MeterLansenDW::status()
{
    string s;
    if (info_codes_ == INFO_CODE_OPEN)
    {
        s.append("OPEN");
    }
    else if (info_codes_ == INFO_CODE_CLOSED)
    {
        s.append("CLOSED");
    }
    else
    {
        s.append("ERR");
    }

    return s;
}
