/*
 Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)

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

using namespace std;

struct Driver : public virtual MeterCommonImplementation
{
    Driver(MeterInfo &mi, DriverInfo &di);
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("vario411");
    di.setMeterType(MeterType::HeatMeter);
    di.addLinkMode(LinkMode::C1);
    di.addLinkMode(LinkMode::T1);
    di.addDetection(MANUFACTURER_TCH, 0x04, 0x28);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
});

Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_NO_IV);

    addNumericFieldWithExtractor(
       "energy_at_old_date",
       "Total energy consumption at the end of the year",
       PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
       Quantity::Energy,
       VifScaling::Auto,
       FieldMatcher::build()
       .set(MeasurementType::Instantaneous)
       .set(VIFRange::AnyEnergyVIF)
       .set(StorageNr(1))
       .set(TariffNr(0))
       .set(IndexNr(1))
       .set(DifVifKey("4406"))
       );

   addStringFieldWithExtractor(
       "old_date",
       "Date at end of previous period",
       PrintProperty::JSON | PrintProperty::FIELD ,
       FieldMatcher::build()
       .set(DifVifKey("426C"))
       .set(MeasurementType::Instantaneous)
       .set(VIFRange::Date)
       .set(StorageNr(1))
       );
}

// Test: Techem vario411 67627875 NOKEY
//telegram=|624468507578626728048C00F3900F002C25FEEB0600BA84134D9202A1327AFF003007102F2F_4406E1190000426CBF2C0F206730E2E7516874F5DB46B5A97816F575A29A1EA2717D6ADE5C2FE64517ED2B0497EE0FF64C2674CD0832572C484DDFED30|+22
//{"media":"heat","meter":"vario411","name":"hw3","id":"67627875","energy_at_old_date_kwh":6625,"old_date":"2021-12-31","timestamp":"2022-11-21T12:41:15Z","device":"rtlwmbus[00000001]","rssi_dbm":114}
