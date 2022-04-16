/*
 Copyright (C) 2020 Fredrik Öhrström (gpl-3.0-or-later)

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

#define INFO_CODE_SMOKE 0x0004
#define INFO_CODE_TEST  0x0008

struct MeterLansenSM : public virtual MeterCommonImplementation {
    MeterLansenSM(MeterInfo &mi);

    string status();
    bool smokeDetected();

private:

    void processContent(Telegram *t);

    private:

    uint16_t info_codes_ {};

};

MeterLansenSM::MeterLansenSM(MeterInfo &mi) :
    MeterCommonImplementation(mi, "lansensm")
{
    setMeterType(MeterType::SmokeDetector);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    addPrint("status", Quantity::Text,
             [&](){ return status(); },
             "The current status: OK, SMOKE, TEST or 'SMOKE TEST'.",
             PrintProperty::FIELD | PrintProperty::JSON);
}

shared_ptr<Meter> createLansenSM(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterLansenSM(mi));
}

bool MeterLansenSM::smokeDetected()
{
    return info_codes_ & INFO_CODE_SMOKE;
}

void MeterLansenSM::processContent(Telegram *t)
{
    /*
      (lansensm) 11: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (lansensm) 12: FD vif (Second extension of VIF-codes)
      (lansensm) 13: 97 vife (Error flags (binary))
      (lansensm) 14: 1D vife (Response delay time [bittimes])
      (lansensm) 15: 0000
      (lansensm) 17: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (lansensm) 18: FD vif (Second extension of VIF-codes)
      (lansensm) 19: 08 vife (Access Number (transmission count))
      (lansensm) 1a: 4C020000
      (lansensm) 1e: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (lansensm) 1f: FD vif (Second extension of VIF-codes)
      (lansensm) 20: 3A vife (Dimensionless / no VIF)
      (lansensm) 21: 46750000
    */
    int offset;

    extractDVuint16(&t->dv_entries, "02FD971D", &offset, &info_codes_);
    t->addMoreExplanation(offset, " info codes (%s)", status().c_str());
}

string MeterLansenSM::status()
{
    string s;
    bool smoke = info_codes_ & INFO_CODE_SMOKE;
    if (smoke)
    {
        s.append("SMOKE ");
    }

    bool test = info_codes_ & INFO_CODE_TEST;
    if (test)
    {
        s.append("TEST ");
    }

    if (s.length() > 0)
    {
        s.pop_back();
        return s;
    }
    return "OK";
}
