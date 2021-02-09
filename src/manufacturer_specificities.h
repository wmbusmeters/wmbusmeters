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

#ifndef MANUFACTURER_SPECIFICITIES_H
#define MANUFACTURER_SPECIFICITIES_H

#include"util.h"

enum class DiehlAddressTransformMethod {
    NONE,
    SWAPPING,
    SAP_PRIOS,
    SAP_PRIOS_STANDARD
};

// Diehl: Is "A field" coded as version/type/serialnumber instead of standard serialnumber/version/type?
DiehlAddressTransformMethod mustTransformDiehlAddress(uchar c_field, int m_field, uchar ci_field, int tpl_cfg);

// Diehl: swap "A field" to make it compliant to standard
void transformDiehlAddress(std::vector<uchar>& frame, DiehlAddressTransformMethod method);

#endif
