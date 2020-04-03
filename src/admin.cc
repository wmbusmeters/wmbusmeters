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
#include<malloc.h>
#include<menu.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>

#define BG_PAIR 1
#define WIN_PAIR 2
#define TITLE_PAIR 3
#define HILIGHT_PAIR 4

#include <menu.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

const char *choices[] = {
                        "Detect wmbus receivers",
                        "Detect meters",
                        "Edit config",
                        "Edit meters",
                        "Stop daemon",
                        "Start daemon",
                        "Exit",
                        (char *)NULL,
                  };

/*
loglevel=debug
device=auto
logtelegrams=false
format=json
meterfiles=/var/log/wmbusmeters/meter_readings
meterfilesaction=append
meterfilesnaming=name-id
meterfilestimestamp=day
logfile=/var/log/wmbusmeters/wmbusmeters.log
    */
void print_in_middle(WINDOW *win, int starty, int startx, int width, const char *str, chtype color);

int width, height;

int main()
{

    ITEM **my_items;
	int c;
	MENU *my_menu;
    WINDOW *my_menu_win;
    int n_choices, i;

	initscr();
    getmaxyx(stdscr, height, width);
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

	/* Create items */
    n_choices = ARRAY_SIZE(choices);
    my_items = (ITEM **)calloc(n_choices, sizeof(ITEM *));
    for(i = 0; i < n_choices; ++i)
    {
        my_items[i] = new_item(choices[i], NULL);
    }

	my_menu = new_menu((ITEM **)my_items);

    int w = 40;
    int h = 20;
    int x = width/2-w/2;
    int y = height/2-h/2;
    my_menu_win = newwin(h, w, y, x);
	set_menu_fore(my_menu, COLOR_PAIR(HILIGHT_PAIR));
	set_menu_back(my_menu, COLOR_PAIR(WIN_PAIR));
	set_menu_grey(my_menu, COLOR_PAIR(3));

    keypad(my_menu_win, TRUE);

    set_menu_win(my_menu, my_menu_win);
    set_menu_sub(my_menu, derwin(my_menu_win, h/2, w-2, 5, 1));

    set_menu_mark(my_menu, " * ");

    box(my_menu_win, 0, 0);
    wbkgd(my_menu_win, COLOR_PAIR(WIN_PAIR));

    print_in_middle(my_menu_win, 1, 0, w, "wmbusmeters", COLOR_PAIR(WIN_PAIR));
	mvwaddch(my_menu_win, 2, 0, ACS_LTEE);
	mvwhline(my_menu_win, 2, 1, ACS_HLINE, 38);
	mvwaddch(my_menu_win, 2, w-1, ACS_RTEE);
	refresh();

	post_menu(my_menu);
	wrefresh(my_menu_win);

    bool running = true;
    do
    {
        c = wgetch(my_menu_win);
        switch(c)
        {
        case KEY_DOWN:
            menu_driver(my_menu, REQ_DOWN_ITEM);
            break;
        case KEY_UP:
            menu_driver(my_menu, REQ_UP_ITEM);
            break;
        case '\n':
            running = false;
            break;
		}
        wrefresh(my_menu_win);
	} while (running);

    unpost_menu(my_menu);
    free_menu(my_menu);
    for(i = 0; i < n_choices; ++i)
    {
        free_item(my_items[i]);
    }
	endwin();
}

void print_in_middle(WINDOW *win, int starty, int startx, int width, const char *str, chtype color)
{
    int length, x, y;
	float temp;

	if(win == NULL)
    {
		win = stdscr;
    }
	getyx(win, y, x);
	if(startx != 0)
    {
		x = startx;
    }
	if(starty != 0)
    {
		y = starty;
    }
	if(width == 0)
    {
		width = 80;
    }

	length = strlen(str);
	temp = (width - length)/ 2;
	x = startx + (int)temp;
	wattron(win, color);
	mvwprintw(win, y, x, "%s", str);
	wattroff(win, color);
	refresh();
}
