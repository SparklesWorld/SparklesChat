/*
 * SparklesChat
 * Copyright (C) 2014-2015 NovaSquirrel
 *
 * X-Chat
 * Copyright (C) 2002 Peter Zelezny.
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

extern int CurHookId;
extern int TimerID;
void DeleteTimerById(int Id);
extern EventHook *FirstTimer;

void Sq_RegisterFunc(HSQUIRRELVM v,SQFUNCTION f,const char *fname, int ParamNum, const char *params);
ClientConnection *ConnectionForTab(ClientTab *Tab);
extern cJSON *PluginPref;

void *Spark_FindSymbol(const char *SymbolName) {
  struct {
    const char *Name;
    void *Pointer;
  } Table[] = {
    {"FirstTab",       &FirstTab},
    {"FirstAddon",     &FirstAddon},
    {"FirstEventType", &FirstEventType},
    {"FirstCommand",   &FirstCommand},
    {"FirstHelp",      &FirstHelp},
    {"MenuMain",       &MenuMain},
    {"MenuChannel",    &MenuChannel},
    {"MenuUser",       &MenuUser},
    {"MenuUserTab",    &MenuUserTab},
    {"MenuTextEdit",   &MenuTextEdit},
    {"MainConfig",     &MainConfig},
    {"ContextForTab",  ContextForTab},
    {"FindTab",        FindTab},
    {"FindTabById",    FindTabById},
    {NULL}
  };
  for(int i=0;Table[i].Name;i++)
    if(!strcmp(Table[i].Name, SymbolName))
      return Table[i].Pointer;
  return NULL;
}

xchat_hook *MakeXChatHook(xchat_plugin *ph, EventHook *EHook, const char *EventName, int Type, void *Userdata) {
  xchat_hook *Hook = (xchat_hook*)malloc(sizeof(xchat_hook));
  if(!Hook) return NULL;
  EHook->XChatHook = Hook;
  Hook->Hook = EHook;
  Hook->XChatPlugin = ph;
  Hook->EventName = strdup(EventName);
  Hook->HookType = Type;
  Hook->Userdata = Userdata;
  return Hook;
}

extern int CurHookId;

xchat_hook *AddXChatEventHook(xchat_plugin *ph, EventType **List, const char *EventName, int (*callback) (char *word[], char *word_eol[], void *user_data), int Priority, int Flags, int Need0, int Need1, void *Userdata) {
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "new hook: %s", EventName);
  int RealPriority;
  switch(Priority) {
    case XCHAT_PRI_HIGHEST:   RealPriority = PRI_HIGHEST; break;
    case XCHAT_PRI_MORE_HIGH: RealPriority = PRI_HIGHER;  break;
    case XCHAT_PRI_HIGH:      RealPriority = PRI_HIGH;    break;
    default:
    case XCHAT_PRI_NORM:      RealPriority = PRI_NORMAL;  break;
    case XCHAT_PRI_LOW:       RealPriority = PRI_LOW;     break;
    case XCHAT_PRI_MORE_LOW:  RealPriority = PRI_LOWER;   break;
    case XCHAT_PRI_LOWEST:    RealPriority = PRI_LOWEST;  break;
  }
  EventType *Type = *List;

  if(!Type) { // have to make the type list?
    Type = (EventType*)calloc(1,sizeof(EventType));
    if(!Type) return NULL;
    *List = Type;
    strlcpy(Type->Type, EventName, sizeof(Type->Type));
  } else { // have to make the type? (if list is already started)
    while(1) {
      if(!strcasecmp(Type->Type, EventName))
        break;
      if(!Type->Next) {
        Type->Next = (EventType*)calloc(1,sizeof(EventType));
        if(!Type->Next) return NULL;
        Type->Next->Prev = Type;
        Type=Type->Next;
        strlcpy(Type->Type, EventName, sizeof(Type->Type));
        break;
      }
      Type = Type->Next;
    }
  }

  EventHook *Hook = (EventHook*)calloc(1,sizeof(EventHook));
  if(!Hook) return NULL;
  if(!Type->Hooks[RealPriority])
    Type->Hooks[RealPriority] = Hook;
  else {
    EventHook *Find = Type->Hooks[RealPriority];
    while(Find->Next)
      Find = Find->Next;
    Find->Next = Hook;
    Hook->Prev = Find;
  }

  Hook->Flags = Flags;
  Hook->HookInfo.FlagsNeed.EventFlagsNeed0 = Need0;
  Hook->HookInfo.FlagsNeed.EventFlagsNeed1 = Need1;
  Hook->HookId = CurHookId++;
  Hook->Script = NULL;
  Hook->Function.Native = callback;

  xchat_hook *XChatHook = MakeXChatHook(ph, Hook, EventName, *List==FirstCommand?HOOK_COMMAND:HOOK_EVENT, Userdata);
  if(!XChatHook) {
    free(Hook);
    return NULL;
  }
  return XChatHook;
}

xchat_hook *xchat_hook_command (xchat_plugin *ph, const char *name,
int pri, int (*callback) (char *word[], char *word_eol[], void *user_data), const char *help_text, void *userdata) {
  return AddXChatEventHook(ph, &FirstCommand, name, (void*)callback, pri, EHOOK_COMMAND_CALLBACK, EF_ALREADY_HANDLED, 0, userdata);
}

xchat_hook *xchat_hook_server (xchat_plugin *ph, const char *name,
int pri, int (*callback) (char *word[], char *word_eol[], void *user_data), void *userdata) {
  char Name[strlen(name)+11];
  sprintf(Name, "rawserver %s", name);
  return AddXChatEventHook(ph, &FirstEventType, Name, (void*)callback, pri, EHOOK_XCHAT_EVENT_CALLBACK, EF_ALREADY_HANDLED, 0, userdata);
}

xchat_hook *xchat_hook_print (xchat_plugin *ph, const char *name, int pri,
int (*callback) (char *word[], void *user_data), void *userdata) {
  return AddXChatEventHook(ph, &FirstEventType, name, (void*)callback, pri, EHOOK_XCHAT_EVENT_CALLBACK, EF_ALREADY_HANDLED, 0, userdata);
}

xchat_hook *xchat_hook_timer (xchat_plugin *ph, int timeout,
int (*callback) (void *user_data), void *userdata) {
  EventHook NewTimer;
  memset(&NewTimer, 0, sizeof(NewTimer));
  NewTimer.HookInfo.Timer.Timeout = SDL_GetTicks() + timeout;
  NewTimer.Script = NULL;
  NewTimer.Function.Native = callback;
  NewTimer.HookId = TimerID;
  NewTimer.Next = FirstTimer;

  EventHook *AllocTimer = (EventHook*)malloc(sizeof(EventHook));
  if(!AllocTimer)
    return NULL;
  *AllocTimer = NewTimer;

  // make XChat hook too
  xchat_hook *Hook = MakeXChatHook(ph, AllocTimer, "", HOOK_TIMER, userdata);
  if(!Hook) {
    free(AllocTimer);
    return NULL;
  }

  if(FirstTimer)
    FirstTimer->Prev = AllocTimer;

  FirstTimer = AllocTimer;
  return Hook;
}

xchat_hook *xchat_hook_fd (xchat_plugin *ph, int fd, int flags,
int (*callback) (int fd, int flags, void *user_data), void *userdata) {
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "hook fd");
  // no reason to support this?
  return NULL;
}
int xchat_read_fd(xchat_plugin *ph, void *src, char *buf, int *len) {
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "read fd");
  // nor this?
  return 0;
}

void *xchat_unhook(xchat_plugin *ph, xchat_hook *hook) {
  switch(hook->HookType) {
     case HOOK_EVENT:
       DelEventHookId(&FirstEventType, hook->EventName, hook->Hook->HookId);
       break;
     case HOOK_COMMAND:
       DelEventHookId(&FirstCommand, hook->EventName, hook->Hook->HookId);
       break;
     case HOOK_TIMER:
       DeleteTimerById(hook->Hook->HookId);
       break;
  }
  if(hook->EventName) free(hook->EventName);
  void *Userdata = hook->Userdata;
  if(hook->Userdata) free(hook->Userdata);
  free(hook);
  return Userdata;
}

int Spark_AddMessage(xchat_plugin *ph, const char *Message, time_t Time, int Flags);
void xchat_print(xchat_plugin *ph, const char *text) {
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "print %s", text);
  Spark_AddMessage(ph, text, 0, 0);
}

void xchat_printf(xchat_plugin *ph, const char *format, ...) {
  va_list args;
  char *buf;
  va_start(args, format);
  buf = strdup_vprintf(format, args);
  va_end(args);
  xchat_print(ph, buf);
  free(buf);
}

void xchat_command(xchat_plugin *ph, const char *command) {
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "command, using %s", command);
  char Command[strlen(command)+1];
  strcpy(Command, command);
  char *CmdArgs = strchr(Command, ' ');
  if(CmdArgs)
    *(CmdArgs++) = 0;

  xchat_context *Context = ph->Context;
  char Buffer[CONTEXT_BUF_SIZE];
  StartEvent(Command, CmdArgs, ContextForTab(Context, Buffer), FirstCommand, 0);
}

void xchat_commandf(xchat_plugin *ph, const char *format, ...) {
  va_list args;
  char *buf;
  va_start(args, format);
  buf = strdup_vprintf(format, args);
  va_end(args);
  xchat_command(ph, buf);
  free(buf);
}

int xchat_nickcmp(xchat_plugin *ph, const char *s1, const char *s2) {
  // fix later to have the weird IRC compare
  return strcasecmp(s1, s2);
}

int xchat_set_context(xchat_plugin *ph, xchat_context *ctx) {
  ph->Context = ctx;
  return 1;
}

xchat_context *xchat_find_context(xchat_plugin *ph, const char *servname, const char *channel) {
  char FindMe[CONTEXT_BUF_SIZE];
  if(servname && channel) {
    sprintf(FindMe, "%s/%s", servname, channel);
    return FindTab(FindMe);
  } else if(!servname && channel) {
    xchat_context *Context = ph->Context;
    if(Context->Parent) Context = Context->Parent;
    sprintf(FindMe, "%s/%s", Context->Name, channel);
    return FindTab(FindMe);    
  } else if(servname && !channel) {
    return FindTab(servname);
  } else { // get currently focused tab
    return GetFocusedTab();
  }
}

xchat_context *xchat_get_context(xchat_plugin *ph) {
  return ph->Context;
}

const char *ConnectionExtraInfo(ClientConnection *Connection, ClientTab *Tab, const char *Info) {
  // call ExtraInfo(Socket, Tab, Info) in script
  // to do: finish
  HSQUIRRELVM v = Connection->Script;
  SQInteger top = sq_gettop(v);        // top of the stack, to reset to it afterwards
  static char StrInteger[10];          // buffer for converting a string to an integer
  static char *Buffer = NULL;          // buffer for holding a value returned from a Squirrel script
  char Context[CONTEXT_BUF_SIZE];      // context buffer
  const SQChar *GetStr;                // returned string
  SQInteger GetValue;                  // returned integer
  sq_pushroottable(v);
  sq_pushstring(v, _SC("ExtraInfo"), -1);

  if(SQ_SUCCEEDED(sq_get(v, -2))) {
    // call function
    sq_pushroottable(v);
    sq_pushinteger(v, Connection->Socket);
    sq_pushstring(v, ContextForTab(Tab, Context), -1);
    sq_pushstring(v, Info, -1);
    sq_call(v, 4, SQTrue,SQTrue);

    switch(sq_gettype(v, -1)) {
      case OT_STRING:
        sq_getstring(v, -1, &GetStr);
        Buffer = realloc(Buffer, strlen(GetStr)+1);
        strcpy(Buffer, GetStr);
        sq_settop(v,top);
        return Buffer;
      case OT_INTEGER:
        sq_getinteger(v, -1, &GetValue);
        sprintf(StrInteger, "%i", GetValue);
        sq_settop(v,top);
        return StrInteger;
      default:
        sq_settop(v,top);
        return NULL;
    }

  }
  sq_settop(v,top);
  return NULL;
}

const char *xchat_get_info(xchat_plugin *ph, const char *id) {
  ClientTab *Tab = ph->Context;
  ClientConnection *Connection = NULL;
  if(ph->Context)
    Connection = ConnectionForTab(ph->Context);
  if(!Connection) // Most stuff will require a connection. Will also be false if no context.
    return NULL;

  if(!strcasecmp(id, "away")) {
    return ConnectionExtraInfo(Connection, Tab, "away");
  } else if(!strcmp(id, "channel")) {
    return Tab->Name;
  } else if(!strcmp(id, "charset")) {
    return "UTF-8";
  } else if(!strcmp(id, "host")) {
    return ConnectionExtraInfo(Connection, Tab, "host");
  } else if(!strcmp(id, "inputbox")) {
    return Tab->Inputbox;
  } else if(!strcmp(id, "libdirfs")) { // library directory. e.g. /usr/lib/xchat. The same directory used for auto-loading plugins
    return "./plugin"; 
  } else if(!strcmp(id, "modes")) {
    return ConnectionExtraInfo(Connection, Tab, "modes");
  } else if(!strcmp(id, "network")) {
    return ConnectionExtraInfo(Connection, Tab, "network");
  } else if(!strcmp(id, "protocol")) {
    ClientAddon *Addon = AddonForScript(Connection->Script);
    if(Addon) return Addon->Name;
  } else if(!strcmp(id, "nick") && Connection) {
    return Connection->YourName;
  } else if(!strcmp(id, "ticket") && Connection) {
    return Connection->Ticket;
  } else if(!strcmp(id, "nickserv") && Connection) {
    return ConnectionExtraInfo(Connection, Tab, "nickserv");
  } else if(!strcmp(id, "server") && Connection) {
    return ConnectionExtraInfo(Connection, Tab, "server");
  } else if(!strcmp(id, "topic")) {
    return ConnectionExtraInfo(Connection, Tab, "topic");
  } else if(!strcmp(id, "version")) {
    return CLIENT_VERSION;
  } else if(!strcmp(id, "win_status")) {
    return "normal"; //todo: determine to show active, hidden or normal
  } else if(!strcmp(id, "xchatdir") || !strcmp(id, "xchatdirfs") || !strcmp(id, "configdir")) {
    return PrefPath;
  }
  return NULL;
}

int xchat_get_prefs(xchat_plugin *ph, const char *name, const char **string, int *integer) {
  // will support only "state_cursor", "id" and "input_command_char"
  return 0;
}

xchat_list *xchat_list_get(xchat_plugin *ph, const char *name) {
  xchat_list *List = (xchat_list*)malloc(sizeof(xchat_list));
  List->Index = -1;
  if(!strcasecmp(name, "channels")) {
    List->Type = LIST_USERS;
    List->State.Tab = FirstTab;
    return List;
  } else if(!strcasecmp(name, "users")) {
    List->Type = LIST_USERS;
    List->State.Tab = ph->Context;
    return List;
  } else if(!strcasecmp(name, "addons")) {
    List->Type = LIST_ADDONS;
    List->State.Addon = FirstAddon;
    return List;
  } else if(!memcmp(name, "config ", 7)) {
    List->Type = LIST_CONFIG;
    char Search[strlen(name+7)+1];
    strcpy(Search, name+7);
    List->State.JSON = cJSON_Search(MainConfig, Search);
    if(!List->State.JSON) {
      free(List->State.JSON);
      return NULL;
    }
    return List;
  }
  return NULL;
}

void xchat_list_free(xchat_plugin *ph, xchat_list *xlist) {
  free(xlist);
}

const char * const *xchat_list_fields(xchat_plugin *ph, const char *name) {
  static const char * const channels_fields[] = {
    "schannel",	"schannelkey", "schantypes", "pcontext", "iflags", "iid", "imaxmodes",
    "snetwork", "snickmodes", "snickprefixes", "sserver", "itype", "iusers", "itabflags",
    "sicon", "pguidata1", "pguidata2", "pguidata3", "pguidata4", NULL
  };
  static const char * const users_fields[] = {
    "saccount", "iaway", "shost", "tlasttalk", "snick", "sprefix", "srealname", "iselected", NULL
  };
  static const char * const config_fields[] = {
    "sname", "itype", "iint", "sstring", NULL
  };
  static const char * const addon_fields[] = {
    "sname", "sauthor", "ssummary", "sversion", "spath", "pcplugin", NULL
  };
  static const char * const list_of_lists[] = {
    "channels",	"users", "addons", NULL
  };

  if(!strcmp(name, "channels"))
    return channels_fields;
  if(!strcmp(name, "users"))
    return users_fields;
  if(!strcmp(name, "lists"))
    return list_of_lists;
  if(!strcmp(name, "addons"))
    return addon_fields;
  if(memcmp(name, "config ", 7))
    return config_fields;
  return NULL;
}

int xchat_list_next(xchat_plugin *ph, xchat_list *xlist) {
  if(xlist->Index < 0) { // use next for the first time to get the first item
    xlist->Index = 0;
    if((xlist->Type == LIST_CHANNELS || xlist->Type == LIST_USERS) && !xlist->State.Tab) return 0;
    if(xlist->Type == LIST_USERS && !xlist->State.Tab->NumUsers) return 0;
    if(xlist->Type == LIST_CONFIG && !xlist->State.JSON) return 0;
    // Let's just assume an addons list is valid; if it wasn't, how would we be running this code?
    return 1;
  }
  switch(xlist->Type) {
    case LIST_CHANNELS:
      if(xlist->State.Tab->Child) xlist->State.Tab = xlist->State.Tab->Child;
      else if(xlist->State.Tab->Next) xlist->State.Tab = xlist->State.Tab->Next;
      else if(xlist->State.Tab->Parent && xlist->State.Tab->Parent->Next) xlist->State.Tab = xlist->State.Tab->Parent->Next;
      else return 0;
      return 1;
    case LIST_USERS:
      if(xlist->State.Tab->NumUsers > xlist->Index) {
        xlist->Index++;
        return 1;
      }
      return 0;
    case LIST_CONFIG:
      xlist->State.JSON = xlist->State.JSON->next;
      return 1;
  }
  return 0;
}

int Spark_SetListStr(xchat_plugin *ph, xchat_list *xlist, const char *name, const char *newvalue) {
  return 1;
}

int Spark_SetListInt(xchat_plugin *ph, xchat_list *xlist, const char *name, int newvalue) {
  return 1;
}

void *Spark_ListPtr(xchat_plugin *ph, xchat_list *xlist, const char *name) {
  switch(xlist->Type) {
    case LIST_CHANNELS:
      if(!strcasecmp(name, "guidata1"))
        return xlist->State.Tab->GUIData[0];
      else if(!strcasecmp(name, "guidata2"))
        return xlist->State.Tab->GUIData[1];
      else if(!strcasecmp(name, "guidata3"))
        return xlist->State.Tab->GUIData[2];
      else if(!strcasecmp(name, "guidata4"))
        return xlist->State.Tab->GUIData[3];
  }
  return NULL; // to do: return 1 if successful
}

int Spark_SetListPtr(xchat_plugin *ph, xchat_list *xlist, const char *name, void *newvalue) {
  switch(xlist->Type) {
    case LIST_CHANNELS:
      if(!strcasecmp(name, "guidata1"))
        xlist->State.Tab->GUIData[0] = newvalue;
      else if(!strcasecmp(name, "guidata2"))
        xlist->State.Tab->GUIData[1] = newvalue;
      else if(!strcasecmp(name, "guidata3"))
        xlist->State.Tab->GUIData[2] = newvalue;
      else if(!strcasecmp(name, "guidata4"))
        xlist->State.Tab->GUIData[3] = newvalue;
  }
  return 1; // to do: return 1 if successful
}

const char *xchat_list_str(xchat_plugin *ph, xchat_list *xlist, const char *name) {
  switch(xlist->Type) {
    ClientTab *Tab;
    ClientConnection *Connection;
    char Temp[CHANNELUSER_NAME_LEN+20];
    case LIST_CHANNELS:
      Tab = xlist->State.Tab;
      Connection = ConnectionForTab(Tab);
      if(!strcmp(name, "name") || !strcmp(name, "channel"))
        return Tab->Name;
      else if(!strcmp(name, "context"))
        return (const char*)Tab;
      else if(!strcmp(name, "network"))
        return ConnectionExtraInfo(Connection, Tab, "network");
      else if(!strcmp(name, "server"))
        return ConnectionExtraInfo(Connection, Tab, "server");
      else if(!strcmp(name, "channelkey"))
        return ConnectionExtraInfo(Connection, Tab, "channelkey");
      else if(!strcmp(name, "chantypes"))
        return ConnectionExtraInfo(Connection, Tab, "chantypes");
      else if(!strcmp(name, "nickmodes"))
        return ConnectionExtraInfo(Connection, Tab, "nickmodes");
      else if(!strcmp(name, "icon"))
        return Tab->Icon;
      return NULL;
    case LIST_USERS:
      Tab = xlist->State.Tab;
      Connection = ConnectionForTab(Tab);
      if(!strcmp(name, "nick") || !strcmp(name, "name"))
        return Tab->Users[xlist->Index].Name;
      else if(!strcmp(name, "host"))
        sprintf(Temp, "host %s", Tab->Users[xlist->Index].Name);
      else if(!strcmp(name, "realname"))
        sprintf(Temp, "realname %s", Tab->Users[xlist->Index].Name);
      else if(!strcmp(name, "prefix"))
        sprintf(Temp, "prefix %s", Tab->Users[xlist->Index].Name);
      else if(!strcmp(name, "account"))
        sprintf(Temp, "account %s", Tab->Users[xlist->Index].Name);
      if(*Temp)
        return ConnectionExtraInfo(Connection, Tab, Temp);
      return NULL;
    case LIST_CONFIG:
      if(!strcmp(name, "name"))
        return xlist->State.JSON->string;
      else if(!strcmp(name, "string"))
        return xlist->State.JSON->valuestring;
      return NULL;
    case LIST_ADDONS:
      if(!strcmp(name, "name"))
        return xlist->State.Addon->Name;
      else if(!strcmp(name, "author"))
        return xlist->State.Addon->Author;
      else if(!strcmp(name, "summary"))
        return xlist->State.Addon->Summary;
      else if(!strcmp(name, "version"))
        return xlist->State.Addon->Version;
      else if(!strcmp(name, "path"))
        return xlist->State.Addon->Path;
      else if(!strcmp(name, "cplugin"))
        return (const char*)xlist->State.Addon->CPlugin;
      return NULL;
  }
  return NULL;
}

int xchat_list_int(xchat_plugin *ph, xchat_list *xlist, const char *name) {
  ClientTab *Tab;
  ClientConnection *Connection;
  switch(xlist->Type) {
    case LIST_CHANNELS:
      Tab = xlist->State.Tab;
      Connection = ConnectionForTab(Tab);
      if(!strcmp(name, "flags")) {
        return Connection->Flags & 3; // connected and connecting
      } else if(!strcmp(name, "tabflags"))
          return Tab->Flags;
      else if(!strcmp(name, "id")) {
        if(Tab->Parent) return Tab->Parent->Id;
        return Tab->Id;
      } else if(!strcmp(name, "uid")) {
        return Tab->Id;
      } else if(!strcmp(name, "type")) {
        if(Tab->Flags & TAB_SERVER) return 1;
        if(Tab->Flags & TAB_CHANNEL) return 2;
        if(Tab->Flags & TAB_QUERY) return 3;
        return 0;
      } else if(!strcmp(name, "maxmodes")) {
        return strtol(ConnectionExtraInfo(Connection, Tab, "server"), NULL, 10);
      } else if(!strcmp(name, "users"))
        return Tab->NumUsers;
      return -1;
    case LIST_USERS:
      if(!strcmp(name, "away"))
        return (xlist->State.Tab->Users[xlist->Index].Flags & CHANNELUSER_AWAY) != 0;
      else if(!strcmp(name, "selected"))
        return (xlist->State.Tab->Users[xlist->Index].Flags & CHANNELUSER_SELECTED) != 0;
      else if(!strcmp(name, "lasttalk"))
        return xlist->State.Tab->Users[xlist->Index].LastTalk;
      return -1;
    case LIST_CONFIG:
      if(!strcmp(name, "int"))
        return xlist->State.JSON->valueint;
      if(!strcmp(name, "type")) {
        switch(xlist->State.JSON->type) {
          case cJSON_Number:
            return 1;
          case cJSON_String:
            return 2;
          case cJSON_Array:
          case cJSON_Object:
            return 3;
          case cJSON_False:
          case cJSON_True:
            return 4;
        }
      }
      return -1;
  }
  return -1;
}

time_t xchat_list_time(xchat_plugin *ph, xchat_list *xlist, const char *name) {
  return xchat_list_int(ph, xlist, name);
}

void *xchat_plugingui_add(xchat_plugin *ph, const char *filename,
const char *name, const char *desc, const char *version, char *reserved) {
  //adds a fake plugin to the list
  return NULL;
}

void xchat_plugingui_remove(xchat_plugin *ph, void *handle) {
  //removes a fake plugin from the list
}

int xchat_emit_print(xchat_plugin *ph, const char *event_name, ...) {
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "emit print");
  return 0;
}

char *xchat_gettext(xchat_plugin *ph, const char *msgid) {
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "gettext");
  return "";
}

void xchat_send_modes(xchat_plugin *ph, const char **targets, int ntargets,
int modes_per_line, char sign, char mode) {
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "send modes");
}

char *xchat_strip(xchat_plugin *ph, const char *str, int len, int flags) {
  char *Out = (char*)malloc(strlen(str)); // ignore len for now
  StripIRCText(Out, str, flags);
  return Out;
}

void xchat_free(xchat_plugin *ph, void *ptr) {
  free(ptr);
}

cJSON *GetPluginPref(xchat_plugin *ph) {
  ClientAddon *Addon = ph->Addon;
  cJSON *Get = cJSON_GetObjectItem(PluginPref, Addon->Name);
  if(!Get)
    cJSON_AddItemToObject(PluginPref, Addon->Name, Get=cJSON_CreateObject());
  return Get;
}

// later change the pluginpref functions to directly put a cJSON struct inside the addon
int xchat_pluginpref_set_str(xchat_plugin *ph, const char *var, const char *value) {
  cJSON *Pref = GetPluginPref(ph);
  cJSON *Find = cJSON_GetObjectItem(Pref, var);
  if(Find) {
    Find->valuestring = realloc(Find->valuestring, strlen(value)+1);
    strcpy(Find->valuestring, value);
  } else
    cJSON_AddStringToObject(Find, var, value);
  return 1;
}

int xchat_pluginpref_get_str(xchat_plugin *ph, const char *var, char *dest) {
  cJSON *Pref = GetPluginPref(ph);
  cJSON *Find = cJSON_GetObjectItem(Pref, var);
  if(Find) {
    if(Find->valuestring)
      strcpy(dest, Find->valuestring);
    else // docs explicitly say you can read an int as a string
      sprintf(dest, "%i", Find->valueint);
    return 1;
  }
  return 0;
}

int xchat_pluginpref_set_int(xchat_plugin *ph, const char *var, int value) {
  cJSON *Pref = GetPluginPref(ph);
  cJSON *Find = cJSON_GetObjectItem(Pref, var);
  if(Find)
    Find->valueint = value;
  else
    cJSON_AddNumberToObject(Find, var, value);
  return 1;
}

int xchat_pluginpref_get_int(xchat_plugin *ph, const char *var) {
  cJSON *Pref = GetPluginPref(ph);
  cJSON *Find = cJSON_GetObjectItem(Pref, var);
  if(Find)
    return Find->valueint;
  return -1;
}

int xchat_pluginpref_delete(xchat_plugin *ph, const char *var) {
  // not implemented yet
  return 0;
}

int xchat_pluginpref_list(xchat_plugin *ph, char *dest) {
  *dest = 0;
  cJSON *Pref = GetPluginPref(ph);
  while(Pref) {
    strcat(dest, Pref->string);
    strcat(dest, ",");
    Pref = Pref->next;
  }
  return 1;
}

int Spark_TextFileExists(const char *Name);
int Spark_SaveTextFile(const char *Name, const char *Contents);
char *Spark_LoadTextFile(const char *Name);

int SqX_AddMessage(const char *Message, ClientTab *Tab, time_t Time, int Flags);
int Spark_AddMessage(xchat_plugin *ph, const char *Message, time_t Time, int Flags) {
  SqX_AddMessage(Message, ph->Context, Time, Flags);
  return 0;
}

int Spark_AddMessageF(xchat_plugin *ph, const char *Message, time_t Time, int Flags, ...) {
  va_list args;
  char *buf;
  va_start (args, Flags);
  buf = strdup_vprintf(Message, args);
  va_end (args);
  int Msg = Spark_AddMessage(ph, buf, Time, Flags);
  free(buf);
  return Msg;
}

int Spark_TempMessage(xchat_plugin *ph, const char *Message) {
  return Spark_AddMessage(ph, Message, 0, CMF_TEMP_ALERT);
}

int Spark_TempMessageF(xchat_plugin *ph, const char *Message,...) {
  va_list args;
  char *buf;
  va_start (args, Message);
  buf = strdup_vprintf(Message, args);
  va_end (args);
  int Msg = Spark_TempMessage(ph, buf);
  free(buf);
  return Msg;
}

xchat_hook *Spark_AddCommandHook(xchat_plugin *ph, const char *CommandName,
int (*Function) (const char *Word[], const char *WordEol[], void *Extra), int Priority, const char *Help, const char *Syntax, void *Extra) {
  return NULL;
}

xchat_hook *Spark_AddEventHookWord(xchat_plugin *ph, const char *EventName,
int (*Function) (const char *Word[], void *Extra), int Priority, int Flags, int Need0, int Need1, void *Extra) {
  return NULL;
}

xchat_hook *Spark_AddEventHookJSON(xchat_plugin *ph, const char *EventName,
int (*Function) (void *JSON, void *Extra), int Priority, int Flags, int Need0, int Need1, void *Extra) {
  return NULL;
}

int Spark_StartEvent(xchat_plugin *ph, const char *TypeName, const char *EventInfo, int Flags) {
  xchat_context *Context = ph->Context;
  char Buffer[CONTEXT_BUF_SIZE];
  return StartEvent(TypeName, EventInfo, ContextForTab(Context, Buffer), FirstEventType, 0);
}

size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);

void Spark_CurlPost(void (*Function) (void *Data, int Size, void *Extra), void *Extra, const char *URL, const char *Form) {
  struct SqCurl *State = (struct SqCurl*)calloc(1,sizeof(struct SqCurl));
  if(!State) return;

  if(Form) {
    State->Form = (char*)malloc(strlen(Form)+1);
    strcpy(State->Form, Form);
  }

  State->Script = NULL;
  State->Data.Native.Function = Function;
  State->Data.Native.Extra = Extra;

  CURL *Curl = curl_easy_init();
  curl_easy_setopt(Curl, CURLOPT_URL, URL);
  curl_easy_setopt(Curl, CURLOPT_NOPROGRESS, 1);
  curl_easy_setopt(Curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(Curl, CURLOPT_WRITEDATA, State);
  curl_easy_setopt(Curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(Curl, CURLOPT_SSL_VERIFYHOST, 0L);
  if(State->Form)
    curl_easy_setopt(Curl, CURLOPT_POSTFIELDS, State->Form);
  curl_multi_add_handle(MultiCurl, Curl);
  curl_easy_setopt(Curl, CURLOPT_PRIVATE, State);
  SqCurlRunning = 1;
}

void Spark_CurlGet(void (*Function) (void *Data, int Size, void *Extra), void *Extra, const char *URL) {
  Spark_CurlPost(Function, Extra, URL, NULL);
}

extern int SockId;
int ConvertSocketFlags(char *TypeCopy);
void DeleteSocketById(int Socket);
int Spark_NetOpen(void (*Function) (int Socket, int Event, char *Data), const char *Host, const char *Type) {
  char TypeCopy[strlen(Type)+1];
  strcpy(TypeCopy, Type);

  SDL_LockMutex(LockSockets);
  struct SqSocket *Socket = (struct SqSocket*)calloc(1,sizeof(struct SqSocket));
  if(!Socket) return 0;
  strlcpy(Socket->Hostname, Host, sizeof(Socket->Hostname));
 
  Socket->Flags = ConvertSocketFlags(TypeCopy);
  Socket->Bio = NULL;
  Socket->Script = NULL;
  Socket->Function.Native = Function;
  Socket->Id = SockId;
  Socket->Socket = -1;
  Socket->Next = FirstSock;
  int BufferSize = 0x10000;
  Socket->Buffer = (char*)malloc(BufferSize);
  Socket->Buffer[0] = 0;
  Socket->BufferSize = BufferSize;
  if(FirstSock)
    FirstSock->Prev = Socket;
  FirstSock = Socket;
  IPC_WriteF(&EventToSocket, "O%i", SockId); // open socket

  SDL_UnlockMutex(LockSockets);
  return SockId++;
}

void Spark_NetSend(int Socket, const char *Text) {
  IPC_WriteF(&EventToSocket, "M%i %s", Socket, Text);
}

void Spark_NetClose(int Socket, int Reopen) {
  DeleteSocketById(Socket);
  // todo: reopen
}

int Spark_ListSkipTo(xchat_plugin *ph, xchat_list *List, const char *SkipTo) {
  do {
    if(!strcasecmp(xchat_list_str(ph, List, "name"),SkipTo))
      return 1;
  } while(xchat_list_next(ph, List));
  return 0;
}

void Spark_LockResource(int Resource) {
  if(Resource == 0)
    SDL_LockMutex(LockTabs);
}

void Spark_UnlockResource(int Resource) {
  if(Resource == 0)
    SDL_UnlockMutex(LockTabs);
}

void Spark_Delay(unsigned long int Delay) {
  SDL_Delay(Delay);
}

void Spark_PollGUIMessages(int (*Callback)(int Code, char *Data1, char *Data2)) {
//  SDL_LockMutex(LockMTR);
  SDL_Event e;
  while(SDL_PollEvent(&e) != 0) {
    if(e.type == SDL_QUIT)
      quit = 1;
    else if (e.type == MainThreadEvent) {
      int Return = Callback(e.user.code, e.user.data1, e.user.data2);
      if(Return == -1) continue;
      if(!(Return & MTR_SAVE1) && e.user.data1) free(e.user.data1);
      if(!(Return & MTR_SAVE2) && e.user.data2) free(e.user.data2);
    }
  }
//  SDL_UnlockMutex(LockMTR);
}

void Spark_DebugPrintf(const char *format, ...) {
  va_list vl;
  va_start(vl, format);
  SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, format, vl);
  va_end(vl);
}

sparkles_plugin BaseSparklesPlugin = {
  (void*)Sq_RegisterFunc,
  Spark_FindSymbol,
  strlcpy,
  memcasecmp,
  UnicodeLen,
  UnicodeForward,
  UnicodeBackward,
  StripIRCText,
  VisibleStrLen,
  Spark_SaveTextFile,
  Spark_LoadTextFile,
  Spark_TextFileExists,
  Spark_AddMessage,
  Spark_AddMessageF,
  Spark_TempMessage,
  Spark_TempMessageF,
  WildMatch,
  XChatTokenize,
  FilenameOnly,
  Spark_AddCommandHook,
  Spark_AddEventHookWord,
  Spark_AddEventHookJSON,
  Spark_StartEvent,
  PathIsSafe,
  GetConfigStr,
  GetConfigInt,
  SDL_GetClipboardText,
  SDL_SetClipboardText,
  Spark_CurlGet,
  Spark_CurlPost,
  Spark_ListSkipTo,
  Spark_NetOpen,
  Spark_NetSend,
  Spark_NetClose,
  Spark_LockResource,
  Spark_UnlockResource,
  Spark_PollGUIMessages,
  QueueEvent,
  Spark_SetListStr,
  Spark_SetListInt,
  Spark_ListPtr,
  Spark_SetListPtr,
  Spark_Delay,
  Spark_DebugPrintf,
  sq_pushstring,
  sq_pushfloat,
  sq_pushinteger,
  sq_pushbool,
  sq_pushnull,
  sq_getstring,
  sq_getfloat,
  sq_getinteger,
  sq_getbool,
  sq_gettop,
  sq_settop,
};

xchat_plugin BaseXChatPlugin = {
  xchat_hook_command,
  xchat_hook_server,
  xchat_hook_print,
  xchat_hook_timer,
  xchat_hook_fd,
  xchat_unhook,
  xchat_print,
  xchat_printf,
  xchat_command,
  xchat_commandf,
  xchat_nickcmp,
  xchat_set_context,
  xchat_find_context,
  xchat_get_context,
  xchat_get_info,
  xchat_get_prefs,
  xchat_list_get,
  xchat_list_free,
  xchat_list_fields,
  xchat_list_next,
  xchat_list_str,
  xchat_list_int,
  xchat_plugingui_add,
  xchat_plugingui_remove,
  xchat_emit_print,
  xchat_read_fd,
  xchat_list_time,
  xchat_gettext,
  xchat_send_modes,
  xchat_strip,
  xchat_free,
  xchat_pluginpref_set_str,
  xchat_pluginpref_get_str,
  xchat_pluginpref_set_int,
  xchat_pluginpref_get_int,
  xchat_pluginpref_delete,
  xchat_pluginpref_list
};
