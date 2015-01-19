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

int CreateDirectoriesForPath(const char *Folders) {
  char Temp[strlen(Folders)+1];
  strcpy(Temp, Folders);
  struct stat st = {0};

  char *Try = Temp;
  if(Try[1] == ':' && Try[2] == '\\') // ignore drive names
    Try = FindCloserPointer(strchr(Try+3, '/'), strchr(Try+3, '\\'));

  while(Try) {
    char Restore = *Try;
    *Try = 0;
    if(stat(Temp, &st) == -1) {
      MakeDirectory(Temp);
      if(stat(Temp, &st) == -1)
        return 0;
    }
    *Try = Restore;
    Try = FindCloserPointer(strchr(Try+1, '/'), strchr(Try+1, '\\'));
  }
  return 1;
}

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

int IsInsideRect(int X1, int Y1, int X2, int Y2, int W, int H) {
  if(X1<X2) return 0;
  if(Y1<Y2) return 0;
  if(X1>=(X2+W)) return 0;
  if(Y1>=(Y2+H)) return 0;
  return 1;
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
  if(!pid) execl("xdg-open", URL, NULL);
  int status;
  waitpid(pid, &status, 0);
#endif
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

const char *FilenameOnly(const char *Path) {
  const char *Temp = strrchr(Path, '/');
  if(!Temp) Temp = strrchr(Path, '\\');
  if(!Temp) return Temp;
  return Temp+1;
}

void SDL_MessageBox(int Type, const char *Title, SDL_Window *Window, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  char Buffer[512];
  vsprintf(Buffer, fmt, argp);
  SDL_ShowSimpleMessageBox(Type, Title, Buffer, Window);
  va_end(argp);
}

