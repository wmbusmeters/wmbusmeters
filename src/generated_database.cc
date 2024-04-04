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

// Generated 2024-04-04_20:51

BuiltinDriver builtins_[] =
{
    { "elster", "driver{name=elster meter_type=GasMeter default_fields=name,id,total_m3,timestamp detect{mvt=ELS,81,03}use=actuality_duration_s field{name=total quantity=Volume match{measurement_type=Instantaneous vif_range=Volume}about{de='Der Gesamtwasserverbrauch.'en='The total water consumption.'fr='''La consommation totale d'eau.'''sv='Den totala vattenförbrukningen.'}}}", false },
    { "iperl", "driver{name=iperl meter_type=WaterMeter default_fields=name,id,total_m3,max_flow_m3h,timestamp detect{mvt=SEN,68,06 mvt=SEN,68,07 mvt=SEN,7c,07}field{name=total quantity=Volume match{measurement_type=Instantaneous vif_range=Volume}about{de='Der Gesamtwasserverbrauch.'en='The total water consumption.'fr='''La consommation totale d'eau.'''sv='Den totala vattenförbrukningen.'}}field{name=max_flow quantity=Flow match{measurement_type=Instantaneous vif_range=VolumeFlow}about{en='The maximum flow recorded during previous period.'}}}", false },
    { "kampress", "driver{name=kampress default_fields=name,id,status,pressure_bar,max_pressure_bar,min_pressure_bar,timestamp meter_type=PressureSensor detect{mvt=KAM,01,18}field{name=status quantity=Text info=status_and_error_flags match{measurement_type=Instantaneous vif_range=ErrorFlags}lookup{name=ERROR_FLAGS map_type=BitToString mask_bits=0xffff default_message=OK map{name=DROP info='Unexpected drop in pressure in relation to average pressure.'value=0x01 test=Set}map{name=SURGE info='Unexpected increase in pressure in relation to average pressure.'value=0x02 test=Set}map{name=HIGH info='Average pressure has reached configurable limit. Default 15 bar.'value=0x04 test=Set}map{name=LOW info='Average pressure has reached configurable limit. Default 1.5 bar.'value=0x08 test=Set}map{name=TRANSIENT info='Pressure changes quickly over short timeperiods. Average is fluctuating.'value=0x10 test=Set}map{name=COMM_ERROR info='Cannot measure properly or bad internal communication.'value=0x20 test=Set}}}field{name=pressure quantity=Pressure info='The measured pressure.'match{measurement_type=Instantaneous vif_range=Pressure}}field{name=max_pressure quantity=Pressure info='The maximum pressure measured during ?'match{measurement_type=Maximum vif_range=Pressure}}field{name=min_pressure quantity=Pressure info='The minimum pressure measured during ?'match{measurement_type=Minimum vif_range=Pressure}}field{name=alfa info='We do not know what this is.'quantity=Dimensionless vif_scaling=None match{difvifkey=05FF09}}field{name=beta info='We do not know what this is.'quantity=Dimensionless vif_scaling=None match{difvifkey=05FF0A}}}", false },
    { "werhlemodwm", "driver{name=werhlemodwm meter_type=WaterMeter default_fields=name,id,total_m3,timestamp detect{mvt=WZG,03,16}use=meter_datetime use=target_date use=target_m3 use=total_m3 use=fabrication_no field{name=next_target quantity=PointInTime display_unit=date match{measurement_type=Instantaneous vif_range=Date add_combinable=FutureValue storage_nr=1}}}", false },
};

MapToDriver builtins_mvts_[] =
{
    { { MANUFACTURER_ELS,0x81,0x03 }, "elster" },
    { { MANUFACTURER_SEN,0x68,0x06 }, "iperl" },
    { { MANUFACTURER_SEN,0x68,0x07 }, "iperl" },
    { { MANUFACTURER_SEN,0x7c,0x07 }, "iperl" },
    { { MANUFACTURER_KAM,0x01,0x18 }, "kampress" },
    { { MANUFACTURER_WZG,0x03,0x16 }, "werhlemodwm" },
};
