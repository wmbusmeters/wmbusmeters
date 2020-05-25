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

#include<curses.h>
#include<menu.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>

#include"serial.h"
#include"wmbus.h"

#define BG_PAIR 1
#define WIN_PAIR 2
#define TITLE_PAIR 3
#define HILIGHT_PAIR 4

#include <menu.h>

#define LIST_OF_MAIN_MENU \
    X(DETECT_WMBUS_RECEIVERS, "Detect wmbus receivers") \
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
    X(IM871A, "im871a")

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

void detectWMBUSReceivers();
void probeFor(string type, AccessCheck(*func)(string,SerialCommunicationManager*));

void printAt(WINDOW *win, int y, int x, const char *str, chtype color);
void printMiddle(WINDOW *win, int y, int width, const char *str, chtype color);

int selectFromMenu(const char *title, const char *menu[]);
void displayInformation(string title, vector<string> entries, int px=-1, int py=-1);

int screen_width, screen_height;
unique_ptr<SerialCommunicationManager> handler;

int main()
{
    handler = createSerialCommunicationManager(0, 0, false);

	initscr();
    getmaxyx(stdscr, screen_height, screen_width);
	start_color();
    cbreak();
    curs_set(0);
    noecho();
	keypad(stdscr, TRUE);

    init_pair(BG_PAIR, COLOR_WHITE, COLOR_BLUE);
    init_pair(WIN_PAIR, COLOR_BLACK, COLOR_WHITE);
    init_pair(TITLE_PAIR, COLOR_WHITE, COLOR_CYAN);
    init_pair(HILIGHT_PAIR, COLOR_WHITE, COLOR_RED);
    wbkgd(stdscr, COLOR_PAIR(BG_PAIR));

    bool running = true;

    do
    {
        int c = selectFromMenu("wmbusmeters admin", main_menu);

        switch (static_cast<MainMenuType>(c))
        {
        case MainMenuType::DETECT_WMBUS_RECEIVERS:
            detectWMBUSReceivers();
            break;
        case MainMenuType::LISTEN_FOR_METERS:
            break;
        case MainMenuType::EDIT_CONFIG:
            break;
        case MainMenuType::EDIT_METERS:
            break;
        case MainMenuType::STOP_DAEMON:
            break;
        case MainMenuType::START_DAEMON:
            break;
        case MainMenuType::EXIT_ADMIN:
            running = false;
            break;
        }
    } while (running);


	endwin();
}

void printAt(WINDOW *win, int y, int x, const char *str, chtype color)
{
	wattron(win, color);
	mvwprintw(win, y, x, "%s", str);
	wattroff(win, color);
	refresh();
}

void printMiddle(WINDOW *win, int y, int width, const char *str, chtype color)
{
    int len = strlen(str);
    int wh, ww;

	getyx(win, wh, ww);
    ((void)wh);

    int x = (width-len)/2;
	wattron(win, color);
	mvwprintw(win, y, x, "%s", str);
	wattroff(win, color);
	refresh();
}

int countEntries(const char *entries[])
{
    int i = 0;
    for (; entries[i] != 0; ++i);
    return i+1;
}

int maxWidth(const char *entries[])
{
    int max = 0;
    for (int i=0; entries[i] != 0; ++i)
    {
        int n = strlen(entries[i]);
        if (max < n) max = n;
    }
    return max;
}

int maxWidth(vector<string> entries)
{
    int max = 0;
    for (string& s : entries)
    {
        int n = s.length();
        if (max < n) max = n;
    }
    return max;
}

void updateStatus()
{
    vector<string> devices = handler->listSerialDevices();
    displayInformation("Serial ports", devices, 1, 1);
}

int selectFromMenu(const char *title, const char *entries[])
{
    int selected  = -1;
    ITEM **menu_items;
	int c;
	MENU *menu;
    WINDOW *frame_window, *menu_window;
    int n_choices, i;

    n_choices = countEntries(entries);
    menu_items = (ITEM **)calloc(n_choices, sizeof(ITEM *));
    for(i = 0; i < n_choices; ++i)
    {
        menu_items[i] = new_item(entries[i], NULL);
    }

	menu = new_menu(menu_items);
    int mw = 0;
    int mh = 0;
    scale_menu(menu, &mh, &mw);
    int w = mw+2;
    int h = mh+4;
    if (w-2 < (int)strlen(title))
    {
        w = (int)strlen(title)+2;
    }
    int x = screen_width/2-w/2;
    int y = screen_height/2-h/2;
    frame_window = newwin(h, w, y, x);

    int mx = (w-mw)/2;
    int my = 3;
    menu_window = derwin(frame_window, mh, mw, my, mx);

    set_menu_fore(menu, COLOR_PAIR(HILIGHT_PAIR));
	set_menu_back(menu, COLOR_PAIR(WIN_PAIR));
	set_menu_grey(menu, COLOR_PAIR(3));

    keypad(frame_window, TRUE);

    set_menu_win(menu, frame_window);
    set_menu_sub(menu, menu_window);

    set_menu_mark(menu, ">");

    box(frame_window, 0, 0);
    wbkgd(frame_window, COLOR_PAIR(WIN_PAIR));

    printMiddle(frame_window, 1, w, title, COLOR_PAIR(WIN_PAIR));
	mvwaddch(frame_window, 2, 0, ACS_LTEE);
	mvwhline(frame_window, 2, 1, ACS_HLINE, 38);
	mvwaddch(frame_window, 2, w-1, ACS_RTEE);
	refresh();

	post_menu(menu);
	wrefresh(frame_window);

    wtimeout(frame_window, 1000);

    bool running = true;
    do
    {
        fprintf(stderr, "GURKA");
        c = wgetch(frame_window);
        ITEM *cur = current_item(menu);
        selected = item_index(cur);
        switch(c)
        {
        case ERR:
            updateStatus();
            break;
        case KEY_DOWN:
            if (selected < n_choices-2)
            {
                menu_driver(menu, REQ_DOWN_ITEM);
            }
            else
            {
                menu_driver(menu, REQ_FIRST_ITEM);
            }
            break;
        case KEY_UP:
            if (selected > 0)
            {
                menu_driver(menu, REQ_UP_ITEM);
            }
            else
            {
                menu_driver(menu, REQ_LAST_ITEM);
            }
            break;
        case '\n':
            running = false;
            break;
		}
        wrefresh(frame_window);
	} while (running);

    unpost_menu(menu);
    free_menu(menu);
    delwin(frame_window);
    erase();
    refresh();
    for(i = 0; i < n_choices; ++i)
    {
        free_item(menu_items[i]);
    }

    return selected;
}

void displayInformation(string title, vector<string> entries, int px, int py)
{
    WINDOW *frame_window;

    int mw = maxWidth(entries)+1;
    int mh = entries.size();
    int w = mw+2;
    int h = mh+4;
    if (w-2 < (int)title.length())
    {
        w = (int)title.length()+2;
    }
    int x = screen_width/2-w/2;
    int y = screen_height/2-h/2;
    if (px != -1)
    {
        x = px;
    }
    if (py != -1)
    {
        y = py;
    }
    frame_window = newwin(h, w, y, x);

    keypad(frame_window, TRUE);

    box(frame_window, 0, 0);
    wbkgd(frame_window, COLOR_PAIR(WIN_PAIR));

    printMiddle(frame_window, 1, w, title.c_str(), COLOR_PAIR(WIN_PAIR));
	mvwaddch(frame_window, 2, 0, ACS_LTEE);
	mvwhline(frame_window, 2, 1, ACS_HLINE, 38);
	mvwaddch(frame_window, 2, w-1, ACS_RTEE);
	refresh();

    int r = 3;
    for (string e : entries)
    {
        printAt(frame_window, r, 1, e.c_str(), COLOR_PAIR(WIN_PAIR));
        r++;
    }
	wrefresh(frame_window);

    bool running = true;
    do
    {
        int c = wgetch(frame_window);
        switch(c)
        {
        case 27:
        case '\n':
            running = false;
            break;
		}
        wrefresh(frame_window);
	} while (running);

    delwin(frame_window);
    erase();
    refresh();
}

void detectWMBUSReceivers()
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
    }
}

void probeFor(string type, AccessCheck (*check)(string,SerialCommunicationManager*))
{
    vector<string> devices = handler->listSerialDevices();
    vector<string> entries;
    for (string& device : devices)
    {
        string tty = "?";
        AccessCheck ac = checkAccessAndDetect(
            handler.get(),
            [=](string d, SerialCommunicationManager* m){ return check(d, m);},
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
    displayInformation("Probed serial devices", entries);
}
