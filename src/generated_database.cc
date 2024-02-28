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

// Generated 2024-02-12_01:46

BuiltinDriver builtins_[] =
{
    { "elster", "driver{name=elster meter_type=GasMeter default_fields=name,id,total_m3,timestamp detect{mvt=ELS,81,03}use=actuality_duration_s field{name=total quantity=Volume match{measurement_type=Instantaneous vif_range=Volume}about{de='Der Gesamtwasserverbrauch.'en='The total water consumption.'fr='''La consommation totale d'eau.'''sv='Den totala vattenförbrukningen.'}}}", false },
    { "iperl", "driver{name=iperl meter_type=WaterMeter default_fields=name,id,total_m3,max_flow_m3h,timestamp detect{mvt=SEN,68,06 mvt=SEN,68,07 mvt=SEN,7c,07}field{name=total quantity=Volume match{measurement_type=Instantaneous vif_range=Volume}about{de='Der Gesamtwasserverbrauch.'en='The total water consumption.'fr='''La consommation totale d'eau.'''sv='Den totala vattenförbrukningen.'}}field{name=max_flow quantity=Flow match{measurement_type=Instantaneous vif_range=VolumeFlow}about{en='The maximum flow recorded during previous period.'}}}", false },
};

MapToDriver builtins_mvts_[] =
{
    { { MANUFACTURER_ELS,0x81,0x03 }, "elster" },
    { { MANUFACTURER_SEN,0x68,0x06 }, "iperl" },
    { { MANUFACTURER_SEN,0x68,0x07 }, "iperl" },
    { { MANUFACTURER_SEN,0x7c,0x07 }, "iperl" },
};
