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
// uses http://www.projectpluto.com/win32a.htm on Windows
#include "chat.h"
#include <curses.h>
#define WITH_CTRL(x) (x&0x1F)
#ifdef __PDCURSES__
#define either_getmouse nc_getmouse
#else
#define either_getmouse getmouse
#endif

// stub to replace after refactoring
