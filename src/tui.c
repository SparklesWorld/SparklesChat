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
// uses http://www.projectpluto.com/win32a.htm on Windows
#include "chat.h"
#include <curses.h>
#define WITH_CTRL(x) (x&0x1F)
#ifdef __PDCURSES__
#define either_getmouse nc_getmouse
#else
#define either_getmouse getmouse
#endif

int UsingTUI = 0;
int IsInsideRect(int X1, int Y1, int X2, int Y2, int W, int H);
int IsInsideWidget(int X1, int Y1, GUIDialog *D);
int TUI_RenderText(WINDOW *Window, int Y, int X, const char *Text);
int TUI_RenderHeight(WINDOW *Window, const char *Text);
extern GUIDialog MainDialog[];
extern GUIState MainGUI;
extern GUIState *GUIStates[NUM_GUISTATES];
extern SDL_Color IRCColors[];
char *MsgRightOrExtended(ClientMessage *Msg);

enum KeyShortcuts {
  UKEY_SCROLLUP_1LINE,
  UKEY_SCROLLDN_1LINE,
  UKEY_SCROLLUP_1PAGE,
  UKEY_SCROLLDN_1PAGE,
  UKEY_MOVEUP_1TAB,
  UKEY_MOVEDN_1TAB,
  UKEY_FIND_TEXT,
  UKEY_FIND_TAB,
  UKEY_INSERT_CHAR,
  UKEY_OPEN_MENU,
  UKEY_EXIT_CLIENT,
  UKEY_LENGTH,
};
int KeyConfig[UKEY_LENGTH];

void TUI_KeyConfig() {
  static const char *Names[] = {
    "Scroll up a line",
    "Scroll down a line",
    "Scroll up a page",
    "Scroll down a page",
    "Move up a tab",
    "Move down a tab",
    "Find text",
    "Find tab",
    "Insert character",
    "Open the menu",
    "Exit the client"
  };
  erase();
  refresh();
  nodelay(stdscr, FALSE);
  mvprintw(1, 2, "Configure the keys before you use the text interface.");
  mvprintw(2, 2, "Press a key to set it. Up/Down to move through the list.");
  mvprintw(3, 2, "Use space for keys you don't want to set. Press enter to finish.");

  for(int i = 0; i < UKEY_LENGTH; i++) {
    KeyConfig[i] = ' ';
    mvprintw(5+i, 2, "%20s", Names[i]);
  }
  int CurKey = 0;
  while(1) {
    move(5+CurKey, 23);
    refresh();
    int k = getch();
    if(k == KEY_UP || k == KEY_DOWN) {
      if(k == KEY_UP)
        CurKey--;
      if(k == KEY_DOWN)
        CurKey++;
      if(CurKey < 0) CurKey = UKEY_LENGTH-1;
      if(CurKey >= UKEY_LENGTH) CurKey = 0;
      continue;
    } else if(k == '\n')
      break;
    else {
      if(k == ' ')
        mvprintw(5+CurKey, 23, "    ", k);
      else
        mvprintw(5+CurKey, 23, "%.4x", k);
      KeyConfig[CurKey] = k;
    }
  }
  for(int i = 0; i < UKEY_LENGTH; i++)
    if(KeyConfig[i] == ' ')
      KeyConfig[i] = -1;
  nodelay(stdscr, TRUE);
};

WINDOW *TUI_GetWindow(int msg, GUIDialog *d) {
  if(msg == MSG_END) {
    delwin((WINDOW*)d->Extra.Window);
    return NULL;
  }
  if(d->Extra.Window) {
    if(getbegx((WINDOW*)d->Extra.Window) != d->x || getbegy((WINDOW*)d->Extra.Window) != d->y)
      mvwin((WINDOW*)d->Extra.Window, d->y, d->x);
    if(getmaxx((WINDOW*)d->Extra.Window) != d->w || getmaxy((WINDOW*)d->Extra.Window) != d->h)
      wresize((WINDOW*)d->Extra.Window, d->h, d->w);
    return (WINDOW*)d->Extra.Window;
  }
  d->Extra.Window = newwin(d->h, d->w, d->y, d->x);
  return (WINDOW*)d->Extra.Window;
}
int TUI_ScreenClear(int msg,struct GUIDialog *d, GUIState *s) {
  if(msg == MSG_START || msg == MSG_DRAW) {
    erase();
    // add in the line at the bottom
    mvhline(LINES-2, 0, ACS_HLINE, COLS);
    mvaddch(LINES-2, s->Dialog[GUIPOS_CHANNELTAB].w-1, ACS_BTEE);
    refresh();
  }
  return D_O_K;
}
int TUI_StaticText(int msg,struct GUIDialog *d, GUIState *s) {
  WINDOW *Window = TUI_GetWindow(msg, d);
  if(msg == MSG_START || msg == MSG_DRAW) {
    werase(Window);
    mvwprintw(Window, 0, 0, "%s", d->dp);
    wrefresh(Window);
  }
  return D_O_K;
}
int TUI_PopupMenu(int msg,struct GUIDialog *d, GUIState *s) {
  WINDOW *Window = TUI_GetWindow(msg, d);
  if(msg == MSG_START || msg == MSG_DRAW) {
    werase(Window);
    mvwprintw(Window, 0, 0, "pop");
    wrefresh(Window);
  }
  return D_O_K;
}
typedef struct TUIChatViewState {
  int Scroll;
  int Drawn;
} TUIChatViewState;
int TUI_ChatView(int msg,struct GUIDialog *d, GUIState *s) {
// todo: better scrolling, winsdelln() use instead of redrawing everything
  WINDOW *Window = TUI_GetWindow(msg, d);
  struct TUIChatViewState *ChatInfo = (struct TUIChatViewState*)d->dp3;
  ClientTab *Tab = (ClientTab*)d->dp;

  if(!ChatInfo) {
    ChatInfo = (struct TUIChatViewState*)malloc(sizeof(struct TUIChatViewState));
    memset(ChatInfo, 0, sizeof(struct TUIChatViewState));
    d->dp3 = ChatInfo;
  }
  if(msg == MSG_CHAR) {
    int k = s->MessageKey.Curses;
    if(k == KeyConfig[UKEY_SCROLLUP_1LINE]) {
      ChatInfo->Scroll++;
      if(ChatInfo->Scroll >= Tab->NumMessages)
        ChatInfo->Scroll = Tab->NumMessages-1;
      ChatInfo->Drawn = 0;
    } else if(k == KeyConfig[UKEY_SCROLLDN_1LINE]) {
      ChatInfo->Scroll--;
      if(ChatInfo->Scroll < 0)
        ChatInfo->Scroll = 0;
      ChatInfo->Drawn = 0;
    } else if(k == KeyConfig[UKEY_SCROLLUP_1PAGE]) { // to do: actually make it by pages???
      ChatInfo->Scroll += 10;
      if(ChatInfo->Scroll >= Tab->NumMessages)
        ChatInfo->Scroll = Tab->NumMessages-1;
      ChatInfo->Drawn = 0;
    } else if(k == KeyConfig[UKEY_SCROLLDN_1PAGE]) {
      ChatInfo->Scroll -= 10;
      if(ChatInfo->Scroll < 0)
        ChatInfo->Scroll = 0;
      ChatInfo->Drawn = 0;
    }
  }
  if(msg == MSG_ACTIVATE) { // dp was changed to a new tab
    ChatInfo->Scroll = 0;
    ChatInfo->Drawn = 0;
  }
  if(msg == MSG_INFO_ADDED) {
    if(ChatInfo->Scroll)
      ChatInfo->Scroll += Tab->UndrawnLines;
    ChatInfo->Drawn = 0;
  }
  if(!ChatInfo->Drawn || msg == MSG_START || msg == MSG_DRAW) {
    if(!ChatInfo->Drawn)
      werase(Window);
    if(Tab && Tab->Messages && ChatInfo->Drawn!=2) {
      Tab->UndrawnLines = 0;
      int DrawY = getmaxy(Window), CurMessage;
      char TimestampBuffer[80];
      char FullMessage[INPUTBOX_SIZE];
      const char *TimestampFormat = GetConfigStr("(%I:%M%p) ", "ChatView/TimeStampDisplayFormat");

      if(!ChatInfo->Drawn) { // full redraw
        for(CurMessage=Tab->NumMessages-1-ChatInfo->Scroll;CurMessage>=0;CurMessage--) {
          ClientMessage *Msg = &Tab->Messages[CurMessage];
          struct tm *TimeInfo = localtime(&Msg->Time);
          strftime(TimestampBuffer,sizeof(TimestampBuffer),TimestampFormat,TimeInfo);
          if(*Msg->Left)
            snprintf(FullMessage, sizeof(FullMessage), "%s%s %s", TimestampBuffer, Msg->Left, MsgRightOrExtended(Msg));
          else
            snprintf(FullMessage, sizeof(FullMessage), "%s%s", TimestampBuffer, MsgRightOrExtended(Msg));

          DrawY -= TUI_RenderHeight(Window, FullMessage);
          TUI_RenderText(Window, DrawY, 0, FullMessage);
          if(DrawY <= 0) {
            if(DrawY)
              mvwprintw(Window, 0, 0, "...");
            break;
          }
        }
      }
      ChatInfo->Drawn = 1;
    }
    wrefresh(Window);
  }
  if(msg == MSG_END)
    free(ChatInfo);
  return D_O_K;
}

int TUI_ChatEdit(int msg,struct GUIDialog *d, GUIState *s) {
  // this widget needs extra information, so it allocates it and stores a pointer at dp3
  WINDOW *Window = TUI_GetWindow(msg, d);
  char *EditBuffer = (char*)d->dp;
  int NeedRedraw = 0;

  if(msg == MSG_START)
    strcat(EditBuffer, " ");
  struct TextEditInfo *EditInfo = (struct TextEditInfo*)d->dp3;
  if(!EditInfo) {
    EditInfo = (struct TextEditInfo*)malloc(sizeof(struct TextEditInfo));
    memset(EditInfo, 0, sizeof(struct TextEditInfo));
    EditInfo->ScrollBytes = EditBuffer;
    EditInfo->CursorBytes = EditBuffer;
    // we're not gonna use SelectChars/SelectBytes for now
    d->dp3 = EditInfo;
  }

  if(msg == MSG_CHAR) {
    int k = s->MessageKey.Curses;
    char *Temp;
    switch(k) {
      case KEY_LEFT:
        Temp = UnicodeBackward(EditInfo->CursorBytes, 1, EditBuffer);
        if(Temp) {
          EditInfo->CursorChars--;
          EditInfo->CursorBytes = Temp;
          NeedRedraw = 1;
        }
        break;
      case KEY_RIGHT:
        Temp = UnicodeForward(EditInfo->CursorBytes, 1);
        if(Temp) {
          EditInfo->CursorChars = UnicodeLen(EditBuffer, Temp);
          EditInfo->CursorBytes = Temp;
          NeedRedraw = 1;
        }
        break;
      case WITH_CTRL('h'):
      case KEY_BACKSPACE:
        Temp = UnicodeBackward(EditInfo->CursorBytes, 1, EditBuffer);
        if(Temp) {
          EditInfo->CursorChars--;
          memmove(Temp, EditInfo->CursorBytes, strlen(EditInfo->CursorBytes)+1);
          EditInfo->CursorBytes = Temp;
          NeedRedraw = 1;
        }
        break;
      case KEY_HOME:
        EditInfo->CursorChars = 0;
        EditInfo->CursorBytes = EditBuffer;
        NeedRedraw = 1;
        break;
      case KEY_END:
        EditInfo->CursorChars = UnicodeLen(EditBuffer, NULL)-1;
        EditInfo->CursorBytes = UnicodeForward(EditBuffer, EditInfo->CursorChars);
        NeedRedraw = 1;
        break;
      case KEY_DC: // delete 
        Temp = UnicodeForward(EditInfo->CursorBytes, 1);
        if(Temp) {
          memmove(EditInfo->CursorBytes, Temp, strlen(Temp)+1);
          NeedRedraw = 1;
        }
        break;
      default:
        if(k < ' ' ||  k >= 127)
          break;

        memmove(EditInfo->CursorBytes+1, EditInfo->CursorBytes, strlen(EditInfo->CursorBytes)+1);
        
        *EditInfo->CursorBytes = k;
        EditInfo->CursorBytes++;
        EditInfo->CursorChars++;
        NeedRedraw = 1;
    }
    if(k == KEY_ENTER || k == '\n') {
      char CopyBuffer[strlen(EditBuffer)+1];
      strcpy(CopyBuffer, EditBuffer);
      char *EditStart = CopyBuffer;
      char *LastSpace = strrchr(CopyBuffer, ' ');
      *LastSpace = 0;

      if(*CopyBuffer == '/' && CopyBuffer[1] == '/')
        EditStart = CopyBuffer + 2;

      // clear
      EditInfo->CursorChars = 0;
      EditInfo->CursorBytes = EditBuffer;
      strcpy(EditBuffer, " ");

      const char *Context = "";
      GUIDialog *ChannelTabs = FindDialogWithProc(NULL, s, NULL, Widget_ChannelTabs);
      char ContextBuffer[CONTEXT_BUF_SIZE];
      if(ChannelTabs && (ClientTab*)ChannelTabs->dp)
        Context = ContextForTab((ClientTab*)ChannelTabs->dp, ContextBuffer);

      if(*CopyBuffer == '/' && CopyBuffer[1] != '/') { // command
        EditStart++;
        char Command[strlen(EditStart)+1];
        strcpy(Command, EditStart);
        char *CmdArgs = strchr(Command, ' ');
        if(CmdArgs)
          *(CmdArgs++) = 0;
        QueueEvent(Command, CmdArgs, Context, 'C');
      } else // say
        QueueEvent("say", EditStart, Context, 'C');
      NeedRedraw = 1;
    }
  }

  // keep the cursor inside the text box
  int CursorInBox = EditInfo->CursorChars-EditInfo->ScrollChars;
  int MaxInBox = (d->w-1);
  if(CursorInBox < 0) {
    EditInfo->ScrollChars = EditInfo->CursorChars;
    EditInfo->ScrollBytes = EditInfo->CursorBytes;
  } else if(CursorInBox >= MaxInBox) { // max chars
    EditInfo->ScrollChars = EditInfo->CursorChars - MaxInBox + 1;
    EditInfo->ScrollBytes = UnicodeForward(EditBuffer, EditInfo->ScrollChars);
  }

  if(msg == MSG_START || msg == MSG_DRAW || NeedRedraw) {
    attron(A_REVERSE);    
    mvprintw(LINES-2, COLS-4, "%.4i", UnicodeLen(EditBuffer, NULL)-1);
    attroff(A_REVERSE);

    const char FormatChars[] = "cbiupr-";
    const char FormatCodes[] = {0x03,0x02,0x1d,0x1f,0x0f,0x16,'\n',0};
    werase(Window);
    wmove(Window, 0, 0);
    char *Peek = EditInfo->ScrollBytes;
    while(*Peek && (getcurx(Window) < d->w-1)) {
      if(*Peek < 32) { // start of ascii
        for(int i = 0; FormatCodes[i]; i++)
          if(FormatCodes[i] == *Peek) {
            wattron(Window, A_REVERSE);
            waddch(Window, FormatChars[i]);
            wattroff(Window, A_REVERSE);
            break;
          }
      } else
        waddch(Window, *Peek);
      Peek++;
    }
    wrefresh(Window);
  }
  if(msg == MSG_END)
    free(EditInfo);
  return D_O_K;
}
void StartPopupCommandMenu(GUIState *s, cJSON *JSON, char *Include);
int TUI_MenuButton(int msg,struct GUIDialog *d, GUIState *s) {
  WINDOW *Window = TUI_GetWindow(msg, d);
  if(msg == MSG_START || msg == MSG_DRAW) {
    werase(Window);
    wattron(Window, A_REVERSE);
    mvwaddch(Window, 0,   0,  'O');
    wattroff(Window, A_REVERSE);
    wrefresh(Window);
  }
  else if(msg == MSG_CLICK || msg == MSG_DCLICK) {
    *s->TempCmdTarget = 0;
    StartPopupCommandMenu(s, MenuMain, NULL);
  }
  return D_O_K;
}
int TUI_ChannelTabs(int msg,struct GUIDialog *d, GUIState *s) {
  WINDOW *Window = TUI_GetWindow(msg, d);
  ClientTab *SelectTab = (ClientTab*)d->dp;
  ClientTab *OldTab = (ClientTab*)d->dp;
  char ContextBuffer[CONTEXT_BUF_SIZE];
  char ContextBuffer2[CONTEXT_BUF_SIZE];
  int NeedDraw = 0;
  int ChangedTab = 0;
  if(msg == MSG_CLICK) {
    MEVENT event;
    if(either_getmouse(&event) != OK) return D_O_K;
    int Num = event.y;
    ClientTab *Tab = FirstTab;
    while(Tab) {
      if(!(Tab->Flags & TAB_HIDDEN)) {
        if(!Num)
          break;
        else
          Num--;
      }
      if(Tab->Child) Tab = Tab->Child;
      else if(Tab->Next) Tab = Tab->Next;
      else if(Tab->Parent) Tab = Tab->Parent->Next;
    }
    if(!Tab)
      return D_O_K;
    d->dp = Tab;
    SelectTab = Tab;
    SelectTab->Flags &= ~TAB_COLOR_MASK;
    ChangedTab = 1;
  }
  if(msg == MSG_CHAR) {
    if(s->MessageKey.Curses == KeyConfig[UKEY_MOVEUP_1TAB]) {
      if(SelectTab->Prev) {
        SelectTab = SelectTab->Prev;
        if(SelectTab->Child) {
          SelectTab = SelectTab->Child;
          while(SelectTab->Next) SelectTab = SelectTab->Next; // find last channel
        }
      } else if(SelectTab->Parent) SelectTab = SelectTab->Parent;
      else { // wrap to very end
        SelectTab = FirstTab;
        while(SelectTab->Next) SelectTab = SelectTab->Next; // last server
        if(SelectTab->Child) {                              // has channels?
          SelectTab = SelectTab->Child;                     // find last channel
          while(SelectTab->Next) SelectTab = SelectTab->Next;
        }
      }
      d->dp = SelectTab;
      NeedDraw = 1;
      ChangedTab = 1;
    } else if(s->MessageKey.Curses == KeyConfig[UKEY_MOVEDN_1TAB]) {
      if(SelectTab->Child) SelectTab = SelectTab->Child;
      else if(SelectTab->Next) SelectTab = SelectTab->Next;
      else if(SelectTab->Parent && SelectTab->Parent->Next) SelectTab = SelectTab->Parent->Next;
      else SelectTab = FirstTab;
      d->dp = SelectTab;
      NeedDraw = 1;
      ChangedTab = 1;
    }
  }
  if(msg == MSG_ACTIVATE || ChangedTab) {
    GUIDialog *ChatView = FindDialogWithProc(NULL, s, NULL, Widget_ChatView);
    if(ChatView) {
      ChatView->dp = SelectTab;
      SelectTab->Flags &= ~TAB_COLOR_MASK;
      RunWidget(MSG_ACTIVATE, ChatView, s);
      if(SelectTab && OldTab)
        QueueEvent("focus tab", ContextForTab(OldTab, ContextBuffer2), ContextForTab(SelectTab, ContextBuffer), 'E');
    }
    NeedDraw = 1;
  }
  if(msg == MSG_START || msg == MSG_DRAW || NeedDraw) {
    werase(Window);
    mvwvline(Window, d->y, d->w-1, ACS_VLINE, d->h);
    ClientTab *Tab = FirstTab;
    int CurDrawn = 0;
    char TabNameCutoff[d->w];
    while(Tab) {
      if(!(Tab->Flags & TAB_HIDDEN)) {
//        int Color = Tab->Flags & TAB_COLOR_MASK;
        int StartDrawX = 0;
        if(Tab->Parent) StartDrawX++;
        char Icon = '?';
        // pick the right icon
        if(Tab->Flags & TAB_CHANNEL)    Icon = '#';
        if(Tab->Flags & TAB_QUERY)      Icon = '@';
        if(Tab->Flags & TAB_SERVER)     Icon = '$';
        if(Tab->Flags & TAB_NOTINCHAN)  Icon = '-';
        wattron(Window, A_REVERSE);
        mvwaddch(Window, CurDrawn, StartDrawX, Icon);
        wattroff(Window, A_REVERSE);
        StartDrawX++;

        // set attributes for actual channel name
        if(Tab->Flags & TAB_NOTINCHAN)  wattron(Window, A_DIM);
        if(Tab->Flags & TAB_COLOR_TALK) wattron(Window, A_BOLD);
        if(Tab->Flags & TAB_COLOR_NAME) wattron(Window, A_BLINK);
        if(Tab == SelectTab)            wattron(Window, A_REVERSE);
        memcpy(TabNameCutoff, Tab->Name, d->w-1);
        TabNameCutoff[d->w - StartDrawX - 1] = 0;
        mvwprintw(Window, CurDrawn, StartDrawX, "%s", TabNameCutoff);
        wattrset(Window, A_NORMAL);
        CurDrawn++;
      }
      if(Tab->Child) Tab = Tab->Child;
      else if(Tab->Next) Tab = Tab->Next;
      else if(Tab->Parent) Tab = Tab->Parent->Next; // if NULL, loop terminates
      else break;
    }
    wrefresh(Window);
  }
  return D_O_K;
}
void TUI_ResizeMainGUI(GUIState *s) {
  int WW = COLS, WH = LINES;
  s->Dialog[GUIPOS_INPUTBOX].h = 1;
  s->Dialog[GUIPOS_YOURNAME].h = 1;
//  s->Dialog[GUIPOS_INPUTCHARS].h = 1;
  s->Dialog[GUIPOS_CHANNELTAB].x = 0;
  s->Dialog[GUIPOS_CHANNELTAB].y = 0;
  s->Dialog[GUIPOS_CHATVIEW].y = 0;
  s->Dialog[GUIPOS_INPUTBOX].y = WH-1;
  s->Dialog[GUIPOS_YOURNAME].y = WH-2;
//  s->Dialog[GUIPOS_INPUTCHARS].y = WH-2;

  s->Dialog[GUIPOS_SPARKICON].y = WH-2;
  s->Dialog[GUIPOS_SPARKICON].h = 1;
  s->Dialog[GUIPOS_SPARKICON].x = 0;
  s->Dialog[GUIPOS_SPARKICON].w = 1;

  s->Dialog[GUIPOS_YOURNAME].w = UnicodeLen((char*)s->Dialog[GUIPOS_YOURNAME].dp, NULL);
//  s->Dialog[GUIPOS_INPUTCHARS].w = 4;
//  s->Dialog[GUIPOS_INPUTCHARS].x = WW-1-4;

  s->Dialog[GUIPOS_INPUTBOX].x = 0;
  s->Dialog[GUIPOS_INPUTBOX].w = WW-s->Dialog[GUIPOS_INPUTBOX].x;
  s->Dialog[GUIPOS_YOURNAME].x = s->Dialog[GUIPOS_INPUTBOX].x;

  s->Dialog[GUIPOS_CHANNELTAB].h = WH-2;

  s->Dialog[GUIPOS_CHATVIEW].x = s->Dialog[GUIPOS_CHANNELTAB].x+s->Dialog[GUIPOS_CHANNELTAB].w;
  s->Dialog[GUIPOS_CHATVIEW].w = WW-1-s->Dialog[GUIPOS_CHATVIEW].x;
  s->Dialog[GUIPOS_CHATVIEW].h = s->Dialog[GUIPOS_CHANNELTAB].h;

  RunWidget(MSG_WIN_RESIZE, &s->Dialog[GUIPOS_CHATVIEW], s);
  for(int i=0;i<GUIPOS_LENGTH;i++)
    TUI_GetWindow(0, &s->Dialog[i]);
  s->Flags |= GSF_NEED_REDRAW;
}

int TUI_RenderText(WINDOW *Window, int Y, int X, const char *Text) {
  wmove(Window, Y, X);
  wattrset(Window, A_NORMAL);
  int Bold=0, Italic=0, Underline=0, Sup=0, Sub=0, Strike=0, Hidden=0, Link=0, Reverse=0;
  int BG=IRCCOLOR_BG, FG=1;
  char Text2[strlen(Text)+1], *Poke;
  int i;

  char *Find;
  while(1) {
    unsigned char k = *Text;
    switch(k) {
      case 0xfe:
        break;
      case 0x02:
        Bold ^= 1;
        if(Bold)
          wattron(Window, A_BOLD);
        else
          wattroff(Window, A_BOLD);
        break;
      case 0x04:
        for(i=1;i<7;i++)
          if(!isxdigit(Text[i]))
            break;
        if(i==7)
          Text+=6;
        break;
      case 0x03:
        FG=0;
        if(isdigit(Text[1])) {
          FG = Text[1]-'0';
          Text++;
          if(Text[1] == ',') {
            Text++;
            if(isdigit(Text[1])) {
              BG = Text[1]-'0';
              Text++;
              if(isdigit(Text[1])) {
                BG = ((BG*10)+(Text[1]-'0'))&31;
                Text++;
              }
            }
          } else if(isdigit(Text[1])) {
            FG = (FG*10)+(Text[1]-'0');
            Text++;
            if(Text[1] == ',') {
              Text++;
              if(isdigit(Text[1])) {
                BG = Text[1]-'0';
                Text++;
                if(isdigit(Text[1])) {
                  BG = ((BG*10)+(Text[1]-'0'))&31;
                  Text++;
                }
              }
            }
          }
        }
        // todo: update color
        break;
      case 0x0f:
        Bold = 0; Italic = 0;
        Underline = 0; Sup = 0;
        Sub = 0; Strike = 0;
        FG = 1;
        wattrset(Window, A_NORMAL);
        break;
      case 0x16:
        Reverse ^= 1;
        if(Reverse)
          wattron(Window, A_REVERSE);
        else
          wattroff(Window, A_REVERSE);
        break;
      case 0x1d:
        Italic ^= 1;
#ifdef __PDCURSES__
        if(Italic)
          wattron(Window, A_ITALIC);
        else
          wattroff(Window, A_ITALIC);
#endif
        break;
      case 0x1f:
        Underline ^= 1;
        if(Underline)
          wattron(Window, A_UNDERLINE);
        else
          wattroff(Window, A_UNDERLINE);
        break;
      case 0xff:
        switch((unsigned char)*(++Text)) {
          case FORMAT_SUBSCRIPT:
            Sub ^= 1;
            if(Sub)
              wattron(Window, A_DIM);
            else
              wattroff(Window, A_DIM);
            break;
          case FORMAT_SUPERSCRIPT:
            Sup ^= 1;
            if(Sup)
              wattron(Window, A_DIM);
            else
              wattroff(Window, A_DIM);
            break;
          case FORMAT_STRIKEOUT:
            Strike ^= 1;
            break;
          case FORMAT_HIDDEN:
            Hidden ^= 1;
            break;
          case FORMAT_PLAIN_ICON:
            break;
          case FORMAT_ICON_LINK:
            break;
          case FORMAT_URL_LINK:
            Link ^= 1;
            if(Link) {
              Find = strchr(Text, ']');
              if(Find) Text = Find;
            }
            break;
          case FORMAT_NICK_LINK:
          case FORMAT_COMMAND_LINK:
            Link ^= 1;
            if(Link) {
              Find = strchr(Text, 0xfe);
              if(Find) Text = Find;
            }
            break;
          case FORMAT_CANCEL_COLORS:
            FG = 1;
            break;
          case FORMAT_BG_ONLY:
            //BGOnly = 1;
            break;
        }
        break;
      default:
        if(k>=0x20) {
          Poke = Text2;
          while((unsigned char)Text[0]>=0x20 && (unsigned char)Text[0]<0xfe)
            *(Poke++) = *(Text++);
          *Poke = 0;

          wprintw(Window, "%s", Text2);

          Text--; // will ++ after the loop
        }
    }

    if(*Text)
      Text++;
    else break;
  }

  return 1;
}

int TUI_RenderHeight(WINDOW *Window, const char *Text) {
  int Width=getmaxx(Window);
  char Stripped[strlen(Text)+1];
  StripIRCText(Stripped, Text,0);

//  int Chars = VisibleStrLen(Text, 0);   <--- doesn't work right, or I'd be using it instead
  int Chars = UnicodeLen(Stripped, NULL);
  int Lines = Chars / Width;
  if(Chars % Width) Lines++;
  return Lines;
}

int InitTextGUI() {
  UsingTUI = 1;
  initscr();
  start_color();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  nodelay(stdscr, TRUE);
  mousemask(BUTTON1_CLICKED|REPORT_MOUSE_POSITION, NULL);

  MainDialog[GUIPOS_CHANNELTAB].w = 15;
  MainGUI.UpdateFunc = TUI_ResizeMainGUI;
  MainDialog[GUIPOS_INPUTCHARS].proc = Widget_Dummy;

  char KeyPath[260];
  sprintf(KeyPath, "%sTUI_Key_Config.txt", PrefPath);
  FILE *ConfigFile = fopen(KeyPath, "rb");
  if(!ConfigFile) {
    TUI_KeyConfig();
    ConfigFile = fopen(KeyPath, "wb");
    for(int i = 0; i < UKEY_LENGTH; i++)
      fprintf(ConfigFile, "%x ", KeyConfig[i]);
  } else {
    for(int i = 0; i < UKEY_LENGTH; i++)
      fscanf(ConfigFile, "%x", &KeyConfig[i]);
  }
  fclose(ConfigFile);

  /* In the pdcurses port I'm using, color pairs don't seem to work properly
     and always have color 7 for FG and 0 for BG. This would make brown on
     gray, so colors are left as the default right now */
  return 1;
}
void RunTextGUI() {
  SDL_Event e;
  SDL_LockMutex(LockTabs);
  SDL_LockMutex(LockDialog);
  while(SDL_PollEvent(&e) != 0) {
    if(e.type == SDL_QUIT)
      quit = 1;
    else if (e.type == MainThreadEvent) {
      GUIDialog *Dialog;
      GUIState *State;
      ClientTab *Tab;
      switch(e.user.code) {
        case MTR_DIRTY_TABS:       //
          Dialog = FindDialogWithProc(NULL, NULL, &State, Widget_ChannelTabs);
          if(Dialog)
            Dialog->flags |= D_DIRTY;
          break;
        case MTR_INFO_ADDED:       // d1 = context
          Tab = FindTab(e.user.data1);
          Dialog = FindDialogWithProc(NULL, NULL, &State, Widget_ChatView);
          if(Dialog && Tab == (ClientTab*)Dialog->dp) {
            Dialog->flags |= D_DIRTY;
            RunWidget(MSG_INFO_ADDED, Dialog, State);
          } else
            Tab->UndrawnLines = 0;
          free(e.user.data1);
          break;
        // implement all or most of these later
        case MTR_EDIT_CUT:         // d1 = context
        case MTR_EDIT_COPY:        // d1 = context
        case MTR_EDIT_DELETE:      // d1 = context
        case MTR_EDIT_PASTE:       // d1 = context
        case MTR_EDIT_SELECT_ALL:  // d1 = context
        case MTR_EDIT_INPUT_TEXT:  // d1 = context, d2 = text
        case MTR_EDIT_INPUT_CHAR:  // d1 = context, d2 = char
        case MTR_EDIT_SET_TEXT:    // d1 = context, d2 = text
        case MTR_EDIT_APPEND_TEXT: // d1 = context, d2 = text
        case MTR_EDIT_SET_CURSOR:  // d1 = context, d2 = (int)pos
          if(e.user.data1)
            free(e.user.data1);
          if(e.user.data2)
            free(e.user.data2);
          break;
        case MTR_GUI_COMMAND:      // d1 = context, d2 = command name
          Tab = FindTab(e.user.data1);
          free(e.user.data1);
          if(!Tab) break;
          if(!strcasecmp(e.user.data2, "focus")) {
            Dialog = FindDialogWithProc(NULL, NULL, &State, Widget_ChannelTabs);
            if(Dialog) {
              Dialog->dp = Tab;
              RunWidget(MSG_ACTIVATE, Dialog, State);
            }
          }
          free(e.user.data2);
          break;
        default:
          break;
      }
    }
  }
  int k = getch();
  MainGUI.MessageKey.Curses = k;
  if(k != ERR) {
    if(k==KEY_RESIZE)
      resize_term(0, 0);
    else if(k == KeyConfig[UKEY_EXIT_CLIENT])
      quit = 1;
    else if(k==KEY_MOUSE) {
      MEVENT event;
      if(either_getmouse(&event) != OK) return;
      for(int i=0;GUIStates[CurWindow]->Dialog[i].proc;i++)
        if(!(GUIStates[CurWindow]->Dialog[i].flags & D_HIDDEN))
          if(IsInsideWidget(event.x, event.y, &GUIStates[CurWindow]->Dialog[i]))
            RunWidget(MSG_CLICK, &GUIStates[CurWindow]->Dialog[i], GUIStates[CurWindow]);
    } else
      for(int i=0;GUIStates[CurWindow]->Dialog[i].proc;i++)
        RunWidget(MSG_CHAR, &GUIStates[CurWindow]->Dialog[i], GUIStates[CurWindow]);
  }
  // update stuff
  for(int i=0;i<NUM_GUISTATES;i++) {
    if(GUIStates[i]) {
      if((GUIStates[i]->Flags & (GSF_NEED_INITIALIZE|GSF_NEED_UPDATE)) && GUIStates[i]->UpdateFunc)
        GUIStates[i]->UpdateFunc(GUIStates[i]);
      if(GUIStates[i]->Flags & GSF_NEED_INITIALIZE)
        for(int j=0;GUIStates[i]->Dialog[j].proc;j++)
          RunWidget(MSG_START, &GUIStates[i]->Dialog[j], GUIStates[i]);
      if(GUIStates[i]->Flags & GSF_NEED_REDRAW)
        for(int j=0;GUIStates[i]->Dialog[j].proc;j++)
          RunWidget(MSG_DRAW, &GUIStates[i]->Dialog[j], GUIStates[i]);
      if(GUIStates[i]->Flags & GSF_NEED_PRESENT)
        refresh();
      GUIStates[i]->Flags &= ~(GSF_NEED_INITIALIZE|GSF_NEED_REDRAW|GSF_NEED_PRESENT|GSF_NEED_UPDATE);
    }
  }
  for(int j=0;GUIStates[CurWindow]->Dialog[j].proc;j++) // fix dirty state
    if(GUIStates[CurWindow]->Dialog[j].flags & D_DIRTY)
      RunWidget(MSG_DRAW, &GUIStates[CurWindow]->Dialog[j], GUIStates[CurWindow]); // automatically clears D_DIRTY

  GUIDialog *ChatEdit = FindDialogWithProc(NULL, NULL, NULL, Widget_ChatEdit);
  struct TextEditInfo *EditInfo = (struct TextEditInfo*)ChatEdit->dp3;
  SDL_UnlockMutex(LockTabs);
  SDL_UnlockMutex(LockDialog);

  move(LINES-1, EditInfo->CursorChars-EditInfo->ScrollChars);
  refresh();
}

void EndTextGUI() {
  for(int i=0;i<NUM_GUISTATES;i++) {
    if(GUIStates[i]) {
      if(GUIStates[i]->Flags)
        for(int j=0;GUIStates[i]->Dialog[j].proc;j++)
          RunWidget(MSG_END, &GUIStates[i]->Dialog[j], GUIStates[i]);
    }
  }
  endwin();
}
