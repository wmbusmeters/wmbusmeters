/*
 Copyright (C) 2021 Fredrik Öhrström

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
#include"util.h"

using namespace std;

struct MeterUnismart : public virtual GasMeter, public virtual MeterCommonImplementation {
    MeterUnismart(MeterInfo &mi);

    // Total gas counted through the meter
    double totalGasConsumption(Unit u);
    bool  hasTotalGasConsumption();

private:
    void processContent(Telegram *t);

    double total_gas_consumption_m3_ {};
};

shared_ptr<GasMeter> createUnismart(MeterInfo &mi)
{
    return shared_ptr<GasMeter>(new MeterUnismart(mi));
}

MeterUnismart::MeterUnismart(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::UNISMART)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalGasConsumption(u); },
             "The total gas consumption recorded by this meter.",
             true, true);

}

void MeterUnismart::processContent(Telegram *t)
{
    int offset;
    string key;

    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_gas_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_gas_consumption_m3_);
    }
}

double MeterUnismart::totalGasConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_gas_consumption_m3_, Unit::M3, u);
}

bool MeterUnismart::hasTotalGasConsumption()
{
    return true;
}
