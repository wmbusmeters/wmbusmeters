/*
 Copyright (C) 2020 Fredrik Öhrström

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

#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<syslog.h>
#include<time.h>

#include"serial.h"
#include"shell.h"
#include"ui.h"
#include"wmbus.h"

bool running_as_root_ = false;
bool member_of_dialout_ = false;

#define LIST_OF_MAIN_MENU \
    X(DETECT_WMBUS_RECEIVERS, "Detect wmbus receiver") \
    X(RESET_WMBUS_RECEIVERS, "Reset wmbus receiver") \
    X(LISTEN_FOR_METERS, "Listen for meters") \
    X(EDIT_CONFIG, "Edit config") \
    X(EDIT_METERS, "Edit meters") \
    X(STOP_DAEMON, "Stop daemon") \
    X(START_DAEMON, "Start daemon") \
    X(EXIT_ADMIN, "Exit")

enum class MainMenuType {
#define X(name,description) name,
LIST_OF_MAIN_MENU
#undef X
};

const char *main_menu[] = {
#define X(name,description) description,
LIST_OF_MAIN_MENU
#undef X
    (char *)NULL,
};

#define LIST_OF_WMBUS_RECEIVERS \
    X(AMB8465, "amb8465") \
    X(CUL, "cul") \
    X(IM871A, "im871a") \
    X(RC1180, "rc1180")

enum class ReceiversType {
#define X(name,description) name,
LIST_OF_WMBUS_RECEIVERS
#undef X
};

const char *receivers_menu[] = {
#define X(name,description) description,
LIST_OF_WMBUS_RECEIVERS
#undef X
    (char *)NULL,
};

bool detectIfRoot();
string userName();
bool detectIfMemberOfGroup(string group);
void detectWMBUSReceiver();
void resetWMBUSReceiver();
void probeFor(string type, AccessCheck(*func)(Detected*,shared_ptr<SerialCommunicationManager>));

void stopDaemon();
void startDaemon();

shared_ptr<SerialCommunicationManager> handler;

WINDOW *status_window;
WINDOW *serial_ports_window;
WINDOW *processes_window;

void alwaysOnScreen();

int main(int argc, char **argv)
{
    if (argc == 2 && (!strcmp(argv[1], "--debug") || !strcmp(argv[1], "--trace")))
    {
        if (!strcmp(argv[1], "--trace")) traceEnabled(true); else debugEnabled(true);
        setlogmask(LOG_UPTO (LOG_INFO));
        openlog("wmbusmeters-admin", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
        enableSyslog();
    }

    initUI();
    clear();

    /*
    refresh();
    int x=0;
    int y=0;
    for (int i=0;i<10; ++i) {
        printAt(stdscr, y, x, "HEJSAN", COLOR_PAIR(BG_PAIR));
        y++;
        x++;
    };
    refresh();
    for(;;) {}
	endwin();
    return 0;
*/
    running_as_root_ = detectIfRoot();
    member_of_dialout_ = detectIfMemberOfGroup("dialout");

    handler = createSerialCommunicationManager(0, 0);

    initUI();

    bool running = true;

    registerUpdateCB(alwaysOnScreen);
    alwaysOnScreen();

    do
    {
        int c = selectFromMenu("wmbusmeters admin", main_menu);

        switch (static_cast<MainMenuType>(c))
        {
        case MainMenuType::DETECT_WMBUS_RECEIVERS:
            detectWMBUSReceiver();
            break;
        case MainMenuType::RESET_WMBUS_RECEIVERS:
            resetWMBUSReceiver();
            break;
        case MainMenuType::LISTEN_FOR_METERS:
            notImplementedYet("Listen for meters");
            break;
        case MainMenuType::EDIT_CONFIG:
            notImplementedYet("Edit config");
            break;
        case MainMenuType::EDIT_METERS:
            notImplementedYet("Edit meters");
            break;
        case MainMenuType::STOP_DAEMON:
            stopDaemon();
            break;
        case MainMenuType::START_DAEMON:
            startDaemon();
            break;
        case MainMenuType::EXIT_ADMIN:
            running = false;
            break;
        }
    } while (running);
}

void alwaysOnScreen()
{
    vector<string> info;

    if (member_of_dialout_ == false)
    {
        info.push_back("Not member of dialout!");
    }

    vector<int> daemons;
    detectProcesses("wmbusmetersd", &daemons);
    if (daemons.size() == 0)
    {
        info.push_back("No daemons running.");
    }
    else
    {
        for (int i : daemons)
        {
            info.push_back("Daemon "+to_string(i));
        }
    }

    vector<int> processes;
    detectProcesses("wmbusmeters", &processes);

    if (processes.size() == 0)
    {
    }
    else
    {
        for (int i : processes)
        {
            info.push_back("Process "+to_string(i));
        }
    }

    vector<string> status;
    time_t now = time(NULL);
    struct tm nowt {};
    localtime_r(&now, &nowt);
    status.push_back("wmbusmeters-admin");
    status.push_back(strdatetimesec(&nowt));
    string name = "["+userName()+"]";
    status.push_back(name);
    displayStatusLineNoWait(&status_window, status, 0, 0);

    displayInformationNoWait(&status_window, "Problems", info, 2, 2);

    vector<string> devices = handler->listSerialTTYs();
    if (devices.size() == 0)
    {
        devices.push_back("No serial ports found!");
    }
    //info.insert(info.end(), devices.begin(), devices.end());

    displayInformationNoWait(&serial_ports_window, "Serial ports", devices, 1, 15);

    erase();
    wrefresh(status_window);
    wrefresh(serial_ports_window);
}

void detectWMBUSReceiver()
{
    int c = selectFromMenu("Select your wmbus radio device", receivers_menu);
    switch (static_cast<ReceiversType>(c))
    {
    case ReceiversType::AMB8465:
        probeFor("amb8465", detectAMB8465);
        break;
    case ReceiversType::CUL:
        probeFor("cul", detectCUL);
        break;
    case ReceiversType::IM871A:
        probeFor("im871a", detectIM871A);
        break;
    case ReceiversType::RC1180:
        probeFor("rc1180", detectRC1180);
        break;
    }
}

void resetWMBUSReceiver()
{
    int c = selectFromMenu("Select your wmbus radio device", receivers_menu);
    switch (static_cast<ReceiversType>(c))
    {
    case ReceiversType::AMB8465:
    {
        vector<string> devices = handler->listSerialTTYs();
        if (devices.size() == 0)
        {
            vector<string> entries;
            displayInformationAndWait("No serial ports!", entries);
            return;
        }
        int c = selectFromMenu("Select device", devices);
        string device = devices[c];
        int was_baud = 0;
        AccessCheck ac = factoryResetAMB8465(device, handler, &was_baud);
        if (ac == AccessCheck::AccessOK)
        {
            vector<string> entries;
            entries.push_back("amb8465 "+device+" using "+to_string(was_baud));
            displayInformationAndWait("Factory reset successful", entries);
        }
        else
        {
            vector<string> entries;
            entries.push_back(device);
            displayInformationAndWait("No amb8465 response from", entries);
        }
        break;
    }
    case ReceiversType::CUL:
        notImplementedYet("Resetting cul");
        break;
    case ReceiversType::IM871A:
        notImplementedYet("Resetting im871a");
        break;
    case ReceiversType::RC1180:
        notImplementedYet("Resetting RC1180");
        break;
    }
}

void probeFor(string type, AccessCheck (*check)(Detected*,shared_ptr<SerialCommunicationManager>))
{
    Detected detected {};
    vector<string> devices = handler->listSerialTTYs();
    vector<string> entries;
    for (string& device : devices)
    {
        string tty = "?";
        AccessCheck ac = checkAccessAndDetect(
            handler,
            [&](string d, shared_ptr<SerialCommunicationManager> m){ detected.specified_device.file=d; return check(&detected, m);},
            type,
            device);

        if (ac == AccessCheck::AccessOK)
        {
            tty = device+" DETECTED "+type;
        }
        else if (ac == AccessCheck::NotThere)
        {
            tty = device+" nothing there";
        }
        else if (ac == AccessCheck::NotSameGroup)
        {
            tty = device+" not same group";
        }
        entries.push_back(tty);
    }
    if (entries.size() == 0)
    {
        entries.push_back("No serial devices found.");
    }
    displayInformationAndWait("Probed serial devices", entries);
}

bool detectIfRoot()
{
    vector<string> args;
    vector<string> envs;
    args.push_back("-u");
    string out;
    invokeShellCaptureOutput("/usr/bin/id", args, envs, &out, true);

    return out == "0\n";
}

string userName()
{
    vector<string> args;
    vector<string> envs;
    args.push_back("-u");
    args.push_back("-n");
    string out;
    invokeShellCaptureOutput("/usr/bin/id", args, envs, &out, true);

    return out;
}

bool detectIfMemberOfGroup(string group)
{
    vector<string> args;
    vector<string> envs;
    string out;
    invokeShellCaptureOutput("/usr/bin/groups", args, envs, &out, true);

    out = out+" "; // Guarantee that the line ends with space.

    size_t p = out.find(group+" ");
    if (p == 0) return true;

    p = out.find(" "+group+" ");
    if (p != string::npos) return true;

    return false;
}


void stopDaemon()
{
    vector<string> info;
    info.push_back("Enter sudo password to execute:");
    info.push_back("systemctl stop wmbusmeters");

    debug("(passowrd) calling inputfield\n");
    string pwd = inputField("Stop daemon", info, "Password");
    debug("(passowrd) GOT %s\n", pwd.c_str());
    //string pwd = displayInformationAndInput("Stop daemon", info, 1, 1);
        //vector<string> args;
    //vector<string> envs;
    //args.push_back("gurka");
    //   string out;
//    invokeShellCaptureOutput("systemctl stop wmbusmeters", args, envs, &out, true);
}

void startDaemon()
{
}

/*
static char* trim_whitespaces(char *str)
{
	char *end;

	// trim leading space
	while(isspace(*str))
		str++;

	if(*str == 0) // all spaces?
		return str;

	// trim trailing space
	end = str + strnlen(str, 128) - 1;

	while(end > str && isspace(*end))
		end--;

	// write new null terminator
	*(end+1) = '\0';

	return str;
}
*/
