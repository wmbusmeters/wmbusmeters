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

#ifndef UI_H
#define UI_H

#include<assert.h>
#include<curses.h>
#include<form.h>
#include<menu.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>

#include<functional>
#include<string>
#include<vector>

#define BG_PAIR 1
#define WIN_PAIR 2
#define TITLE_PAIR 3
#define HILIGHT_PAIR 4

void initUI();

void registerUpdateCB(std::function<void()> cb);

void printAt(WINDOW *win, int y, int x, const char *str, chtype color);
void printMiddle(WINDOW *win, int y, int width, const char *str, chtype color);

int selectFromMenu(const char *title, const char *menu[]);
int selectFromMenu(const char *title, std::vector<std::string> menu);
void displayInformationAndWait(std::string title, std::vector<std::string> entries, int px=-1, int py=-1);
void displayInformationNoWait(WINDOW **win, std::string title, std::vector<std::string> entries, int px=-1, int py=-1);
void displayStatusLineNoWait(WINDOW **win, std::vector<std::string> entries, int px=-1, int py=-1);
std::string displayInformationAndInput(std::string title, std::vector<std::string> entries, int px=-1, int py=-1);
std::string inputField(std::string title, std::vector<std::string> entries, std::string label);
void notImplementedYet(std::string msg);

#endif
