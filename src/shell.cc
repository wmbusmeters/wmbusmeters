/*
 Copyright (C) 2017-2020 Fredrik Öhrström (gpl-3.0-or-later)

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
#include "shell.h"
#include "util.h"

#include "utils/signal_handling.h"

#include <assert.h>
#include <fcntl.h>
#include <memory.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

// Posix says that this variable just exists.
// (On some systems this is also declared in unistd.h)
extern char **environ;

// Not the result borrows memory from env and thus it's lifetime.
vector<const char*> prepareEnv(vector<string>& envs)
{
    map<string, const char*> env_map;
    int i;

    for (i = 0; environ[i]; i++) {
        string e = environ[i];
        vector<string> parts = splitString(e, '=');
        env_map[parts[0]] = environ[i];
    }

    for (auto &e : envs) {
        vector<string> parts = splitString(e, '=');
        env_map[parts[0]] = e.c_str();
        debug("(shell) env \"%s\"\n", e.c_str());
    }

    vector<const char*> env(env_map.size()+1);
    i = 0;
    for (auto &e : env_map) {
        env[i] = e.second;
        i++;
    }
    env[i] = NULL;

    return env;
}

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

    vector<const char*> env = prepareEnv(envs);

    pid_t pid = fork();
    int status;
    if (pid == 0) {
        // I am the child!
        restoreSignalHandlers();
        close(0); // Close stdin
#if (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__)
        environ = (char**)&env[0];
        execvp(program.c_str(), (char*const*)&argv[0]);
#else
        execvpe(program.c_str(), (char*const*)&argv[0], (char*const*)&env[0]);
#endif

        perror("Execvp failed:");
        // Use _exit() to avoid running parent's atexit handlers and destructors
        // which can deadlock in a forked child.
        _exit(127);
    } else {
        if (pid == -1) {
            error(EXIT_SOCKET_ERROR, "(shell) could not fork!\n");
        }
        debug("(shell) waiting for child %d to complete.\n", pid);
        // Wait for the child to finish!
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            // Child exited properly.
            int rc = WEXITSTATUS(status);
            debug("(shell) %s: return code %d\n", program.c_str(), rc);
            if (rc == 127) {
                warning("(shell) invoking %s failed!\n", program.c_str());
            }
            else if (rc != 0) {
                warning("(shell) %s exited with non-zero return code: %d\n", program.c_str(), rc);
            }
        }
    }
    delete[] p;
}

bool invokeBackgroundShell(string program, vector<string> args, vector<string> envs, int *fd_out, int *pid, bool bidirectional)
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

    vector<const char*> env = prepareEnv(envs);

    *fd_out = -1;
    *pid = 0;
    int rc = bidirectional ? socketpair(AF_UNIX, SOCK_STREAM, 0, link) : pipe(link);
    if (rc == -1) {
        delete[] p;
        return false;
    }
    // Keep both endpoints away from stdio before dup2 in the child, even
    // when the caller has closed stdin/stdout/stderr. Also prevent leaks
    // into subsequently executed commands.
    for (int j = 0; j < 2; j++) {
        if (link[j] < 3) {
            int moved = fcntl(link[j], F_DUPFD, 3);
            if (moved < 0) {
                close(link[0]); close(link[1]); delete[] p;
                return false;
            }
            close(link[j]);
            link[j] = moved;
        }
        if (fcntl(link[j], F_SETFD, FD_CLOEXEC) < 0) {
            close(link[0]); close(link[1]); delete[] p;
            return false;
        }
    }
#ifdef SO_NOSIGPIPE
    if (bidirectional) {
        int on = 1;
        setsockopt(link[0], SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
    }
#endif

    *pid = fork();
    if (*pid == 0) {
        // I am the child!
        // Restore the handlers in the child.
        restoreSignalHandlers();

        // Make this child a process group leader,
        // so that we can easily terminate it and all its
        // subprocesses later one!
        setpgid(0, 0);
        // Serial commands exchange bytes through stdin/stdout. Diagnostics
        // stay on stderr instead of entering the receiver data stream.
        dup2 (link[1], STDOUT_FILENO);
        if (bidirectional) dup2(link[1], STDIN_FILENO);
        else dup2(link[1], STDERR_FILENO);
        // Close return pipe, not duped.
        close(link[0]);
        // Close old forward fd pipe.
        close(link[1]);
        if (!bidirectional) close(0);

#if (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__)
        environ = (char**)&env[0];
        execvp(program.c_str(), (char*const*)&argv[0]);
#else
        execvpe(program.c_str(), (char*const*)&argv[0], (char*const*)&env[0]);
#endif

        perror("Execvp failed:");
        // Use _exit() to avoid running parent's atexit handlers and destructors
        // which can deadlock in a forked child.
        _exit(127);
    }

    close(link[1]);
    if (*pid < 0) {
        close(link[0]);
        *pid = 0;
        delete[] p;
        return false;
    }
    // Also set the group in the parent, so immediate shutdown cannot race
    // the child's setpgid().
    setpgid(*pid, *pid);

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
        if (rc == 127) {
            warning("(bgshell) invoking child %d failed!\n", pid);
        }
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

    // Sending SIGTERM to the pid will properly shut down the subshell
    // and its contents.
    // You can check process group ids for rtl_sdr|rtl_wmbus using this command,
    // replace rtl with whatever it is that you start inside the subshell.
    //    ps -axcf -o pid,ppid,pgid,comm  | grep -B 10 -A 10 rtl
    debug("(shell) sending SIGTERM to process group %d\n", pid);
    int rc = kill(-pid, SIGTERM);
    if (rc < 0) {
        debug("(bgshell) could not sigterm -%d, exited already?\n", pid);
        return;
    }
    // Wait for the child to finish!
    debug("(bgshell) sent sigterm, now waiting for child %d to exit.\n", pid);
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

int invokeShellCaptureOutput(string program, vector<string> args, vector<string> envs, string *out, bool do_not_warn_if_fail)
{
    int rc = 0;
    int pid;
    int link[2];
    vector<const char*> argv(args.size()+2);
    char *p = new char[program.length()+1];
    strcpy(p, program.c_str());
    argv[0] = p;
    int i = 1;

    debug("(shell) exec (capture output) \"%s\"\n", program.c_str());
    for (auto &a : args) {
        argv[i] = a.c_str();
        i++;
        debug("(shell) arg \"%s\"\n", a.c_str());
    }
    argv[i] = NULL;

    vector<const char*> env = prepareEnv(envs);

    if (pipe(link) == -1) {
        error(EXIT_PIPE_ERROR, "(shell) could not create pipe!\n");
    }

    pid = fork();
    if (pid == 0) {
        // I am the child!
        restoreSignalHandlers();
        // Redirect stdout and stderr to pipe
        dup2 (link[1], STDOUT_FILENO);
        dup2 (link[1], STDERR_FILENO);
        // Close return pipe, not duped.
        close(link[0]);
        // Close old forward fd pipe.
        close(link[1]);
        close(0); // Close stdin

#if (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__)
        environ = (char**)&env[0];
        execvp(program.c_str(), (char*const*)&argv[0]);
#else
        execvpe(program.c_str(), (char*const*)&argv[0], (char*const*)&env[0]);
#endif

        perror("Execvp failed:");
        // Use _exit() to avoid running parent's atexit handlers and destructors
        // which can deadlock in a forked child.
        _exit(127);
    }

    close(link[1]);

    int fd_out = link[0];
    delete[] p;

    string data;
    uchar buf[32768];
    for(;;)
    {
        ssize_t n = read(fd_out, buf, sizeof(buf));
        if (n <= 0)
        {
            break;
        }
        data.insert(data.end(), buf, buf+n);
    }

    debug("(shell) output: >>>%s<<<\n", data.c_str());

    *out = data;

    int status;
    int pp = waitpid(pid, &status, 0);
    if (pp < 0) {
        debug("(shell) cannot stop pid %d, exited already?\n", pid);
        return 127;
    }
    if (WIFEXITED(status)) {
        // Child exited properly.
        rc = WEXITSTATUS(status);
        debug("(shell) return code %d\n", rc);
        if (rc != 0) {
            if (!do_not_warn_if_fail)
            {
                if (rc == 127) {
                    warning("(shell) invoking %s failed!\n", program.c_str());
                }
                else {
                    warning("(shell) exited with non-zero return code: %d\n", rc);
                }
            }
        }
    }
    if (WIFSIGNALED(status)) {
        // Child forcefully terminated
        debug("(shell) %d terminated due to signal %d\n", pid,  WTERMSIG(status));
    } else
    {
        debug("(shell) %d exited\n", pid);
    }

    return rc;
}

void detectProcesses(string cmd, vector<int> *pids)
{
    vector<string> args;
    vector<string> envs;
    args.push_back(cmd);
    string out;

    int rc = invokeShellCaptureOutput("pgrep", args, envs, &out, true);

    if (!rc) return;

    char buf[out.size()+1];
    strcpy(buf, out.c_str());
    char *pch;
    pch = strtok (buf," \n");
    while (pch != NULL)
    {
        pids->push_back(atoi(pch));
        pch = strtok (NULL, " \n");
    }
}
