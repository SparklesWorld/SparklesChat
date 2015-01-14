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
#define PLUGIN_C
#include "chat.h"
#include <openssl/md5.h>

extern GUIState *GUIStates[];
unsigned long CurHookId = 0;
extern EventHook *FirstTimer;

void Sq_PrintFunc(HSQUIRRELVM v,const SQChar *s,...) {
  va_list vl;
  va_start(vl, s);
  SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, s, vl);
  va_end(vl);
}

void Sq_ErrorFunc(HSQUIRRELVM v,const SQChar *s,...) {
  va_list vl;
  va_start(vl, s);
  SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_CRITICAL, s, vl);
  va_end(vl);
}

ClientAddon *AddonForScript(HSQUIRRELVM v) {
  return (ClientAddon*)sq_getforeignptr(v);
}
ClientConnection *ConnectionForTab(ClientTab *Tab) {
  if(Tab->Connection) return Tab->Connection;
  if(Tab->Parent && Tab->Parent->Connection) return Tab->Parent->Connection;
  return NULL;
}
SQInteger Sq_MD5(HSQUIRRELVM v) {
  const SQChar *Input;
  sq_getstring(v, 2, &Input);
  unsigned char Out[MD5_DIGEST_LENGTH] = "";
  MD5((const unsigned char*)Input, strlen(Input), Out);
  char OutString[16*2+1] = "";
  for(int i=0;i<MD5_DIGEST_LENGTH;i++)
    sprintf(OutString+i*2, "%.2x", Out[i]);
  sq_pushstring(v, OutString, -1);
  return 1;
}
SQInteger Sq_Time(HSQUIRRELVM v) {
  sq_pushinteger(v, time(NULL));
  return 1;
}
SQInteger Sq_GetTicks(HSQUIRRELVM v) {
  sq_pushinteger(v, SDL_GetTicks());
  return 1;
}
char *Spark_LoadTextFile(const char *File) {
  if(!PathIsSafe(File))
    return NULL;
  char FilePath[260];
  sprintf(FilePath, "%ssqdata/%s.txt", PrefPath, File);
  FILE *TextFile = fopen(FilePath,"rb");
  if(!TextFile)
    return NULL;
  fseek(TextFile, 0, SEEK_END);
  unsigned long FileSize = ftell(TextFile);
  rewind(TextFile);
  char *Buffer = (char*)malloc(sizeof(char)*FileSize);
  if(!Buffer) {
    fclose(TextFile);
    return NULL;
  }
  if(FileSize != fread(Buffer,1,FileSize,TextFile)) {
    fclose(TextFile);
    return NULL;
  }
  fclose(TextFile);
  return Buffer;
}

SQInteger Sq_LoadTextFile(HSQUIRRELVM v) {
  const SQChar *File;
  sq_getstring(v, 2, &File);
  char *Contents = Spark_LoadTextFile(File);
  if(Contents) {
    sq_pushstring(v, Contents, -1);
    free(Contents);
  } else {
    sq_pushnull(v);
  }
  return 1;
}

int Spark_SaveTextFile(const char *File, const char *Contents) {
  if(!PathIsSafe(File))
    return 0;
  char FilePath[260];
  sprintf(FilePath, "%ssqdata", PrefPath);

  struct stat st = {0}; // make sqdata directory if it doesn't exist
  if (stat(FilePath, &st) == -1)
    MakeDirectory(FilePath);

  sprintf(FilePath, "%ssqdata/%s.txt", PrefPath, File);
  FILE *Handle = fopen(FilePath, "wb");
  if(!Handle) {
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open %s for writing by a script", FilePath);
    return 0;
  }
  fwrite(Contents, sizeof(SQChar), strlen(Contents), Handle);
  fclose(Handle);
  return 1;
}

SQInteger Sq_SaveTextFile(HSQUIRRELVM v) {
  const SQChar *File, *Contents;
  sq_getstring(v, 2, &File);
  sq_getstring(v, 3, &Contents);
  sq_pushbool(v, Spark_SaveTextFile(File, Contents)?SQTrue:SQFalse);
  return 1;
}

int Spark_TextFileExists(const char *File) {
  if(!PathIsSafe(File))
    return 0;
  char FilePath[260];
  sprintf(FilePath, "%ssqdata/%s.txt", PrefPath, File);
  FILE *TextFile = fopen(FilePath,"rb");
  if(TextFile) {
    fclose(TextFile);
    return 1;
  }
  return 0;
}

SQInteger Sq_TextFileExists(HSQUIRRELVM v) {
  const SQChar *File;
  sq_getstring(v, 2, &File);
  sq_pushbool(v, Spark_TextFileExists(File)?SQTrue:SQFalse);
  return 1;
}

void ParamSplit(HSQUIRRELVM v, int Style) {
  const SQChar *Str;
  sq_getstring(v, 2, &Str);
  char ArgBuff[strlen(Str)+1];
  const char *Word[32];
  const char *WordEol[32];
  int i;
  int NumWords = XChatTokenize(Str, ArgBuff, Word, WordEol, 32, Style);
  sq_newarray(v, 0);

  // word array
  sq_newarray(v, 0);
  for(i=0;i<NumWords;i++) {
    sq_pushstring(v, Word[i], -1);
    sq_arrayappend(v, -2);  
  }
  sq_arrayappend(v, -2);

  // word_eol array
  sq_newarray(v, 0);
  for(i=0;i<NumWords;i++) {
    sq_pushstring(v, WordEol[i], -1);
    sq_arrayappend(v, -2);  
  }
  sq_arrayappend(v, -2);
}
SQInteger Sq_TextParamSplit(HSQUIRRELVM v) {
  ParamSplit(v, 0);
  return 1;
}
SQInteger Sq_CmdParamSplit(HSQUIRRELVM v) {
  ParamSplit(v, TOKENIZE_MULTI_WORD);
  return 1;
}

void DeleteTimerById(int Id);
void RunSquirrelMisc() { // currently only handles timers
  Uint32 Now = SDL_GetTicks();
  for(EventHook *Timer = FirstTimer; Timer;) {
    EventHook *Next = Timer->Next;
    if(SDL_TICKS_PASSED(Now, Timer->HookInfo.Timer.Timeout)) {
      if(Timer->Script) {
        SQInteger top = sq_gettop(Timer->Script);
        sq_pushobject(Timer->Script, Timer->Function.Squirrel);
        sq_pushroottable(Timer->Script);
        sq_pushobject(Timer->Script, Timer->HookInfo.Timer.Extra);
        sq_call(Timer->Script,2,SQTrue,SQTrue);
        if(OT_INTEGER == sq_gettype(Timer->Script,-1)) {
          SQInteger IntResult;
          sq_getinteger(Timer->Script, -1, &IntResult);
          Timer->HookInfo.Timer.Timeout = Now+IntResult;
        } else
          DeleteTimerById(Timer->HookId);
        sq_settop(Timer->Script,top);
      } else {
        int (*callback) (void *user_data) = Timer->Function.Native;
        if(!callback(Timer->XChatHook->Userdata))
          DeleteTimerById(Timer->HookId);
      }
    }
    Timer = Next;
  }

  int Num;
  if(SqCurlRunning) {
    curl_multi_perform(MultiCurl, &Num);
    SqCurlRunning = Num;
  }
  CURLMsg *TransferInfo;
  while((TransferInfo = curl_multi_info_read(MultiCurl, &Num))) {
    if(TransferInfo->msg == CURLMSG_DONE) {
      struct SqCurl *State = NULL;
      CURL *Easy = TransferInfo->easy_handle;

      if(curl_easy_getinfo(Easy, CURLINFO_PRIVATE, (char*)&State) != CURLE_OK)
        continue;
      int Return = TransferInfo->data.result;

      HSQUIRRELVM v = State->Script;
      if(v) {
        SQInteger top = sq_gettop(v);
        sq_pushobject(v, State->Data.Squirrel.Handler);
        sq_pushroottable(v);
        sq_pushinteger(v, Return);
        if(!Return)
          sq_pushstring(v, State->Memory,-1);
        else
          sq_pushstring(v, curl_easy_strerror(Return),-1);
        sq_pushobject(v, State->Data.Squirrel.Extra);
        sq_call(v, 4, SQFalse,SQTrue);
        sq_settop(v, top);
        sq_release(v, &State->Data.Squirrel.Handler);
        sq_release(v, &State->Data.Squirrel.Extra);
      } else
        State->Data.Native.Function(State->Memory, State->Size, State->Data.Native.Extra);

      curl_multi_remove_handle(MultiCurl, Easy);
      curl_easy_cleanup(Easy);
      if(State->Memory)
        free(State->Memory);
      if(State->Form)
        free(State->Form);
      free(State);
    }
  }
}

size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
// adapted from a curl example
  size_t realsize = size * nmemb;
  struct SqCurl *mem = (struct SqCurl*)userp;
 
  mem->Memory = realloc(mem->Memory, mem->Size + realsize + 1);
  if(!mem->Memory)
    return 0;

  memcpy(mem->Memory + mem->Size, contents, realsize);
  mem->Size += realsize;
  mem->Memory[mem->Size] = 0;
  return realsize;
}

SQInteger Sq_CurlGetPost(HSQUIRRELVM v) {
  struct SqCurl *State = (struct SqCurl*)calloc(1,sizeof(struct SqCurl));
  if(!State) return 0;
  int ArgCount = sq_gettop(v);
  const SQChar *URL, *Form = NULL;
  HSQOBJECT Handler, Extra;
  sq_resetobject(&Handler);
  sq_resetobject(&Extra);

  sq_getstackobj(v, 2, &Handler);
  sq_getstackobj(v, 3, &Extra);
  sq_getstring(v, 4, &URL);
  if(ArgCount == 5) {
    sq_getstring(v, 5, &Form);
    State->Form = (char*)malloc(strlen(Form)+1);
    strcpy(State->Form, Form);
  }

  State->Script = v;
  State->Data.Squirrel.Handler = Handler;
  State->Data.Squirrel.Extra = Extra;
  sq_addref(v, &Handler);
  sq_addref(v, &Extra);

  CURL *Curl = curl_easy_init();
  curl_easy_setopt(Curl, CURLOPT_URL, URL);
  curl_easy_setopt(Curl, CURLOPT_NOPROGRESS, 1);
  curl_easy_setopt(Curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(Curl, CURLOPT_WRITEDATA, State);
//  curl_easy_setopt(Curl, CURLOPT_CAPATH, "ca-bundle.crt");
  curl_easy_setopt(Curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(Curl, CURLOPT_SSL_VERIFYHOST, 0L);
  if(State->Form) // hi I'm Jake from State Form
    curl_easy_setopt(Curl, CURLOPT_POSTFIELDS, State->Form);
  curl_multi_add_handle(MultiCurl, Curl);
  curl_easy_setopt(Curl, CURLOPT_PRIVATE, State);
  SqCurlRunning = 1;
  return 0;
}

SQInteger Sq_StrToL(HSQUIRRELVM v) {
  SQInteger Base;
  const SQChar *Str;
  sq_getstring(v, 2, &Str);
  sq_getinteger(v, 3, &Base);
  sq_pushinteger(v, strtol(Str, NULL, Base));
  return 1;
}

SQInteger Sq_AsciiChr(HSQUIRRELVM v) {
  SQInteger Char;
  sq_getinteger(v, 2, &Char);
  char Out[2];
  Out[0] = Char;
  Out[1] = 0;
  sq_pushstring(v, Out, -1);
  return 1;
}
SQInteger Sq_URLEncode(HSQUIRRELVM v) {
  const SQChar *Str;
  sq_getstring(v, 2, &Str);
  const char *EncodeThese = "/\\@#$%^&?/+=[]{};':\"<>,";
  char Buffer[4096];
  char *Out = Buffer;
  for(const char *In = Str; *In; In++) {
    if(strchr(EncodeThese, *In)) {
      char Temp[10];
      sprintf(Temp, "%%%.2X", (unsigned char)(*In)&255);
      strcpy(Out, Temp);
      Out += strlen(Temp);
    } else
    *(Out++) = *In;
  }
  *Out = 0;
  sq_pushstring(v, Buffer, -1);
  return 1;
}
SQInteger Sq_Interpolate(HSQUIRRELVM v) {
  const SQChar *Str, *WordString;
  char Out[4096];
  sq_getstring(v, 2, &Str);
  sq_getstring(v, 3, &WordString);
  char *Poke = Out;
  const char *Peek = Str;

  char WordBuff[strlen(Str)+1];
  const char *Word[32];
  const char *WordEol[32];
  XChatTokenize(WordString, WordBuff, Word, WordEol, 32, TOKENIZE_MULTI_WORD);

  while(*Peek) {
    int Span = strcspn(Peek, "%&");
    memcpy(Poke, Peek, Span);
    Poke += Span;
    Peek += Span;
    if(*Peek == '%' || *Peek == '&') {
      char Extra = Peek[1];
      if(Extra == '%')
        *(Poke++) = '%';
      else if(Extra == '&')
        *(Poke++) = '&';
      else {
        if(isdigit(Extra)) {
          int WhichWord = Extra - '1';
          strcpy(Poke, (*Peek=='%')?Word[WhichWord]:WordEol[WhichWord]);
          Poke = strrchr(Poke, 0);
        } else { // look in the list of extra words
          int top = sq_gettop(v);;
          sq_pushnull(v); //null iterator
          while(SQ_SUCCEEDED(sq_next(v,-2))) {
             const SQChar *ThisWord;
             sq_getstring(v, -1, &ThisWord);
             if(ThisWord[0]==Extra) {
               strcpy(Poke, ThisWord+1);
               Poke = strrchr(Poke, 0);
               break;
             }
             sq_pop(v,2);
          }
          sq_pop(v,1); //pops the null iterator
          sq_settop(v, top);
        }
      }
      Peek+= 2;
    }
  }
  *Poke = 0;

  sq_pushstring(v, Out, -1);  
  return 1;
}

SQInteger Sq_FindAddon(HSQUIRRELVM v) {
  const SQChar *AddonName;
  SQBool Required;
  sq_getstring(v, 2, &AddonName);
  sq_getbool(v, 3, &Required);
  ClientAddon *Addon = FindAddon(AddonName, &FirstAddon);
  sq_pushbool(v, Addon?SQTrue:SQFalse);
  if(!Addon && Required)
    LoadAddon(AddonName, &FirstAddon);
  return 1;
}
SQInteger Sq_LoadAddon(HSQUIRRELVM v) {
  const SQChar *AddonName;
  sq_getstring(v, 2, &AddonName);
  LoadAddon(AddonName, &FirstAddon);
  return 0;
}
SQInteger Sq_ReloadAddon(HSQUIRRELVM v) {
  const SQChar *AddonName;
  sq_getstring(v, 2, &AddonName);
  ClientAddon *Addon = FindAddon(AddonName, &FirstAddon);
  if(Addon)
    UnloadAddon(Addon, &FirstAddon, 1);
  return 0;
}
SQInteger Sq_UnloadAddon(HSQUIRRELVM v) {
  const SQChar *AddonName;
  sq_getstring(v, 2, &AddonName);
  ClientAddon *Addon = FindAddon(AddonName, &FirstAddon);
  if(Addon)
    UnloadAddon(Addon, &FirstAddon, 0);
  return 0;
}
SQInteger Sq_CallAddon(HSQUIRRELVM v) {
  const SQChar *AddonName, *Function;
  sq_getstring(v, 2, &AddonName);
  sq_getstring(v, 3, &Function);
  // figure out how I want to read multiple args
  // not implemented until then
  return 0;
}
SQInteger Sq_EvalAddon(HSQUIRRELVM v) {
  const SQChar *AddonName, *Code;
  sq_getstring(v, 2, &AddonName);
  sq_getstring(v, 3, &Code);
  ClientAddon *Addon = FindAddon(AddonName, &FirstAddon);
  if(!Addon) return 0;

  SQInteger top = sq_gettop(Addon->Script);
  sq_pushroottable(Addon->Script);
  sq_pushstring(Addon->Script,_SC("api"),-1);
  if(!SQ_SUCCEEDED(sq_get(Addon->Script,-2))) return 0;
  sq_pushstring(Addon->Script,_SC("RunStringPublic"),-1);
  if(SQ_SUCCEEDED(sq_get(Addon->Script,-2))) {
    sq_pushroottable(Addon->Script); // push "this"
    sq_pushstring(Addon->Script, Code,-1);
    sq_call(Addon->Script,2,SQTrue,SQTrue);

    const SQChar *StringResult;
    sq_getstring(Addon->Script, -1, &StringResult);
    sq_pushstring(v, StringResult, -1);
    sq_settop(Addon->Script,top);
  }
  return 1;
}
SQInteger Sq_GetConfigNames(HSQUIRRELVM v) {
  SDL_LockMutex(LockConfig);
  const SQChar *Search;
  sq_getstring(v, 2, &Search);
  char Search2[strlen(Search)+1];
  strcpy(Search2, Search);
  cJSON *Find = cJSON_Search(MainConfig, Search2);
  if(Find && Find->child) {
    Find=Find->child;
    sq_newarray(v, 0);
    while(Find) {
      sq_pushstring(v, Find->string, -1);
      sq_arrayappend(v, -2);
      Find = Find->next;
    }
  }
  else sq_pushnull(v);
  SDL_UnlockMutex(LockConfig);
  return 1;
}
SQInteger Sq_GetConfigInt(HSQUIRRELVM v) {
  SDL_LockMutex(LockConfig);
  const SQChar *Search;
  SQInteger Default;
  sq_getstring(v, 2, &Search);
  sq_getinteger(v, 3, &Default);
  char Search2[strlen(Search)+1];
  strcpy(Search2, Search);
  cJSON *Find = cJSON_Search(MainConfig, Search2);
  if(Find) sq_pushinteger(v, Find->valueint);
  else sq_pushinteger(v, Default);
  SDL_UnlockMutex(LockConfig);
  return 1;
}
SQInteger Sq_GetConfigStr(HSQUIRRELVM v) {
  SDL_LockMutex(LockConfig);
  const SQChar *Search, *Default;
  sq_getstring(v, 2, &Search);
  sq_getstring(v, 3, &Default);
  char Search2[strlen(Search)+1];
  strcpy(Search2, Search);
  cJSON *Find = cJSON_Search(MainConfig, Search2);
  if(Find) sq_pushstring(v, Find->valuestring, -1);
  else sq_pushstring(v, Default, -1);
  SDL_UnlockMutex(LockConfig);
  return 1;
}

ClientTab *GetFocusedTab() {
  GUIDialog *ChatView = FindDialogWithProc(NULL, NULL, NULL, Widget_ChatView);
  ClientTab *Tab = (ClientTab*)ChatView->dp;
  return Tab;
}

SQInteger Sq_GetInfo(HSQUIRRELVM v) {
  const SQChar *What;
  sq_getstring(v, 2, &What);

  if(!strcasecmp(What, "ClientName"))  
    sq_pushstring(v, CLIENT_NAME, -1);
  else if(!strcasecmp(What, "ClientVersion"))
    sq_pushstring(v, CLIENT_VERSION, -1);
  else if(!strcasecmp(What, "PrefPath"))
    sq_pushstring(v, PrefPath, -1);
  else if(!strcasecmp(What, "CurlVersion"))
    sq_pushstring(v, curl_version(), -1);
  else if(!strcasecmp(What, "Platform"))
    sq_pushstring(v, SDL_GetPlatform(), -1);
  else if(!strcasecmp(What, "Context")) {
    char Buffer[CONTEXT_BUF_SIZE];
    sq_pushstring(v, ContextForTab(GetFocusedTab(), Buffer), -1);
  } else
    sq_pushnull(v);
  return 1;
}

SQInteger Sq_WildMatch(HSQUIRRELVM v) {
  const SQChar *Str1, *Str2;
  sq_getstring(v, 2, &Str1);
  sq_getstring(v, 3, &Str2);
  sq_pushbool(v, WildMatch(Str1, Str2)?SQTrue:SQFalse);
  return 1;
}

int ConvertSocketFlags(char *TypeCopy) {
  int Flags = 0;
  char *Token = strtok(TypeCopy, " ");
  while(Token) {
    if(!strcasecmp(Token, "ssl"))
      Flags |= SQSOCK_SSL;
    if(!strcasecmp(Token, "websocket"))
      Flags |= SQSOCK_WEBSOCKET;
    if(!strcasecmp(Token, "nosplit"))
      Flags |= SQSOCK_NOT_LINED;
    Token = strtok(NULL, " ");
  }
  return Flags;
}

int SockId = 1;
int CreateSqSock(HSQUIRRELVM v, HSQOBJECT Handler, const char *Host, int Flags);
SQInteger Sq_NetOpen(HSQUIRRELVM v) {
  const SQChar *Host, *Type;
  HSQOBJECT Handler;
  sq_resetobject(&Handler);
  sq_getstackobj(v, 2, &Handler);
  sq_getstring(v, 3, &Host);
  sq_getstring(v, 4, &Type);
  char TypeCopy[strlen(Type)+1];
  strcpy(TypeCopy, Type);
  SDL_LockMutex(LockSockets);
  CreateSqSock(v, Handler, Host, ConvertSocketFlags(TypeCopy));
  SDL_UnlockMutex(LockSockets);
  sq_pushinteger(v, SockId++);
  return 1;
}
SQInteger Sq_NetSend(HSQUIRRELVM v) {
  SQInteger Id; const SQChar *Text;
  sq_getinteger(v, 2, &Id);
  sq_getstring(v, 3, &Text);
  IPC_WriteF(SocketQueue[0], "M%i %s", Id, Text);
  return 0;
}
SqSocket *FindSockById(int Id);

void DeleteSocketById(int Id);
SQInteger Sq_NetClose(HSQUIRRELVM v) {
  SQInteger Id; SQBool Reopen;
  sq_getinteger(v, 2, &Id);
  sq_getbool(v, 3, &Reopen);
  if(Reopen) {
    SDL_LockMutex(LockSockets);
    SqSocket *Sock = FindSockById(Id);
    if(!Sock) return 0;
    Sock->Flags &= (SQSOCK_WEBSOCKET|SQSOCK_SSL|SQSOCK_NOT_LINED);
    *Sock->Buffer = 0;
    SDL_UnlockMutex(LockSockets);
    IPC_WriteF(SocketQueue[0], "O%i", SockId);
  } else {
    SDL_LockMutex(LockSockets);
    DeleteSocketById(Id);
    SDL_UnlockMutex(LockSockets);
  }
  return 0;
}

int TimerID = 0;
void DeleteTimerById(int Id) {
  for(EventHook *Timer = FirstTimer; Timer; Timer=Timer->Next)
    if(Timer->HookId == Id) {
      if(FirstTimer == Timer)
        FirstTimer = Timer->Next;
      if(Timer->Script) {
        sq_release(Timer->Script, &Timer->Function.Squirrel);
        sq_release(Timer->Script, &Timer->HookInfo.Timer.Extra);
      }
      free(Timer);
      return;
    }
}

SQInteger Sq_DelTimer(HSQUIRRELVM v) {
  SQInteger Timer;
  sq_getinteger(v, 2, &Timer);
  DeleteTimerById(Timer);
  return 0;
}

SQInteger Sq_AddTimer(HSQUIRRELVM v) {
  HSQOBJECT Handler, Extra;
  sq_resetobject(&Handler);
  sq_resetobject(&Extra);
  SQInteger Time;

  sq_getinteger(v, 2, &Time);
  sq_getstackobj(v, 3, &Handler);
  sq_getstackobj(v, 4, &Extra);

  EventHook NewTimer;
  memset(&NewTimer, 0, sizeof(NewTimer));
  NewTimer.HookInfo.Timer.Timeout = SDL_GetTicks() + Time;
  NewTimer.HookInfo.Timer.Extra = Extra;
  NewTimer.Script = v;
  NewTimer.Function.Squirrel = Handler;
  NewTimer.HookId = TimerID;
  NewTimer.Next = FirstTimer;

  EventHook *AllocTimer = (EventHook*)malloc(sizeof(EventHook));
  if(!AllocTimer)
    return 0;
  *AllocTimer = NewTimer;
  if(FirstTimer)
    FirstTimer->Prev = AllocTimer;

  sq_addref(v, &Handler);
  sq_addref(v, &Extra);
  FirstTimer = AllocTimer;

  sq_pushinteger(v, TimerID++);
  return 1;
}

SQInteger Sq_AddEventHook(HSQUIRRELVM v) {
  const SQChar *EventName;
  HSQOBJECT Handler;
  sq_resetobject(&Handler);
  SQInteger Priority, Flags, Need0, Need1;
  sq_getstring(v, 2, &EventName);
  sq_getstackobj(v, 3, &Handler);
  sq_getinteger(v, 4, &Priority);
  sq_getinteger(v, 5, &Flags);
  sq_getinteger(v, 6, &Need0);
  sq_getinteger(v, 7, &Need1);
  sq_pushinteger(v, CurHookId); // AddEventHook increases this for us
  AddEventHook(&FirstEventType, v, EventName, Handler, Priority, Flags, Need0, Need1);
  return 1;
}
SQInteger Sq_DelEventHook(HSQUIRRELVM v) {
  const SQChar *EventName;
  SQInteger HookId;
  sq_getstring(v, 2, &EventName);
  sq_getinteger(v, 3, &HookId);
  DelEventHookId(&FirstEventType, EventName, HookId);
  return 0;
}
SQInteger Sq_AddCommandHook(HSQUIRRELVM v) {
  const SQChar *EventName, *HelpText, *Syntax;
  HSQOBJECT Handler;
  sq_resetobject(&Handler);
  SQInteger Priority;
  sq_getstring(v, 2, &EventName);
  sq_getstackobj(v, 3, &Handler);
  sq_getinteger(v, 4, &Priority);
  sq_getstring(v, 5, &HelpText);
  sq_getstring(v, 6, &Syntax);
  AddEventHook(&FirstCommand, v, EventName, Handler, Priority, 0, EF_ALREADY_HANDLED, 0);
  sq_pushinteger(v, CurHookId++);
  return 1;
}
SQInteger Sq_DelCommandHook(HSQUIRRELVM v) {
  const SQChar *EventName;
  SQInteger HookId;
  sq_getstring(v, 2, &EventName);
  sq_getinteger(v, 3, &HookId);
  DelEventHookId(&FirstCommand, EventName, HookId);
  return 0;
}

SQInteger Sq_GetClipboard(HSQUIRRELVM v) {
  if(SDL_HasClipboardText())
    sq_pushstring(v, SDL_GetClipboardText(), -1);
  else
    sq_pushnull(v);
  return 1;
}
SQInteger Sq_SetClipboard(HSQUIRRELVM v) {
  const SQChar *Str;
  sq_getstring(v, 2, &Str);
  SDL_SetClipboardText(Str);
  return 0;
}
SQInteger Sq_NativeCommand(HSQUIRRELVM v) {
  const SQChar *Name;
  const SQChar *Args;
  const SQChar *Context;
  sq_getstring(v, 2, &Name);
  sq_getstring(v, 3, &Args);
  sq_getstring(v, 4, &Context);
  sq_pushinteger(v, NativeCommand(Name, Args, Context));
  return 1;
}

ClientTab *FindTab(const char *Context2) {
// to do: possibly use a mutex here
  char Context[strlen(Context2)+1];
  strcpy(Context, Context2);

//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "searching for tab %s", Context);
  char *Channel=NULL, *Server=NULL;
  int ChanId=-1, ServId=-1;
  char *Split = strchr(Context, '/');
  if(Split) {
    *Split = 0;
    Channel = Split+1;
    Split = strchr(Channel, ':');
    if(Split) {
      *Split = 0;
      ChanId = strtol(Split+1, NULL, 0);
    }
  }
  Server = Context;
  Split = strchr(Server, ':');
  if(Split) {
    *Split = 0;
    ServId = strtol(Split+1, NULL, 0);
  }
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "looking for serv:%s chan:%s", Server?:"no", Channel?:"no");

  ClientTab *Tab = FirstTab;
  // find server anyway
  while(1) {
    if(!strcasecmp(Tab->Name, Server) && (ServId==-1 || ServId==Tab->Id))
      break;
    Tab = Tab->Next;
    if(!Tab) goto Error;
  }
  if(!Channel) return Tab;
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "found server");
  // if channel specified, match channel too
  Tab = Tab->Child;
  while(1) {
    if(!strcasecmp(Tab->Name, Channel) && (ChanId==-1 || ChanId==Tab->Id))
      break;
    Tab = Tab->Next;
    if(!Tab) goto Error;
  }
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "found channel too");

  return Tab;
Error:
  return NULL;
}
char *ContextForTab(ClientTab *Tab, char *Buffer) {
  if(!Tab) *Buffer = 0;
  else if(Tab->Parent)
    sprintf(Buffer, "%s:%i/%s:%i", Tab->Parent->Name, Tab->Parent->Id, Tab->Name, Tab->Id);
  else sprintf(Buffer, "%s:%i", Tab->Name, Tab->Id); 
  return Buffer;
}

void ChannelTabsIsDirty() {
  GUIDialog *ChannelTabs = FindDialogWithProc(NULL, NULL, NULL, Widget_ChannelTabs);
  if(ChannelTabs)
    ChannelTabs->flags |= D_DIRTY;
}

int TabIdCounter = 1;
SQInteger Sq_TabCreate(HSQUIRRELVM v) {
//  SDL_LockMutex(LockTabs);
  const SQChar *Name, *Context;
  SQInteger Flags;
  sq_getstring(v, 2, &Name);
  sq_getstring(v, 3, &Context);
  sq_getinteger(v, 4, &Flags);

  ClientTab NewTab;
  memset(&NewTab, 0, sizeof(struct ClientTab));
  strlcpy(NewTab.Name, Name, sizeof(NewTab.Name));
  NewTab.Flags = Flags;
  NewTab.Id = TabIdCounter++;
  ClientConnection *Connection = NULL;
  if(Flags & TAB_SERVER) {
    strcpy(NewTab.Icon, ":00");
    Connection = (ClientConnection*)calloc(1,sizeof(ClientConnection));
    *Connection->YourName = 0;
    Connection->Script = v;
  } else if(Flags & TAB_CHANNEL) strcpy(NewTab.Icon, ":01");
  else if(Flags & TAB_QUERY) strcpy(NewTab.Icon, ":02");
  else strcpy(NewTab.Icon, ":04");
  NewTab.Script = v;
  NewTab.Connection = Connection;

  ClientTab *NewPtr = (ClientTab*)malloc(sizeof(ClientTab));
  *NewPtr = NewTab;
  ClientTab *Tab;

  if(*Context) {
    Tab = FindTab(Context);
    if(!Tab) goto Error;
    if(Tab->Parent) { // channel, find end
      while(Tab->Next) Tab=Tab->Next;
      Tab->Next = NewPtr;
      NewPtr->Prev = Tab;
      NewPtr->Parent = Tab->Parent;
    } else { // given a server to put a tab inside
      if(Tab->Child) { // already has channels? find the end
        Tab=Tab->Child;
        while(Tab->Next) Tab=Tab->Next;
        Tab->Next = NewPtr;
        NewPtr->Prev = Tab;
        NewPtr->Parent = Tab->Parent;
      } else { // no channels yet, this is the first one
        Tab->Child = NewPtr;
        NewPtr->Parent = Tab;
      }
    }
  } else { // making a server tab
    if(!FirstTab) {
      FirstTab = NewPtr;
    } else {
      Tab = FirstTab;
      while(Tab->Next) Tab = Tab->Next;
      Tab->Next = NewPtr;
      NewPtr->Prev = Tab;
    }
  }

  char Buffer[CONTEXT_BUF_SIZE];
  StartEvent("tab create", "", ContextForTab(NewPtr, Buffer), FirstEventType, 0);
  sq_pushstring(v, Buffer, -1);
  ChannelTabsIsDirty();
  SDL_UnlockMutex(LockTabs);
  return 1;

Error:
  SDL_UnlockMutex(LockTabs);
  return 0;
}
void CleanTab(ClientTab *Tab) {
  for(int i=0;i<Tab->NumMessages;i++)
    if(Tab->Messages[i].RightExtend)
      free(Tab->Messages[i].RightExtend);
}
SQInteger Sq_TabRemove(HSQUIRRELVM v) {
// something is causing this to crash, when used on a connected server
  const SQChar *Context;
  sq_getstring(v, 2, &Context);
  ClientTab *Tab = FindTab(Context);
  if(!Tab) return 0;
  char Buffer[CONTEXT_BUF_SIZE];
  StartEvent("tab remove", "", ContextForTab(Tab, Buffer), FirstEventType, 0);

  // if we have the tab we're removing focused, move the focus
  ClientTab *Focus = GetFocusedTab();
  if(Tab == Focus) {
    if(Focus->Prev) Focus = Focus->Prev;
    else if(Focus->Next) Focus = Focus->Next;
    else if(Focus->Parent) Focus = Focus->Parent;
    // otherwise we're screwed
    MainThreadRequest(MTR_GUI_COMMAND, StringClone(ContextForTab(Focus, Buffer)), StringClone("focus"));
  }

  if(Tab->Connection) {
    if(Tab->Connection->Ticket)
      free(Tab->Connection->Ticket);
    free(Tab->Connection);
  }
  if(Tab->Parent) { // channel
    if(Tab->Prev) {
      Tab->Prev->Next = Tab->Next;
      if(Tab->Next) Tab->Next->Prev = Tab->Prev;
    } else { // removing first channel in list
      Tab->Parent->Child = Tab->Next;
      if(Tab->Next) Tab->Next->Prev = NULL;
    }
  } else { // server
    if(Tab->Prev) {
      Tab->Prev->Next = Tab->Next;
      if(Tab->Next) Tab->Next->Prev = Tab->Prev;
    } else {
      if(Tab->Next) Tab->Next->Prev = NULL;
      FirstTab = Tab->Next;
    }

    ClientTab *FreeChans = Tab->Child;
    while(FreeChans) {
       ClientTab *Next = FreeChans->Next;
       CleanTab(FreeChans);
       free(FreeChans);
       FreeChans = Next;
    }
  }

  CleanTab(Tab);
  free(Tab);
  ChannelTabsIsDirty();
  return 0;
}
SQInteger Sq_TabSetFlags(HSQUIRRELVM v) {
  SDL_LockMutex(LockTabs);
  const SQChar *Context;
  SQInteger Flags;
  sq_getstring(v, 2, &Context);
  sq_getinteger(v, 3, &Flags);
  ClientTab *Tab = FindTab(Context);
  if(Tab)
    Tab->Flags = Flags;
  ChannelTabsIsDirty();
  SDL_UnlockMutex(LockTabs);
  return 0;
}
SQInteger Sq_TabGetFlags(HSQUIRRELVM v) {
  const SQChar *Context;
  sq_getstring(v, 2, &Context);
  ClientTab *Tab = FindTab(Context);
  if(!Tab) sq_pushnull(v);
  else sq_pushinteger(v, Tab->Flags);
  return 1;
}
SQInteger Sq_TabSetInfo(HSQUIRRELVM v) {
  SDL_LockMutex(LockTabs);
  const SQChar *Context, *Info, *NewValue;
  sq_getstring(v, 2, &Context);
  sq_getstring(v, 3, &Info);
  sq_getstring(v, 4, &NewValue);

  ClientTab *Tab = FindTab(Context);
  if(!Tab) {
    SDL_UnlockMutex(LockTabs);
    return 0;
  }
  ClientConnection *Connection = ConnectionForTab(Tab);

  if(!strcasecmp(Info, "Ticket")) {
    Connection->Ticket = (char *)realloc(Connection->Ticket, strlen(NewValue)+1);
    strcpy(Connection->Ticket, NewValue);
  } else if(!strcasecmp(Info, "Nick")) {
    strlcpy(Connection->YourName, NewValue, sizeof(Connection->YourName));
  } else if(!strcasecmp(Info, "InputStatus")) {
    strlcpy(Connection->InputStatus, NewValue, sizeof(Connection->InputStatus));
  } else if(!strcasecmp(Info, "Socket")) {
    Connection->Socket = strtol(NewValue, NULL, 10);
  } else if(!strcasecmp(Info, "Channel")) {
    strlcpy(Tab->Name, NewValue, sizeof(Tab->Name));
    ChannelTabsIsDirty();
  }
  SDL_UnlockMutex(LockTabs);
  return 0;
}
SQInteger Sq_TabGetInfo(HSQUIRRELVM v) {
  const SQChar *Context, *Info;
  char Buffer[CONTEXT_BUF_SIZE];

  sq_getstring(v, 2, &Context);
  sq_getstring(v, 3, &Info);
  if(!*Context) { // no context
    if(!strcasecmp(Info, "List")) {
      sq_newarray(v, 0);
      ClientTab *Servers = FirstTab;
      while(Servers) {
        sq_pushstring(v, ContextForTab(Servers, Buffer), -1);
        sq_arrayappend(v, -2);
        Servers = Servers->Next;
      }
      return 1;
    }
    sq_pushnull(v);
    return 1;
  }
  ClientTab *Tab = FindTab(Context);
  if(!Tab) { sq_pushnull(v); return 1; }

  ClientConnection *Connection = ConnectionForTab(Tab);
  if(!strcasecmp(Info, "Channel")) {
    sq_pushstring(v, Tab->Name, -1); return 1;
  } else if(!strcasecmp(Info, "Icon")) {
    sq_pushstring(v, Tab->Icon, -1); return 1;
  } else if(!strcasecmp(Info, "Server")) {
    if(Tab->Parent)
      sq_pushstring(v, Tab->Parent->Name, -1);
    else
      sq_pushstring(v, Tab->Name, -1);
    return 1;
  } else if(!strcasecmp(Info, "Nick")) {
    if(Connection)
      sq_pushstring(v, Connection->YourName, -1);
    else
      sq_pushnull(v);
    return 1;
  } else if(!strcasecmp(Info, "Protocol")) {
    if(Connection) {
      ClientAddon *Addon = AddonForScript(Connection->Script);
      if(Addon)
        sq_pushstring(v, Addon->Name, -1);
      else
        sq_pushnull(v);
    } else
      sq_pushnull(v);
    return 1;
  } else if(!strcasecmp(Info, "Ticket")) {
    if(Connection)
      sq_pushstring(v, Connection->Ticket, -1);
    else
      sq_pushnull(v);
    return 1;
  } else if(!strcasecmp(Info, "Socket")) {
    if(Connection)
      sq_pushinteger(v, Connection->Socket);
    else
      sq_pushnull(v);
    return 1;
  } else if(!strcasecmp(Info, "Flags")) {
    sq_pushinteger(v, Tab->Flags); return 1;
  } else if(!strcasecmp(Info, "Id")) {
    sq_pushinteger(v, Tab->Id); return 1;
  } else if(!strcasecmp(Info, "List")) {
    sq_newarray(v, 0);
    if(Tab->Flags & TAB_SERVER) {
      ClientTab *Channels = Tab->Child;
      while(Channels) {
        sq_pushstring(v, ContextForTab(Channels, Buffer), -1);
        sq_arrayappend(v, -2);
        Channels = Channels->Next;
      }
      return 1;
    } else if(Tab->Flags & TAB_CHANNEL) {
      // list users in channel
    } else
      sq_pushnull(v);
    return 1;
  }
  sq_pushnull(v); return 1;
}
SQInteger Sq_TabExists(HSQUIRRELVM v) {
  const SQChar *Context;
  sq_getstring(v, 2, &Context);
  sq_pushbool(v, FindTab(Context)?SQTrue:SQFalse);
  return 1;
}
SQInteger Sq_TabFind(HSQUIRRELVM v) {
// to do later, will let you specify a parent to search in or context
  const SQChar *Name, *Parent;
  sq_getstring(v, 2, &Name);
  sq_getstring(v, 2, &Parent);
  return 1;
}

void Sq_EitherEvent(HSQUIRRELVM v, EventType *FirstType) {
  const SQChar *Name;
  const SQChar *Args;
  const SQChar *Context;
  sq_getstring(v, 2, &Name);
  sq_getstring(v, 3, &Args);
  sq_getstring(v, 4, &Context);
  StartEvent(Name, Args, Context, FirstType, 0);
}
SQInteger Sq_StartEvent(HSQUIRRELVM v) {
  Sq_EitherEvent(v, FirstEventType);
  return 0;
}
SQInteger Sq_StartCommand(HSQUIRRELVM v) {
  Sq_EitherEvent(v, FirstCommand);
  return 0;
}

void Sq_DecodeJSONTable(HSQUIRRELVM v, cJSON *Item) {
  if(!Item) return;
  while(Item) {
    if(Item->string)
      sq_pushstring(v, Item->string, -1);
    switch(Item->type) {
      case cJSON_False:
        sq_pushbool(v, SQFalse);
        break;
      case cJSON_True:
        sq_pushbool(v, SQTrue);
        break;
      case cJSON_NULL:
        sq_pushnull(v);
        break;
      case cJSON_Number:
        if(Item->valueint == Item->valuedouble)
          sq_pushinteger(v, Item->valueint);
        else
          sq_pushfloat(v, Item->valuedouble);
        break;
      case cJSON_String:
        sq_pushstring(v, Item->valuestring, -1);
        break;
      case cJSON_Array:
        sq_newarray(v, 0);
        Sq_DecodeJSONTable(v, Item->child);
        break;
      case cJSON_Object:
        sq_newtable(v);
        Sq_DecodeJSONTable(v, Item->child);
        break;
    }
    if(Item->string)
      sq_newslot(v,-3,SQFalse);
    else
      sq_arrayappend(v, -2);
    Item = Item->next;
  }
}

SQInteger Sq_DecodeJSON(HSQUIRRELVM v) { // more security than compilestring()
  const SQChar *Str;
  sq_getstring(v, 2, &Str);
  if(Str[0]!='{' && Str[0]!='[') {
    if(!strcasecmp(Str, "true"))
      sq_pushbool(v, SQTrue);
    else if(!strcasecmp(Str, "false"))
      sq_pushbool(v, SQFalse);
    else if(isdigit(Str[0]) || (Str[0]=='-' && isdigit(Str[1])))
      sq_pushinteger(v, strtol(Str, NULL, 0));
    else
      sq_pushstring(v, Str, -1);
    return 1;
  }

  cJSON *Root = cJSON_Parse(Str);
  if(!Root || !Root->child) {
    sq_pushnull(v);
    return 1;
  }
  sq_newtable(v);
  Sq_DecodeJSONTable(v, Root->child);
  cJSON_Delete(Root);
  return 1;
}

SQInteger Sq_ConvertBBCode(HSQUIRRELVM v) {
  const SQChar *Input;
  SQInteger Flags = 0;
  int ArgCount = sq_gettop(v);
  sq_getstring(v, 2, &Input);
  if(ArgCount == 3)
    sq_getinteger(v, 3, &Flags);
  char Out[8192];
  sq_pushstring(v, ConvertBBCode(Out, Input, Flags), -1);
  return 1;
}

long unsigned CurMessageId = 0;
int SqX_AddMessage(const char *Message, ClientTab *Tab, time_t Time, int Flags) {
  if(!Tab) { // no tab to print onto?
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", Message);
    return -1;
  }
  if(!Tab->Messages) { // need to allocate channel message buffer?
    int ArraySize = Tab->MessageBufferSize;
    if(!ArraySize) ArraySize = GetConfigInt(500, "ChatView/ScrollbackSize");
    Tab->MessageBufferSize = ArraySize;
    Tab->Messages = (ClientMessage*)malloc(ArraySize*sizeof(ClientMessage));
    if(!Tab->Messages) { 
      SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "failed to allocate a tab message buffer", ArraySize);
      return 0;
    }
    Tab->NumMessages = 0;
  }

  ClientMessage *TabMsg = &Tab->Messages[Tab->NumMessages++];
  if(Tab->NumMessages >= Tab->MessageBufferSize) {
    if(Tab->Messages[0].RightExtend) // free if extra memory is used
      free(Tab->Messages[0].RightExtend);
    // a big memmove is probably worth saving complexity on making the buffer circular
    memmove(&Tab->Messages[0], &Tab->Messages[1], (Tab->MessageBufferSize-1)*sizeof(ClientMessage));
    Tab->NumMessages--;
  }

  TabMsg->Time = Time;
  TabMsg->Flags = Flags;
  char *Split = strchr(Message, '\t');
  const char *Right = Message;
  *TabMsg->Left = 0;
  if(Split) {
    unsigned int LeftLen = Split - Message;
    if(LeftLen >= sizeof(TabMsg->Left)) LeftLen = sizeof(TabMsg->Left)-1;
    memcpy(TabMsg->Left, Message, LeftLen);
    TabMsg->Left[LeftLen] = 0;
    Right = Split+1;
  }

  TabMsg->Id = CurMessageId;
  TabMsg->RightExtend = NULL;
  const int MaxRight = sizeof(TabMsg->Right)-1; // -1 for terminator

  strlcpy(TabMsg->Right, Right, MaxRight);
  if(strlen(Right)>MaxRight) {
    TabMsg->RightExtend = (char*)malloc(strlen(Right)-MaxRight+1);
    strcpy(TabMsg->RightExtend, Right+MaxRight-1);
  }
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "right: %s%s", TabMsg->Right, TabMsg->RightExtend?:"");

  Tab->UndrawnLines++;
  if(Tab->UndrawnLines == 1) {
    char Buffer[CONTEXT_BUF_SIZE];
    ContextForTab(Tab, Buffer);
    MainThreadRequest(MTR_INFO_ADDED, StringClone(Buffer), NULL);
  }
  return CurMessageId++;
}

SQInteger Sq_AddMessage(HSQUIRRELVM v) {
  const SQChar *Message, *Context;
  SQInteger Time, Flags;
  sq_getstring(v, 2, &Message);
  sq_getstring(v, 3, &Context);
  sq_getinteger(v, 4, &Time);
  if(!Time)
    Time = time(NULL);
  sq_getinteger(v, 5, &Flags);
  ClientTab *Tab = FindTab(Context);
  if(!Tab) return -1;

  sq_pushinteger(v, SqX_AddMessage(Message, Tab, Time, Flags));
  return 1;
}

/* functions to interact with Squirrel */
void Sq_GetAddonInfo(HSQUIRRELVM v, const char *Field, char *CopyTo, int CopyToSize) {
  const SQChar *Str;
  sq_pushstring(v, _SC("AddonInfo"), -1);
  sq_get(v, -2); // table pushed
  sq_pushstring(v, _SC(Field), -1);
  sq_get(v, -2);
  sq_getstring(v, -1, &Str);
  strlcpy(CopyTo, Str, CopyToSize);
  sq_pop(v,2); // pop both the string and table
}

int Sq_DoFile(HSQUIRRELVM VM, const char *File) {
  return sqstd_dofile(VM, _SC(File), SQFalse, SQTrue);
}

void Sq_RegisterFunc(HSQUIRRELVM v,SQFUNCTION f,const char *fname, int ParamNum, const char *params) {
  sq_pushstring(v,fname,-1);
  sq_newclosure(v,f,0);
  sq_newslot(v,-3,SQFalse);
  if(params)
    sq_setparamscheck(v, ParamNum, _SC(params)); 
}

HSQUIRRELVM Sq_Open(const char *File) {
  HSQUIRRELVM v; 
  v = sq_open(1024);
  sqstd_seterrorhandlers(v);
  sq_setprintfunc(v, Sq_PrintFunc,Sq_ErrorFunc);

  sq_pushroottable(v);
  sqstd_register_mathlib(v);
  sqstd_register_stringlib(v);
  sq_pop(v,1);

  sq_pushroottable(v);
  sq_pushstring(v, "api", -1);
  sq_newtable(v);
  sq_newslot(v,-3,SQFalse);
  sq_pushstring(v, "api", -1);
  sq_get(v, -2);

  Sq_RegisterFunc(v,   Sq_StrToL,         "Num",            2, "si");
  Sq_RegisterFunc(v,   Sq_AsciiChr,       "Chr",            1, "i");
  Sq_RegisterFunc(v,   Sq_WildMatch,      "WildMatch",      2, "ss");
  Sq_RegisterFunc(v,   Sq_SetClipboard,   "SetClipboard",   1, "s");
  Sq_RegisterFunc(v,   Sq_GetClipboard,   "GetClipboard",   0, "");
  Sq_RegisterFunc(v,   Sq_AddTimer,       "AddTimer",       3, "ic.");
  Sq_RegisterFunc(v,   Sq_DelTimer,       "DelTimer",       1, "i");
  Sq_RegisterFunc(v,   Sq_AddEventHook,   "AddEventHook",   6, "sciiii");
  Sq_RegisterFunc(v,   Sq_DelEventHook,   "DelEventHook",   2, "si");
  Sq_RegisterFunc(v,   Sq_AddCommandHook, "AddCommandHook", 5, "sciss");
  Sq_RegisterFunc(v,   Sq_DelCommandHook, "DelCommandHook", 2, "si");
  Sq_RegisterFunc(v,   Sq_DecodeJSON,     "DecodeJSON",     1, "s");
  Sq_RegisterFunc(v,   Sq_GetInfo,        "GetInfo",        1, "s");
  Sq_RegisterFunc(v,   Sq_NativeCommand,  "NativeCommand",  3, "sss");
  Sq_RegisterFunc(v,   Sq_StartEvent,     "_Event",         3, "sss");
  Sq_RegisterFunc(v,   Sq_StartCommand,   "Command",        3, "sss");
  Sq_RegisterFunc(v,   Sq_LoadTextFile,   "LoadTextFile",   1, "s");
  Sq_RegisterFunc(v,   Sq_SaveTextFile,   "SaveTextFile",   2, "ss");
  Sq_RegisterFunc(v,   Sq_TextFileExists, "TextFileExists", 1, "s");
  Sq_RegisterFunc(v,   Sq_CmdParamSplit,  "CmdParamSplit",  1, "s");
  Sq_RegisterFunc(v,   Sq_TextParamSplit, "TextParamSplit", 1, "s");
  Sq_RegisterFunc(v,   Sq_TabCreate,      "TabCreate",      3, "ssi");
  Sq_RegisterFunc(v,   Sq_TabExists,      "TabExists",      1, "s");
  Sq_RegisterFunc(v,   Sq_TabFind,        "TabFind",        2, "ss");
  Sq_RegisterFunc(v,   Sq_TabRemove,      "TabRemove",      1, "s");
  Sq_RegisterFunc(v,   Sq_TabSetFlags,    "TabSetFlags",    2, "si");
  Sq_RegisterFunc(v,   Sq_TabGetFlags,    "TabGetFlags",    1, "s");
  Sq_RegisterFunc(v,   Sq_TabGetInfo,     "TabGetInfo",     2, "ss");
  Sq_RegisterFunc(v,   Sq_TabSetInfo,     "TabSetInfo",     3, "sss");
  Sq_RegisterFunc(v,   Sq_AddMessage,     "AddMessage",     4, "ssii");
  Sq_RegisterFunc(v,   Sq_ConvertBBCode,  "ConvertBBCode", -1, "s");
  Sq_RegisterFunc(v,   Sq_Time,           "Time",           0, "");
  Sq_RegisterFunc(v,   Sq_MD5,            "MD5",            1, "s");
  Sq_RegisterFunc(v,   Sq_FindAddon,      "FindAddon",      2, "sb");
  Sq_RegisterFunc(v,   Sq_LoadAddon,      "LoadAddon",      1, "s");
  Sq_RegisterFunc(v,   Sq_ReloadAddon,    "ReloadAddon",    1, "s");
  Sq_RegisterFunc(v,   Sq_UnloadAddon,    "UnloadAddon",    1, "s");
  Sq_RegisterFunc(v,   Sq_CallAddon,      "CallAddon",      2, "ss");
  Sq_RegisterFunc(v,   Sq_EvalAddon,      "EvalAddonJSON",  2, "ss");
  Sq_RegisterFunc(v,   Sq_GetConfigInt,   "GetConfigInt",   2, "is");
  Sq_RegisterFunc(v,   Sq_GetConfigStr,   "GetConfigStr",   2, "ss");
  Sq_RegisterFunc(v,   Sq_GetConfigNames, "GetConfigNames", 1, "s");
  Sq_RegisterFunc(v,   Sq_CurlGetPost,    "CurlGet",        3, "c.s");
  Sq_RegisterFunc(v,   Sq_CurlGetPost,    "CurlPost",       4, "c.ss");
  Sq_RegisterFunc(v,   Sq_URLEncode,      "URLEncode",      1, "s");
  Sq_RegisterFunc(v,   Sq_NetOpen,        "NetOpen",        3, "css");
  Sq_RegisterFunc(v,   Sq_NetSend,        "NetSend",        2, "is");
  Sq_RegisterFunc(v,   Sq_NetClose,       "NetClose",       2, "ib");
  Sq_RegisterFunc(v,   Sq_Interpolate,    "Interpolate",    3, "ssa");
  Sq_RegisterFunc(v,   Sq_GetTicks,       "GetTicks",       0, "");
  sq_pop(v,1); // pop api table

  Sq_DoFile(v, "data/scripts/common.nut");
  if(File) Sq_DoFile(v, File);
  return v;
}

void Sq_Close(HSQUIRRELVM v) {
// recipe for disaster: Sq_Close and sq_close with different behavior
  sq_pop(v,1);
  sq_close(v); 
}

const char *FilenameOnly(const char *Path) {
  const char *Temp = strrchr(Path, '/');
  if(!Temp) Temp = strrchr(Path, '\\');
  if(!Temp) return Temp;
  return Temp+1;
}

ClientAddon *FindAddon(const char *Name, ClientAddon **First) {
  for(ClientAddon *Find = *First; Find; Find=Find->Next)
    if(!strcasecmp(Name, Find->Name) || !strcasecmp(Name, Find->Path))
      return Find;
  return NULL;
}

ClientAddon *UnloadAddon(ClientAddon *Addon, ClientAddon **First, int Reload);

extern sparkles_plugin BaseSparklesPlugin;
extern xchat_plugin BaseXChatPlugin;
ClientAddon *LoadAddon(const char *Path, ClientAddon **First) {
  const char *OnlyExtension = strrchr(Path, '.');

  ClientAddon *NewAddon = NULL;

  if(!*First) {
    NewAddon = (ClientAddon*)calloc(1,sizeof(ClientAddon));
    *First = NewAddon;
  } else {
    ClientAddon *Find = *First;
    while(Find->Next) {
      const char *Temp1 = FilenameOnly(Path);
      const char *Temp2 = FilenameOnly(Find->Path);

      if(!strcasecmp(Temp1, Temp2)) // don't load same addon more than once
        return NULL;
      Find = Find->Next;
    }
    NewAddon = (ClientAddon*)calloc(1,sizeof(ClientAddon)); // all pointers NULL
    Find->Next = NewAddon;
    NewAddon->Prev = Find;
  }
  if(!NewAddon) return NULL;

  strlcpy(NewAddon->Path, Path, sizeof(NewAddon->Path));

  if(!strcasecmp(OnlyExtension, ".nut")) { // Squirrel script
    NewAddon->Script = Sq_Open(Path);
    Sq_GetAddonInfo(NewAddon->Script, "Name", NewAddon->Name, sizeof(NewAddon->Name));
    Sq_GetAddonInfo(NewAddon->Script, "Author", NewAddon->Author, sizeof(NewAddon->Author));
    Sq_GetAddonInfo(NewAddon->Script, "Summary", NewAddon->Summary, sizeof(NewAddon->Summary));
    Sq_GetAddonInfo(NewAddon->Script, "Version", NewAddon->Version, sizeof(NewAddon->Version));
    sq_setforeignptr(NewAddon->Script, (SQUserPointer)NewAddon);
  } else if(!strcasecmp(OnlyExtension, ".so") || !strcasecmp(OnlyExtension, ".dll")) { // C plugin
    char *plugin_name = "", *plugin_desc = "", *plugin_version = "", *plugin_author = "";
    int (*Sparkles_InitPlugin)(xchat_plugin *plugin_handle, sparkles_plugin *sparkles_handle, char **plugin_name, char **plugin_desc, char **plugin_version, char **plugin_author, char *arg);
    int (*xchat_plugin_init)(xchat_plugin *plugin_handle, char **plugin_name, char **plugin_desc, char **plugin_version, char *arg);

    NewAddon->CPlugin = SDL_LoadObject(Path);
    if(!NewAddon->CPlugin)
      goto Error;

    NewAddon->SparklesPlugin = (sparkles_plugin*)malloc(sizeof(sparkles_plugin));
    if(!NewAddon->SparklesPlugin)
      goto Error;
    memcpy(NewAddon->SparklesPlugin, &BaseSparklesPlugin, sizeof(sparkles_plugin));

    NewAddon->XChatPlugin = (xchat_plugin*)malloc(sizeof(xchat_plugin));
    if(!NewAddon->XChatPlugin)
      goto Error;
    memcpy(NewAddon->XChatPlugin, &BaseXChatPlugin, sizeof(xchat_plugin));

    GUIDialog *ChatView = FindDialogWithProc(NULL, NULL, NULL, Widget_ChatView);
    NewAddon->XChatPlugin->Context = (ClientTab*)ChatView->dp;

    // loaded, try different entry points
    Sparkles_InitPlugin = SDL_LoadFunction(NewAddon->CPlugin, "Sparkles_InitPlugin");
    if(Sparkles_InitPlugin) 
      Sparkles_InitPlugin(NewAddon->XChatPlugin, NewAddon->SparklesPlugin, &plugin_name, &plugin_desc, &plugin_version, &plugin_author, NULL);
    else {
      xchat_plugin_init = SDL_LoadFunction(NewAddon->CPlugin, "hexchat_plugin_init");
      if(!xchat_plugin_init)
        xchat_plugin_init = SDL_LoadFunction(NewAddon->CPlugin, "xchat_plugin_init");
      if(!xchat_plugin_init) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Plugin %s doesn't contain an init function?", Path);
        goto Error;
      }

      xchat_plugin_init(NewAddon->XChatPlugin, &plugin_name, &plugin_desc, &plugin_version, NULL);
    }

    strlcpy(NewAddon->Name, plugin_name, sizeof(NewAddon->Name));
    strlcpy(NewAddon->Author, plugin_author, sizeof(NewAddon->Author));
    strlcpy(NewAddon->Summary, plugin_desc, sizeof(NewAddon->Summary));
    strlcpy(NewAddon->Version, plugin_version, sizeof(NewAddon->Version));
  } else { // not .nut, .dll or .so
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Unrecognized file extension for %s", Path);
  }
  return NewAddon;

Error:
  UnloadAddon(NewAddon, First, 0);
  return NULL;
}

void UnloadAddonHooks(ClientAddon *Addon, EventType **FirstType) {
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "unloading hooks for %s", Addon->Name);
  for(int Priority = PRI_HIGHEST; Priority > -1; Priority--)
    for(EventType *TypeSearch = *FirstType; TypeSearch; TypeSearch=TypeSearch->Next)
      if(TypeSearch->Hooks[Priority]) {
        EventHook *Hook = TypeSearch->Hooks[Priority];
        while(Hook) {
          EventHook *Next = Hook->Next;
          if( (Addon->Script && Addon->Script == Hook->Script)
            ||(!Addon->Script && Hook->XChatHook && Hook->XChatHook->XChatPlugin->Addon == Addon)) {
//            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "yes, removing %s at %i", TypeSearch->Type, Priority);
            DelEventHookInList(&TypeSearch->Hooks[Priority], Hook);
          }
          Hook = Next;
        }
      }
}

ClientAddon *UnloadAddon(ClientAddon *Addon, ClientAddon **First, int Reload) {
  if(!Addon) return NULL;
  UnloadAddonHooks(Addon, &FirstEventType);
  UnloadAddonHooks(Addon, &FirstCommand);
  if(Addon == *First) *First = Addon->Next;
  ClientAddon AddonRestore = *Addon;
  if(Addon->Script) Sq_Close(Addon->Script);
  if(Addon->CPlugin) {
    int (*DeInit)(void) = SDL_LoadFunction(Addon->CPlugin, "Sparkles_DeInitPlugin");
    if(!DeInit)
      DeInit = SDL_LoadFunction(Addon->CPlugin, "hexchat_plugin_deinit");
    if(!DeInit)
      DeInit = SDL_LoadFunction(Addon->CPlugin, "xchat_plugin_deinit");
    if(DeInit)
      DeInit();
    SDL_UnloadObject(Addon->CPlugin);
  }
  if(Addon->SparklesPlugin) free(Addon->SparklesPlugin);
  if(Addon->XChatPlugin) free(Addon->XChatPlugin);
  free(Addon);
  if(Reload)
    return LoadAddon(AddonRestore.Path, First);
  return NULL;
}
