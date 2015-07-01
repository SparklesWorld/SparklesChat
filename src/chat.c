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
#include "chat.h"

int RecurseLevel = 0;
int GUIType = 2; // default to external gui
int quit = 0;
int SqCurlRunning = 0;
char *PrefPath = NULL;
char *BasePath = NULL;
CURLM *MultiCurl;
Uint32 MainThreadEvent;
IPC_Holder MainToEvent, SocketToEvent, EventToMain, EventToSocket;

cJSON *MenuMain, *MenuChannel, *MenuUser, *MenuUserTab, *MenuTextEdit, *MainConfig, *PluginPref;

ClientTab *FirstTab = NULL;
ClientAddon *FirstAddon = NULL;
//ClientAddon *FirstProtocol = NULL;
EventType *FirstEventType = NULL;
EventType *FirstCommand = NULL;
EventHook *FirstTimer = NULL;
CommandHelp *FirstHelp = NULL;
SDL_mutex *LockConfig, *LockTabs, *LockEvent, *LockSockets, *LockDialog, *LockMTR;

const char *GUINames[] = {"SDL", "Text", NULL};
int (*InitGUI[])(void) = {InitMinimalGUI, InitTextGUI};
void (*RunGUI[])(void) = {RunMinimalGUI, RunTextGUI};
void (*EndGUI[])(void) = {EndMinimalGUI, EndTextGUI};

void PluginPref_Save() {
  // todo
}
int RunEventThread(void *Data);
int RunSocketThread(void *Data);

int main( int argc, char* args[] ) {
  if(SDL_Init(0) < 0){
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return 0;
  }

  srand(time(NULL));
  SDL_SetHint("SDL_HINT_VIDEO_ALLOW_SCREENSAVER", "1");
  InitSock();
  curl_global_init(CURL_GLOBAL_ALL);
  MultiCurl = curl_multi_init();

  LockConfig = SDL_CreateMutex();
  LockTabs = SDL_CreateMutex();
  LockEvent = SDL_CreateMutex();
  LockSockets = SDL_CreateMutex();
  LockMTR = SDL_CreateMutex();

  MainConfig = cJSON_Load("config.json");
  if(!MainConfig) {
    SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", NULL, "config.json has errors, so the client can't start.\n"
                                                        "Make sure there's no comma after the last item in every {}.");
    return 0;
  }

  const char *GUITypeString = GetConfigStr("gtk", "Client/GUI");
  for(int i=0;GUINames[i];i++)
    if(!strcasecmp(GUINames[i],GUITypeString)) {
      GUIType = i;
      break;
    }
  if(GUIType == 0) // only initialize video if we're using it
    SDL_InitSubSystem(SDL_INIT_VIDEO);
  PluginPref = cJSON_Load("pluginpref.json");
  if(!PluginPref)
    PluginPref = cJSON_CreateObject();

  IPC_New(&MainToEvent);
  IPC_New(&EventToMain);
  IPC_New(&SocketToEvent);
  IPC_New(&EventToSocket);

  MainThreadEvent = SDL_RegisterEvents(1);

  freopen("CON", "w", stdout); // supposed to fix problems with SDL eating stdout/stderr
  freopen("CON", "w", stderr);

  PrefPath = SDL_GetPrefPath("Bushytail Software","SparklesChat");
  BasePath = SDL_GetBasePath();

  MenuMain = cJSON_Load("data/menus/mainmenu.json");
  MenuChannel = cJSON_Load("data/menus/channelmenu.json");
  MenuUser = cJSON_Load("data/menus/usermenu.json");
  MenuUserTab = cJSON_Load("data/menus/usermenutab.json");
  MenuTextEdit = cJSON_Load("data/menus/texteditmenu.json");

  SDL_Thread *EventThread = SDL_CreateThread(RunEventThread, "Event", NULL);
  SDL_Thread *SocketThread = SDL_CreateThread(RunSocketThread, "Socket", NULL);

  {
    cJSON *Autoload = cJSON_GetObjectItem(MainConfig, "Client");
    Autoload = cJSON_GetObjectItem(Autoload, "Autoload");

	for (int i=0; i<cJSON_GetArraySize(Autoload); i++) {
      cJSON *LoadMe = cJSON_GetArrayItem(Autoload,i);
      char Path[strlen(LoadMe->valuestring)+1];
      strcpy(Path, LoadMe->valuestring);
      char *Separator = strrchr(Path, '.');
      if(!Separator)
        continue;
      *Separator = 0;
      AutoloadDirectory(Path, Separator+1, AutoloadAddon);
	}
  }

  // later all GUIs will be plugins and this will be cleaner
  if(GUIType == 2) {
    char *DLLPath = (char*)malloc(260*sizeof(char));
    sprintf(DLLPath, "%sgui%s.%s", BasePath, GUITypeString, SHARED_OBJ_EXTENSION);
    ClientAddon *Addon = LoadAddon(DLLPath, &FirstAddon);
    free(DLLPath);
    if(!Addon) {
      SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", NULL, "Addon specified in the config file didn't load or doesn't exist");
      return 0;
    }
    void (*Start)() = SDL_LoadFunction(Addon->CPlugin, "Sparkles_StartGUI");
    if(!Start) {
      SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", NULL, "Addon specified in the config file doesn't contain Sparkles_StartGUI()");
      return 0;
    }
    Start();
  } else if(GUIType < 2) {
    (*InitGUI[GUIType])();
    while(!quit) {
      (*RunGUI[GUIType])();
      SDL_Delay(17);
    }
  }
  while(FirstAddon)
    UnloadAddon(FirstAddon, &FirstAddon, 0);
  if(GUIType < 2)
    (*EndGUI[GUIType])();
  quit = 1;
  SDL_WaitThread(EventThread, NULL);
  SDL_WaitThread(SocketThread, NULL);
  IPC_Free(&MainToEvent);
  IPC_Free(&EventToMain);
  IPC_Free(&SocketToEvent);
  IPC_Free(&EventToSocket);

  cJSON_Delete(MenuMain);
  cJSON_Delete(MenuChannel);
  cJSON_Delete(MenuUser);
  cJSON_Delete(MenuUserTab);
  cJSON_Delete(MenuTextEdit);
  cJSON_Delete(MainConfig);
  cJSON_Delete(PluginPref);

  SDL_DestroyMutex(LockConfig);
  SDL_DestroyMutex(LockTabs);
  SDL_DestroyMutex(LockEvent);
  SDL_DestroyMutex(LockSockets);
  SDL_DestroyMutex(LockMTR);

  curl_multi_cleanup(MultiCurl);
  curl_global_cleanup();
  EndSock();
  if(PrefPath) SDL_free(PrefPath);
  if(BasePath) SDL_free(BasePath);
  SDL_Quit();
  return 0;
}
