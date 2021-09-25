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
    /*
 (unismart) 11: 0C dif (8 digit BCD Instantaneous value)
(unismart) 12: 78 vif (Fabrication no)
(unismart) 13: 96221603
(unismart) 17: 04 dif (32 Bit Integer/Binary Instantaneous value)
(unismart) 18: 6D vif (Date and time type)
(unismart) 19: 122DAF29
(unismart) 1d: 0C dif (8 digit BCD Instantaneous value)
(unismart) 1e: 94 vif (Volume 10⁻² m³)
(unismart) 1f: 3A vife (uncorrected meter unit)
(unismart) 20: * 00170900 total consumption (917.000000 m3)
(unismart) 24: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
(unismart) 25: 6D vif (Date and time type)
(unismart) 26: 0026A129
(unismart) 2a: 4C dif (8 digit BCD Instantaneous value storagenr=1)
(unismart) 2b: 94 vif (Volume 10⁻² m³)
(unismart) 2c: 3A vife (uncorrected meter unit)
(unismart) 2d: 32110900
(unismart) 31: 01 dif (8 Bit Integer/Binary Instantaneous value)
(unismart) 32: FD vif (Second extension FD of VIF-codes)
(unismart) 33: 67 vife (Special supplier information)
(unismart) 34: 00
(unismart) 35: 02 dif (16 Bit Integer/Binary Instantaneous value)
(unismart) 36: FD vif (Second extension FD of VIF-codes)
(unismart) 37: 74 vife (Reserved)
(unismart) 38: F00C
(unismart) 3a: 0D dif (variable length Instantaneous value)
(unismart) 3b: FD vif (Second extension FD of VIF-codes)
(unismart) 3c: 0C vife (Model/Version)
(unismart) 3d: 06 varlen=6
(unismart) 3e: 554747342020
(unismart) 44: 01 dif (8 Bit Integer/Binary Instantaneous value)
(unismart) 45: FD vif (Second extension FD of VIF-codes)
(unismart) 46: 0B vife (Parameter set identification)
(unismart) 47: 02
(unismart) 48: 01 dif (8 Bit Integer/Binary Instantaneous value)
(unismart) 49: 7F vif (Manufacturer specific)
(unismart) 4a: 14
(unismart) 4b: 06 dif (48 Bit Integer/Binary Instantaneous value)
(unismart) 4c: 6D vif (Date and time type)
(unismart) 4d: 1E120DAF296D
(unismart) 53: 2F skip
(unismart) 54: 2F skip
(unismart) 55: 2F skip
(unismart) 56: 2F skip
(unismart) 57: 2F skip
(unismart) 58: 2F skip
(unismart) 59: 2F skip
(unismart) 5a: 2F skip
(unismart) 5b: 2F skip
(unismart) 5c: 2F skip
(unismart) 5d: 2F skip
(unismart) 5e: 2F skip

*/
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
