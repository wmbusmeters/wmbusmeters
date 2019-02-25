/*
 Copyright (C) 2017-2019 Fredrik Öhrström

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

#include "shell.h"
#include "util.h"

#include <assert.h>
#include <memory.h>
#include <pthread.h>
#include <sys/types.h>
#if defined(__APPLE__) && defined(__MACH__)
#include <sys/wait.h>
#else
#include <wait.h>
#endif

#include <unistd.h>

void invokeShell(string program, vector<string> args, vector<string> envs)
{
    vector<const char*> argv(args.size()+2);
    char *p = new char[program.length()+1];
    strcpy(p, program.c_str());
    argv[0] = p;
    int i = 1;
    debug("exec \"%s\"\n", program.c_str());
    for (auto &a : args) {
        argv[i] = a.c_str();
        i++;
        debug("arg \"%s\"\n", a.c_str());
    }
    argv[i] = NULL;

    vector<const char*> env(envs.size()+1);
    env[0] = p;
    i = 0;
    for (auto &e : envs) {
        env[i] = e.c_str();
        i++;
        debug("env \"%s\"\n", e.c_str());
    }
    env[i] = NULL;

    pid_t pid = fork();
    int status;
    if (pid == 0) {
        // I am the child!
        close(0); // Close stdin
        delete[] p;
#if defined(__APPLE__) && defined(__MACH__)
        execve(program.c_str(), (char*const*)&argv[0], (char*const*)&env[0]);
#else
        execvpe(program.c_str(), (char*const*)&argv[0], (char*const*)&env[0]);
#endif

        perror("Execvp failed:");
        error("Invoking shell %s failed!\n", program.c_str());
    } else {
        if (pid == -1) {
            error("Could not fork!\n");
        }
        debug("waiting for child %d.\n", pid);
        // Wait for the child to finish!
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            // Child exited properly.
            int rc = WEXITSTATUS(status);
            debug("%s: return code %d\n", program.c_str(), rc);
            if (rc != 0) {
                warning("%s exited with non-zero return code: %d\n", program.c_str(), rc);
            }
        }
    }
    delete[] p;
}

bool invokeBackgroundShell(string program, vector<string> args, vector<string> envs, int *out, int *err, int *pid)
{
    int link[2];
    vector<const char*> argv(args.size()+2);
    char *p = new char[program.length()+1];
    strcpy(p, program.c_str());
    argv[0] = p;
    int i = 1;
    debug("exec background \"%s\"\n", program.c_str());
    for (auto &a : args) {
        argv[i] = a.c_str();
        i++;
        debug("arg \"%s\"\n", a.c_str());
    }
    argv[i] = NULL;

    vector<const char*> env(envs.size()+1);
    env[0] = p;
    i = 0;
    for (auto &e : envs) {
        env[i] = e.c_str();
        i++;
        debug("env \"%s\"\n", e.c_str());
    }
    env[i] = NULL;

    if (pipe(link) == -1) {
        error("Could not create pipe!\n");
    }

    *pid = fork();
    if (*pid == 0) {
        // I am the child!
        // Redirect stdout and stderr to pipe
        dup2 (link[1], STDOUT_FILENO);
        dup2 (link[1], STDERR_FILENO);
        // Close return pipe, not duped.
        close(link[0]);
        // Close old forward fd pipe.
        close(link[1]);
        close(0); // Close stdin

        delete[] p;
#if defined(__APPLE__) && defined(__MACH__)
        execve(program.c_str(), (char*const*)&argv[0], (char*const*)&env[0]);
#else
        execvpe(program.c_str(), (char*const*)&argv[0], (char*const*)&env[0]);
#endif

        perror("Execvp failed:");
        warning("Invoking shell %s failed!\n", program.c_str());
        return false;
    }

    *out = link[0];
    delete[] p;
    return true;
}

void stopBackgroundShell(int pid)
{
    assert(pid > 0);

    kill(pid, SIGINT);
    int status;
    debug("waiting for child %d.\n", pid);
    // Wait for the child to finish!
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        // Child exited properly.
        int rc = WEXITSTATUS(status);
        debug("bgshell: return code %d\n", rc);
        if (rc != 0) {
            warning("bgshell: exited with non-zero return code: %d\n", rc);
        }
    }
}
