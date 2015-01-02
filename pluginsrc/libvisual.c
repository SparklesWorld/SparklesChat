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
#include "../src/chat.h"
#include "xchat-plugin.h"
#include "sparkles-plugin.h"

xchat_plugin *ph;
sparkles_plugin *sph;

static int test_cb(char *word[], char *word_eol[], void *userdata) {
  for(int i=0;i<10;i++)
    xchat_printf(ph, "%i = %s, %s", i, word[i], word_eol[i]);
  return XCHAT_EAT_ALL;
}

int Sparkles_InitPlugin(xchat_plugin *plugin_handle, sparkles_plugin *sparkles_handle, char **plugin_name, char **plugin_desc, char **plugin_version, char **plugin_author, char *arg) {
  ph = plugin_handle;
  sph = sparkles_handle;
  xchat_hook_command(ph, "testvisual", XCHAT_PRI_NORM, test_cb, "", 0);

  *plugin_name = "Visual";
  *plugin_desc = "Overlay functions";
  *plugin_author = "NovaSquirrel";
  *plugin_version = "0.01";
  return 1;
}

int Sparkles_DeInitPlugin() {
  xchat_print(ph, "BYE");
  return 1;
}

int Sparkles_InstallLibrary(SquirrelVM v) {
  return 1;
}
