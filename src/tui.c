/*
 * SparklesChat
 *
 * Copyright (C) 2014-2015 NovaSquirrel
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
// uses http://www.projectpluto.com/win32a.htm
#include "chat.h"
#include <curses.h>

void TextGUI_Redraw() {
  erase();
  refresh();
}

extern SDL_Color IRCColors[];
int InitTextGUI() {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  nodelay(stdscr, TRUE);
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "colors %i color pairs %i", COLORS, COLOR_PAIRS);

  if(can_change_color()) {
    for(int i=0;i<IRCCOLOR_PALETTE_SIZE;i++) {
      init_color(i, (IRCColors[i].r*1000)>>8, (IRCColors[i].g*1000)>>8, (IRCColors[i].b*1000)>>8);
      init_pair(i, i, IRCCOLOR_BG);
    }
  }

  TextGUI_Redraw();
//  box(stdscr,'|','-');
  border('|', '|', '-', '-', '.', '.', '\'', '\'');
  return 1;
}
void RunTextGUI() {
  int k = getch();
  if(k==ERR) return;
  if(k==KEY_RESIZE) {
    resize_term(0, 0);
    border('|', '|', '-', '-', '.', '.', '\'', '\'');
  }
  //SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%c or %i", k, k);
  mvprintw(5, 5, "%c or %i", k, k);
  if(k=='q')
    quit = 1;
}
void EndTextGUI() {
  endwin();
}
