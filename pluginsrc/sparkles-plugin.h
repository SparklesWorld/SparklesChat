/* You can distribute this header with your plugins for easy compilation */
/* Shamelessly derived from XChat's own plugin header */
#ifndef SPARKLES_PLUGIN_H
#define SPARKLES_PLUGIN_H

#include <time.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _sparkles_plugin sparkles_plugin;
typedef struct SQVM* SquirrelVM;

#ifndef _SQUIRREL_H_
typedef int SQInteger;
typedef unsigned int SQUnsignedInteger;
typedef float SQFloat;
typedef SQUnsignedInteger SQBool;
typedef char SQChar;
typedef SQInteger SQRESULT;
struct SQObject;
typedef SQObject HSQOBJECT;
#endif

struct _sparkles_plugin
{
  void (*Spark_RegisterFunc)(SquirrelVM v,int (*f)(SquirrelVM v),const char *fname, int ParamNum, const char *params);
  void *(*Spark_FindSymbol)(const char *SymbolName);
  void (*Spark_strlcpy)(char *Destination, const char *Source, int MaxLength);
  int  (*Spark_memcasecmp)(const char *Text1, const char *Text2, int Length);
  int  (*Spark_UnicodeLen)(const char *Text, const char *EndPoint);
  char *(*Spark_UnicodeForward)(char *Text, int CharsAhead);
  char *(*Spark_UnicodeBackward)(char *Text, int CharsAhead, char *First);
  char *(*Spark_StripIRCText)(char *Out, const char *Text, int Flags);
  int   (*Spark_VisibleStrLen)(const char *Text, int Flags);
  int   (*Spark_SaveTextFile)(const char *Name, const char *Contents);
  char *(*Spark_LoadTextFile)(const char *Name);
  int   (*Spark_TextFileExists)(const char *Name);
  int   (*Spark_AddMessage)(xchat_plugin *ph, const char *Message, time_t Time, int Flags);
  int   (*Spark_AddMessageF)(xchat_plugin *ph, const char *Message, time_t Time, int Flags,...);
  int   (*Spark_TempMessage)(xchat_plugin *ph, const char *Message);
  int   (*Spark_TempMessageF)(xchat_plugin *ph, const char *Message,...);
  int   (*Spark_WildMatch)(const char *TestMe, const char *Wild);
  int   (*Spark_XChatTokenize)(const char *Input, char *WordBuff, const char **Word, const char **WordEol, int WordSize, int Flags);
  const char *(*Spark_FilenameOnly)(const char *Path);
  xchat_hook *(*Spark_AddCommandHook)(xchat_plugin *ph, const char *CommandName, int (*Function) (const char *Word[], const char *WordEol[], void *Extra), int Priority, const char *Help, const char *Syntax, void *Extra);
  xchat_hook *(*Spark_AddEventHookWord)(xchat_plugin *ph, const char *EventName, int (*Function) (const char *Word[], void *Extra), int Priority, int Flags, int Need0, int Need1, void *Extra);
  xchat_hook *(*Spark_AddEventHookJSON)(xchat_plugin *ph, const char *EventName, int (*Function) (void *JSON, void *Extra), int Priority, int Flags, int Need0, int Need1, void *Extra);
  int   (*Spark_StartEvent)(xchat_plugin *ph, const char *TypeName, const char *EventInfo, int Flags);
  int   (*Spark_PathIsSafe)(const char *Path);
  const char *(*Spark_GetConfigStr)(const char *Default, const char *s,...);
  int   (*Spark_GetConfigInt)(int Default, const char *s,...);
  char *(*Spark_GetClipboard)();
  int   (*Spark_SetClipboard)(const char *New);
  void  (*Spark_CurlGet)(void (*Function) (void *Data, int Size, void *Extra), void *Extra, const char *URL);
  void  (*Spark_CurlPost)(void (*Function) (void *Data, int Size, void *Extra), void *Extra, const char *URL, const char *Form);
  int (*Spark_ListSkipTo)(xchat_plugin *ph, xchat_list *List, const char *SkipTo);
  int   (*Spark_NetOpen)(void (*Function) (int Socket, int Event, char *Data), const char *Host, const char *Type);
  void  (*Spark_NetSend)(int Socket, const char *Text);
  void  (*Spark_NetClose)(int Socket, int Reopen);
  void  (*Spark_LockResource)(int Resource);
  void  (*Spark_UnlockResource)(int Resource);
  void  (*Spark_PollGUIMessages)(int (*Callback)(int Code, char *Data1, char *Data2));
  void  (*Spark_QueueEvent)(const char *TypeName, const char *EventInfo, const char *EventContext, char EventType);
  int   (*Spark_SetListStr) (xchat_plugin *ph, xchat_list *xlist, const char *name, const char *newvalue);
  int   (*Spark_SetListInt) (xchat_plugin *ph, xchat_list *xlist, const char *name, int newvalue);
  void *(*Spark_ListPtr)(xchat_plugin *ph, xchat_list *xlist, const char *name);
  int   (*Spark_SetListPtr)(xchat_plugin *ph, xchat_list *xlist, const char *name, void *newvalue);
  void  (*Spark_Delay)(long unsigned time);
  void  (*Spark_DebugPrintf)(const char *format, ...);

// Squirrel stuff
  void  (*Sq_PushString)(SquirrelVM v, const SQChar *s, SQInteger len);
  void  (*Sq_PushFloat)(SquirrelVM v, SQFloat f);
  void  (*Sq_PushInteger)(SquirrelVM v, SQInteger n);
  void  (*Sq_PushBool)(SquirrelVM v, SQBool b);
  void  (*Sq_PushNull)(SquirrelVM v);
  SQRESULT (*Sq_GetString)(SquirrelVM v,SQInteger idx,const SQChar **c);
  SQRESULT (*Sq_GetFloat)(SquirrelVM v,SQInteger idx,SQFloat *f);
  SQRESULT (*Sq_GetInteger)(SquirrelVM v,SQInteger idx,SQInteger *i);
  SQRESULT (*Sq_GetBool)(SquirrelVM v,SQInteger idx,SQBool *p);
  SQInteger (*Sq_GetTop)(SquirrelVM v);
  void (*Sq_SetTop)(SquirrelVM v,SQInteger newtop);
};

#ifndef PLUGIN_C
void Spark_RegisterFunc(SquirrelVM v,int (*f)(void *v),const char *fname, int ParamNum, const char *params);
void *Spark_FindSymbol(const char *SymbolName);
void  Spark_strlcpy(char *Destination, const char *Source, int MaxLength);
int   Spark_memcasecmp(const char *Text1, const char *Text2, int Length);
int   Spark_UnicodeLen(const char *Text, const char *EndPoint);
char *Spark_UnicodeForward(char *Text, int CharsAhead);
char *Spark_UnicodeBackward(char *Text, int CharsAhead, char *First);
char *Spark_StripIRCText(char *Out, const char *Text, int Flags);
int   Spark_VisibleStrLen(const char *Text, int Flags);
int   Spark_SaveTextFile(const char *Name, const char *Contents);
char *Spark_LoadTextFile(const char *Name);
int   Spark_TextFileExists(const char *Name);
int   Spark_AddMessage(xchat_plugin *ph, const char *Message, time_t Time, int Flags);
int   Spark_AddMessageF(xchat_plugin *ph, const char *Message, time_t Time, int Flags,...);
int   Spark_TempMessage(xchat_plugin *ph, const char *Message);
int   Spark_TempMessageF(xchat_plugin *ph, const char *Message,...);
int   Spark_WildMatch(const char *TestMe, const char *Wild);
int   Spark_XChatTokenize(const char *Input, char *WordBuff, const char **Word, const char **WordEol, int WordSize, int Flags);
const char *Spark_FilenameOnly(const char *Path);
xchat_hook *Spark_AddCommandHook(xchat_plugin *ph, const char *CommandName, int (*Function) (const char *Word[], const char *WordEol[], void *Extra), int Priority, const char *Help, const char *Syntax, void *Extra);
xchat_hook *Spark_AddEventHookWord(xchat_plugin *ph, const char *EventName, int (*Function) (const char *Word[], void *Extra), int Priority, int Flags, int Need0, int Need1, void *Extra);
xchat_hook *Spark_AddEventHookJSON(xchat_plugin *ph, const char *EventName, int (*Function) (void *JSON, void *Extra), int Priority, int Flags, int Need0, int Need1, void *Extra);
int   Spark_StartEvent(xchat_plugin *ph, const char *TypeName, const char *EventInfo, int Flags);
int   Spark_PathIsSafe(const char *Path);
const char *Spark_GetConfigStr(const char *Default, const char *s,...);
int   Spark_GetConfigInt(int Default, const char *s,...);
char *Spark_GetClipboard();
int   Spark_SetClipboard(const char *New);
void  Spark_CurlGet(void (*Function) (void *Data, int Size, void *Extra), void *Extra, const char *URL);
void  Spark_CurlPost(void (*Function) (void *Data, int Size, void *Extra), void *Extra, const char *URL, const char *Form);
int   Spark_ListSkipTo(xchat_plugin *ph, xchat_list *List, const char *SkipTo);
int   Spark_NetOpen(void (*Function) (int Socket, int Event, char *Data), const char *Host, const char *Type);
void  Spark_NetSend(int Socket, const char *Text);
void  Spark_NetClose(int Socket, int Reopen);
void  Spark_LockResource(int Resource);
void  Spark_UnlockResource(int Resource);
void  Spark_PollGUIMessages(int (*Callback)(int Code, char *Data1, char *Data2));
void  Spark_QueueEvent(const char *TypeName, const char *EventInfo, const char *EventContext, char EventType);
int   Spark_SetListStr(xchat_plugin *ph, xchat_list *xlist, const char *name, const char *newvalue);
int   Spark_SetListInt(xchat_plugin *ph, xchat_list *xlist, const char *name, int newvalue);
void *Spark_ListPtr(xchat_plugin *ph, xchat_list *xlist, const char *name);
int   Spark_SetListPtr(xchat_plugin *ph, xchat_list *xlist, const char *name, void *newvalue);
void  Spark_Delay(long unsigned time);
void  Spark_DebugPrintf(const char *format, ...);

// Squirrel stuff
void  Sq_PushString(SquirrelVM v,const SQChar *s,SQInteger len);
void  Sq_PushFloat(SquirrelVM v,SQFloat f);
void  Sq_PushInteger(SquirrelVM v,SQInteger n);
void  Sq_PushBool(SquirrelVM v,SQBool b);
void  Sq_PushNull(SquirrelVM v);
SQRESULT Sq_GetString(SquirrelVM v,SQInteger idx,const SQChar **c);
SQRESULT Sq_GetInteger(SquirrelVM v,SQInteger idx,SQInteger *i);
SQRESULT Sq_GetFloat(SquirrelVM v,SQInteger idx,SQFloat *f);
SQRESULT Sq_GetBool(SquirrelVM v,SQInteger idx,SQBool *p);
SQInteger Sq_GetTop(SquirrelVM v);
void Sq_SetTop(SquirrelVM v,SQInteger newtop);
#endif

#if !defined(PLUGIN_C)
#ifndef SPARKLES_PLUGIN_HANDLE
#define SPARKLES_PLUGIN_HANDLE (sph)
#endif
#define Spark_RegisterFunc ((SPARKLES_PLUGIN_HANDLE)->Spark_RegisterFunc)
#define Spark_FindSymbol ((SPARKLES_PLUGIN_HANDLE)->Spark_FindSymbol)
#define Spark_RegisterFunc ((SPARKLES_PLUGIN_HANDLE)->Spark_RegisterFunc)
#define Spark_strlcpy ((SPARKLES_PLUGIN_HANDLE)->Spark_strlcpy)
#define Spark_memcasecmp ((SPARKLES_PLUGIN_HANDLE)->Spark_memcasecmp)
#define Spark_UnicodeLen ((SPARKLES_PLUGIN_HANDLE)->Spark_UnicodeLen)
#define Spark_UnicodeForward ((SPARKLES_PLUGIN_HANDLE)->Spark_UnicodeForward)
#define Spark_UnicodeBackward ((SPARKLES_PLUGIN_HANDLE)->Spark_UnicodeBackward)
#define Spark_StripIRCText ((SPARKLES_PLUGIN_HANDLE)->Spark_StripIRCText)
#define Spark_VisibleStrLen ((SPARKLES_PLUGIN_HANDLE)->Spark_VisibleStrLen)
#define Spark_SaveTextFile ((SPARKLES_PLUGIN_HANDLE)->Spark_SaveTextFile)
#define Spark_LoadTextFile ((SPARKLES_PLUGIN_HANDLE)->Spark_LoadTextFile)
#define Spark_TextFileExists ((SPARKLES_PLUGIN_HANDLE)->Spark_TextFileExists)
#define Spark_AddMessage ((SPARKLES_PLUGIN_HANDLE)->Spark_AddMessage)
#define Spark_AddMessageF ((SPARKLES_PLUGIN_HANDLE)->Spark_AddMessageF)
#define Spark_TempMessage ((SPARKLES_PLUGIN_HANDLE)->Spark_TempMessage)
#define Spark_TempMessageF ((SPARKLES_PLUGIN_HANDLE)->Spark_TempMessageF)
#define Spark_WildMatch ((SPARKLES_PLUGIN_HANDLE)->Spark_WildMatch)
#define Spark_XChatTokenize ((SPARKLES_PLUGIN_HANDLE)->Spark_XChatTokenize)
#define Spark_FilenameOnly ((SPARKLES_PLUGIN_HANDLE)->Spark_FilenameOnly)
#define Spark_AddCommandHook ((SPARKLES_PLUGIN_HANDLE)->Spark_AddCommandHook)
#define Spark_AddEventHookWord ((SPARKLES_PLUGIN_HANDLE)->Spark_AddEventHookWord)
#define Spark_AddEventHookJSON ((SPARKLES_PLUGIN_HANDLE)->Spark_AddEventHookJSON)
#define Spark_StartEvent ((SPARKLES_PLUGIN_HANDLE)->Spark_StartEvent)
#define Spark_PathIsSafe ((SPARKLES_PLUGIN_HANDLE)->Spark_PathIsSafe)
#define Spark_GetConfigStr ((SPARKLES_PLUGIN_HANDLE)->Spark_GetConfigStr)
#define Spark_GetConfigInt ((SPARKLES_PLUGIN_HANDLE)->Spark_GetConfigInt)
#define Spark_GetClipboard ((SPARKLES_PLUGIN_HANDLE)->Spark_GetClipboard)
#define Spark_SetClipboard ((SPARKLES_PLUGIN_HANDLE)->Spark_SetClipboard)
#define Spark_CurlGet ((SPARKLES_PLUGIN_HANDLE)->Spark_CurlGet)
#define Spark_CurlPost ((SPARKLES_PLUGIN_HANDLE)->Spark_CurlPost)
#define Spark_ListSkipTo ((SPARKLES_PLUGIN_HANDLE)->Spark_ListSkipTo)
#define Spark_NetOpen ((SPARKLES_PLUGIN_HANDLE)->Spark_NetOpen)
#define Spark_NetSend ((SPARKLES_PLUGIN_HANDLE)->Spark_NetSend)
#define Spark_NetClose ((SPARKLES_PLUGIN_HANDLE)->Spark_NetClose)
#define Spark_LockResource ((SPARKLES_PLUGIN_HANDLE)->Spark_LockResource)
#define Spark_UnlockResource ((SPARKLES_PLUGIN_HANDLE)->Spark_UnlockResource)
#define Spark_PollGUIMessages ((SPARKLES_PLUGIN_HANDLE)->Spark_PollGUIMessages)
#define Spark_QueueEvent ((SPARKLES_PLUGIN_HANDLE)->Spark_QueueEvent)
#define Spark_SetListStr ((SPARKLES_PLUGIN_HANDLE)->Spark_SetListStr)
#define Spark_SetListInt ((SPARKLES_PLUGIN_HANDLE)->Spark_SetListInt)
#define Spark_ListPtr ((SPARKLES_PLUGIN_HANDLE)->Spark_ListPtr)
#define Spark_SetListPtr ((SPARKLES_PLUGIN_HANDLE)->Spark_SetListPtr)
#define Spark_Delay ((SPARKLES_PLUGIN_HANDLE)->Spark_Delay)
#define Spark_DebugPrintf ((SPARKLES_PLUGIN_HANDLE)->Spark_DebugPrintf)

#define Sq_PushString ((SPARKLES_PLUGIN_HANDLE)->Sq_PushString)
#define Sq_PushFloat ((SPARKLES_PLUGIN_HANDLE)->Sq_PushFloat)
#define Sq_PushInteger ((SPARKLES_PLUGIN_HANDLE)->Sq_PushInteger)
#define Sq_PushBool ((SPARKLES_PLUGIN_HANDLE)->Sq_PushBool)
#define Sq_PushNull ((SPARKLES_PLUGIN_HANDLE)->Sq_PushNull)
#define Sq_GetString ((SPARKLES_PLUGIN_HANDLE)->Sq_GetString)
#define Sq_GetFloat ((SPARKLES_PLUGIN_HANDLE)->Sq_GetFloat)
#define Sq_GetInteger ((SPARKLES_PLUGIN_HANDLE)->Sq_GetInteger)
#define Sq_GetBool ((SPARKLES_PLUGIN_HANDLE)->Sq_GetBool)
#define Sq_GetTop ((SPARKLES_PLUGIN_HANDLE)->Sq_GetTop)
#define Sq_SetTop ((SPARKLES_PLUGIN_HANDLE)->Sq_SetTop)
#endif

#ifdef __cplusplus
}
#endif
#endif