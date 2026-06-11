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

#include "always.h"
#include "log.h"
#include "util.h"

#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string>
#include <unistd.h>
#include <vector>

bool checkCharacterDeviceExists(const char *tty, bool fail_if_not)
{
    struct stat info;

    int rc = stat(tty, &info);
    if (rc != 0) {
        if (fail_if_not) {
            error(EXIT_DEVICE_ERROR, "Device \"%s\" does not exist.\n", tty);
        } else {
            return false;
        }
    }
    if (!S_ISCHR(info.st_mode)) {
        if (fail_if_not) {
            error(EXIT_DEVICE_ERROR, "Device %s is not a character device.\n", tty);
        } else {
            return false;
        }
    }
    return true;
}

bool checkFileExists(const char *file)
{
    struct stat info;

    int rc = stat(file, &info);
    if (rc != 0) {
        return false;
    }
    if (!S_ISREG(info.st_mode)) {
        return false;
    }
    return true;
}

bool checkIfSimulationFile(const char *file)
{
    if (!checkFileExists(file))
    {
        return false;
    }
    const char *filename = strrchr(file, '/');
    if (filename) {
        filename++;
    } else {
        filename = file;
    }
    if (strncmp(filename, "simulation", 10)) {
        return false;
    }
    return true;
}

bool checkIfDirExists(const char *dir)
{
    struct stat info;

    int rc = stat(dir, &info);
    if (rc != 0) {
        return false;
    }
    if (!S_ISDIR(info.st_mode)) {
        return false;
    }
    return faccessat(AT_FDCWD, dir, W_OK | X_OK, AT_EACCESS) == 0;
}

bool listFiles(const std::string& dir, std::vector<std::string> *files)
{
    DIR *dp = NULL;
    struct dirent *dptr = NULL;

    if (NULL == (dp = opendir(dir.c_str())))
    {
        return false;
    }
    errno = 0;
    while(NULL != (dptr = ::readdir(dp)))
    {
        if (!strcmp(dptr->d_name,".") ||
            !strcmp(dptr->d_name,".."))
        {
            // Ignore . ..  dirs.
            continue;
        }
        size_t len = strlen(dptr->d_name);
        if (len > 0 && dptr->d_name[len-1] == '~')
        {
            // Ignore emacs backup files ending in ~
            continue;
        }
        files->push_back(std::string(dptr->d_name));
        errno = 0;
    }
    bool read_error = (errno != 0);
    closedir(dp);
    if (read_error) { files->clear(); return false; }

    return true;
}

int loadFile(const std::string& file, std::vector<std::string> *lines)
{
    char block[32768+1];
    std::vector<uchar> buf;

    int fd = open(file.c_str(), O_RDONLY);
    if (fd == -1) {
        return -1;
    }
    while (true) {
        ssize_t n = read(fd, block, sizeof(block));
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            error(EXIT_FILE_ERROR, "Could not read file %s errno=%d\n", file.c_str(), errno);
        }
        buf.insert(buf.end(), block, block+n);
        if (n < (ssize_t)sizeof(block)) {
            break;
        }
    }
    close(fd);

    bool eof, err;
    auto i = buf.begin();
    for (;;) {
        std::string line = eatTo(buf, i, '\n', 32768, &eof, &err);
        if (err) {
            error(EXIT_FILE_ERROR, "Error parsing simulation file.\n");
        }
        if (line.length() > 0) {
            lines->push_back(line);
        }
        if (eof) break;
    }

    return 0;
}

bool loadFile(const std::string& file, std::vector<char> *buf)
{
    const int blocksize = 1024;
    char block[blocksize];

    int fd = open(file.c_str(), O_RDONLY);
    if (fd == -1) {
        warning("Could not open file %s errno=%d\n", file.c_str(), errno);
        return false;
    }
    while (true) {
        ssize_t n = read(fd, block, sizeof(block));
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            warning("Could not read file %s errno=%d\n", file.c_str(), errno);
            close(fd);

            return false;
        }
        buf->insert(buf->end(), block, block+n);
        if (n < (ssize_t)sizeof(block)) {
            break;
        }
    }
    close(fd);
    return true;
}

bool appendFile(const std::string &file, const std::string &line)
{
    int fd = open(file.c_str(), O_WRONLY|O_APPEND|O_CREAT, 0644);
    if (fd >= 0)
    {
        ssize_t l = write(fd, line.c_str(), line.length());
        if (l != (ssize_t)line.length()) { close(fd); return false; }
        l = write(fd, "\n", 1);
        if (l != 1) { close(fd); return false; }
        close(fd);
        return true;
    }
    return false;
}
