/*
 Copyright (C) 2024 Fredrik Öhrström (gpl-3.0-or-later)

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

BuiltinDriver builtins_[] =
{
    { "elster", "driver{name=elster meter_type=GasMeter default_fields=name,id,total_m3,timestamp detect{mvt=ELS,81,03}field{name=total quantity=Volume match{measurement_type=Instantaneous vif_range=Volume}}test{args='Gas elster 05105025 NOKEY'telegram=3644A511640010253837722550100593158103E70020052F2F_0374E602000C137034220302FD74EE0F2F2F2F2F2F2F2F2F2F2F2F2F2F2F json='{\"media\":\"gas\",\"meter\":\"elster\",\"name\":\"Gas\",\"id\":\"05105025\",\"total_m3\":3223.47,\"timestamp\":\"1111-11-11T11:11:11Z\"}'fields='Gas;05105025;3223.47;1111-11-11 11:11.11'}}", false },
    { "iperl", "driver{name=iperl meter_type=WaterMeter default_fields=name,id,total_m3,max_flow_m3h,timestamp detect{mvt=SEN,68,06 mvt=SEN,68,07 mvt=SEN,7c,07}field{name=total quantity=Volume match{measurement_type=Instantaneous vif_range=Volume}about{de='Der Gesamtwasserverbrauch.'en='The total water consumption.'fr='''La consommation totale d'eau.'''sv='Den totala vattenförbrukningen.'}}field{name=max_flow quantity=Flow match{measurement_type=Instantaneous vif_range=VolumeFlow}about{en='The maximum flow recorded during previous period.'}}test{args='MoreWater iperl 12345699 NOKEY'coment='Test iPerl T1 telegram, that after decryption, has 2f2f markers.'telegram=1E44AE4C9956341268077A36001000_2F2F0413181E0000023B00002F2F2F2F json='{\"media\":\"water\",\"meter\":\"iperl\",\"name\":\"MoreWater\",\"id\":\"12345699\",\"total_m3\":7.704,\"max_flow_m3h\":0,\"timestamp\":\"1111-11-11T11:11:11Z\"}'fields='MoreWater;12345699;7.704;0;1111-11-11 11:11.11'}test{args='WaterWater iperl 33225544 NOKEY'comment='Test iPerl T1 telegram not encrypted, which has no 2f2f markers.'telegram=1844AE4C4455223368077A55000000_041389E20100023B0000 json='{\"media\":\"water\",\"meter\":\"iperl\",\"name\":\"WaterWater\",\"id\":\"33225544\",\"total_m3\":123.529,\"max_flow_m3h\":0,\"timestamp\":\"1111-11-11T11:11:11Z\"}'fields='WaterWater;33225544;123.529;0;1111-11-11 11:11.11'}}", false },
};

MapToDriver builtins_mvts_[] =
{
    { { MANUFACTURER_ELS,0x81,0x03 }, "elster" },
    { { MANUFACTURER_SEN,0x68,0x06 }, "iperl" },
    { { MANUFACTURER_SEN,0x68,0x07 }, "iperl" },
    { { MANUFACTURER_SEN,0x7c,0x07 }, "iperl" },
};
