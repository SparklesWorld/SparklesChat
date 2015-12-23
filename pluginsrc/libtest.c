/*
 * SparklesChat
 *
 * Copyright (C) 2015 NovaSquirrel
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
#include <squirrel.h>
#include "xchat-plugin.h"
#include "sparkles-plugin.h"

xchat_plugin *ph;
sparkles_plugin *sph;

int Sparkles_InitPlugin(xchat_plugin *plugin_handle, sparkles_plugin *sparkles_handle, char **plugin_name, char **plugin_desc, char **plugin_version, char **plugin_author, char *arg) {
  ph = plugin_handle;
  sph = sparkles_handle;
  *plugin_name = "Library install test";
  *plugin_desc = "";
  *plugin_author = "NovaSquirrel";
  *plugin_version = "0.01";

  xchat_print(ph, "Initialized libtest");
  return 1;
}

int Sparkles_DeInitPlugin() {
  xchat_print(ph, "Deinitialized libtest");
  return 1;
}

SQInteger Sq_Add(HSQUIRRELVM v) {
  SQInteger Int1, Int2;
  Sq_GetInteger(v, 2, &Int1);
  Sq_GetInteger(v, 3, &Int2);
  Sq_PushInteger(v, Int1+Int2);
  return 1;
}

int Sparkles_ImportLibrary(SquirrelVM v) {
  Spark_RegisterFunc(v, Sq_Add, "add", 2, "ii");
  return 1;
}
