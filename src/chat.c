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
int GUIType = 0;
int quit = 0;
int SqCurlRunning = 0;
char *PrefPath = NULL;
CURLM *MultiCurl;
Uint32 MainThreadEvent;
IPC_Queue *EventQueue[1];
IPC_Queue *SocketQueue[1];

cJSON *MenuMain, *MenuChannel, *MenuUser, *MenuUserTab, *MenuTextEdit, *MainConfig, *PluginPref;

ClientTab *FirstTab = NULL;
ClientAddon *FirstAddon = NULL;
//ClientAddon *FirstProtocol = NULL;
EventType *FirstEventType = NULL;
EventType *FirstCommand = NULL;
EventHook *FirstTimer = NULL;
CommandHelp *FirstHelp = NULL;
SDL_mutex *LockConfig, *LockTabs, *LockEvent, *LockSockets;

cJSON *cJSON_Search(cJSON *Root, char *SearchString) {
// SearchString is formatted like "Path1\\Path2\\Path3". Individual paths are like a/b/c/d/e

  char *Paths = SearchString;
  do {
    char *ThisPath = Paths;
    Paths = strchr(ThisPath, '\\');
    if(Paths) *(Paths++) = 0;

    cJSON *Find = Root;
    while(1) {
      char *ThisItem = ThisPath;
      ThisPath = strchr(ThisItem, '/');
      if(ThisPath) *(ThisPath++) = 0;
      Find = cJSON_GetObjectItem(Find,ThisItem);
      if(!Find) // try another path then
        break;
      if(!ThisPath) // no more items?
        return Find; // this is the right item
    }
  } while (Paths);
  return NULL;
}
const char *GetConfigStr(const char *Default, const char *s,...) {
  char Path[512]; va_list vl; va_start(vl, s);
  vsprintf(Path, s, vl); va_end(vl);
  cJSON *Find = cJSON_Search(MainConfig, Path);
  if(Find) return Find->valuestring;
  return Default;
}
int GetConfigInt(int Default, const char *s,...) {
  char Path[512]; va_list vl; va_start(vl, s);
  vsprintf(Path, s, vl); va_end(vl);
  cJSON *Find = cJSON_Search(MainConfig, Path);
  if(Find) return Find->valueint;
  return Default;
}
int PathIsSafe(const char *Path) {
  if(strstr(Path, "..")) return 0; // no parent directories
  if(Path[0] == '/') return 0;     // no root
  if(strchr(Path, ':')) return 0;  // no drives
  return 1;
}
int MakeDirectory(const char *Path) {
#ifdef _WIN32
  return CreateDirectory(Path, NULL);
#else
  return !mkdir(Path, 0700);
#endif
}

cJSON *cJSON_Load(const char *Filename) {
  FILE *File = fopen(Filename, "rb");
  if(!File) {
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s not found", Filename);
    return NULL;
  }
  fseek(File, 0, SEEK_END);
  unsigned long FileSize = ftell(File);
  rewind(File);
  char *Buffer = (char*)malloc(sizeof(char)*FileSize);
  if(!Buffer || (FileSize != fread(Buffer,1,FileSize,File))) {
    fclose(File);
    return NULL;
  }
  fclose(File);
  cJSON *JSON = cJSON_Parse(Buffer);
  free(Buffer);
  if(!JSON)
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s failed to load", Filename);
  return JSON;
}
void strlcpy(char *Destination, const char *Source, int MaxLength) {
  // MaxLength is directly from sizeof() so it includes the zero
  int SourceLen = strlen(Source);
  if((SourceLen+1) < MaxLength)
    MaxLength = SourceLen + 1;
  memcpy(Destination, Source, MaxLength-1);
  Destination[MaxLength-1] = 0;
}
int memcasecmp(const char *Text1, const char *Text2, int Length) {
  for(;Length;Length--)
    if(tolower(*(Text1++)) != tolower(*(Text2++)))
      return 1;
  return 0;
}
int WildMatch(const char *TestMe, const char *Wild) {
  char NewWild[strlen(Wild)+1];
  char NewTest[strlen(TestMe)+1];
  const char *Asterisk = strchr(Wild, '*');
  if(Asterisk && !Asterisk[1]) return(!memcasecmp(TestMe, Wild, strlen(Wild)-1));

  strcpy(NewTest, TestMe);
  strcpy(NewWild, Wild);
  int i;
  for(i=0;NewWild[i];i++)
    NewWild[i] = tolower(NewWild[i]);
  for(i=0;NewTest[i];i++)
    NewTest[i] = tolower(NewTest[i]);
  return !fnmatch(NewWild, NewTest, FNM_NOESCAPE);
}

/*void AutoloadProtocol(const char *Filename) {
  LoadAddon(Filename, &FirstProtocol);
}*/
void AutoloadAddon(const char *Filename) {
  LoadAddon(Filename, &FirstAddon);
}

void AutoloadDirectory(const char *Directory, const char *Filetype, void (*Handler)(const char *Path)) {
  DIR *dir;
  struct dirent *ent;
  if((dir = opendir(Directory))) {
    while((ent = readdir(dir))) {
      if(*ent->d_name == '.') continue;
      char *Temp = strrchr(ent->d_name, '.');
      if(!Temp) continue;
      if(strcasecmp(Temp+1, Filetype)) continue;
      char FullPath[strlen(Directory)+strlen(ent->d_name)+1];
      strcpy(FullPath, Directory);
      strcat(FullPath, ent->d_name);
      Handler(FullPath);
    }
    closedir(dir);
  }
}

int AsyncExec(const char *Command) {
#ifdef _WIN32
  STARTUPINFO sInfo; 
  PROCESS_INFORMATION pInfo;
  memset(&sInfo, 0, sizeof (sInfo));
  memset(&pInfo, 0, sizeof (pInfo));
  sInfo.cb = sizeof(sInfo);
  sInfo.dwFlags = STARTF_USESTDHANDLES;
  sInfo.hStdInput = NULL;
  sInfo.hStdOutput = NULL;
  sInfo.hStdError = NULL;
  char commandLine[strlen(Command)+20];
  sprintf(commandLine, "cmd.exe /c %s", Command);
  CreateProcess(0, commandLine, 0, 0, TRUE, NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW, 0, 0, &sInfo, &pInfo);
  return 1;
#elif defined(__unix__)
// to do: implement
  return 0;
#endif
}

void MainThreadRequest(int Code, void *Data1, void *Data2) {
  SDL_Event event;
  SDL_zero(event);
  event.type = MainThreadEvent;
  event.user.code = Code;
  event.user.data1 = Data1;
  event.user.data2 = Data2;
  SDL_PushEvent(&event);
}

void URLOpen(const char *URL) {
#ifdef _WIN32
  ShellExecute(NULL, NULL, URL, NULL, NULL, SW_SHOWNORMAL);
#elif defined(__unix__)
  pid_t pid = fork();
  if(pid == -1) return; // error
  if(!pid) execl("xdg-open", URL);
  int status;
  waitpid(pid, &status, 0);
#endif
}

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
  if(SDL_Init(SDL_INIT_VIDEO) < 0){
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

  MainConfig = cJSON_Load("config.json");
  const char *GUITypeString = GetConfigStr("SDL", "Client/GUI");
  for(int i=0;GUINames[i];i++)
    if(!strcasecmp(GUINames[i],GUITypeString)) {
      GUIType = i;
      break;
    }
  PluginPref = cJSON_Load("pluginpref.json");
  if(!PluginPref)
    PluginPref = cJSON_CreateObject();

  IPC_New(EventQueue, 16, 1);
  IPC_New(SocketQueue, 16, 1);
  MainThreadEvent = SDL_RegisterEvents(1);

  freopen("CON", "w", stdout);
  freopen("CON", "w", stderr);
  PrefPath = SDL_GetPrefPath("Bushytail Software","SparklesChat");

  AutoloadDirectory("data/protocol/", "nut", AutoloadAddon);
  AutoloadDirectory("data/addons/", "nut", AutoloadAddon);

  MenuMain = cJSON_Load("data/menus/mainmenu.json");
  MenuChannel = cJSON_Load("data/menus/channelmenu.json");
  MenuUser = cJSON_Load("data/menus/usermenu.json");
  MenuUserTab = cJSON_Load("data/menus/usermenutab.json");
  MenuTextEdit = cJSON_Load("data/menus/texteditmenu.json");

  SDL_Thread *EventThread = SDL_CreateThread(RunEventThread, "Event", NULL);
  SDL_Thread *SocketThread = SDL_CreateThread(RunSocketThread, "Socket", NULL);
  (*InitGUI[GUIType])();
  while(!quit) {
    (*RunGUI[GUIType])();
    SDL_Delay(17);
  }
  while(FirstAddon)
    UnloadAddon(FirstAddon, &FirstAddon, 0);
  (*EndGUI[GUIType])();
  SDL_WaitThread(EventThread, NULL);
  SDL_WaitThread(SocketThread, NULL);
  IPC_Free(EventQueue, 1);
  IPC_Free(SocketQueue, 1);

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

  curl_multi_cleanup(MultiCurl);
  curl_global_cleanup();
  EndSock();
  if(PrefPath) SDL_free(PrefPath);
  SDL_Quit();
  return 0;
}
