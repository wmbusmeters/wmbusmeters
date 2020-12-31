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

#define INFO_CODE_SMOKE 0x0001

struct MeterTSD2 : public virtual SmokeDetector, public virtual MeterCommonImplementation
{
    MeterTSD2(MeterInfo &mi);

    string status();
    bool smokeDetected();

private:

    void processContent(Telegram *t);

    private:

    uint16_t info_codes_ {};
    bool error_ {};
};

MeterTSD2::MeterTSD2(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterType::TSD2)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    addPrint("status", Quantity::Text,
             [&](){ return status(); },
             "The current status: OK, SMOKE or ERROR.",
             true, true);
}

shared_ptr<SmokeDetector> createTSD2(MeterInfo &mi)
{
    return shared_ptr<SmokeDetector>(new MeterTSD2(mi));
}

bool MeterTSD2::smokeDetected()
{
    return info_codes_ & INFO_CODE_SMOKE;
}

void MeterTSD2::processContent(Telegram *t)
{
    vector<uchar> data;
    t->extractPayload(&data);

    if (data.size() > 0)
    {
        info_codes_ = data[0];
        error_ = false;
        // Check CRC etc here....
    }
    else
    {
        error_ = true;
    }
}

string MeterTSD2::status()
{
    string s;
    bool smoke = info_codes_ & INFO_CODE_SMOKE;
    if (smoke)
    {
        s.append("SMOKE ");
    }

    if (error_)
    {
        s.append("ERROR ");
    }

    if (s.length() > 0)
    {
        s.pop_back();
        return s;
    }
    return "OK";
}
