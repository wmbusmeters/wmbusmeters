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

#include "access_check.h"
#include "log.h"

#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

const char* toString(AccessCheck ac)
{
    switch (ac)
    {
        #define X(name,value) case AccessCheck::name: return #name;
        LIST_OF_ACCESS_CHECK_VALUE
        #undef X
    }
    return "UNKNOWN";
}

AccessCheck checkIfExistsAndHasAccess(const std::string& device)
{
    struct stat device_sb;

    int ok = stat(device.c_str(), &device_sb);

    // The file did not exist.
    if (ok) return AccessCheck::NoSuchDevice;

    int r = access(device.c_str(), R_OK);
    int w = access(device.c_str(), W_OK);
    if (r == 0 && w == 0)
    {
        // We have read and write access!
        return AccessCheck::AccessOK;
    }

    // We are not permitted to read and write to this tty. Why?
    // Lets check the group settings.

#if defined(__APPLE__) && defined(__MACH__)
        int my_groups[256];
#else
        gid_t my_groups[256];
#endif
    int ngroups = 256;

    struct passwd *p = getpwuid(getuid());

    // What are the groups I am member of?
    int rc = getgrouplist(p->pw_name, p->pw_gid, my_groups, &ngroups);
    if (rc < 0) {
        error(EXIT_PERMISSION_ERROR, "(wmbusmeters) cannot handle users with more than 256 groups\n");
    }

    // What is the group of the tty?
    struct group *device_group = getgrgid(device_sb.st_gid);

    // Go through my groups to see if the device's group is in there.
    for (int i=0; i<ngroups; ++i)
    {
        if (my_groups[i] == device_group->gr_gid)
        {
            // We belong to the same group as the tty. Typically dialout.
            // Then there is some other reason for the lack of access.
            return AccessCheck::NoPermission;
        }
    }
    // We have examined all the groups that we belong to and yet not
    // found the device's group. We can at least conclude that we
    // being in the device's group would help, ie dialout.
    return AccessCheck::NotSameGroup;
}