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

#include"rtlsdr.h"
#include"util.h"

// Include rtl-sdr which is licensed under GPL v2 or later
// so it happily relicenses with wmbusmeters under GPL v3 or later.
#include<rtl-sdr.h>

using namespace std;

vector<string> listRtlSdrDevices()
{
    vector<string> devices;

    uint32_t n = rtlsdr_get_device_count();

    char  mfct[256];
    char product[256];
    char serial[256];

    for (uint32_t i=0; i<n; ++i)
    {
        rtlsdr_get_device_usb_strings(i, mfct, product, serial);
        string name;
        strprintf(name, "%d_%s_%s_%s", i, mfct, product, serial);
        devices.push_back(name);
    }
    return devices;
}

AccessCheck detectRTLSDR(string device, Detected *detected)
{
    uint32_t i = indexFromRtlSdrName(device);

    uint32_t n = rtlsdr_get_device_count();

    // Check that the index extracted exists in the device name.
    // Would be nice to properly test if the device can be opened.
    if (i < n)
    {
        detected->setAsFound("", WMBusDeviceType::DEVICE_RTLWMBUS, 0, false);
        return AccessCheck::AccessOK;
    }

    // Something is wrong.
    return AccessCheck::NotThere;
}

/*
Find /dev/swradio0 1 2 3 etc
Works in Ubuntu where the dev device is automatically created.

    struct dirent **entries;
    vector<string> found_swradios;
    string devdir = "/dev/";

    int n = scandir(devdir.c_str(), &entries, NULL, sorty);
    if (n < 0)
    {
        perror("scandir");
        return found_swradios;
    }

    for (int i=0; i<n; ++i)
    {
        string name = entries[i]->d_name;

        if (name ==  ".." || name == ".")
        {
            free(entries[i]);
            continue;
        }
        // swradio0 swradio1 swradio2 ...
        if (name.length() > 7 &&
            !strncmp(name.c_str(), "swradio", 7) &&
            name[7] >= '0' &&
            name[7] <= '9')
        {
            string rtlsdr = devdir+name;
            found_swradios.push_back(rtlsdr);
        }
        free(entries[i]);
    }
    free(entries);

    return found_swradios;
*/
