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

extern int CurHookId;
extern int RecurseLevel;
extern ClientAddon *CurAddon;

int StartEvent(const char *TypeName, const char *EventInfo, const char *EventContext, EventType *FirstType, int SpecialFlags) {
  SDL_LockMutex(LockTabs);
  int Handled = 0;
  long Flags = 0;
  char InfoBuffer[9001]="";
  if(!EventInfo) EventInfo = "";
  RecurseLevel++;
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "looking for tab for command %s", TypeName);
  ClientTab *Tab = FindTab(EventContext);
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "doing command %s", TypeName);

  // SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "starting event type %s", TypeName);
  for(int Priority = PRI_HIGHEST; Priority > -1; Priority--)
    for(EventType *TypeSearch = FirstType; TypeSearch; TypeSearch=TypeSearch->Next)
      if(TypeSearch->Hooks[Priority] && WildMatch(TypeName, TypeSearch->Type))
        for(EventHook *Hook = TypeSearch->Hooks[Priority]; Hook; Hook=Hook->Next) {
          if(Hook->HookInfo.FlagsNeed.EventFlagsNeed0 & Flags || ((Hook->HookInfo.FlagsNeed.EventFlagsNeed1 & Flags) != Hook->HookInfo.FlagsNeed.EventFlagsNeed1)) continue;

          if(Hook->Script) { // Squirrel script
            if((Hook->Flags & EHOOK_PROTOCOL_COMMAND) && Tab && (Hook->Script != Tab->Script)) continue;

            SQInteger top = sq_gettop(Hook->Script);
            sq_pushroottable(Hook->Script);
            sq_pushobject(Hook->Script, Hook->Function.Squirrel);
            sq_pushroottable(Hook->Script); // push "this"
            sq_pushstring(Hook->Script,TypeName,-1);
            sq_pushstring(Hook->Script,EventInfo,-1);
            sq_pushstring(Hook->Script,EventContext,-1);
            sq_call(Hook->Script,4,SQTrue,SQTrue);

            if(OT_INTEGER == sq_gettype(Hook->Script,-1)) {
              SQInteger IntResult;
              sq_getinteger(Hook->Script, -1, &IntResult);
              switch(IntResult) {
                case ER_NORMAL:
                  break;
                case ER_HANDLED:
                case ER_BADSYNTAX:
                  Flags |= EF_ALREADY_HANDLED;
                  Handled = 1;
                  break;
                case ER_DELETE:
                  return 1;
              }
            } else if(OT_STRING == sq_gettype(Hook->Script,-1)) {
              const SQChar *StringResult;
              sq_getstring(Hook->Script, -1, &StringResult);
              EventInfo = InfoBuffer;
              strlcpy(InfoBuffer, StringResult, sizeof(InfoBuffer));
            break;
          }
          sq_settop(Hook->Script,top);
        } else { // C plugin
//          SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "C hook: %p", Hook->Function.Native);
          int Response = -1;
          Hook->XChatHook->XChatPlugin->Context = Tab;
          if(Hook->Flags & EHOOK_COMMAND_CALLBACK) {
            int (*callback) (const char *word[], const char *word_eol[], void *user_data) = Hook->Function.Native;
            char EventInfo2[strlen(TypeName)+strlen(EventInfo)+2];
            // XChat commands expect the first word to be the command name
            sprintf(EventInfo2, "%s %s", TypeName, EventInfo);
            char ArgBuff[strlen(EventInfo2)+1];
            const char *Word[32];
            const char *WordEol[32];
            XChatTokenize(EventInfo2, ArgBuff, Word, WordEol, 32, TOKENIZE_MULTI_WORD | TOKENIZE_EMPTY_FIRST);
            Response = callback(Word, WordEol, Hook->XChatHook->Userdata);
//            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "command hook, got %i", Response);
          } else if(Hook->Flags & EHOOK_XCHAT_EVENT_CALLBACK) {
//            int (*callback) (char *word[], void *user_data) = Hook->Function.Native;
//            Response = callback();
          } else if(EHOOK_SPARK_JSON_CALLBACK) {
//            int (*callback) (cJSON *JSON, void *user_data) = Hook->Function.Native;
//            Response = callback();
          } else {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Unknown C plugin hook type");
          }
          switch(Response) {
            case XCHAT_EAT_PLUGIN:
              Flags |= EF_ALREADY_HANDLED;
            case XCHAT_EAT_XCHAT:
              Handled = 1;
            case XCHAT_EAT_NONE:
            default:
              break;
            case XCHAT_EAT_ALL:
              return 1;
          }
        }
      }
  RecurseLevel--;
  SDL_UnlockMutex(LockTabs);
  return Handled;
}

int AddEventHook(EventType **EventList, HSQUIRRELVM v, const char *EventName, HSQOBJECT Handler, int Priority, int Flags, int Need0, int Need1) {
  if(Priority & 8) { // AddCommandHook embeds PROTOCOL_CMD in the priority
    Priority &= ~8;
    Flags |= EHOOK_PROTOCOL_COMMAND;
  }
  if(Priority < PRI_LOWEST || Priority > PRI_HIGHEST) return 0;
  EventType *Type = *EventList;

  if(!Type) { // have to make the type list?
    Type = (EventType*)calloc(1,sizeof(EventType));
    if(!Type) return 0;
    *EventList = Type;
    strlcpy(Type->Type, EventName, sizeof(Type->Type));
  } else { // have to make the type? (if list is already started)
    while(1) {
      if(!strcasecmp(Type->Type, EventName))
        break;
      if(!Type->Next) {
        Type->Next = (EventType*)calloc(1,sizeof(EventType));
        if(!Type->Next) return 0;
        Type->Next->Prev = Type;
        Type=Type->Next;
        strlcpy(Type->Type, EventName, sizeof(Type->Type));
        break;
      }
      Type = Type->Next;
    }
  }

  EventHook *Hook = (EventHook*)calloc(1,sizeof(EventHook));
  if(!Hook) return 0;
  if(!Type->Hooks[Priority])
    Type->Hooks[Priority] = Hook;
  else {
    EventHook *Find = Type->Hooks[Priority];
    while(Find->Next)
      Find = Find->Next;
    Find->Next = Hook;
    Hook->Prev = Find;
  }

  Hook->Flags = Flags;
  Hook->HookInfo.FlagsNeed.EventFlagsNeed0 = Need0;
  Hook->HookInfo.FlagsNeed.EventFlagsNeed1 = Need1;
  Hook->HookId = CurHookId++;
//  Hook->Addon = Addon;
  Hook->Script = v;
//  strlcpy(Hook->Function.Squirrel, Handler, sizeof(Hook->Function.Squirrel));
  Hook->Function.Squirrel = Handler;
  sq_addref(v,&Handler);
  return 1;
}


void DelEventHookInList(EventHook **List, EventHook *Hook) {
  if(Hook->Script)
    sq_release(Hook->Script, &Hook->Function.Squirrel);
  if(Hook->Prev)
    Hook->Prev->Next = Hook->Next;
  if(Hook->Next)
    Hook->Next->Prev = Hook->Prev;
  if(Hook == *List)
    *List = Hook->Next;
  free(Hook);
}

void DelEventHookId(EventType **EventList, const char *EventName, int HookId) {
  for(EventType *Type = *EventList; Type; Type = Type->Next)
    if(!strcasecmp(Type->Type, EventName))
      for(int P=0; P<=PRI_HIGHEST; P++)
        for(EventHook *Hook = Type->Hooks[P]; Hook; Hook = Hook->Next)
          if(HookId == Hook->HookId) {
            DelEventHookInList(&Type->Hooks[P], Hook);
            return;
          }
}

int XChatTokenize(const char *Input, char *WordBuff, const char **Word, const char **WordEol, int WordSize, int Flags) {
  int i;
  for(i=0;i<WordSize;i++) {
    Word[i] = "";
    WordEol[i] = "";
  }
  strcpy(WordBuff, Input);

  i = (Flags & TOKENIZE_EMPTY_FIRST)?1:0;
  char *Peek = WordBuff;
  while(1) {
    int IsMulti = Flags & TOKENIZE_MULTI_WORD && Peek[0] == '\"';
    char *Next;
    if(IsMulti) // todo: "" inside multiword tokens yet
      Next = strchr(Peek+1, '"');
    else
      Next = strchr(Peek+1, ' ');

    Word[i] = Peek+IsMulti;
    WordEol[i] = Input + (Peek - WordBuff);

    if(++i >= WordSize)
      break;

    if(!Next) break;
    *Next = 0;

    Peek = Next+1;
    while(*Peek == ' ') Peek++;
  }
  return i;
}

extern GUIState *GUIStates[];

GUIDialog *FindDialogWithProc(const char *Context, GUIState *InState, GUIState **State, DIALOG_PROC Proc) {
  GUIState *LookIn = InState;
  if(!LookIn) LookIn = GUIStates[CurWindow];

  if(State)
    *State = LookIn;
  for(int i=0;LookIn->Dialog[i].proc;i++)
    if(LookIn->Dialog[i].proc==Proc)
      return &LookIn->Dialog[i];
  return NULL;
}

int NativeCommand(const char *Command, const char *Args, const char *Context) {
  char ArgBuff[strlen(Args)+1];
  const char *Word[32];
  const char *WordEol[32];
  XChatTokenize(Args, ArgBuff, Word, WordEol, 32, TOKENIZE_MULTI_WORD);

  if(!strcasecmp(Command, "client")) {
    if(!strcasecmp(Word[0], "cut")) {
      MainThreadRequest(MTR_EDIT_CUT, StringClone(Context), NULL);
      return ER_HANDLED;
    } else if(!strcasecmp(Word[0], "copy")) {
      MainThreadRequest(MTR_EDIT_COPY, StringClone(Context), NULL);
      return ER_HANDLED;
    } else if(!strcasecmp(Word[0], "paste")) {
      MainThreadRequest(MTR_EDIT_PASTE, StringClone(Context), NULL);
      return ER_HANDLED;
    } else if(!strcasecmp(Word[0], "delete")) {
      MainThreadRequest(MTR_EDIT_PASTE, StringClone(Context), NULL);
      return ER_HANDLED;
    } else if(!strcasecmp(Word[0], "selectall")) {
      MainThreadRequest(MTR_EDIT_SELECT_ALL, StringClone(Context), NULL);
      return ER_HANDLED;
    } else if(!strcasecmp(Word[0], "inputaddchar")) {
      MainThreadRequest(MTR_EDIT_INPUT_CHAR, StringClone(Context), StringClone(WordEol[1]));
      return ER_HANDLED;
    } else if(!strcasecmp(Word[0], "inputaddtext")) {
      MainThreadRequest(MTR_EDIT_INPUT_TEXT, StringClone(Context), StringClone(WordEol[1]));
      return ER_HANDLED;
    } else if(!strcasecmp(Word[0], "settext")) {
      MainThreadRequest(MTR_EDIT_SET_TEXT, StringClone(Context), StringClone(WordEol[1]));
      return ER_HANDLED;
    } else if(!strcasecmp(Word[0], "appendtext")) {
      MainThreadRequest(MTR_EDIT_APPEND_TEXT, StringClone(Context), StringClone(WordEol[1]));
      return ER_HANDLED;
    } else if(!strcasecmp(Word[0], "setcursor")) {
      MainThreadRequest(MTR_EDIT_SET_CURSOR, StringClone(Context), StringClone(WordEol[1]));
      return ER_HANDLED;
    } else if(!strcasecmp(Word[0], "url")) {
      URLOpen(WordEol[1]);
      return ER_HANDLED;
    } else {
      //SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "unrecognized native command: %s", Word[0]);
    }
  } else if(!strcasecmp(Command, "connect") || !strcasecmp(Command, "sslconnect")) {
    int UseSSL = tolower(Command[0])=='s';
    ClientAddon *Addon = FindAddon(Word[0], &FirstAddon);
    if(Addon) {
      char ConnectString[strlen(WordEol[1]+6)];
      strcpy(ConnectString, WordEol[1]);
      if(UseSSL) strcat(ConnectString, " -ssl");

      if(Addon->Script) {
        SQInteger top = sq_gettop(Addon->Script);
        sq_pushroottable(Addon->Script);
        sq_pushstring(Addon->Script,_SC("Connect"),-1);
        if(SQ_SUCCEEDED(sq_get(Addon->Script,-2))) {
          sq_pushroottable(Addon->Script);
          sq_pushstring(Addon->Script, ConnectString,-1);
          sq_call(Addon->Script,2,SQFalse,SQTrue);
        }
        sq_settop(Addon->Script,top);
      } else {
        void (*Connect)(const char *ConnectString);
        Connect = SDL_LoadFunction(Addon->CPlugin, "Sparkles_Connect");
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Running %p", Connect);
        if(Connect)
          Connect(ConnectString);
        else
          SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Can't find Sparkles_Connect function in plugin");
      }
    }
    return ER_HANDLED;
  } else if(!strcasecmp(Command, "load")) {
    LoadAddon(WordEol[0], &FirstAddon);
    return ER_HANDLED;
  } else if(!strcasecmp(Command, "unload")) {
    ClientAddon *Addon = FindAddon(WordEol[0], &FirstAddon);
    if(Addon)
      UnloadAddon(Addon, &FirstAddon, 0);
    return ER_HANDLED;
  } else if(!strcasecmp(Command, "reload")) {
    ClientAddon *Addon = FindAddon(WordEol[0], &FirstAddon);
    if(Addon)
      UnloadAddon(Addon, &FirstAddon, 1);
    return ER_HANDLED;
  } else if(!strcasecmp(Command, "gui")) {
    MainThreadRequest(MTR_GUI_COMMAND, StringClone(Context), StringClone(WordEol[0]));
    return ER_HANDLED;
  }
  return ER_NORMAL;
}

void RunSqSockHandle(HSQUIRRELVM v, HSQOBJECT Handler, int Id, int Event, const char *Text) {
  SQInteger top = sq_gettop(v);
  sq_pushobject(v, Handler);
  sq_pushroottable(v);
  sq_pushinteger(v,Id);
  sq_pushinteger(v,Event);
  sq_pushstring(v, Text, -1);
  sq_call(v, 4, SQFalse,SQTrue);
  sq_settop(v, top);
}

void DoSockEvent(SqSocket *Socket, int Event, const char *Text) {
  RunSqSockHandle(Socket->Script, Socket->Function.Squirrel, Socket->Id, Event, Text);
}

/* Event thread queue format (input)
  "C Type Context Info" - Command broadcast
  "E Type Context Info" - Event broadcast
  "S SockId Event Text" - Sock stores what script it is  automatically */
int RunEventThread(void *Data) {
  while(!quit) {
    char *Command;
    do {
      Command = IPC_Read(EventQueue[IPC_IN], 100);
      if(Command) {
//        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "event: %s", Command);
        if(Command[0] == 'C' || Command[0]=='E') { // command or event
          char *TypeName, *EventInfo, *EventContext;

          TypeName = Command+1;
          EventContext = strchr(TypeName, '\xff')+1;
          EventContext[-1] = 0;
          EventInfo = strchr(EventContext, '\xff')+1;
          EventInfo[-1] = 0;
      
          StartEvent(TypeName, EventInfo, EventContext, (*Command=='C')?FirstCommand:FirstEventType, 0);
        } else if(Command[0]=='S') { // socket event
          int SockId = strtol(Command+1, NULL, 10);
          SqSocket *Sock = FindSockById(SockId);
          int Event = strtol(strchr(Command, ';')+1, NULL, 10);
          if(Sock->Script)
            DoSockEvent(Sock, Event, strchr(Command, ':')+1);
          else
            Sock->Function.Native(Sock->Id, Event, strchr(Command, ':')+1);
        } else
          SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Unrecognized queue command");
        free(Command);
      }
    } while(Command);
    RunSquirrelMisc();
  }
  return 0;
}

void QueueEvent(const char *TypeName, const char *EventInfo, const char *EventContext, char EventType) {
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "queueing an event");
  IPC_WriteF(EventQueue[IPC_IN], "%c%s\xff%s\xff%s", EventType, TypeName, EventContext, EventInfo);
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "queued the event");
}
