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

#include "log.h"
#include "shell.h"
#include "utils/download.h"

#include <cassert>
#include <cstring>
#include <string>
#include <vector>

#define DRIVER_SERVER_URL "https://wmbusmeters.org/drivers/"

const char * basic_auth_cred_ = nullptr;

void setBasicAuth(const char *cred)
{
    basic_auth_cred_ = cred;
}

bool no_network_ = false;

void setNoNetwork(bool v)
{
    no_network_ = v;
}

char download_dir_[256] = {0};

void setDownloadDir(const char *dir)
{
    assert(strlen(dir) < sizeof(download_dir_));
    strcpy(download_dir_, dir);
}

const char *downloadDir()
{
    if (download_dir_[0]) return download_dir_;

    const char *home = getenv("HOME");
    snprintf(download_dir_, 256, "%s/.local/share/wmbusmeters/wmbusmeters.drivers.d", home);
    return download_dir_;
}

int download(const char *suffix, const char *file, const char *local_file)
{
    bool exists = false;

    FILE *f = fopen(local_file, "rb");
    if (f)
    {
        // Already downloaded.
        fclose(f);
        exists = true;
    }

    // Not in download dir, download...
    // curl https://wmbusmeters.org/drivers/iperl.xmq
    char url[256];
    snprintf(url, 256, DRIVER_SERVER_URL "%s%s", file, suffix);

    if (no_network_)
    {
        if (exists)
        {
            verbose("(driver) using cache for %s (no network access)\n", file);
            return 304;
        }
        warning("(driver) no driver %s found in cache and no network access\n", file);
        return -1;
    }

    std::vector<std::string> args;
    args.push_back("-s");
    args.push_back("--fail");
    args.push_back("--create-dirs");
    args.push_back("-R");
    args.push_back("--write-out");
    args.push_back("%{http_code}");
    if (
        basic_auth_cred_ != nullptr &&
        strlen(basic_auth_cred_) != 0
    )
    {
        args.push_back("-u");
        args.push_back(basic_auth_cred_);
    }
    if (exists)
    {
        // The -z time comparison with a local file, only works if the local file does exist.
        args.push_back("-z");
        args.push_back(local_file);
    }
    args.push_back("-o");
    args.push_back(local_file);
    args.push_back(url);

    std::string curl = "curl";

    std::string cmd = curl+" ";
    for (auto a : args) cmd += a+" ";

    verbose("(driver) checking %s\n", url);

    std::string out;
    invokeShellCaptureOutput(curl, args, {}, &out, true);
    char *endptr = NULL;
    const char *startptr = out.c_str();
    long code = strtol(out.c_str(), &endptr, 10);

    if (endptr != NULL && endptr != startptr && *endptr == 0)
    {
        // Code is valid and it was decoded properly.
        return code;
    }

    warning("wmbusmeters: failed to fetch %s using this command: %s\n", suffix, cmd.c_str());

    return -1;
}
