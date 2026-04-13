/*
 Copyright (C) 2017-2026 Fredrik Öhrström (gpl-3.0-or-later)

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

#ifndef SHORT_MANUAL_H
#define SHORT_MANUAL_H

inline constexpr const char* SHORT_MANUAL = R"MANUAL(
Usage: wmbusmeters {options} [device] { [meter_name] [meter_driver] [meter_id] [meter_key] }*
       wmbusmeters {options} [hex]    { [meter_name] [meter_driver] [meter_id] [meter_key] }*
       wmbusmetersd {options} [pid_file]

As {options} you can use:

    --alarmexpectedactivity=mon-fri(08-17),sat-sun(09-12) Specify when the timeout is tested, default is mon-sun(00-23)
    --alarmshell=<cmdline> invokes cmdline when an alarm triggers
    --alarmtimeout=<time> Expect a telegram to arrive within <time> seconds, eg 60s, 60m, 24h during expected activity.
    --analyze Analyze a telegram to find the best driver.
    --analyze=<key> Analyze a telegram to find the best driver use the provided decryption key.
    --analyze=<driver> Analyze a telegram and use only this driver.
    --analyze=<driver>:<key> Analyze a telegram and use only this driver with this key.
    --calculate_field_unit='...' Add field_unit to the json and calculate it using the formula. E.g.
    --calculate_sumtemp_c='external_temperature_c+flow_temperature_c'
    --calculate_flow_f=flow_temperature_c
    --debug for a lot of information
    --donotprobe=<tty> do not auto-probe this tty. Use multiple times for several ttys or specify "all" for all ttys.
    --driver=<file> load a driver
    --driversdir=<dir> load all drivers in dir
    --exitafter=<time> exit program after time, eg 20h, 10m 5s
    --format=<hr/json/fields> for human readable, json or semicolon separated fields
    --help list all options
    --identitymode=(id|id-mfct|full|none) group meter state based on the identity mode. Default is id.
    --ignoreduplicates=<bool> ignore duplicate telegrams, remember the last 10 telegrams
    --field_xxx=yyy always add "xxx"="yyy" to the json output and add shell env METER_xxx=yyy (--json_xxx=yyy also works)
    --license print GPLv3+ license
    --listento=<mode> listen to one of the c1,t1,s1,s1m,n1a-n1f link modes
    --listento=<mode>,<mode> listen to more than one link mode at the same time, assuming the dongle supports it
    --listenvs=<meter_driver> list the env variables available for the given meter driver
    --listfields=<meter_driver> list the fields selectable for the given meter driver
    --printdriver=<meter_driver> print xmq driver source code
    --listmeters list all meter drivers
    --listmeters=<search> list all meter drivers containing the text <search>
    --listunits list all unit suffixes that can be used for typing values
    --logfile=<file> use this file for logging or --logfile=syslog
    --logtelegrams log the contents of the telegrams for easy replay
    --logtimestamps=<when> add log timestamps: always never important
    --meterfiles=<dir> store meter readings in dir
    --meterfilesaction=(overwrite|append) overwrite or append to the meter readings file
    --meterfilesnaming=(name|id|name-id) the meter file is the meter's: name, id or name-id
    --meterfilestimestamp=(never|day|hour|minute|micros) the meter file is suffixed with a
                          timestamp (localtime) with the given resolution.
    --metershell=<cmdline> invokes cmdline with env variables the first time a meter is seen since startup
    --nodeviceexit if no wmbus devices are found, then exit immediately
    --normal for normal logging
    --oneshot wait for an update from each meter, then quit
    --overridedevice=<device> override device in config files. Use only in combination with --useconfig= option
    --ppjson pretty print the json
    --pollinterval=<time> time between polling of meters, must be set to get polling.
    --resetafter=<time> reset the wmbus dongle regularly, default is 23h
    --selectfields=id,timestamp,total_m3 select only these fields to be printed (--listfields=<meter> to list available fields)
    --separator=<c> change field separator to c
    --shell=<cmdline> invokes cmdline with env variables containing the latest reading
    --silent do not print informational messages nor warnings
    --trace for tons of information
    --useconfig=<dir> load config <dir>/wmbusmeters.conf and meters from <dir>/wmbusmeters.d
    --usestderr write notices/debug/verbose and other logging output to stderr (the default)
    --usestdoutforlogging write debug/verbose and logging output to stdout
    --verbose for more information
    --version print version
)MANUAL";
#endif
