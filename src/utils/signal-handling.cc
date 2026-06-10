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

#include"always.h"
#include"log.h"
#include"util.h"
#include"shell.h"
#include"version.h"

#include<algorithm>
#include<assert.h>
#include<dirent.h>
#include<errno.h>
#include<fcntl.h>
#include<functional>
#include<math.h>
#include<set>
#include<signal.h>
#include<stdarg.h>
#include<stddef.h>
#include<string.h>
#include<string>
#include<sys/stat.h>
#include<sys/time.h>
#include<sys/types.h>
#include<syslog.h>
#include<time.h>
#include<unistd.h>

using namespace std;

// Sigint, sigterm will call the exit handler.
function<void()> exit_handler_;

bool signalsInstalled()
{
    return exit_handler_ != NULL;
}

bool got_hupped_ {};

void exitHandler(int signum)
{
    got_hupped_ = signum == SIGHUP;
    if (exit_handler_) exit_handler_();
}

bool gotHupped()
{
    return got_hupped_;
}

pthread_t wake_me_up_on_sig_chld_ {};

void wakeMeUpOnSigChld(pthread_t t)
{
    wake_me_up_on_sig_chld_ = t;
}

void doNothing(int signum)
{
}

void signalMyself(int signum)
{
    if (wake_me_up_on_sig_chld_)
    {
        if (signalsInstalled())
        {
            pthread_kill(wake_me_up_on_sig_chld_, SIGUSR1);
        }
    }
}

struct sigaction old_int, old_hup, old_term, old_chld, old_usr1, old_usr2;

void onExit(function<void()> cb)
{
    exit_handler_ = cb;
    struct sigaction new_action;

    new_action.sa_handler = exitHandler;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_flags = 0;

    sigaction(SIGINT, &new_action, &old_int);
    sigaction(SIGHUP, &new_action, &old_hup);
    sigaction(SIGTERM, &new_action, &old_term);

    new_action.sa_handler = signalMyself;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_flags = 0;
    sigaction(SIGCHLD, &new_action, &old_chld);

    new_action.sa_handler = doNothing;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_flags = 0;
    sigaction(SIGUSR1, &new_action, &old_usr1);

    new_action.sa_handler = doNothing;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_flags = 0;
    sigaction(SIGUSR2, &new_action, &old_usr2);
}

void restoreSignalHandlers()
{
    exit_handler_ = NULL;

    sigaction(SIGINT, &old_int, NULL);
    sigaction(SIGHUP, &old_hup, NULL);
    sigaction(SIGTERM, &old_term, NULL);
    sigaction(SIGCHLD, &old_chld, NULL);
    sigaction(SIGUSR1, &old_usr1, NULL);
    sigaction(SIGUSR2, &old_usr2, NULL);
}
