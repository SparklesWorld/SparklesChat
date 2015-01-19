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
#ifndef SPARKLES_HEADER
#define SPARKLES_HEADER
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <squirrel.h>
#include <sqstdio.h>
#include <sqstdstring.h>
#include <sqstdmath.h>
#include <sqstdaux.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#ifdef _WIN32
#include <windows.h>
#include "fnmatch.h"
#include <io.h>
#else
#include <fnmatch.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <curl/curl.h>
#include "cJSON.h"
typedef struct ClientTab _xchat_context;
#include "../pluginsrc/xchat-plugin.h"
#include "../pluginsrc/sparkles-plugin.h"
#define CLIENT_NAME "Sparkles"
#define CLIENT_VERSION "0.01"
#define CONTEXT_BUF_SIZE 90
#define NUM_GUISTATES 10

/* enum definitions */
enum ContextTypes {
  CONTEXT_NONE,
  CONTEXT_NETWORK,
  CONTEXT_CHANNEL,
  CONTEXT_USER,
};

enum Priorities {
  PRI_LOWEST,
  PRI_LOWER,
  PRI_LOW,
  PRI_NORMAL,
  PRI_HIGH,
  PRI_HIGHER,
  PRI_HIGHEST
};

enum EventFlags {
  EF_ALREADY_HANDLED = 1, // still present, but don't handle by anything else
  EF_DONT_SHOW = 2, // still log, but don't show event
};

enum EventReturnValue {
  ER_NORMAL,    // don't change anything
  ER_HANDLED,   // don't delete, but mark already handled
  ER_DELETE,    // delete event, stop anything at all from reacting to it
  ER_BADSYNTAX, // bad command syntax
};

// fake pipes
typedef struct IPC_Message {
  SDL_atomic_t Used;
  int Id;
  char *Text;
} IPC_Message;

typedef struct IPC_Queue {
  int Size;
  SDL_atomic_t MakeId;
  SDL_atomic_t UseId;
  SDL_mutex *Mutex;
  SDL_sem *Semaphore;
  IPC_Message *Queue;
  SDL_cond *Condition;
} IPC_Queue;

#define IPC_IN  0
#define IPC_OUT 1
int IPC_New(IPC_Queue *Out[], int Size, int Queues);
void IPC_Free(IPC_Queue *Holder[], int Queues);
void IPC_Write(IPC_Queue *Queue, const char *Text);
void IPC_WriteF(IPC_Queue *Queue, const char *format, ...);
char *IPC_Read(IPC_Queue *Queue, int Timeout);
void QueueEvent(const char *TypeName, const char *EventInfo, const char *EventContext, char EventType);

enum FontVersion {
  FONT_NORMAL,
  FONT_BOLD,
  FONT_ITALIC,
  FONT_BOLD_ITALIC,
};

typedef struct FontSet {
  TTF_Font *Font[4];
  int Width, Height;
} FontSet;

typedef struct RenderTextMode {
  int MaxWidth;
  int LineVSpace;
  int MaxHeight;
  int Flags;
  int DrawnHeight;

  char *AlreadyStripped;
  int TestX, TestY;
  char *TestResult;
  int RestResultLimit;
} RenderTextMode;
#define RENDERMODE_RIGHT_JUSTIFY 1 /* right justify the text */
#define RENDERMODE_TEST_XY       2 /* do test at X,Y in the rendered text */

struct ClientTab;

typedef struct ClientAddon { // used for protocols too, and plugins
  char Name[32];
  char Author[32];
  char Summary[100];
  char Version[32];
  char Path[260]; // path to the script
  HSQUIRRELVM Script;

  void *CPlugin;      // if non-NULL, is a C plugin. SDL_LoadObject() result.
  int Flags;
  sparkles_plugin *SparklesPlugin;
  xchat_plugin *XChatPlugin;

  struct ClientAddon *Prev, *Next;
} ClientAddon;

typedef struct ClientConnection {
  HSQUIRRELVM Script;
  int Flags;
  char YourName[80];
  char InputStatus[256];
  char *Ticket;
  int Socket;
} ClientConnection;
#define CON_CONNECTED  1
#define CON_CONNECTING 2

typedef struct ClientMessage {
  time_t Time;
  unsigned short Flags;
//  unsigned short DrawHeight;
  unsigned long Id;
  char Left[64];
//  char *Right 
  char Right[512];
  char *RightExtend;
} ClientMessage;

#define CMF_MSGC_0     0  /* default */
#define CMF_MSGC_1     1  /* people talking */
#define CMF_MSGC_2     2  /* entering/leaving */
#define CMF_MSGC_3     3  /* misc non-chat messages like modes */
#define CMF_MSGC_4     4  /* advertisements */
#define CMF_MSGC_5     5  /* */
#define CMF_MSGC_6     6  /* */
#define CMF_MSGC_7     7  /* */
#define CMF_MSGC_MASK  7  /* message class mask */
#define CMF_HIDDEN     8  /* hidden for any reason */
#define CMF_IGNORED    16 /* message source is ignored */
#define CMF_SCROLLBACK 32 /* loaded from scrollback */
#define CMF_TEMP_ALERT 64 /* message isn't logged and can be cleared away easily*/
#define CMF_NO_LOGGING 128 /* don't log this message, but display it still */
#define CMF_LOG_ONLY   256 /* only log this message, don't display */

#define ICONLIST_SIZE 8
typedef struct ListEntryIcons {
  char Len;
  char IconSet[ICONLIST_SIZE];
  char IconX[ICONLIST_SIZE];
  char IconY[ICONLIST_SIZE];
} ListEntryIcons;

// use realloc to resize the list if needed
#define CHANNELUSER_NAME_LEN 64
typedef struct ChannelUser {
  char Name[CHANNELUSER_NAME_LEN];
  signed short Rank;
  unsigned short Flags;
  ListEntryIcons Icons;
  time_t LastTalk;
} ChannelUser;
#define CHANNELUSER_AWAY 1
#define CHANNELUSER_SELECTED 2

#define INPUTBOX_SIZE 8192
#define MESSAGE_SIZE 512
#define INPUT_HISTORY_LEN 16
typedef struct ClientTab {
  char Name[64]; // name of tab to display
  char Icon[200]; // probably a url
  ListEntryIcons IconList;
  int Flags, Id;

  FILE *LogFile; // file to log to
  ClientConnection *Connection;
  HSQUIRRELVM Script;

  int NumMessages;         // number of messages actually in the buffer
  int MessageBufferSize;   // max number of messages the buffer can hold
  ClientMessage *Messages; // list of messages

  int NumUsers, UserBufferSize; // number of current users, size of Users[]
  // use a config item to set steps for it to grow/shrink
  ChannelUser *Users;

  char InputHistory[MESSAGE_SIZE*INPUT_HISTORY_LEN];
  char Inputbox[INPUTBOX_SIZE];
  int UndrawnLines;
  struct ClientTab *Parent, *Child, *Prev, *Next;
} ClientTab;

#define TAB_COLOR_NORM   0 /* normal channel color */
#define TAB_COLOR_TALK   1 /* someone talked */
#define TAB_COLOR_DATA   2 /* other data, not talking*/
#define TAB_COLOR_NAME   3 /* name said */
#define TAB_COLOR_MASK   3 /* mask to only get colors */
#define TAB_CHANNEL      4 /* is a channel */
#define TAB_QUERY        8 /* is a query */
#define TAB_SERVER      16 /* is a server */
#define TAB_NOTINCHAN   32 /* user isn't in the channel, whether through being kicked or having parted */
#define TAB_NOLOGGING   64 /* don't log this tab */
#define TAB_HIDDEN     128 /* hidden, detached tab */
#define TAB_IS_TYPING  256 /* user is typing */ 
#define TAB_HAS_TYPED  512 /* has stuff typed */
struct _xchat_hook;

typedef struct EventHook {
  unsigned long Flags;
  unsigned long HookId;
  union {
    struct {
      unsigned long EventFlagsNeed0; // these flags need to be 0 to trigger hook
      unsigned long EventFlagsNeed1; // these flags need to be 1 to trigger hook
    } FlagsNeed;
    struct {
      Uint32 Timeout;
      HSQOBJECT Extra;
    } Timer;
  } HookInfo;
  HSQUIRRELVM Script;
  union {
    HSQOBJECT Squirrel;
    void *Native;
  } Function;
  struct _xchat_hook *XChatHook;
  struct EventHook *Prev, *Next;
} EventHook;
#define EHOOK_PROTOCOL_COMMAND 1     /* tab's script must match command's script */
#define EHOOK_COMMAND_CALLBACK 2     /* int a(Word[], WordEol[], Extra) */
#define EHOOK_XCHAT_EVENT_CALLBACK 4 /* int a(Word[], Extra) */
#define EHOOK_SPARK_JSON_CALLBACK 8  /* int a(JSON, Extra) */

typedef struct EventType {
// todo later: keep track of the number of hooks
  char Type[64];
  EventHook *Hooks[PRI_HIGHEST+1];
  struct EventType *Prev, *Next;
} EventType;

typedef struct CommandHelp {
  char *HelpText;
  char *Syntax;
} CommandHelp;

// dialog stuff
struct GUIDialog;
struct GUIState;
typedef int (*DIALOG_PROC)(int msg, struct GUIDialog *d, struct GUIState *s);

typedef struct GUIDialog {
   DIALOG_PROC proc;
   int x, y, w, h;               /* position and size of the object */
   int fg, bg;                   /* foreground and background colors */
   int flags;                    /* flags about the object state */
   int d1, d2;                   /* any data the object might require */
   void *dp, *dp2, *dp3;         /* pointers to more object data */
   union {
     void *Window;
     void *dp4;
   } Extra;
} GUIDialog;

typedef struct GUIState {
  int Flags, X,Y, WinWidth, WinHeight;
  SDL_Renderer *Renderer;
  void (*UpdateFunc)(struct GUIState *s);
  FontSet *DefaultFont;
  GUIDialog *Dialog;
  SDL_Window *Window;

  GUIDialog *PriorityClickDialog;
  GUIDialog *DragDialog;
  const char *MessageText; // text sent to a widget, like for a key press
  union {
    SDL_Keysym *SDL; // key sent to a widget 
    int Curses;
  } MessageKey;
  char TempCmdTarget[CONTEXT_BUF_SIZE]; // for popups and such
} GUIState;

enum GSFFlags {
  GSF_NEED_INITIALIZE = 1, /* needs initialize messages sent */
  GSF_NEED_REDRAW = 2,     /* needs draw messages sent */
  GSF_NEED_PRESENT = 4,    /* needs SDL_RenderPresent() */
  GSF_NEED_UPDATE = 8,     /* needs updated to fit the window */
};

struct TextEditInfo {
  int   ScrollChars,  CursorChars,  SelectChars;
  char *ScrollBytes, *CursorBytes, *SelectBytes;
  int   HasSelect;
};
#define MAX_MENU_LEVELS 5
typedef struct MenuState {
  int CurLevel;   // current menu level
  int NumOptions; // number of options for the current menu item
  int X, Y, CurHilight;
  unsigned int Ticks;
  cJSON *Menus[MAX_MENU_LEVELS];
  const char *Picked[MAX_MENU_LEVELS];
  char Include[70];
  char *Target;
} MenuState;

enum SysCursorId {
  SYSCURSOR_NORMAL,
  SYSCURSOR_TEXT,
  SYSCURSOR_SIZE_NS,
  SYSCURSOR_SIZE_EW,
  SYSCURSOR_SIZE_ALL,
  SYSCURSOR_LINK,
  SYSCURSOR_DISABLE,
  SYSCURSOR_MAX
};
extern SDL_Cursor *SystemCursors[SYSCURSOR_MAX];

/* bits for the flags field */
#define D_EXIT          1        /* object makes the dialog exit */
#define D_SELECTED      2        /* object is selected */
#define D_GOTFOCUS      4        /* object has the input focus */
#define D_GOTMOUSE      8        /* mouse is on top of object (unused) */
#define D_HIDDEN        16       /* object is not visible */
#define D_DISABLED      32       /* object is visible but inactive */
#define D_DIRTY         64       /* object needs to be redrawn */
#define D_KEYWATCH      128      /* object wants MSG_KEY even without focus */
#define D_WANTCLICKAWAY 256      /* object will get MSG_CLICKAWAY when clicking on something else */

/* return values for the dialog procedures */
#define D_O_K           0        /* normal exit status */
#define D_CLOSE         1        /* request to close the dialog */
#define D_REDRAW        2        /* request to redraw the dialog */
#define D_REDRAWME      4        /* request to redraw this object */
#define D_WANTFOCUS     8        /* this object wants the input focus */
#define D_CAN_DRAG      16       /* click can be extended into a drag */
#define D_REDRAW_ALL    32       /* request to redraw all active dialogs */
#define D_DONTWANTMOUSE 64       /* this object does not want mouse focus */

/* messages for the dialog procedures */
#define MSG_START       1        /* start the dialog, initialise */
#define MSG_END         2        /* dialog is finished - cleanup */
#define MSG_DRAW        3        /* draw the object */
#define MSG_CLICK       4        /* mouse click on the object */
#define MSG_DCLICK      5        /* double click on the object */
#define MSG_KEY         6        /* key press, read MessageKey */
#define MSG_CHAR        7        /* text input, read MessageText */
#define MSG_DRAG        8        /* object is being dragged */
#define MSG_DRAGDROPPED 9        /* cursor dropped after dragging */
#define MSG_WANTFOCUS   10       /* does object want the input focus? */
#define MSG_GOTFOCUS    11       /* got the input focus (not needed) */
#define MSG_LOSTFOCUS   12       /* lost the input focus */
#define MSG_GOTMOUSE    13       /* mouse on top of object */
#define MSG_LOSTMOUSE   14       /* mouse moved away from object */
#define MSG_IDLE        15       /* update any background stuff */
#define MSG_RADIO       16       /* clear radio buttons */
#define MSG_WHEEL       17       /* mouse wheel moved */
#define MSG_LPRESS      18       /* mouse left button pressed */
#define MSG_LRELEASE    19       /* mouse left button released */
#define MSG_MPRESS      20       /* mouse middle button pressed */
#define MSG_MRELEASE    21       /* mouse middle button released */
#define MSG_RPRESS      22       /* mouse right button pressed */
#define MSG_RRELEASE    23       /* mouse right button released */
#define MSG_WANTMOUSE   24       /* does object want the mouse? */

#define MSG_ACTIVATE    25       /* activate menus and such */
#define MSG_CLICKAWAY   26       /* clicked on something else */
#define MSG_MOUSEMOVE   27       /* given to the PriorityClickDialog */
#define MSG_WIN_RESIZE  28       /* window was resized */

#define MSG_INFO_ADDED  29       /* info was added */

#define TOKENIZE_MULTI_WORD  1   /* "a b c" is a single word */
#define TOKENIZE_EMPTY_FIRST 2   /* for XChat compatibility */

char *strdup_vprintf(const char* format, va_list ap);
void strlcpy(char *Destination, const char *Source, int MaxLength);
int memcasecmp(const char *Text1, const char *Text2, int Length);
int UnicodeLen(const char *Text, const char *EndPoint);
char *UnicodeForward(char *Text, int CharsAhead);
char *UnicodeBackward(char *Text, int CharsAhead, char *First);
char *UnicodeWordStep(char *Text, int Direction, char *Start);
char *StripIRCText(char *Out, const char *Text, int Flags);
int VisibleStrLen(const char *Text, int Flags);
void rectfill(SDL_Renderer *Bmp, int X1, int Y1, int X2, int Y2);
void sblit(SDL_Surface* SrcBmp, SDL_Surface* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height);
void blit(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height);
void blitz(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height, int Width2, int Height2);
void blitfull(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int DestX, int DestY);
void SDL_MessageBox(int Type, const char *Title, SDL_Window *Window, const char *fmt, ...);
SDL_Surface *SDL_LoadImage(const char *FileName, int Flags);
SDL_Texture *LoadTexture(const char *FileName, int Flags);
void Free_FontSet(FontSet *Fonts);
int RenderText(SDL_Renderer *Renderer, FontSet *Font, int X, int Y, RenderTextMode *Mode, const char *Text);
int RenderSimpleText(SDL_Renderer *Renderer, FontSet *Font, int Flags, int X, int Y, const char *Text);
int Load_FontSet(FontSet *Fonts, int Size, const char *Font2, const char *Font1, const char *Font3, const char *Font4);
void AutoloadDirectory(const char *Directory, const char *Filetype, void (*Handler)(const char *Path));
const char *FilenameOnly(const char *Path);
ClientAddon *LoadAddon(const char *Path, ClientAddon **FirstAddon);
ClientAddon *UnloadAddon(ClientAddon *Addon, ClientAddon **FirstAddon, int Reload);
ClientAddon *FindAddon(const char *Name, ClientAddon **FirstAddon);
cJSON *cJSON_Search(cJSON *Root, char *SearchString);
int AddEventHook(EventType **List, HSQUIRRELVM v, const char *EventName, HSQOBJECT Handler, int Priority, int Flags, int Need0, int Need1);
void DelEventHookId(EventType **List, const char *EventName, int HookId);
void DelEventHookInList(EventHook **List, EventHook *Hook);
int WildMatch(const char *TestMe, const char *Wild);
cJSON *cJSON_Load(const char *Filename);
int StartEvent(const char *TypeName, const char *EventInfo, const char *EventContext, EventType *FirstType, int Flags);
int NativeCommand(const char *Command, const char *Args, const char *Context);
int XChatTokenize(const char *Input, char *WordBuff, const char **Word, const char **WordEol, int WordSize, int Flags);
int AsyncExec(const char *Command);
void AutoloadDirectory(const char *Directory, const char *Filetype, void (*Handler)(const char *Path));
void AutoloadAddon(const char *Filename);
void URLOpen(const char *URL);
void GUI_SetCursor(int CursorNum);
int PathIsSafe(const char *Path);
int MakeDirectory(const char *Path);
ClientTab *FindTab(const char *Context);
const char *GetConfigStr(const char *Default, const char *s,...);
int GetConfigInt(int Default, const char *s,...);
GUIDialog *FindDialogWithProc(const char *Context, GUIState *InState, GUIState **State, DIALOG_PROC Proc);
char *ContextForTab(ClientTab *Tab, char *Buffer);
int IsInsideRect(int X1, int Y1, int X2, int Y2, int W, int H);
int SimulateWordWrap(FontSet *Fonts, RenderTextMode *Mode, const char *Text);
__attribute__ ((hot)) const char *ConvertBBCode(char *Output, const char *Input, int Flags);
ClientConnection *ConnectionForTab(ClientTab *Tab);
ClientAddon *AddonForScript(HSQUIRRELVM v);
char *StringClone(const char *CloneMe);
ClientTab *GetFocusedTab();
void TextInterpolate(char *Out, const char *In, char Prefix, const char *ReplaceThis, const char *ReplaceWith[]);
char *FindCloserPointer(char *A, char *B);
int CreateDirectoriesForPath(const char *Folders);
void MainThreadRequest(int Code, void *Data1, void *Data2);
void RunSquirrelMisc();
void InitSock();
void EndSock();

int InitMinimalGUI();
void RunMinimalGUI();
void EndMinimalGUI();

int InitTextGUI();
void RunTextGUI();
void EndTextGUI();
#define ZeroStruct(x) memset(&x, sizeof(x), 0);

// Squirrel
int Sq_DoFile(HSQUIRRELVM VM, const char *File);
HSQUIRRELVM Sq_Open(const char *File);
void Sq_Close(HSQUIRRELVM v);

// dialog widgets
int RunWidget(int msg,struct GUIDialog *d, GUIState *s);
int Widget_ScreenClear(int msg,struct GUIDialog *d, GUIState *s);
int Widget_StaticText(int msg,struct GUIDialog *d, GUIState *s);
int Widget_TextEdit(int msg,struct GUIDialog *d, GUIState *s);
int Widget_ChatEdit(int msg,struct GUIDialog *d, GUIState *s);
int Widget_StaticImage(int msg,struct GUIDialog *d, GUIState *s);
int Widget_MenuButton(int msg,struct GUIDialog *d, GUIState *s);
int Widget_PopupMenu(int msg,struct GUIDialog *d, GUIState *s);
int Widget_ChannelTabs(int msg, struct GUIDialog *d, GUIState *s);
int Widget_ChatView(int msg,struct GUIDialog *d, GUIState *s);
int Widget_Dummy(int msg, struct GUIDialog *d, GUIState *s);

// misc dialog stuff
void ResizeMainGUI(GUIState *s);

#define IRCCOLOR_PALETTE_SIZE 48
#define IRCCOLOR_BG 32
#define IRCCOLOR_FG 33
#define IRCCOLOR_ACTIVE 34
#define IRCCOLOR_INACTIVE 35
#define IRCCOLOR_SELECT 36
#define IRCCOLOR_NEWDATA 37
#define IRCCOLOR_NEWMSG 38
#define IRCCOLOR_NEWHIGHLIGHT 39
#define IRCCOLOR_SPELLCHECK 40
#define IRCCOLOR_CURSOR 41
#define IRCCOLOR_FORMATFG 42
#define IRCCOLOR_FORMATBG 43
#define IRCCOLOR_BORDER 44
#define IRCCOLOR_FADED 45

extern ClientTab *FirstTab;
extern ClientAddon *FirstAddon;
extern EventType *FirstEventType;
extern EventType *FirstCommand;
extern CommandHelp *FirstHelp;
extern cJSON *MenuMain;
extern cJSON *MenuChannel;
extern cJSON *MenuUser;
extern cJSON *MenuUserTab;
extern cJSON *MenuTextEdit;
extern cJSON *MainConfig;
extern int CurWindow;
extern SDL_Texture *MainIconSet;
extern const int TabColors[];
extern int SystemIconW, SystemIconH;
extern SDL_Texture *ScreenTexture;
extern int quit;
extern CURLM *MultiCurl;
extern int SqCurlRunning;
extern char *PrefPath;
extern SDL_mutex *LockConfig, *LockTabs, *LockEvent, *LockSockets, *LockDialog;
extern IPC_Queue *EventQueue[1];
extern IPC_Queue *SocketQueue[1];

#define GUIPOS_INPUTBOX 1
#define GUIPOS_YOURNAME 2
#define GUIPOS_INPUTCHARS 3
#define GUIPOS_SPARKICON 4
#define GUIPOS_CHANNELTAB 5
#define GUIPOS_CHATVIEW 6
#define GUIPOS_POPUPMENU 7
#define GUIPOS_LENGTH 7

#define SCROLLBAR_NONE 0
#define SCROLLBAR_HAS_MOUSE 1
#define SCROLLBAR_SCROLL_CHANGE 2

enum FF_Escapes {
  FORMAT_SUBSCRIPT = 0x80,
  FORMAT_SUPERSCRIPT = 0x81,
  FORMAT_STRIKEOUT = 0x82,
  FORMAT_HIDDEN = 0x83,        // only appears when copying and pasting the text
  FORMAT_PLAIN_ICON = 0x84,    // just an icon
  FORMAT_ICON_LINK = 0x85,     // icon, then link
  FORMAT_URL_LINK = 0x86,      //
  FORMAT_COMMAND_LINK = 0x87,  //
  FORMAT_NICK_LINK = 0x88,     // someone's name link
  FORMAT_CANCEL_COLORS = 0x89, //
  FORMAT_BG_ONLY = 0x8a        // only set background
};

enum GUI_Type {
  GUI_SDL_MINIMAL,
  GUI_TEXT_NORMAL,
  GUI_TEXT_BOT,
};

struct SqCurl {
// progress for a Squirrel-caused or C-caused curl transfer
  char *Memory;
  char *Form;
  size_t Size;
  HSQUIRRELVM Script;
  union {
    struct {
      HSQOBJECT Handler, Extra;
    } Squirrel;
    struct {
      void (*Function) (void *Data, int Size, void *Extra);
      void *Extra;
    } Native;
  } Data;
};

enum SqSockCodes {
  SQS_CANT_CONNECT,
  SQS_CONNECTED,
  SQS_NEW_DATA,
  SQS_DISCONNECTED,
};

typedef struct SqSocket {
  int Flags, Id;
  BIO *Bio;
  SSL *Secure;
  IPC_Queue *Queue[2];
  int BufferSize;
  char *Buffer;

  int Port;          // port in use
  int Socket;        // socket number
  int BytesWritten;  // number of bytes written, for binary protocols
  char Hostname[128];

  HSQUIRRELVM Script;
  union {
    void (*Native) (int Socket, int Event, char *Data);
    HSQOBJECT Squirrel;
  } Function;
  struct wslay_event_context *Websocket;

  struct SqSocket *Prev, *Next;
} SqSocket;
#define SQSOCK_SSL          1
#define SQSOCK_NOT_LINED    2
#define SQSOCK_WEBSOCKET    4
#define SQSOCK_CONNECTED    8
#define SQSOCK_CLEANUP      16
#define SQSOCK_WS_CONNECTED 32
#define SQSOCK_WS_BLOCKING  64

extern SqSocket *FirstSock;
SqSocket *FindSockById(int Id);
SqSocket *FindSockBySocket(int Socket);
int SockSend(SqSocket *Sock, const char *Message, int Length);
int SockRecv(SqSocket *Sock, char *Message, int Length);

struct _xchat_hook {
  xchat_plugin *XChatPlugin;
  EventHook *Hook;
  char *EventName; // so far just used for the event/command name
  void *Userdata;
  int HookType;
};
enum HookTypes {
  HOOK_EVENT,
  HOOK_COMMAND,
  HOOK_TIMER,
};

struct _xchat_list {
  int Type;
  union {
    ClientTab *Tab;
    cJSON *JSON;
    ClientAddon *Addon;
  } State;
  int Index;
};
enum ListTypes {
  LIST_INVALID,
  LIST_CHANNELS,
  LIST_USERS,
  LIST_CONFIG,
  LIST_ADDONS
};

extern Uint32 MainThreadEvent;
enum MainThreadRequests {
  MTR_NOTHING,          //
  MTR_DIRTY_TABS,       //
  MTR_INFO_ADDED,       // d1 = context
  MTR_EDIT_CUT,         // d1 = context
  MTR_EDIT_COPY,        // d1 = context
  MTR_EDIT_DELETE,      // d1 = context
  MTR_EDIT_PASTE,       // d1 = context
  MTR_EDIT_SELECT_ALL,  // d1 = context
  MTR_EDIT_INPUT_TEXT,  // d1 = context, d2 = text
  MTR_EDIT_INPUT_CHAR,  // d1 = context, d2 = integer
  MTR_EDIT_SET_TEXT,    // d1 = context, d2 = text
  MTR_EDIT_APPEND_TEXT, // d1 = context, d2 = text
  MTR_EDIT_SET_CURSOR,  // d1 = context, d2 = integer
  MTR_GUI_COMMAND,      // d1 = context, d2 = command
};
#endif
