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

#ifndef ACCESS_CHECK_H_
#define ACCESS_CHECK_H_

#include<string>

// Returns AccessOK if device exists and is accessible.
// NotSameGroup means that there is no permission and the groups do not match.
// NoPermission means some other reason for no access. (missing rw etc)
// Locked means that some other process has locked the tty.
// NoSuchDevice means the tty does not exist.
// NoProperResponse means that we talked to something, but we do not know what it is.
#define LIST_OF_ACCESS_CHECK_VALUE \
    X(AccessOK, 0) \
    X(NotSameGroup, 1) \
    X(NoPermission, 2) \
    X(Locked, 3) \
    X(NoSuchDevice, 4) \
    X(NoProperResponse, 5)

enum class AccessCheck {
    #define X(name,value) name = value,
    LIST_OF_ACCESS_CHECK_VALUE
    #undef X
};

const char* toString(AccessCheck ac);
AccessCheck checkIfExistsAndHasAccess(const std::string& device);

#endif // ACCESS_CHECK_H_
