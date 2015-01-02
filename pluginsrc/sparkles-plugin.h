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

struct _sparkles_plugin
{
  void (*Spark_RegisterFunc)(SquirrelVM v,int (*f)(void *v),const char *fname, int ParamNum, const char *params);
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
  char *(*Spark_StringClone)(const char *String);
  int   (*Spark_NetOpen)(void (*Function) (int Socket, int Event, char *Data), const char *Host, const char *Type);
  void  (*Spark_NetSend)(int Socket, const char *Text);
  void  (*Spark_NetClose)(int Socket, int Reopen);
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
char *Spark_StringClone(const char *CloneMe);
int   Spark_NetOpen(void (*Function) (int Socket, int Event, char *Data), const char *Host, const char *Type);
void  Spark_NetSend(int Socket, const char *Text);
void  Spark_NetClose(int Socket, int Reopen);
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
#define Spark_StringClone ((SPARKLES_PLUGIN_HANDLE)->Spark_StringClone)
#define Spark_NetOpen ((SPARKLES_PLUGIN_HANDLE)->Spark_NetOpen)
#define Spark_NetSend ((SPARKLES_PLUGIN_HANDLE)->Spark_NetSend)
#define Spark_NetClose ((SPARKLES_PLUGIN_HANDLE)->Spark_NetClose)
#endif

#ifdef __cplusplus
}
#endif
#endif