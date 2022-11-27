/*
 Copyright (C) 2017-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

// This is the old style meter detection. Drivers are succesively rewritten
// from meter_xyz.cc to driver_xyz.cc only old style drivers are listed here.
// The new driver_xyz.cc file format is selfcontained so eventually this
// file be empty and go away.
//
// This file is only included into meters.cc
//
// List of numbers that can be used to detect the meter driver from a telegram.
//
//    meter driver,       manufacturer,  media,  version
//
#define METER_DETECTION \
    X(MULTICAL302,MANUFACTURER_KAM, 0x04,  0x30) \
    X(MULTICAL302,MANUFACTURER_KAM, 0x0d,  0x30) \
    X(MULTICAL302,MANUFACTURER_KAM, 0x0c,  0x30) \
    X(MULTICAL403,MANUFACTURER_KAM, 0x0a,  0x34) \
    X(MULTICAL403,MANUFACTURER_KAM, 0x0b,  0x34) \
    X(MULTICAL403,MANUFACTURER_KAM, 0x0c,  0x34) \
    X(MULTICAL403,MANUFACTURER_KAM, 0x0d,  0x34) \
    X(MULTICAL602,MANUFACTURER_KAM, 0x04,  0x1c) \
    X(MULTICAL803,MANUFACTURER_KAM, 0x04,  0x39) \



// End of list
