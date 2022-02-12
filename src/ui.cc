/*
 Copyright (C) 2020-2021 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"ui.h"
#include"util.h"

using namespace std;

int screen_width_;
int screen_height_;

function<void()> update_cb_;

void initUI()
{
    initscr();
    getmaxyx(stdscr, screen_height_, screen_width_);
	start_color();
    cbreak();
    curs_set(1);
    noecho();
	keypad(stdscr, TRUE);

    init_pair(BG_PAIR, COLOR_WHITE, COLOR_BLUE);
    init_pair(WIN_PAIR, COLOR_BLACK, COLOR_WHITE);
    init_pair(TITLE_PAIR, COLOR_WHITE, COLOR_CYAN);
    init_pair(HILIGHT_PAIR, COLOR_WHITE, COLOR_RED);
    wbkgd(stdscr, COLOR_PAIR(BG_PAIR));
}

void exitUI()
{
    endwin();
}

void registerUpdateCB(std::function<void()> cb)
{
    update_cb_ = cb;
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

int selectFromMenu(const char *title, const char *entries[])
{
    vector<string> menu;
    int n_choices = countEntries(entries);

    for (int i=0; i<n_choices; ++i)
    {
        if (entries[i] == NULL) break;
        menu.push_back(entries[i]);
    }
    return selectFromMenu(title, menu);
}

int selectFromMenu(const char *title, vector<string> entries)
{
    int selected  = -1;
    ITEM **menu_items;
	int c;
	MENU *menu;
    WINDOW *frame_window, *menu_window;
    int n_choices, i;

    n_choices = entries.size()+1;
    menu_items = (ITEM **)calloc(n_choices, sizeof(ITEM *));
    for(i = 0; i < n_choices-1; ++i)
    {
        menu_items[i] = new_item(entries[i].c_str(), NULL);
    }
    menu_items[n_choices-1] = NULL;

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
    int x = screen_width_/2-w/2;
    int y = screen_height_/2-h/2;
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

    update_cb_();

    wtimeout(frame_window, 1000);

    bool running = true;
    do
    {
        c = wgetch(frame_window);
        ITEM *cur = current_item(menu);
        selected = item_index(cur);
        switch(c)
        {
        case ERR:
            update_cb_();
            redrawwin(frame_window);
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

void displayInformationAndWait(string title, vector<string> entries, int px, int py)
{
    WINDOW *frame_window;

    update_cb_();

    int mw = maxWidth(entries)+1;
    int mh = entries.size();
    int w = mw+2;
    int h = mh+4;
    if (w-2 < (int)title.length())
    {
        w = (int)title.length()+2;
    }
    int x = screen_width_/2-w/2;
    int y = screen_height_/2-h/2;
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
	//refresh();

    int r = 3;
    for (string e : entries)
    {
        printAt(frame_window, r, 1, e.c_str(), COLOR_PAIR(WIN_PAIR));
        r++;
    }
	wrefresh(frame_window);
    wtimeout(frame_window, 1000);

    bool running = true;
    do
    {
        int c = wgetch(frame_window);
        switch(c)
        {
        case ERR:
            update_cb_();
            redrawwin(frame_window);
            break;
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

void displayInformationNoWait(WINDOW **winp, string title, vector<string> entries, int px, int py)
{
    WINDOW *win = *winp;

    if (win != NULL)
    {
        delwin(win);
        *winp = NULL;
    }
    int mw = maxWidth(entries)+1;
    int mh = entries.size();
    int w = mw+2;
    int h = mh+4;
    if (w-2 < (int)title.length())
    {
        w = (int)title.length()+2;
    }
    int x = screen_width_/2-w/2;
    int y = screen_height_/2-h/2;
    if (px != -1)
    {
        x = px;
    }
    if (py != -1)
    {
        y = py;
    }
    win = newwin(h, w, y, x);
    *winp = win;

    box(win, 0, 0);
    wbkgd(win, COLOR_PAIR(WIN_PAIR));

    printMiddle(win, 1, w, title.c_str(), COLOR_PAIR(WIN_PAIR));
	mvwaddch(win, 2, 0, ACS_LTEE);
	mvwhline(win, 2, 1, ACS_HLINE, 38);
	mvwaddch(win, 2, w-1, ACS_RTEE);
//	refresh();

    int r = 3;
    for (string e : entries)
    {
        printAt(win, r, 1, e.c_str(), COLOR_PAIR(WIN_PAIR));
        r++;
    }
	wrefresh(win);
}

void displayStatusLineNoWait(WINDOW **winp, vector<string> entries, int px, int py)
{
    WINDOW *win = *winp;

    if (win != NULL)
    {
        delwin(win);
        *winp = NULL;
    }
    int w = screen_width_;
    int h = 1;
    int x = 0;
    int y = 0;
    if (px != -1)
    {
        x = px;
    }
    if (py != -1)
    {
        y = py;
    }
    win = newwin(h, w, y, x);
    *winp = win;

    wbkgd(win, COLOR_PAIR(WIN_PAIR));

    int sum = 0;
    for (string & e : entries) sum += e.length();
    int num = entries.size()-1;
    if (num == 0) num = 1;
    int spacing = (w-sum) / num;

//    printMiddle(win, 0, w, title.c_str(), COLOR_PAIR(WIN_PAIR));

    int xx = 0;
    for (string & e : entries)
    {
        printAt(win, 0, xx, e.c_str(), COLOR_PAIR(WIN_PAIR));
        xx += e.length()+spacing;
    }

	wrefresh(win);
}

string displayInformationAndInput(string title, vector<string> entries, int px, int py)
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
    int x = screen_width_/2-w/2;
    int y = screen_height_/2-h/2;
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
	//refresh();

    int r = 3;
    for (string e : entries)
    {
        printAt(frame_window, r, 1, e.c_str(), COLOR_PAIR(WIN_PAIR));
        r++;
    }
	wrefresh(frame_window);

    char buf[128];
    mvwgetnstr(frame_window, 1, 1, buf, 127);

    debug("(sudo) %s\n", buf);
    delwin(frame_window);
    erase();
    refresh();

    return buf;
}

void notImplementedYet(string msg)
{
    vector<string> entries;
    entries.push_back(msg);
    displayInformationAndWait("Not implemented yet", entries);
}

static string driver(FORM *form, FIELD **fields, int ch, WINDOW *win)
{
	//int i;
    string val = "";
	switch (ch) {
    case 10:
			// Or the current field buffer won't be sync with what is displayed
			form_driver(form, REQ_NEXT_FIELD);
			form_driver(form, REQ_PREV_FIELD);
            val = string(field_buffer(fields[1], 0));
            /*
			move(LINES-3, 2);

			for (i = 0; fields[i]; i++)
            {
				printw("%s", trim_whitespaces(field_buffer(fields[i], 0)));

				if (field_opts(fields[i]) & O_ACTIVE)
                {
					printw("\"\t");
                    val = field_buffer(fields[i], 0);
                }
				else
                {
					printw(": \"");
                }
                }
            */
			//refresh();
			//pos_form_cursor(form);
            return val;

		case KEY_DOWN:
			form_driver(form, REQ_NEXT_FIELD);
			form_driver(form, REQ_END_LINE);
			break;

		case KEY_UP:
			form_driver(form, REQ_PREV_FIELD);
			form_driver(form, REQ_END_LINE);
			break;

		case KEY_LEFT:
			form_driver(form, REQ_PREV_CHAR);
			break;

		case KEY_RIGHT:
			form_driver(form, REQ_NEXT_CHAR);
			break;

		// Delete the char before cursor
		case KEY_BACKSPACE:
		case 127:
			form_driver(form, REQ_DEL_PREV);
			break;

		// Delete the char under the cursor
		case KEY_DC:
			form_driver(form, REQ_DEL_CHAR);
			break;

		default:
            debug("CHAR %d\n", ch);
			form_driver(form, ch);
			break;
	}

	wrefresh(win);
    return "";
}

string inputField(string title, vector<string> entries, string label)
{
    FORM *form;
    FIELD *fields[3];
    WINDOW *frame_window;

    update_cb_();

    entries.push_back(""); // Add empty line on which the input field will be displayed.

    int mw = maxWidth(entries)+1;
    int mh = entries.size();
    int w = mw+2;
    int h = mh+4;
    if (w-2 < (int)title.length())
    {
        w = (int)title.length()+2;
    }
    w=50;
    h=20;
    int x = screen_width_/2-w/2;
    int y = screen_height_/2-h/2;
    frame_window = newwin(h, w, y, x);

    keypad(frame_window, TRUE);

    box(frame_window, 0, 0);
    wbkgd(frame_window, COLOR_PAIR(WIN_PAIR));

    printMiddle(frame_window, 1, w, title.c_str(), COLOR_PAIR(WIN_PAIR));
	mvwaddch(frame_window, 2, 0, ACS_LTEE);
	mvwhline(frame_window, 2, 1, ACS_HLINE, w-2);
	mvwaddch(frame_window, 2, w-1, ACS_RTEE);
	wrefresh(frame_window);

    int r = 3;
    for (string e : entries)
    {
        printAt(frame_window, r, 1, e.c_str(), COLOR_PAIR(WIN_PAIR));
        r++;
    }
	wrefresh(frame_window);
//    wtimeout(frame_window, 1000);

	fields[0] = new_field(1, 10, 1, 1, 0, 0);
	fields[1] = new_field(1, 20, 1, 15, 0, 0);
	fields[2] = NULL;
    set_field_fore(fields[0], COLOR_PAIR(WIN_PAIR));
    set_field_fore(fields[1], COLOR_PAIR(BG_PAIR));
	assert(fields[0] != NULL && fields[1] != NULL);

	set_field_buffer(fields[0], 0, "Password");
	set_field_buffer(fields[1], 0, "IFFOBIFFO");

	set_field_opts(fields[0], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
	set_field_opts(fields[1], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE); // O_PUBLIC removed to hide contents.

	set_field_back(fields[1], A_UNDERLINE);

	form = new_form(fields);
	assert(form != NULL);
	set_form_win(form, frame_window);
    WINDOW *subwin = derwin(frame_window, 5, w-2, 5, 1);
    set_form_sub(form, subwin);
    wbkgd(subwin, COLOR_PAIR(WIN_PAIR));
	post_form(form);

    wtimeout(frame_window, 1000);
    wtimeout(subwin, 1000);

	wrefresh(frame_window);
	wrefresh(subwin);
	refresh();

    int ch;
    string pwd = "";
    pos_form_cursor(form);
    form_driver(form, REQ_NEXT_FIELD);
	for (;;)
    {
        ch = getch();
        update_cb_();
        if (ch == ERR) continue;
        pwd = driver(form, fields, ch, subwin);
        if (pwd != "") break;
    }

	unpost_form(form);
	free_form(form);
	free_field(fields[0]);
	free_field(fields[1]);
	delwin(frame_window);
    delwin(subwin);
    refresh();

	return pwd;
}
