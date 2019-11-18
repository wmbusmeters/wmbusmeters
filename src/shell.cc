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
#include <fcntl.h>
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
    debug("(shell) exec \"%s\"\n", program.c_str());
    for (auto &a : args) {
        argv[i] = a.c_str();
        i++;
        debug("(shell) arg \"%s\"\n", a.c_str());
    }
    argv[i] = NULL;

    vector<const char*> env(envs.size()+1);
    env[0] = p;
    i = 0;
    for (auto &e : envs) {
        env[i] = e.c_str();
        i++;
        debug("(shell) env \"%s\"\n", e.c_str());
    }
    env[i] = NULL;

    pid_t pid = fork();
    int status;
    if (pid == 0) {
        // I am the child!
        close(0); // Close stdin
#if defined(__APPLE__) && defined(__MACH__)
        execve(program.c_str(), (char*const*)&argv[0], (char*const*)&env[0]);
#else
        execvpe(program.c_str(), (char*const*)&argv[0], (char*const*)&env[0]);
#endif

        perror("Execvp failed:");
        error("(shell) invoking %s failed!\n", program.c_str());
    } else {
        if (pid == -1) {
            error("(shell) could not fork!\n");
        }
        debug("(shell) waiting for child %d to complete.\n", pid);
        // Wait for the child to finish!
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            // Child exited properly.
            int rc = WEXITSTATUS(status);
            debug("(shell) %s: return code %d\n", program.c_str(), rc);
            if (rc != 0) {
                warning("(shell) %s exited with non-zero return code: %d\n", program.c_str(), rc);
            }
        }
    }
    delete[] p;
}

bool invokeBackgroundShell(string program, vector<string> args, vector<string> envs, int *fd_out, int *pid)
{
    int link[2];
    vector<const char*> argv(args.size()+2);
    char *p = new char[program.length()+1];
    strcpy(p, program.c_str());
    argv[0] = p;
    int i = 1;
    debug("(bgshell) exec background \"%s\"\n", program.c_str());
    for (auto &a : args) {
        argv[i] = a.c_str();
        i++;
        debug("(bgshell) arg \"%s\"\n", a.c_str());
    }
    argv[i] = NULL;

    vector<const char*> env(envs.size()+1);
    env[0] = p;
    i = 0;
    for (auto &e : envs) {
        env[i] = e.c_str();
        i++;
        debug("(bgshell) env \"%s\"\n", e.c_str());
    }
    env[i] = NULL;

    if (pipe(link) == -1) {
        error("(bgshell) could not create pipe!\n");
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

#if defined(__APPLE__) && defined(__MACH__)
        execve(program.c_str(), (char*const*)&argv[0], (char*const*)&env[0]);
#else
        execvpe(program.c_str(), (char*const*)&argv[0], (char*const*)&env[0]);
#endif

        perror("Execvp failed:");
        error("(bgshell) invoking %s failed!\n", program.c_str());
        return false;
    }

    // Make reads from the pipe non-blocking.
    int flags = fcntl(link[0], F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(link[0], F_SETFL, flags);

    *fd_out = link[0];
    delete[] p;
    return true;
}

bool stillRunning(int pid)
{
    if (pid == 0) return false;
    int status;
    int p = waitpid(pid, &status, WNOHANG);
    if (p == 0) {
        // The pid has not exited yet.
        return true;
    }
    if (p < 0) {
        // No pid to wait for.
        return false;
    }
    if (WIFEXITED(status)) {
        // Child exited properly.
        int rc = WEXITSTATUS(status);
        debug("(bgshell) %d exited with return code %d\n", pid, rc);
    }
    else if (WIFSIGNALED(status)) {
        // Child forcefully terminated
        debug("(bgshell) %d terminated due to signal %d\n", pid,  WTERMSIG(status));
    } else
    {
        // Exited for other reasons, whatever those may be.
        debug("(bgshell) %d exited\n", pid);
    }
    return false;
}

void stopBackgroundShell(int pid)
{
    assert(pid > 0);

    // This will actually stop the entire process group.
    // But it is ok, for now, since this function is
    // only called when wmbusmeters is exiting.

    // If we send sigint only to pid, then this will
    // not always propagate properly to the child processes
    // of the bgshell, ie rtl_sdr and rtl_wmbus, thus
    // leaving those hanging in limbo and messing everything up.
    // The solution for now is to send sigint to 0, which
    // menas send sigint to the whole process group that the
    // sender belongs to.
    int rc = kill(0, SIGINT);
    if (rc < 0) {
        debug("(bgshell) could not sigint pid %d, exited already?\n", pid);
        return;
    }
    // Wait for the child to finish!
    debug("(bgshell) sent sigint, now waiting for child %d to exit.\n", pid);
    int status;
    int p = waitpid(pid, &status, 0);
    if (p < 0) {
        debug("(bgshell) cannot stop pid %d, exited already?\n", pid);
        return;
    }
    if (WIFEXITED(status)) {
        // Child exited properly.
        int rc = WEXITSTATUS(status);
        debug("(bgshell) return code %d\n", rc);
        if (rc != 0) {
            warning("(bgshell) exited with non-zero return code: %d\n", rc);
        }
    }
    if (WIFSIGNALED(status)) {
        // Child forcefully terminated
        debug("(bgshell) %d terminated due to signal %d\n", pid,  WTERMSIG(status));
    } else
    {
        debug("(bgshell) %d exited\n", pid);
    }
}
