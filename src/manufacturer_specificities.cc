/*
 Copyright (C) 2021 Vincent Privat

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

#include<set>
#include"manufacturers.h"
#include"manufacturer_specificities.h"

std::set<int> diehl_manufacturers = {
    MANUFACTURER_DME,
    MANUFACTURER_EWT,
    MANUFACTURER_HYD,
    MANUFACTURER_SAP,
    MANUFACTURER_SPL
};

// Diehl: is "A field" coded as version/type/serialnumber instead of standard serialnumber/version/type?
DiehlAddressTransformMethod mustTransformDiehlAddress(uchar c_field, int m_field, uchar ci_field, int tpl_cfg)
{
    if (diehl_manufacturers.find(m_field) != diehl_manufacturers.end())
    {
        if (c_field == 0x44 || c_field == 0x46) // SND_NR / SND_IR
        {
            switch (ci_field) {
                case 0x71: // Alarm
                    return DiehlAddressTransformMethod::SWAPPING; // Real Data
                case 0x7A: // EN 13757-3 Application Layer (short tplh)
                    if (((tpl_cfg >> 8) & 0x10) == 0x10)
                    {
                        return DiehlAddressTransformMethod::SWAPPING; // Real Data
                    }
                    break; // OMS
                case 0xA0: // Manufacturer specific
                case 0xA1: // Manufacturer specific
                case 0xA2: // Manufacturer specific
                case 0xA3: // Manufacturer specific
                case 0xA4: // Manufacturer specific
                case 0xA5: // Manufacturer specific
                case 0xA6: // Manufacturer specific
                case 0xA7: // Manufacturer specific
                    if (m_field == MANUFACTURER_SAP)
                    {
                        return DiehlAddressTransformMethod::SAP_PRIOS; // SAP PRIOS
                    }
                    return DiehlAddressTransformMethod::SWAPPING; // PRIOS
                case 0xB0: // Manufacturer specific
                    if (m_field == MANUFACTURER_SAP)
                    {
                        return DiehlAddressTransformMethod::SAP_PRIOS_STANDARD; // SAP PRIOS STD
                    }
                    break; // Reserved
                case 0xA8: // Manufacturer specific
                case 0xA9: // Manufacturer specific
                case 0xAA: // Manufacturer specific
                case 0xAB: // Manufacturer specific
                case 0xAC: // Manufacturer specific
                case 0xAD: // Manufacturer specific
                case 0xAE: // Manufacturer specific
                case 0xAF: // Manufacturer specific
                case 0xB4: // Manufacturer specific
                case 0xB5: // Manufacturer specific
                case 0xB6: // Manufacturer specific
                case 0xB7: // Manufacturer specific
                    break; // Reserved
                case 0xB1: // Manufacturer specific
                case 0xB2: // Manufacturer specific
                case 0xB3: // Manufacturer specific
                    return DiehlAddressTransformMethod::SWAPPING; // PRIOS SCR
                default:
                    break; // OMS
            }
        }
    }
    return DiehlAddressTransformMethod::NONE;
}

// Diehl: transform "A field" to make it compliant to standard
void transformDiehlAddress(std::vector<uchar>& frame, DiehlAddressTransformMethod transform_method)
{
    if (transform_method == DiehlAddressTransformMethod::SWAPPING)
    {
        debug("(diehl) Pre-processing: swapping address field\n");
        uchar version = frame[4];
        uchar type    = frame[5];
        for (int i = 4; i < 8; i++)
        {
            frame[i] = frame[i+1];
        }
        frame[8] = version;
        frame[9] = type;
    }
    else if (transform_method == DiehlAddressTransformMethod::SAP_PRIOS)
    {
        debug("(diehl) Pre-processing: setting device type to water meter for SAP PRIOS\n");
        frame[8] = 0x00; // version field is used by IZAR as part of meter id on 5 bytes instead of 4
        frame[9] = 0x07; // water meter
    }
    else if (transform_method == DiehlAddressTransformMethod::SAP_PRIOS_STANDARD)
    {
        warning("(diehl) Pre-processing: SAP PRIOS STANDARD transformation not implemented!\n"); // TODO
    }
}

