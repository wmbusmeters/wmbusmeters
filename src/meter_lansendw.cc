/*
 Copyright (C) 2020 Fredrik Öhrström

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

struct MeterLansenDW : public virtual DoorWindowDetector, public virtual MeterCommonImplementation {
    MeterLansenDW(MeterInfo &mi);

    string status();
    bool open();

private:

    void processContent(Telegram *t);

    private:

    uint16_t info_codes_ {};

};

MeterLansenDW::MeterLansenDW(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterType::LANSENDW)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    addPrint("status", Quantity::Text,
             [&](){ return status(); },
             "The current status: OPEN or CLOSED.",
             true, true);
}

unique_ptr<DoorWindowDetector> createLansenDW(MeterInfo &mi)
{
    return unique_ptr<DoorWindowDetector>(new MeterLansenDW(mi));
}

bool MeterLansenDW::open()
{
    return info_codes_ == INFO_CODE_OPEN;
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

    extractDVuint16(&t->values, "02FD1B", &offset, &info_codes_);
    t->addMoreExplanation(offset, " info codes (%s)", status().c_str());
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
