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

int CurWindow = 0;
int ModKeys = 0; // https://wiki.libsdl.org/SDL_Keymod
int SystemIconW = 16, SystemIconH = 16;
int ScreenWidth = 800;
int ScreenHeight = 600;
int KeyboardFocus = 0;
int Minimized = 0;
int MouseFocus = 0;
int ScrollbarW = 20;
extern int UsingTUI;

extern SDL_Color IRCColors[];
extern FontSet ChatFont;
void StartPopupCommandMenu(GUIState *s, cJSON *JSON, char *Include);
SDL_Cursor *SystemCursors[SYSCURSOR_MAX];
char EditBuffer[INPUTBOX_SIZE] = "";
char InputboxChars[10] = "";
char InputStatus[400] = "";

GUIDialog MainDialog[] = {
// proc,              x, y, w, h, fg, bg, f, d1, d2, dp, dp2, dp3
 {Widget_ScreenClear, 0,  0,  0, 0, 0, IRCCOLOR_BG, 0,  0, 0, 0, 0, 0},
 {Widget_ChatEdit,    0, 0, 0, 0, 1, 0, 0,  8192, 0, EditBuffer, &ChatFont, NULL},
 {Widget_StaticText,  0, 0, 0, 0, 0, 0, 0,  0, 0, InputStatus, &ChatFont, NULL},
 {Widget_StaticText,  0, 0, 0, 0, 0, 0, 0,  0, 0, InputboxChars, &ChatFont, NULL},
 {Widget_MenuButton,  0, 0, 0, 0, 0, 0, 0,  0, 0, NULL, NULL, NULL},
 {Widget_ChannelTabs, 0, 0, 140, 0, 0, 0, D_KEYWATCH,  0, 0, NULL, NULL, NULL},
 {Widget_ChatView,    0, 0, 0,   0, 0, 0, D_KEYWATCH,  0, 0, NULL, NULL, NULL},
 {Widget_PopupMenu,   0, 0, 0, 0, 0, 0, D_HIDDEN|D_WANTCLICKAWAY,  0, 0, NULL, NULL, NULL},
 {NULL},
};
GUIDialog DetachedDialog[] = {
// proc,              x, y, w, h, fg, bg, f, d1, d2, dp, dp2, dp3
 {Widget_ScreenClear, 0,  0,  0, 0, 0, IRCCOLOR_BG, 0,  0, 0, 0, 0, 0},
 {Widget_ChatEdit,    0, 0, 0, 0, 1, 0, 0,  8192, 0, EditBuffer, &ChatFont, NULL},
 {Widget_StaticText,  0, 0, 0, 0, 0, 0, 0,  0, 0, InputStatus, &ChatFont, NULL},
 {Widget_StaticText,  0, 0, 0, 0, 0, 0, 0,  0, 0, InputboxChars, &ChatFont, NULL},
 {Widget_MenuButton,  0, 0, 0, 0, 0, 0, 0,  0, 0, NULL, NULL, NULL},
 {Widget_Dummy,       0, 0, 140, 0, 0, 0, D_KEYWATCH,  0, 0, NULL, NULL, NULL},
 {Widget_ChatView,    0, 0, 0,   0, 0, 0, D_KEYWATCH,  0, 0, NULL, NULL, NULL},
 {Widget_PopupMenu,   0, 0, 0, 0, 0, 0, D_HIDDEN|D_WANTCLICKAWAY,  0, 0, NULL, NULL, NULL},
 {NULL},
};

GUIState MainGUI = {
  GSF_NEED_INITIALIZE, 0, 0, 800, 600, NULL, ResizeMainGUI, &ChatFont, MainDialog,
  NULL, NULL, NULL, NULL
};
GUIState *GUIStates[NUM_GUISTATES] = {&MainGUI, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

int TUI_ScreenClear(int msg,struct GUIDialog *d, GUIState *s);
int TUI_StaticText(int msg,struct GUIDialog *d, GUIState *s);
int TUI_PopupMenu(int msg,struct GUIDialog *d, GUIState *s);
int TUI_ChatView(int msg,struct GUIDialog *d, GUIState *s);
int TUI_ChatEdit(int msg,struct GUIDialog *d, GUIState *s);
int TUI_MenuButton(int msg,struct GUIDialog *d, GUIState *s);
int TUI_ChannelTabs(int msg,struct GUIDialog *d, GUIState *s);

int IsInsideWidget(int X1, int Y1, GUIDialog *D) {
  return IsInsideRect(X1, Y1, D->x, D->y, D->w, D->h);
}
int SetACursor;
int WhatCursor = 0;
void GUI_SetCursor(int CursorNum) {
  if(CursorNum != WhatCursor)
    SDL_SetCursor(SystemCursors[CursorNum]);
  WhatCursor = CursorNum;
  SetACursor = 1;
};

SDL_Window *window = NULL;
SDL_Renderer *ScreenRenderer = NULL;
SDL_Texture *SparklesIcon = NULL;
SDL_Surface *WindowIcon = NULL, *WindowIconNotify = NULL;
SDL_Texture *MainIconSet = NULL;
FontSet ChatFont;
FontSet TinyChatFont;

SDL_Keysym *MakeKeysym(SDL_Keysym *Key, int Keycode, int Mod) {
  memset(Key, 0, sizeof(struct SDL_Keysym));
  Key->sym = Keycode;
  Key->mod = Mod;
  // can use SDL_GetScancodeFromKey() if the scancode actually matters
  return Key;
}

SDL_Surface *SDL_LoadImage(const char *FileName, int Flags) {
  SDL_Surface* loadedSurface = IMG_Load(FileName);
  if(loadedSurface == NULL) {
    SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", window, "Unable to load image %s! SDL Error: %s", FileName, SDL_GetError());
    return NULL;
  }
  if(Flags & 1)
    SDL_SetColorKey(loadedSurface, SDL_TRUE, SDL_MapRGB(loadedSurface->format, 255, 0, 255));
  return loadedSurface;
}
SDL_Texture *LoadTexture(const char *FileName, int Flags) {
  SDL_Surface *Surface = SDL_LoadImage(FileName, Flags);
  if(!Surface) return NULL;
  SDL_Texture *Texture = SDL_CreateTextureFromSurface(ScreenRenderer, Surface);
  SDL_FreeSurface(Surface);
  return Texture;
}
int InitMinimalGUI() {
  window = SDL_CreateWindow("Sparkles", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, ScreenWidth, ScreenHeight, SDL_WINDOW_SHOWN| SDL_WINDOW_RESIZABLE);
  if(!window) {
     SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", NULL, "Window could not be created! SDL_Error: %s", SDL_GetError());
     return 0;
  }
  if(!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)){
    SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", NULL, "SDL_image could not initialize! SDL_image Error: %s", IMG_GetError());
    return 0;
  }
  if( TTF_Init() == -1 ) {
    SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", NULL, "SDL_ttf could not initialize! SDL_ttf Error: %s", TTF_GetError());
    return 0;
  }

  ScreenRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_TARGETTEXTURE);
  if(!SDL_RenderTargetSupported(ScreenRenderer)) {
    SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", NULL, "Rendering to a texture isn't supported");
    return 0;
  }
  MainGUI.Renderer = ScreenRenderer;
  MainGUI.Window = window;
  //MainGUI.DefaultFont = &ChatFont;

  SystemCursors[SYSCURSOR_NORMAL] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
  SystemCursors[SYSCURSOR_TEXT] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
  SystemCursors[SYSCURSOR_SIZE_NS] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
  SystemCursors[SYSCURSOR_SIZE_EW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
  SystemCursors[SYSCURSOR_SIZE_ALL] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
  SystemCursors[SYSCURSOR_LINK] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
  SystemCursors[SYSCURSOR_DISABLE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);

  if (!Load_FontSet(&ChatFont, 11, "data/font/font.ttf", "data/font/fontb.ttf", "data/font/fonti.ttf", "data/font/fontbi.ttf"))
    return 0;
  if (!Load_FontSet(&TinyChatFont, 8, "data/font/font.ttf", "data/font/fontb.ttf", "data/font/fonti.ttf", "data/font/fontbi.ttf"))
    return 0;
  WindowIcon = SDL_LoadImage("data/icon.png", 0);
  MainIconSet = LoadTexture("data/iconset.png", 0);
  WindowIconNotify = SDL_LoadImage("data/icon2.png", 0);
  SparklesIcon = SDL_CreateTextureFromSurface(ScreenRenderer, WindowIcon);
  MainDialog[GUIPOS_SPARKICON].dp = SparklesIcon;

  SDL_SetWindowIcon(window, WindowIcon);
  SDL_StartTextInput();
  return 1;
}

void RunMinimalGUI() {
  SDL_Event e;
  SDL_LockMutex(LockTabs);
  SDL_LockMutex(LockDialog);
  while(SDL_PollEvent(&e) != 0) {
    if(e.type == SDL_QUIT)
      quit = 1;
    else if (e.type == MainThreadEvent) {
      GUIDialog *Dialog;
      GUIState *State;
      GUIDialog *ChatEdit = FindDialogWithProc(NULL, NULL, &State, Widget_ChatEdit);
      struct TextEditInfo *EditInfo = (struct TextEditInfo*)ChatEdit->dp3;
      ClientTab *Tab;
      char *Temp;
      SDL_Keysym Key;
      char TempChar[2] = {0,0};
      switch(e.user.code) {
        case MTR_DIRTY_TABS:       //
          Dialog = FindDialogWithProc(NULL, NULL, NULL, Widget_ChannelTabs);
          if(Dialog)
            Dialog->flags |= D_DIRTY;
          break;
        case MTR_INFO_ADDED:       // d1 = context
          Tab = FindTab(e.user.data1);
          Dialog = FindDialogWithProc(NULL, NULL, NULL, Widget_ChatView);
          if(Dialog && Tab == (ClientTab*)Dialog->dp) {
            Dialog->flags |= D_DIRTY;
            RunWidget(MSG_INFO_ADDED, Dialog, State);
          } else
            Tab->UndrawnLines = 0;
          free(e.user.data1);
          break;
        case MTR_EDIT_CUT:         // d1 = context
          Tab = FindTab(e.user.data1);
          free(e.user.data1);
          State->MessageKey.SDL = MakeKeysym(&Key, SDLK_x, KMOD_CTRL);
          RunWidget(MSG_KEY, ChatEdit, State);
          break;
        case MTR_EDIT_COPY:        // d1 = context
          Tab = FindTab(e.user.data1);
          free(e.user.data1);
          State->MessageKey.SDL = MakeKeysym(&Key, SDLK_c, KMOD_CTRL);
          RunWidget(MSG_KEY, ChatEdit, State);
          break;
        case MTR_EDIT_DELETE:      // d1 = context
          Tab = FindTab(e.user.data1);
          free(e.user.data1);
          if(EditInfo->HasSelect) {
            State->MessageKey.SDL = MakeKeysym(&Key, SDLK_BACKSPACE, 0);
            RunWidget(MSG_KEY, ChatEdit, State);
          }
          break;
        case MTR_EDIT_PASTE:       // d1 = context
          Tab = FindTab(e.user.data1);
          free(e.user.data1);
          State->MessageKey.SDL = MakeKeysym(&Key, SDLK_v, KMOD_CTRL);
          RunWidget(MSG_KEY, ChatEdit, State);
          break;
        case MTR_EDIT_SELECT_ALL:  // d1 = context
          Tab = FindTab(e.user.data1);
          free(e.user.data1);
          State->MessageKey.SDL = MakeKeysym(&Key, SDLK_a, KMOD_CTRL);
          RunWidget(MSG_KEY, ChatEdit, State);
          break;
        case MTR_EDIT_INPUT_TEXT:  // d1 = context, d2 = text
          Tab = FindTab(e.user.data1);
          State->MessageText = e.user.data2;
          RunWidget(MSG_CHAR, ChatEdit, State);
          free(e.user.data1);
          free(e.user.data2);
          break;
        case MTR_EDIT_INPUT_CHAR:  // d1 = context, d2 = char
          Tab = FindTab(e.user.data1);
          free(e.user.data1);
          TempChar[0]=strtol(e.user.data2, NULL, 0);
          State->MessageText = TempChar;
          RunWidget(MSG_CHAR, ChatEdit, State);
          free(e.user.data2);
          break;
        case MTR_EDIT_SET_TEXT:    // d1 = context, d2 = text
          Tab = FindTab(e.user.data1);
          free(e.user.data1);
          State->MessageKey.SDL = MakeKeysym(&Key, SDLK_a, KMOD_CTRL);
          RunWidget(MSG_KEY, ChatEdit, State);
          State->MessageText = e.user.data2;
          RunWidget(MSG_CHAR, ChatEdit, State);
          free(e.user.data2);
          break;
        case MTR_EDIT_APPEND_TEXT: // d1 = context, d2 = text
          Tab = FindTab(e.user.data1);
          free(e.user.data1);
          State->MessageKey.SDL = MakeKeysym(&Key, SDLK_END, 0);
          RunWidget(MSG_KEY, ChatEdit, State);
          State->MessageText = e.user.data2;
          RunWidget(MSG_CHAR, ChatEdit, State);
          free(e.user.data2);
          break;
        case MTR_EDIT_SET_CURSOR:  // d1 = context, d2 = (int)pos
          Tab = FindTab(e.user.data1);
          free(e.user.data1);
          Temp = UnicodeForward((char*)ChatEdit->dp, strtol(e.user.data2, NULL, 0));
          if(Temp) {
            EditInfo->CursorChars = UnicodeLen((char*)ChatEdit->dp, Temp);
            EditInfo->CursorBytes = Temp;
            RunWidget(MSG_DRAW, ChatEdit, State);
          }
          free(e.user.data2);
          break;
        case MTR_GUI_COMMAND:      // d1 = context, d2 = command name
          Tab = FindTab(e.user.data1);
          free(e.user.data1);
          if(!strcasecmp(e.user.data2, "focus")) {
            Dialog = FindDialogWithProc(NULL, NULL, NULL, Widget_ChannelTabs);
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
    } else if (e.type == SDL_DROPFILE) { // for loading plugins eventually
      char *dropped_filedir = e.drop.file;
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "File dropped on window", dropped_filedir, NULL);
      QueueEvent("filedrop", dropped_filedir, "", 'E');

      SDL_free(dropped_filedir);
      break;
    } else if(e.type == SDL_MOUSEMOTION) {
      SetACursor = 0;
      if(GUIStates[CurWindow]->PriorityClickDialog && IsInsideWidget(e.button.x, e.button.y, GUIStates[CurWindow]->PriorityClickDialog))
        RunWidget(MSG_MOUSEMOVE, GUIStates[CurWindow]->PriorityClickDialog, GUIStates[CurWindow]);
      else if(GUIStates[CurWindow]->DragDialog && (e.motion.state & SDL_BUTTON_LMASK))
        RunWidget(MSG_DRAG, GUIStates[CurWindow]->DragDialog, GUIStates[CurWindow]);
      else for(int i=0;GUIStates[CurWindow]->Dialog[i].proc;i++) {
        if((&GUIStates[CurWindow]->Dialog[i] != GUIStates[CurWindow]->PriorityClickDialog
          && &GUIStates[CurWindow]->Dialog[i] != GUIStates[CurWindow]->DragDialog ) \
          && IsInsideWidget(e.motion.x, e.motion.y, &GUIStates[CurWindow]->Dialog[i]))
          RunWidget(MSG_MOUSEMOVE, &GUIStates[CurWindow]->Dialog[i], GUIStates[CurWindow]);
      }
      if(!SetACursor)
        GUI_SetCursor(SYSCURSOR_NORMAL);
     } else if(e.type == SDL_KEYDOWN) {
      // https://wiki.libsdl.org/SDL_KeyboardEvent
      GUIStates[CurWindow]->MessageKey.SDL = &e.key.keysym;
      ModKeys = e.key.keysym.mod;
      if(GUIStates[CurWindow]->PriorityClickDialog)
        RunWidget(MSG_KEY, GUIStates[CurWindow]->PriorityClickDialog, GUIStates[CurWindow]);
      else for(int i=0;GUIStates[CurWindow]->Dialog[i].proc;i++)
        if(GUIStates[CurWindow]->Dialog[i].flags & (D_GOTFOCUS | D_KEYWATCH))
          RunWidget(MSG_KEY, &GUIStates[CurWindow]->Dialog[i], GUIStates[CurWindow]);
    } else if(e.type == SDL_KEYUP) {
      ModKeys = e.key.keysym.mod;
    } else if(e.type == SDL_MOUSEBUTTONDOWN) {
      if(GUIStates[CurWindow]->PriorityClickDialog && IsInsideWidget(e.button.x, e.button.y, GUIStates[CurWindow]->PriorityClickDialog))
        RunWidget((e.button.clicks>1)?MSG_DCLICK:MSG_CLICK, GUIStates[CurWindow]->PriorityClickDialog, GUIStates[CurWindow]);
      else for(int i=0;GUIStates[CurWindow]->Dialog[i].proc;i++) {
        if(!(GUIStates[CurWindow]->Dialog[i].flags & D_HIDDEN)) {
          if(IsInsideWidget(e.button.x, e.button.y, &GUIStates[CurWindow]->Dialog[i]))
            RunWidget((e.button.clicks>1)?MSG_DCLICK:MSG_CLICK, &GUIStates[CurWindow]->Dialog[i], GUIStates[CurWindow]);
          else if(GUIStates[CurWindow]->Dialog[i].flags & D_WANTCLICKAWAY)
            RunWidget(MSG_CLICKAWAY, &GUIStates[CurWindow]->Dialog[i], GUIStates[CurWindow]);
       } 
     }
    } else if(e.type == SDL_MOUSEBUTTONUP) {
      GUI_SetCursor(SYSCURSOR_NORMAL);
      if(GUIStates[CurWindow]->DragDialog)
        RunWidget(MSG_DRAGDROPPED, GUIStates[CurWindow]->DragDialog, GUIStates[CurWindow]);
      GUIStates[CurWindow]->DragDialog = NULL;
    } else if(e.type == SDL_TEXTINPUT) {
      GUIStates[CurWindow]->MessageText = e.text.text;
      if(GUIStates[CurWindow]->PriorityClickDialog)
        RunWidget(MSG_CHAR, GUIStates[CurWindow]->PriorityClickDialog, GUIStates[CurWindow]);
      else for(int i=0;GUIStates[CurWindow]->Dialog[i].proc;i++)
        if(GUIStates[CurWindow]->Dialog[i].flags & D_GOTFOCUS)
          RunWidget(MSG_CHAR, &GUIStates[CurWindow]->Dialog[i], GUIStates[CurWindow]);
    } else if(e.type == SDL_WINDOWEVENT) {
      CurWindow = 0; //e.window.windowID - 1; // todo: fix on Linux?
      switch(e.window.event) {
        case SDL_WINDOWEVENT_SIZE_CHANGED:
          MainGUI.WinWidth = e.window.data1;
          MainGUI.WinHeight = e.window.data2;
          MainGUI.Flags|=GSF_NEED_REDRAW|GSF_NEED_UPDATE;
          break;

        case SDL_WINDOWEVENT_EXPOSED:
          MainGUI.Flags|=GSF_NEED_REDRAW;
          break;

        case SDL_WINDOWEVENT_ENTER: MouseFocus = 1; break;
        case SDL_WINDOWEVENT_LEAVE: MouseFocus = 0; break;
        case SDL_WINDOWEVENT_FOCUS_GAINED: KeyboardFocus = 1; break;
        case SDL_WINDOWEVENT_FOCUS_LOST: KeyboardFocus = 0; break;

        case SDL_WINDOWEVENT_MINIMIZED:
          Minimized = 1;
          break;

        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
          Minimized = 0;
          break;
      }
    }
  }
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
       SDL_RenderPresent(GUIStates[i]->Renderer);
     GUIStates[i]->Flags &= ~(GSF_NEED_INITIALIZE|GSF_NEED_REDRAW|GSF_NEED_PRESENT|GSF_NEED_UPDATE);
    }
  }
  for(int j=0;GUIStates[CurWindow]->Dialog[j].proc;j++) // fix dirty state
    if(GUIStates[CurWindow]->Dialog[j].flags & D_DIRTY)
      RunWidget(MSG_DRAW, &GUIStates[CurWindow]->Dialog[j], GUIStates[CurWindow]); // automatically clears D_DIRTY
  SDL_UnlockMutex(LockDialog);
  SDL_UnlockMutex(LockTabs);
}

int RunWidget(int msg,struct GUIDialog *d, GUIState *s) {
  int out = d->proc(msg, d, s);
  if(msg == MSG_DRAW)
    d->flags &= ~D_DIRTY;
  int i;
  if((d->flags & D_HIDDEN) && (msg != MSG_END) && (msg != MSG_START)) return 0;
  switch(out) {
    case D_REDRAW:
      s->Flags|=GSF_NEED_REDRAW;
      break;
    case D_REDRAWME:
      RunWidget(MSG_DRAW, d, s);
      break;
    case D_WANTFOCUS:
      for(i=0;s->Dialog[i].proc;i++) {
        if((&s->Dialog[i] != d) && (s->Dialog[i].flags & D_GOTFOCUS)) {
          s->Dialog[i].flags &= ~D_GOTFOCUS;
          RunWidget(MSG_LOSTFOCUS, &s->Dialog[i], s);
        }
      }
      s->DragDialog = d;
      d->flags |= D_GOTFOCUS;
      RunWidget(MSG_DRAW, d, s);
      break;
  }
  return out;
}

int Widget_ScreenClear(int msg,struct GUIDialog *d, GUIState *s) {
  if(UsingTUI) return TUI_ScreenClear(msg, d, s);
  SDL_Renderer *Renderer = s->Renderer;
  if(msg == MSG_START || msg == MSG_DRAW) {
    SDL_SetRenderDrawColor(Renderer, IRCColors[d->bg].r, IRCColors[d->bg].g, IRCColors[d->bg].b, 255);
    SDL_RenderClear(Renderer);
    s->Flags|=GSF_NEED_PRESENT;
  }
  return D_O_K;
}

int Widget_StaticImage(int msg,struct GUIDialog *d, GUIState *s) {
// dp=texture
  if(UsingTUI) return D_O_K;
  SDL_Renderer *Renderer = s->Renderer;
  if(msg == MSG_START || msg == MSG_DRAW) {
    if(!d->dp) return D_O_K;
    SDL_Rect Dst = {d->x, d->y, d->w, d->h};
    SDL_RenderCopy(Renderer, (SDL_Texture*)d->dp, NULL, &Dst);
    s->Flags|=GSF_NEED_PRESENT;
  }
  return D_O_K;
}

int Widget_StaticText(int msg,struct GUIDialog *d, GUIState *s) {
// dp=text, dp2=font, dp3=rendermode
  if(UsingTUI) return TUI_StaticText(msg, d, s);
  FontSet *Font = (FontSet*)d->dp2;
  int DrawY = d->y;
  if(d->h) DrawY = d->y+(d->h/2-Font->Height/2);
  SDL_Renderer *Renderer = s->Renderer;
  if(msg == MSG_START || msg == MSG_DRAW) {
    RenderText(Renderer, Font, d->x, DrawY, (RenderTextMode*)d->dp3, (const char*)d->dp);
    s->Flags|=GSF_NEED_PRESENT;
  }
  return D_O_K;
}

// TextEditInfo is in chat.h
int Widget_TextEdit(int msg,struct GUIDialog *d, GUIState *s) {
// d1=length, dp=text buffer, dp2=font, dp3=texteditinfo
  SDL_Renderer *Renderer = s->Renderer;
  FontSet *Font = (FontSet*)d->dp2;
  char *EditBuffer = (char*)d->dp;

  // this widget needs extra information, so it allocates it and stores a pointer at dp3
  struct TextEditInfo *EditInfo = (struct TextEditInfo*)d->dp3;
  if(!EditInfo) {
    EditInfo = (struct TextEditInfo*)malloc(sizeof(struct TextEditInfo));
    memset(EditInfo, 0, sizeof(struct TextEditInfo));
    EditInfo->ScrollBytes = EditBuffer;
    EditInfo->CursorBytes = EditBuffer;
    d->dp3 = EditInfo;
  }

  int CharHeight = Font->Height;
  int StartDrawX = d->x+2;
  int StartDrawY = d->y+(d->h/2-CharHeight/2);
  int NeedDraw = 0, NeedDeleteSelect =0;

  if(msg == MSG_MOUSEMOVE || msg == MSG_DRAG)
    GUI_SetCursor(SYSCURSOR_TEXT);

  if((msg == MSG_CLICK || msg == MSG_DCLICK) && (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT))) {
    *s->TempCmdTarget = 0;
    StartPopupCommandMenu(s, MenuTextEdit, NULL);
    return D_O_K;
  }

  int Paste = 0;
  if(msg == MSG_START)
    strcat(EditBuffer, " ");
  else if(msg == MSG_KEY) { // key press, but not text input
    int Key = s->MessageKey.SDL->sym;
    int Cut = (Key == SDLK_x && (s->MessageKey.SDL->mod & KMOD_CTRL)) || (Key == SDLK_DELETE && (s->MessageKey.SDL->mod & KMOD_SHIFT));
    int Copy = (Key == SDLK_c && (s->MessageKey.SDL->mod & KMOD_CTRL)) || (Key == SDLK_INSERT && (s->MessageKey.SDL->mod & KMOD_CTRL));
    Paste = SDL_HasClipboardText() && ((Key == SDLK_v && (s->MessageKey.SDL->mod & KMOD_CTRL)) || (Key == SDLK_INSERT && (s->MessageKey.SDL->mod & KMOD_SHIFT)));

    if(!EditInfo->HasSelect & (Key == SDLK_LEFT || Key == SDLK_RIGHT || Key == SDLK_HOME || Key == SDLK_END) && (s->MessageKey.SDL->mod & KMOD_SHIFT)) {
      EditInfo->SelectChars = EditInfo->CursorChars;
      EditInfo->SelectBytes = EditInfo->CursorBytes;
      EditInfo->HasSelect = 1;
    }

    if((Cut || Copy) && EditInfo->HasSelect) {
      if(Cut) NeedDeleteSelect = 1;
      int Len = EditInfo->CursorBytes - EditInfo->SelectBytes;
      int EditLen = strlen(EditBuffer);
      if(Len == EditLen-1) {
        char *Start = EditInfo->SelectBytes;
        if(Len < 0) {
           Len = EditInfo->SelectBytes - EditInfo->CursorBytes;
           Start = EditInfo->CursorBytes;
        }
        char Temp[Len+1];
        memcpy(Temp, Start, Len);
        Temp[Len] = 0;
        SDL_SetClipboardText(Temp);
      } else {
        char Temp[EditLen+1];
        strcpy(Temp, EditBuffer);
        Temp[EditLen-1] = 0;
        SDL_SetClipboardText(Temp);
      }
    } else if(Key == SDLK_a && (s->MessageKey.SDL->mod & KMOD_CTRL)) {
      EditInfo->SelectChars = 0;
      EditInfo->SelectBytes = EditBuffer;
      EditInfo->CursorChars = UnicodeLen(EditBuffer, NULL)-1;
      EditInfo->CursorBytes = UnicodeForward(EditBuffer, EditInfo->SelectChars);
      EditInfo->HasSelect = 1;
      NeedDraw = 1;
    } else if(EditInfo->HasSelect && Key == SDLK_LEFT && !(s->MessageKey.SDL->mod & KMOD_SHIFT)) {
      if(EditInfo->SelectChars < EditInfo->CursorChars) {
        EditInfo->CursorChars = EditInfo->SelectChars;
        EditInfo->CursorBytes = EditInfo->SelectBytes;
      }
      EditInfo->HasSelect = 0;
      NeedDraw = 1;
    } else if(EditInfo->HasSelect && Key == SDLK_RIGHT && !(s->MessageKey.SDL->mod & KMOD_SHIFT)) {
      if(EditInfo->SelectChars > EditInfo->CursorChars) {
        EditInfo->CursorChars = EditInfo->SelectChars;
        EditInfo->CursorBytes = EditInfo->SelectBytes;
      }
      EditInfo->HasSelect = 0;
      NeedDraw = 1;
    } else if(Key == SDLK_LEFT) {
      if(s->MessageKey.SDL->mod & KMOD_CTRL) { // prev word
        EditInfo->CursorBytes = UnicodeWordStep(EditInfo->CursorBytes, -1, EditBuffer);
        EditInfo->CursorChars = UnicodeLen(EditBuffer, EditInfo->CursorBytes);
        NeedDraw = 1;
      } else {
        char *Temp = UnicodeBackward(EditInfo->CursorBytes, 1, EditBuffer);
        if(Temp) {
          EditInfo->CursorChars--;
          EditInfo->CursorBytes = Temp;
          NeedDraw = 1;
        }
      }
    } else if(Key == SDLK_RIGHT) {
      if(!(s->MessageKey.SDL->mod & KMOD_SHIFT))
        EditInfo->HasSelect = 0;
      if(s->MessageKey.SDL->mod & KMOD_CTRL) { // next word
        EditInfo->CursorBytes = UnicodeWordStep(EditInfo->CursorBytes, 1, EditBuffer);
        EditInfo->CursorChars = UnicodeLen(EditBuffer, EditInfo->CursorBytes);
        NeedDraw = 1;
      } else {
        char *Temp = UnicodeForward(EditInfo->CursorBytes, 1);
        if(Temp) {
          EditInfo->CursorChars = UnicodeLen(EditBuffer, Temp);
          EditInfo->CursorBytes = Temp;
          NeedDraw = 1;
        }
      }
    } else if(Key == SDLK_HOME) {
      EditInfo->CursorChars = 0;
      EditInfo->CursorBytes = EditBuffer;
      NeedDraw = 1;
    } else if(Key == SDLK_END) {
      EditInfo->CursorChars = UnicodeLen(EditBuffer, NULL)-1;
      EditInfo->CursorBytes = UnicodeForward(EditBuffer, EditInfo->CursorChars-1);
      NeedDraw = 1;
    } else if(Key == SDLK_DELETE) {
      if(EditInfo->HasSelect) {
        NeedDeleteSelect = 1;
      } else if(s->MessageKey.SDL->mod & KMOD_CTRL) {
        char *Temp = UnicodeWordStep(EditInfo->CursorBytes, 1, EditBuffer);
        if(Temp) {
          memmove(EditInfo->CursorBytes, Temp, strlen(Temp)+1);
          NeedDraw = 1;
        }
      } else {
        char *Temp = UnicodeForward(EditInfo->CursorBytes, 1);
        if(Temp) {
          memmove(EditInfo->CursorBytes, Temp, strlen(Temp)+1);
          NeedDraw = 1;
        }
      }
    } else if(Key == SDLK_BACKSPACE || Key == SDLK_KP_BACKSPACE) {
      if(EditInfo->HasSelect) {
        NeedDeleteSelect = 1;
      } else if(s->MessageKey.SDL->mod & KMOD_CTRL) {
        char *Temp = EditInfo->CursorBytes;
        EditInfo->CursorBytes = UnicodeWordStep(EditInfo->CursorBytes, -1, EditBuffer);
        EditInfo->CursorChars = UnicodeLen(EditBuffer, EditInfo->CursorBytes);
        memmove(EditInfo->CursorBytes, Temp, strlen(Temp)+1);
        NeedDraw = 1;
      } else {
        char *Temp = UnicodeBackward(EditInfo->CursorBytes, 1, EditBuffer);
        if(Temp) {
          EditInfo->CursorChars--;
          memmove(Temp, EditInfo->CursorBytes, strlen(EditInfo->CursorBytes)+1);
          EditInfo->CursorBytes = Temp;
          NeedDraw = 1;
        }
      }
    }
    if(EditInfo->HasSelect & (Key == SDLK_LEFT || Key == SDLK_RIGHT))
      if(EditInfo->CursorChars == EditInfo->SelectChars)
        EditInfo->HasSelect = 0;
  }

  // cancel drag if it's not needed
  if((msg == MSG_DRAGDROPPED) && (EditInfo->CursorChars == EditInfo->SelectChars)) {
    EditInfo->HasSelect = 0;
    NeedDraw = 1;
  }

  // delete the section if necessary, like if we're inserting text
  if(EditInfo->HasSelect && (msg == MSG_CHAR || Paste || NeedDeleteSelect)) {
    if(EditInfo->CursorChars > EditInfo->SelectChars) { // swap if necessary
      int Temp1 = EditInfo->CursorChars;
      EditInfo->CursorChars = EditInfo->SelectChars;
      EditInfo->SelectChars = Temp1;
      char *Temp2 = EditInfo->CursorBytes;
      EditInfo->CursorBytes = EditInfo->SelectBytes;
      EditInfo->SelectBytes = Temp2;
    }
    int Len = UnicodeLen(EditBuffer, NULL);
    if(Len-1 == EditInfo->SelectChars) {
      *EditInfo->CursorBytes = ' ';
      EditInfo->CursorBytes[1] = 0;
    } else {
      memmove(EditInfo->CursorBytes, EditInfo->SelectBytes, strlen(EditInfo->SelectBytes)+1);
    }
    EditInfo->HasSelect = 0;
    NeedDraw = 1;
  }

  if(msg == MSG_CHAR || Paste) {
    // text input can come from either text input or from paste
    const char *Insert = s->MessageText;
    if(msg != MSG_CHAR)
      Insert = SDL_GetClipboardText();
    if((strlen(Insert)+strlen(EditBuffer))<(unsigned)d->d1) { // only succeed if it will fit
      memmove(EditInfo->CursorBytes+strlen(Insert), EditInfo->CursorBytes, strlen(EditInfo->CursorBytes)+1);
      memcpy(EditInfo->CursorBytes, Insert, strlen(Insert));
      EditInfo->CursorBytes = UnicodeForward(EditInfo->CursorBytes, UnicodeLen(Insert, NULL));
      EditInfo->CursorChars += UnicodeLen(Insert, NULL);
      NeedDraw = 1;
    }
  } else if(msg == MSG_CLICK || msg == MSG_DCLICK || msg == MSG_DRAG) {
    int CursorX, CursorY;
    SDL_GetMouseState(&CursorX, &CursorY);
    CursorX -= StartDrawX;
    CursorY -= StartDrawY;
    
    int Chars = (CursorX/Font->Width)+EditInfo->ScrollChars;
    char *Temp = UnicodeForward(EditBuffer, Chars);

    if(Temp) {
      int NoGUI_SetCursorChars = 0;

      if(msg == MSG_CLICK || msg == MSG_DCLICK) {
        if(SDL_GetModState() & KMOD_SHIFT) {
          if(EditInfo->HasSelect) { // set whichever start or end is closer
            if(abs(EditInfo->SelectChars - Chars) < abs(EditInfo->CursorChars - Chars)) {
              EditInfo->SelectChars = Chars;
              EditInfo->SelectBytes = Temp;
              NoGUI_SetCursorChars = 1;
            } else {
              EditInfo->CursorChars = Chars;
              EditInfo->CursorBytes = Temp;
            }
          } else {
            EditInfo->HasSelect = 1;
            EditInfo->SelectChars = EditInfo->CursorChars;
            EditInfo->SelectBytes = EditInfo->CursorBytes;
          }
        } else
          EditInfo->HasSelect = 0;
      } else if(msg == MSG_DRAG && !EditInfo->HasSelect) {
        EditInfo->HasSelect = 1;
        EditInfo->SelectChars = EditInfo->CursorChars;
        EditInfo->SelectBytes = EditInfo->CursorBytes;
      }
      if(!NoGUI_SetCursorChars) {
        EditInfo->CursorChars = Chars;
        EditInfo->CursorBytes = Temp;
      }
    } else {
      if(CursorX > 0) {
        EditInfo->CursorChars = UnicodeLen(EditBuffer, NULL)-1;
        EditInfo->CursorBytes = UnicodeForward(EditBuffer, EditInfo->CursorChars);
      } else {
        EditInfo->CursorChars = 0;
        EditInfo->CursorBytes = EditBuffer;
      }
    }
    // Not setting NeedDraw because it will get sent a draw signal after D_WANTFOCUS
    return D_WANTFOCUS;
  }

  // keep the cursor inside the text box
  int CursorInBox = EditInfo->CursorChars-EditInfo->ScrollChars;
  int MaxInBox = (d->w/Font->Width);
  if(CursorInBox < 0) {
    EditInfo->ScrollChars = EditInfo->CursorChars;
    EditInfo->ScrollBytes = EditInfo->CursorBytes;
  } else if(CursorInBox >= MaxInBox) { // max chars
    EditInfo->ScrollChars = EditInfo->CursorChars - MaxInBox + 1;
    EditInfo->ScrollBytes = UnicodeForward(EditBuffer, EditInfo->ScrollChars);
  }

  if(NeedDraw||msg == MSG_START||msg==MSG_DRAW||msg==MSG_LOSTFOCUS) {
    if(msg == MSG_LOSTFOCUS)
      EditInfo->HasSelect = 0;

    // clear out the inside
    SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_BG].r, IRCColors[IRCCOLOR_BG].g, IRCColors[IRCCOLOR_BG].b, 255);
    SDL_Rect FillRect = {d->x+1, d->y+1, d->w-2, d->h-2};
    SDL_RenderFillRect(Renderer, &FillRect);

    // draw the border
    if(d->flags & D_GOTFOCUS)
      SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_ACTIVE].r, IRCColors[IRCCOLOR_ACTIVE].g, IRCColors[IRCCOLOR_ACTIVE].b, 255);
    else
      SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_INACTIVE].r, IRCColors[IRCCOLOR_INACTIVE].g, IRCColors[IRCCOLOR_INACTIVE].b, 255);
    SDL_Rect Rect = {d->x, d->y, d->w, d->h};
    SDL_RenderDrawRect(Renderer, &Rect);

    // draw the text
    SDL_Rect Clip = {d->x+1, d->y, d->w-4, d->y+d->h};
    SDL_RenderSetClipRect(Renderer, &Clip);
    RenderSimpleText(Renderer, Font, StartDrawX, StartDrawY, 0, EditInfo->ScrollBytes);

    // draw the cursor if needed
    if(d->flags & D_GOTFOCUS) {
      SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_CURSOR].r, IRCColors[IRCCOLOR_CURSOR].g, IRCColors[IRCCOLOR_CURSOR].b, 255);
      int CursorLineX = (StartDrawX-1)+(EditInfo->CursorChars-EditInfo->ScrollChars)*Font->Width;
      SDL_RenderDrawLine(Renderer, CursorLineX, d->y, CursorLineX, d->y+d->h-1);
    }

    if(EditInfo->HasSelect) {
      int SelX1 = d->x+1+(EditInfo->CursorChars-EditInfo->ScrollChars)*Font->Width;
      int SelX2 = d->x+2+(EditInfo->SelectChars-EditInfo->ScrollChars)*Font->Width;
      if(SelX1 > SelX2) {
        int Temp = SelX1;
        SelX1 = SelX2;
        SelX2 = Temp;
      }
      SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_SELECT].r, IRCColors[IRCCOLOR_SELECT].g, IRCColors[IRCCOLOR_SELECT].b, 255);
      SDL_Rect Select = {SelX1, d->y+1, SelX2-SelX1, d->h-2};
      SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_MOD);
      SDL_RenderFillRect(Renderer, &Select);
      SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_NONE);
      SDL_RenderDrawRect(Renderer, &Select);
    }
    SDL_RenderSetClipRect(Renderer, NULL);

    s->Flags|=GSF_NEED_PRESENT;
  }
  if(msg == MSG_END)
    free(EditInfo);
  return D_O_K;
}

void StartPopupCommandMenu(GUIState *s, cJSON *JSON, char *Include) {
  GUIDialog *d = NULL;
  int i;
  for(i=0;s->Dialog[i].proc && s->Dialog[i].proc != Widget_PopupMenu;i++);
  if(!s->Dialog[i].proc) return;
  d = &s->Dialog[i];
  d->flags &= ~D_HIDDEN;
  d->dp = JSON;
  d->dp2 = Include;
  SDL_GetMouseState(&d->x, &d->y);
  d->x -= 5;
  RunWidget(MSG_ACTIVATE, d, s);
}

const char *PopupItemName(const char *String) {
  const char *Name = String;
  const char *Check = strchr(Name, '>');
  if(Check && Check[1]) Name = Check+1;
  return Name;
}
int PopupItemPassesFilter(const char *Item, const char *Available) {
  if(!strchr(Item, '>')) return 1;
  if(!Available) return 0;
  char Temp[strlen(Item)];
  strcpy(Temp, Item+1);
  char *Poke = strchr(Temp, '>');
  *Poke = 0;
  if(strstr(Available, Temp)) return 1;
  return 0;
}
int Widget_PopupMenu(int msg,struct GUIDialog *d, GUIState *s) {
// d1=tab id, dp=json, dp2=include, dp3=menuinfo
  if(UsingTUI) return TUI_PopupMenu(msg, d, s);
  SDL_Renderer *Renderer = s->Renderer;
  struct MenuState *MenuInfo = (struct MenuState*)d->dp3;
  int RowHeight = ChatFont.Height+4;
  int NeedRedrawHilight = 0;
  int CursorX, CursorY;
  SDL_GetMouseState(&CursorX, &CursorY);

  if(!MenuInfo) {
    MenuInfo = (struct MenuState*)malloc(sizeof(struct MenuState));
    memset(MenuInfo, 0, sizeof(struct MenuState));
    d->dp3 = MenuInfo;
  }
  int Setup = 0;
  if(msg == MSG_ACTIVATE) {
    MenuInfo->Target = NULL;
    if(d->dp2) {
      strlcpy(MenuInfo->Include, (char*)d->dp2, sizeof(MenuInfo->Include));
      char *Fix = strchr(MenuInfo->Include, '|');
      if(Fix) {
        *Fix = 0;
        MenuInfo->Target = Fix+1;
      }
    } else
      *MenuInfo->Include = 0;

    MenuInfo->CurLevel = 0;
    MenuInfo->NumOptions = 0;
    MenuInfo->Menus[0] = (cJSON*)d->dp;
    MenuInfo->Menus[0] = MenuInfo->Menus[0]->child;
    MenuInfo->X = d->x;
    MenuInfo->Y = d->y;
    MenuInfo->Ticks = SDL_GetTicks();
    s->PriorityClickDialog = d;
    Setup = 1;
  }

  if(msg == MSG_CLICKAWAY) {
    d->flags |= D_HIDDEN;
    s->Flags |= GSF_NEED_REDRAW;
    s->PriorityClickDialog = NULL;
    return D_REDRAW;
  }
  if(msg == MSG_MOUSEMOVE) {
    int Which = ((CursorY-d->y-1) / RowHeight);
    if(Which >= 0) {
      if(Which-MenuInfo->CurLevel != MenuInfo->CurHilight) NeedRedrawHilight = 1;
      MenuInfo->CurHilight = Which-MenuInfo->CurLevel;
    }
  }

  if(msg == MSG_KEY) {
    int Key = s->MessageKey.SDL->sym;
    if(Key == SDLK_UP) {
      if(MenuInfo->CurHilight-1 >= -MenuInfo->CurLevel)
        MenuInfo->CurHilight--;
      else
        MenuInfo->CurHilight = MenuInfo->NumOptions-1;
      NeedRedrawHilight = 1;
    } else if(Key == SDLK_DOWN) {
      if(MenuInfo->CurHilight+1 < MenuInfo->NumOptions)
        MenuInfo->CurHilight++;
      else
        MenuInfo->CurHilight = -MenuInfo->CurLevel;
      NeedRedrawHilight = 1;
    }
  }
  if((msg == MSG_CLICK || msg == MSG_DCLICK || (msg == MSG_KEY && s->MessageKey.SDL->sym == SDLK_RETURN))&&(MenuInfo->Ticks+2 < SDL_GetTicks())) {
    if(MenuInfo->CurHilight >= 0) {
      cJSON *Temp = MenuInfo->Menus[MenuInfo->CurLevel];
      for(int i=0;i!=MenuInfo->CurHilight;i++) {
        if(!PopupItemPassesFilter(Temp->string, MenuInfo->Include)) i--;
        Temp = Temp->next;
      }
      //SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", Temp->string);
      if(Temp->child) { // has submenu
        MenuInfo->Picked[MenuInfo->CurLevel] = PopupItemName(Temp->string);
        MenuInfo->Menus[++MenuInfo->CurLevel] = Temp->child;
        Setup = 1;
      } else { // menu option
        //SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", Temp->valuestring);
        d->flags |= D_HIDDEN;
        s->Flags |= GSF_NEED_REDRAW;
        s->PriorityClickDialog = NULL;
        char Command[strlen(Temp->valuestring)+1];
        strcpy(Command, Temp->valuestring);
        char *CmdArgs = strchr(Command, ' ');
        if(CmdArgs)
          *(CmdArgs++) = 0;

        // todo: allow a context to be specified for the popup
        const char *Context = "";
        GUIDialog *ChannelTabs = FindDialogWithProc(NULL, s, NULL, Widget_ChannelTabs);
        char ContextBuffer[CONTEXT_BUF_SIZE];
        if(*s->TempCmdTarget)
          Context = s->TempCmdTarget;
        else if(ChannelTabs && (ClientTab*)ChannelTabs->dp)
          Context = ContextForTab((ClientTab*)ChannelTabs->dp, ContextBuffer);
        QueueEvent(Command, CmdArgs, Context, 'C');
        return D_REDRAW;
      }
    } else if(abs(MenuInfo->CurHilight)-1 < MenuInfo->CurLevel) {
      MenuInfo->CurLevel+=MenuInfo->CurHilight;
      Setup = 1;
    }
  }

  if(Setup) {
    MenuInfo->CurHilight = 0;
    cJSON *Temp = MenuInfo->Menus[MenuInfo->CurLevel];
    int Widest = 0;
    for(MenuInfo->NumOptions = 0;Temp;Temp=Temp->next) {
      if(!PopupItemPassesFilter(Temp->string, MenuInfo->Include)) continue;
      MenuInfo->NumOptions++;
      int Len = VisibleStrLen(PopupItemName(Temp->string), 0);
      if(Len>Widest) Widest = Len;
    }
    for(int i=0;i<MenuInfo->CurLevel;i++) {
      int Len = VisibleStrLen(PopupItemName(MenuInfo->Picked[i]), 0);
      if(Len>Widest) Widest = Len;      
    }
    d->w = (Widest+1) * ChatFont.Width + 4;
    d->h = MenuInfo->NumOptions*RowHeight + MenuInfo->CurLevel*RowHeight;
//    d->y = (MenuInfo->Y - d->h);
    if(d->y+d->h > s->WinHeight) {
      d->y = s->WinHeight-d->h;
      MenuInfo->Y = d->y;
    }
    SDL_WarpMouseInWindow(SDL_GetMouseFocus(), CursorX, MenuInfo->Y+RowHeight/2+MenuInfo->CurLevel*RowHeight);
  }
  if((msg == MSG_DRAW || NeedRedrawHilight) && !(d->flags & D_HIDDEN)) {
    SDL_Rect Rect = {d->x, d->y, d->w, d->h};
    SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_BG].r, IRCColors[IRCCOLOR_BG].g, IRCColors[IRCCOLOR_BG].b, 255);
    SDL_RenderFillRect(Renderer, &Rect);
    SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_BORDER].r, IRCColors[IRCCOLOR_BORDER].g, IRCColors[IRCCOLOR_BORDER].b, 255);
    SDL_RenderDrawRect(Renderer, &Rect);

    cJSON *Temp = MenuInfo->Menus[MenuInfo->CurLevel];
    int Num = 0;
    for(;Temp;Temp=Temp->next) {
      if(!PopupItemPassesFilter(Temp->string, MenuInfo->Include)) continue;
      RenderText(Renderer, &ChatFont, d->x+2, d->y+(Num+MenuInfo->CurLevel)*RowHeight+1, NULL, PopupItemName(Temp->string));
      if(Temp->child)
        RenderSimpleText(Renderer, &ChatFont, d->x+d->w-ChatFont.Width-1, d->y+(Num+MenuInfo->CurLevel)*RowHeight+1, 0, "➔");
      Num++;
    }

    // draw list of previous menu items
    int BackStartY = d->y+1;//MenuInfo->NumOptions*RowHeight+1;
    for(int i=0;i<MenuInfo->CurLevel;i++)
      RenderText(Renderer, &ChatFont, d->x+2, BackStartY+(i*RowHeight), NULL, PopupItemName(MenuInfo->Picked[MenuInfo->CurLevel-i-1]));
    SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_BG].r, IRCColors[IRCCOLOR_BG].g, IRCColors[IRCCOLOR_BG].b, 255);

    SDL_Rect BackRect = {d->x, BackStartY, d->w, d->h-(BackStartY-d->y)};
    SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_MOD);
    SDL_RenderFillRect(Renderer, &BackRect);
    // draw hilight
    SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_SELECT].r, IRCColors[IRCCOLOR_SELECT].g, IRCColors[IRCCOLOR_SELECT].b, 255);
    SDL_Rect HilightRect = {d->x, MenuInfo->Y+MenuInfo->CurLevel*RowHeight+(MenuInfo->CurHilight+1)*RowHeight, d->w, -RowHeight};
    SDL_RenderFillRect(Renderer, &HilightRect);

    SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_NONE);

    if(MenuInfo->CurLevel) {
      SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_BORDER].r, IRCColors[IRCCOLOR_BORDER].g, IRCColors[IRCCOLOR_BORDER].b, 255);
      BackStartY = d->y + MenuInfo->CurLevel*RowHeight;
      SDL_RenderDrawLine(Renderer, d->x, BackStartY, d->x+d->w-1, BackStartY);
    }

    s->Flags|=GSF_NEED_PRESENT;
  }

  if(msg == MSG_END)
    free(MenuInfo);
  return (!Setup)?D_O_K:D_REDRAW;
}

int RunScrollbar(int msg, GUIDialog *d, GUIState *s, int *Scroll, int NumOptions, int StartX) {
  int CursorX, CursorY;
  SDL_GetMouseState(&CursorX, &CursorY);

  if((msg == MSG_CLICK || msg == MSG_DCLICK) && (CursorX>StartX) && (CursorX<(StartX+ScrollbarW))) {
    s->DragDialog = d;
    return SCROLLBAR_HAS_MOUSE;
  }
  int ScrollChanged = 0;
  if(msg == MSG_DRAG) {
    float ScrollPosition = (float)((d->y+d->h)-CursorY)/((float)d->h-2);
    int OldScroll = *Scroll;
    ScrollPosition *= (float)NumOptions;
    *Scroll = (int)ScrollPosition;
    if(*Scroll < 0) *Scroll = 0;
    if(*Scroll >= NumOptions) *Scroll = NumOptions-1;
    if(*Scroll == OldScroll)
      return SCROLLBAR_HAS_MOUSE;
    else
      ScrollChanged = 1;
  }
  if(msg == MSG_DRAW || ScrollChanged) {
    SDL_Renderer *Renderer = s->Renderer;
    SDL_RenderDrawLine(Renderer, StartX, d->y, StartX, d->y+d->h-1);

    SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_BG].r, IRCColors[IRCCOLOR_BG].g, IRCColors[IRCCOLOR_BG].b, 255);
    SDL_Rect FillScrollbar = {StartX+1, d->y+1, ScrollbarW-2, d->h-2};
    SDL_RenderFillRect(Renderer, &FillScrollbar);

    SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_CURSOR].r, IRCColors[IRCCOLOR_CURSOR].g, IRCColors[IRCCOLOR_CURSOR].b, 255);
    float ScrollPosition = (float)*Scroll/((float)NumOptions);
    float ScrollPosition2 = ((float)*Scroll+1)/((float)NumOptions);
    ScrollPosition *= (float)d->h-2;
    ScrollPosition2 *= (float)d->h-2;
    int Height = (int)(ScrollPosition2-ScrollPosition);
    if(!Height) Height = 1;
    SDL_Rect ScrollRect = {StartX, d->y+d->h-1-(int)ScrollPosition2, ScrollbarW-1, Height};
    SDL_RenderFillRect(Renderer, &ScrollRect);
  }
  if(ScrollChanged)
    return SCROLLBAR_SCROLL_CHANGE;
  return SCROLLBAR_NONE;
}

int MessageHeight(ClientMessage *Msg, char *PreStripped, RenderTextMode *ModeRight) {
  int RightLen = strlen(Msg->Right);
  StripIRCText(PreStripped,Msg->Right,0);
  if(Msg->RightExtend) StripIRCText(PreStripped+RightLen,Msg->RightExtend,0);
  ModeRight->AlreadyStripped = PreStripped;
  return SimulateWordWrap(&ChatFont, ModeRight, Msg->Right);
}

char *MsgRightOrExtended(ClientMessage *Msg) {
  static char Buffer[INPUTBOX_SIZE];
  if(!Msg->RightExtend) return Msg->Right;
  strcpy(Buffer, Msg->Right);
  strcpy(Buffer+strlen(Msg->Right), Msg->RightExtend);
  return Buffer;
}

typedef struct ChatViewState {
  int Scroll;
  int LeftRightSplit;
  int DragType;
  int Drawn;
  SDL_Texture *Texture, *Texture2;
} ChatViewState;
int Widget_ChatView(int msg,struct GUIDialog *d, GUIState *s) {
// dp=tab dp3=chatviewinfo
  if(UsingTUI) return TUI_ChatView(msg, d, s);
  SDL_Renderer *Renderer = s->Renderer;
  struct ChatViewState *ChatInfo = (struct ChatViewState*)d->dp3;
  ClientTab *Tab = (ClientTab*)d->dp;
  //int RowHeight = ChatFont.Height+4;
  int CursorX, CursorY, NeedRedraw = 0, InitTexture = 0;
  SDL_GetMouseState(&CursorX, &CursorY);

  if(!ChatInfo) {
    ChatInfo = (struct ChatViewState*)malloc(sizeof(struct ChatViewState));
    memset(ChatInfo, 0, sizeof(struct ChatViewState));
    d->dp3 = ChatInfo;
    ChatInfo->LeftRightSplit = 230;
    InitTexture = 2;
  }
  int OldScroll = ChatInfo->Scroll;
  int LeftRightSplitStartX = d->x+ChatInfo->LeftRightSplit-4;
  int LeftRightSplitEndX = d->x+ChatInfo->LeftRightSplit+4;

  if(msg == MSG_ACTIVATE) { // dp was changed to a new tab
    ChatInfo->Scroll = 0;
    InitTexture = 1;
    ChatInfo->Drawn = 0;
  } else if(msg == MSG_WIN_RESIZE)
    InitTexture = 2;
  if(InitTexture) {
    if(InitTexture == 2) {
      if(ChatInfo->Texture) SDL_DestroyTexture(ChatInfo->Texture);
      if(ChatInfo->Texture2) SDL_DestroyTexture(ChatInfo->Texture2);
      ChatInfo->Texture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, d->w-ScrollbarW-2, d->h-2);
      ChatInfo->Texture2 = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, d->w-ScrollbarW-2, d->h-2);
    }
	SDL_SetRenderTarget(Renderer, ChatInfo->Texture);
    SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_BG].r, IRCColors[IRCCOLOR_BG].g, IRCColors[IRCCOLOR_BG].b, 255);
    SDL_RenderClear(Renderer);
	SDL_SetRenderTarget(Renderer, NULL);
    ChatInfo->Drawn = 0;
    return D_REDRAW;
  }

  int ScrolledAmount = 0;
  if(Tab && (msg!=MSG_DRAG || ChatInfo->DragType)) {
    int OldScroll = ChatInfo->Scroll;
    int ScrollReturn = RunScrollbar(msg, d, s, &ChatInfo->Scroll, Tab->NumMessages, d->x+d->w-ScrollbarW);
    if(ScrollReturn == SCROLLBAR_HAS_MOUSE)
      ChatInfo->DragType = 1;
    else if(ScrollReturn == SCROLLBAR_SCROLL_CHANGE) {
      ScrolledAmount = ChatInfo->Scroll-OldScroll;
      NeedRedraw = 1;
      ChatInfo->Drawn = 1;
    }
  }
  if(msg == MSG_INFO_ADDED) {
    // to do: fix crashes when updating enough times that it exhausts the scrollback
    int Undrawn = Tab->UndrawnLines;
    Tab->UndrawnLines = 0;

    if(!ChatInfo->Scroll) {
      OldScroll += Undrawn;
      ScrolledAmount = -Undrawn;
    } else
      ChatInfo->Scroll += Undrawn;
    NeedRedraw = 1;
    ChatInfo->Drawn = 1;
  }

  if(msg == MSG_DRAGDROPPED) {
    if(!ChatInfo->DragType) {
      ChatInfo->LeftRightSplit = CursorX-d->x;
      ChatInfo->Drawn = 0;
    }
    return D_REDRAW;
  }

  if((msg == MSG_DRAG && !ChatInfo->DragType) || (msg == MSG_MOUSEMOVE && CursorX>LeftRightSplitStartX && CursorX<LeftRightSplitEndX))
    GUI_SetCursor(SYSCURSOR_SIZE_EW);

  if((msg == MSG_CLICK || msg == MSG_DCLICK) && CursorX>LeftRightSplitStartX && CursorX<LeftRightSplitEndX) {
    s->DragDialog = d; ChatInfo->DragType = 0;
  }

  if(NeedRedraw || msg == MSG_START || msg == MSG_ACTIVATE || msg == MSG_DRAW) {
    int ChatW, ChatH;
    SDL_QueryTexture(ChatInfo->Texture, NULL, NULL, &ChatW, &ChatH);

    SDL_SetRenderTarget(Renderer, ChatInfo->Texture);
    if(!ChatInfo->Drawn) {
      SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_BG].r, IRCColors[IRCCOLOR_BG].g, IRCColors[IRCCOLOR_BG].b, 255);
      SDL_RenderClear(Renderer);
    }
    if(Tab && Tab->Messages && ChatInfo->Drawn!=2) {
      int DrawY = d->h-1, CurMessage;
      char TimestampBuffer[80];
      RenderTextMode ModeLeft = {ChatInfo->LeftRightSplit-1, 0, 0, RENDERMODE_RIGHT_JUSTIFY};
      RenderTextMode ModeRight = {d->w-(ChatInfo->LeftRightSplit+2)-ScrollbarW, 0, 0, 0};
      const char *TimestampFormat = GetConfigStr("(%I:%M%p) ", "ChatView/TimeStampDisplayFormat");
      char PreStripped[INPUTBOX_SIZE];
      if(!ChatInfo->Drawn) { // full redraw
        for(CurMessage=Tab->NumMessages-1-ChatInfo->Scroll;CurMessage>=0;CurMessage--) {
          ClientMessage *Msg = &Tab->Messages[CurMessage];
          struct tm *TimeInfo = localtime(&Msg->Time);
          strftime(TimestampBuffer,sizeof(TimestampBuffer),TimestampFormat,TimeInfo);

          DrawY -= MessageHeight(Msg, PreStripped, &ModeRight);
          RenderText(Renderer, &ChatFont, 0, DrawY, NULL, TimestampBuffer);
          RenderText(Renderer, &ChatFont, ChatInfo->LeftRightSplit-1, DrawY, &ModeLeft, Msg->Left);
          RenderText(Renderer, &ChatFont, ChatInfo->LeftRightSplit+2, DrawY, &ModeRight, MsgRightOrExtended(Msg));
          if(DrawY < 0) break;
        }
      } else { // scroll
        if(ScrolledAmount > 0) { // scroll up
          for(int ScrollI = 0; ScrollI < ScrolledAmount; ScrollI++, OldScroll++) {
            ChatInfo->Scroll = OldScroll+1;

            int CopyHeight = 0;
            for(int i=OldScroll;i<ChatInfo->Scroll;i++)
              CopyHeight += MessageHeight(&Tab->Messages[Tab->NumMessages-1-i], PreStripped, &ModeRight);
            // scroll up (SDL_RenderCopy seems not to account for overlapping rectangles)
            SDL_SetRenderTarget(Renderer, ChatInfo->Texture2);
            SDL_Rect CopyRect = {0, 0, ChatW, ChatH-CopyHeight};
            SDL_RenderCopy(Renderer, ChatInfo->Texture, &CopyRect, &CopyRect);
            SDL_SetRenderTarget(Renderer, ChatInfo->Texture);
            SDL_Rect CopyRect2 = {0, CopyHeight, ChatW, ChatH-CopyHeight};
            SDL_RenderCopy(Renderer, ChatInfo->Texture2, &CopyRect, &CopyRect2);
            SDL_Rect ClearRect = {0, 0, ChatW, CopyHeight};
            SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_BG].r, IRCColors[IRCCOLOR_BG].g, IRCColors[IRCCOLOR_BG].b, 255);
  //          SDL_SetRenderDrawColor(Renderer, rand()&255, rand()&255, rand()&255, 255);
            SDL_RenderFillRect(Renderer, &ClearRect);

            // render new text at the top
            int DrawY = ChatH;
            for(int i=Tab->NumMessages-1-ChatInfo->Scroll;i>=0;i--) {
              DrawY -= MessageHeight(&Tab->Messages[i], PreStripped, &ModeRight);
              if(DrawY <= 0 || !i) {
                for(int j=0;j<Tab->NumMessages;j++) {
                  ClientMessage *Msg = &Tab->Messages[i+j];
                  struct tm *TimeInfo = localtime(&Msg->Time);
                  strftime(TimestampBuffer,sizeof(TimestampBuffer),TimestampFormat,TimeInfo); 

                  int Height = MessageHeight(Msg, PreStripped, &ModeRight);
                  RenderText(Renderer, &ChatFont, 0, DrawY, NULL, TimestampBuffer);
                  RenderText(Renderer, &ChatFont, ChatInfo->LeftRightSplit-1, DrawY, &ModeLeft, Msg->Left);
                  RenderText(Renderer, &ChatFont, ChatInfo->LeftRightSplit+2, DrawY, &ModeRight, MsgRightOrExtended(Msg));
                  DrawY += Height;
//                  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s %i", Msg->Right, ModeRight.DrawnHeight);
                  if(DrawY >= CopyHeight) break;
                }
                break;
              }
            }
          }
        } else if(ScrolledAmount < 0) { // scroll down
          for(int ScrollI = 0; ScrollI < abs(ScrolledAmount); ScrollI++, OldScroll--) {
            ChatInfo->Scroll = OldScroll-1;

            int CopyHeight = 0;
            for(int i=OldScroll-1;i>=ChatInfo->Scroll;i--)
              CopyHeight += MessageHeight(&Tab->Messages[Tab->NumMessages-1-i], PreStripped, &ModeRight);
            // scroll down (SDL_RenderCopy seems not to account for overlapping rectangles)
            SDL_SetRenderTarget(Renderer, ChatInfo->Texture2);
            SDL_Rect CopyRect = {0, CopyHeight, ChatW, ChatH-CopyHeight};
            SDL_RenderCopy(Renderer, ChatInfo->Texture, &CopyRect, &CopyRect);
            SDL_SetRenderTarget(Renderer, ChatInfo->Texture);
            SDL_Rect CopyRect2 = {0, 0, ChatW, ChatH-CopyHeight};
            SDL_RenderCopy(Renderer, ChatInfo->Texture2, &CopyRect, &CopyRect2);
            SDL_Rect ClearRect = {0, ChatH-CopyHeight, ChatW, CopyHeight};
            SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_BG].r, IRCColors[IRCCOLOR_BG].g, IRCColors[IRCCOLOR_BG].b, 255);
            SDL_RenderFillRect(Renderer, &ClearRect);

            // render new text at the bottom
            CopyHeight = ChatH-CopyHeight+1;
            for(int i=OldScroll-1;i>=ChatInfo->Scroll;i--) {
              ClientMessage *Msg = &Tab->Messages[Tab->NumMessages-1-i];
              struct tm *TimeInfo = localtime(&Msg->Time);
              strftime(TimestampBuffer,sizeof(TimestampBuffer),TimestampFormat,TimeInfo); 

              RenderText(Renderer, &ChatFont, 0, CopyHeight-1, NULL, TimestampBuffer); //-1 fixes being drawn 1 pixel too low
              RenderText(Renderer, &ChatFont, ChatInfo->LeftRightSplit-1, CopyHeight-1, &ModeLeft, Msg->Left);
              RenderText(Renderer, &ChatFont, ChatInfo->LeftRightSplit+2, CopyHeight-1, &ModeRight, MsgRightOrExtended(Msg));
              CopyHeight += ModeRight.DrawnHeight;
            }
          }
        }
      }

      ChatInfo->Drawn = 2;
      SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_BORDER].r, IRCColors[IRCCOLOR_BORDER].g, IRCColors[IRCCOLOR_BORDER].b, 255);
      SDL_RenderDrawLine(Renderer, ChatInfo->LeftRightSplit, 0, ChatInfo->LeftRightSplit, d->h-1);
    }

	SDL_SetRenderTarget(Renderer, NULL);

    SDL_Rect DstRect = {d->x+1, d->y+1, ChatW, ChatH};
    SDL_RenderCopy(Renderer, ChatInfo->Texture, NULL, &DstRect);
    SDL_RenderSetClipRect(Renderer, NULL);

// do stuff for this texture
    SDL_Rect ClipRect = {d->x+1, d->y+1, d->w-2, d->h-2};
    SDL_Rect Rect = {d->x, d->y, d->w, d->h};
    SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_BORDER].r, IRCColors[IRCCOLOR_BORDER].g, IRCColors[IRCCOLOR_BORDER].b, 255);
    SDL_RenderDrawRect(Renderer, &Rect);
    SDL_RenderSetClipRect(Renderer, &ClipRect);

// do stuff for the chatview texture
    SDL_RenderSetClipRect(Renderer, NULL);
    Rect.w -= ScrollbarW;

    s->Flags|=GSF_NEED_PRESENT;
  }

  if(msg == MSG_END) {
    SDL_DestroyTexture(ChatInfo->Texture);
    SDL_DestroyTexture(ChatInfo->Texture2);
    free(ChatInfo);
  }
  return D_O_K;
}

int Widget_ChatEdit(int msg,struct GUIDialog *d, GUIState *s) {
// wrapper for Widget_TextEdit
  if(UsingTUI) return TUI_ChatEdit(msg, d, s);
  char InsertChar[4]={0,0,0,0};
  int Return = Widget_TextEdit(msg, d, s);

  struct TextEditInfo *EditInfo = (struct TextEditInfo*)d->dp3;
  char *EditBuffer = (char*)d->dp;

  if(msg == MSG_KEY) {
    int Key = s->MessageKey.SDL->sym;
    if(Key == SDLK_RETURN && strcmp(EditBuffer, " ")) {
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
      EditInfo->HasSelect = 0;
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
        else
          CmdArgs = "";
        QueueEvent(Command, CmdArgs, Context, 'C');
      } else // say
        QueueEvent("say", EditStart, Context, 'C');

      Widget_TextEdit(MSG_DRAW, d, s);
    }
    else if(s->MessageKey.SDL->mod & KMOD_CTRL) {
      int FormatKeys[] = {SDLK_b,SDLK_k,SDLK_u,SDLK_i,SDLK_o,0};
      char FormatChars[] = {0x02,0x03,0x1f,0x1d,0x0f,0};
      int i=0;
      for(i=0;FormatKeys[i];i++)
        if(Key == FormatKeys[i]) {
          msg = MSG_CHAR;
          InsertChar[0] = FormatChars[i];
          s->MessageText = InsertChar;
          break;
        }
    }
  }

  if(msg == MSG_CHAR || msg == MSG_KEY || msg == MSG_DRAW) {
    sprintf((char*)s->Dialog[GUIPOS_INPUTCHARS].dp, "%4i", strlen((char*)d->dp)-1);
    RunWidget(MSG_DRAW, &s->Dialog[GUIPOS_INPUTCHARS], s);
  }
  return Return;
}

int Widget_MenuButton(int msg, struct GUIDialog *d, GUIState *s) {
  if(UsingTUI) return TUI_MenuButton(msg, d, s);
  int Return = Widget_StaticImage(msg, d, s);
  if(msg == MSG_CLICK || msg == MSG_DCLICK) {
    *s->TempCmdTarget = 0;
    StartPopupCommandMenu(s, MenuMain, NULL);
  }
  return Return;
}

int Widget_Dummy(int msg, struct GUIDialog *d, GUIState *s) {
  return D_O_K;
}

struct ScrollState {
  int DragType;
  int Scroll;
  int Drawn;
};
int Widget_ChannelTabs(int msg, struct GUIDialog *d, GUIState *s) {
// d1=num entries, d2=drawn, dp=selected tab
  if(UsingTUI) return TUI_ChannelTabs(msg, d, s);
  SDL_Renderer *Renderer = s->Renderer;
  ClientTab *SelectTab = (ClientTab*)d->dp;
  ClientTab *OldTab = (ClientTab*)d->dp;
  int RowHeight = ChatFont.Height+4;
  int CursorX, CursorY, NeedDraw=0;
  int Buttons = SDL_GetMouseState(&CursorX, &CursorY);
  int ResizeBarX = d->x+d->w-9;
  char ContextBuffer[CONTEXT_BUF_SIZE];
  char ContextBuffer2[CONTEXT_BUF_SIZE];

  struct ScrollState *ScrollInfo = (struct ScrollState*)d->dp3;
  if(!ScrollInfo) {
    ScrollInfo = (struct ScrollState*)malloc(sizeof(struct ScrollState));
    memset(ScrollInfo, 0, sizeof(struct ScrollState));
    d->dp3 = ScrollInfo;
  }

/*
//  int ScrolledAmount = 0;
  if(msg!=MSG_DRAW && (msg!=MSG_DRAG || ScrollInfo->DragType)) {
//    int OldScroll = ScrollInfo->Scroll;
    int ScrollReturn = RunScrollbar(msg, d, s, &ScrollInfo->Scroll, 50, ResizeBarX-ScrollbarW);
    if(ScrollReturn == SCROLLBAR_HAS_MOUSE)
      ScrollInfo->DragType = 1;
    else if(ScrollReturn == SCROLLBAR_SCROLL_CHANGE) {
//      ScrolledAmount = ScrollInfo->Scroll-OldScroll;
      ScrollInfo->Drawn = 0;
    }
  }
*/

  if(msg == MSG_ACTIVATE) {
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

  if(msg == MSG_DRAGDROPPED) {
    if(!ScrollInfo->DragType) {
      d->w = CursorX - d->x + 4;
      s->Flags|=GSF_NEED_UPDATE|GSF_NEED_REDRAW;
    }
    return D_REDRAW;
  }
  if((!ScrollInfo->DragType && msg == MSG_DRAG) || (msg == MSG_MOUSEMOVE && CursorX>ResizeBarX && CursorX<(d->x+d->w)))
    GUI_SetCursor(SYSCURSOR_SIZE_EW);

  if(msg == MSG_KEY && (s->MessageKey.SDL->mod & KMOD_CTRL) && SelectTab) {
    int Key = s->MessageKey.SDL->sym;
    if(Key == SDLK_PAGEUP) {
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
      NeedDraw = 1;
    } else if(Key == SDLK_PAGEDOWN) {
      if(SelectTab->Child) SelectTab = SelectTab->Child;
      else if(SelectTab->Next) SelectTab = SelectTab->Next;
      else if(SelectTab->Parent && SelectTab->Parent->Next) SelectTab = SelectTab->Parent->Next;
      else SelectTab = FirstTab;
      NeedDraw = 1;
    }
    d->dp = SelectTab;
    GUIDialog *ChatView = FindDialogWithProc(NULL, s, NULL, Widget_ChatView);
    if(ChatView && SelectTab != (ClientTab*)ChatView->dp) {
      ChatView->dp = SelectTab;
      RunWidget(MSG_ACTIVATE, ChatView, s);
    }
  }

  if((msg == MSG_CLICK || msg == MSG_DCLICK) && (CursorX>ResizeBarX && CursorX<(d->x+d->w))) {
    s->DragDialog = d; ScrollInfo->DragType = 0;
  } else if(msg == MSG_CLICK || msg == MSG_DCLICK) {
    int Num = 0;
    ClientTab *Tab = FirstTab;
    while(Tab) {
      if(!(Tab->Flags & TAB_HIDDEN)) {
        int StartY = d->y+RowHeight*Num+1;
        if(CursorY >= StartY && CursorY < StartY+RowHeight) {
          if(Buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
            ContextForTab(Tab, s->TempCmdTarget);
            if(Tab->Flags & TAB_QUERY)
              StartPopupCommandMenu(s, MenuUserTab, NULL);
            else if(Tab->Flags & TAB_SERVER)
              StartPopupCommandMenu(s, cJSON_GetObjectItem(MenuMain,"Server"), NULL);
            else
              StartPopupCommandMenu(s, MenuChannel, NULL);
            break;
          } else {
            SelectTab = Tab;
            SelectTab->Flags &= ~TAB_COLOR_MASK;
            d->dp = Tab;

            GUIDialog *ChatView = FindDialogWithProc(NULL, s, NULL, Widget_ChatView);
            if(ChatView && Tab != (ClientTab*)ChatView->dp) {
              ChatView->dp = Tab;
              RunWidget(MSG_ACTIVATE, ChatView, s);
            }

            NeedDraw = 1;
            break;
          }
        }
        Num++;
      }      
      if(Tab->Child) Tab = Tab->Child;
      else if(Tab->Next) Tab = Tab->Next;
      else if(Tab->Parent) Tab = Tab->Parent->Next; // if NULL, loop terminates
    }
    if(Tab && OldTab)
      QueueEvent("focus tab", ContextForTab(OldTab, ContextBuffer2), ContextForTab(Tab, ContextBuffer), 'E');
  }

 if(NeedDraw||(msg == MSG_START)||(msg==MSG_DRAW)||(msg==MSG_LOSTFOCUS)) {
    SDL_Rect Rect = {d->x, d->y, d->w, d->h};
    SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_BG].r, IRCColors[IRCCOLOR_BG].g, IRCColors[IRCCOLOR_BG].b, 255);
    SDL_RenderFillRect(Renderer, &Rect);
    SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_BORDER].r, IRCColors[IRCCOLOR_BORDER].g, IRCColors[IRCCOLOR_BORDER].b, 255);
    SDL_RenderDrawRect(Renderer, &Rect);
    Rect.w--;
    Rect.h--;
    SDL_RenderSetClipRect(Renderer, &Rect);

    ClientTab *Tab = FirstTab;
    int CurDrawn = 0;
    while(Tab) {
      if(!(Tab->Flags & TAB_HIDDEN)) {
        int Color = Tab->Flags & TAB_COLOR_MASK;
        int StartDrawX = d->x+1;
        int StartDrawY = d->y+RowHeight*CurDrawn+1;
        if(Tab->Parent) StartDrawX += 8;
        if(*Tab->Icon) {
          int IconX, IconY;
          if(*Tab->Icon == ':') { // default icon set
            IconX = strtol(Tab->Icon+1, NULL, 16);
            IconY = (IconX >> 4) * SystemIconH;
            IconX = (IconX & 15) * SystemIconW;
            blit(MainIconSet, Renderer, IconX, IconY, StartDrawX, StartDrawY, SystemIconW, SystemIconH);
            StartDrawX += SystemIconW + 1;
          }
        }
        RenderSimpleText(Renderer, &ChatFont, StartDrawX, StartDrawY, Color<<2, Tab->Name);
        if(Tab->Flags & TAB_NOTINCHAN) { // strikeout if not in the channel
          SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_FG].r, IRCColors[IRCCOLOR_FG].g, IRCColors[IRCCOLOR_FG].b, 255);
          SDL_RenderDrawLine(Renderer, StartDrawX, StartDrawY+(ChatFont.Height)/2, d->w, StartDrawY+(ChatFont.Height)/2);
        }
        if(Tab == SelectTab) {
          SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_SELECT].r, IRCColors[IRCCOLOR_SELECT].g, IRCColors[IRCCOLOR_SELECT].b, 255);
          SDL_Rect Select = {d->x+1, StartDrawY, d->x+d->w-3, RowHeight};
          SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_MOD);
          SDL_RenderFillRect(Renderer, &Select);
          SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_NONE);
        }
        CurDrawn++;
      }
      if(Tab->Child) Tab = Tab->Child;
      else if(Tab->Next) Tab = Tab->Next;
      else if(Tab->Parent) Tab = Tab->Parent->Next; // if NULL, loop terminates
      else break;
    }
    SDL_SetRenderDrawColor(Renderer, IRCColors[IRCCOLOR_BORDER].r, IRCColors[IRCCOLOR_BORDER].g, IRCColors[IRCCOLOR_BORDER].b, 255);
    SDL_RenderDrawLine(Renderer, ResizeBarX, d->y, ResizeBarX, d->y+d->h-1);
    SDL_RenderSetClipRect(Renderer, NULL);

//    RunScrollbar(MSG_DRAW, d, s, &ScrollInfo->Scroll, 50, ResizeBarX-ScrollbarW);

    d->d1=CurDrawn;
    s->Flags|=GSF_NEED_PRESENT;
  }

  if(msg == MSG_END)
    free(ScrollInfo);
  return D_O_K;
}

void ResizeMainGUI(GUIState *s) {
  int WW = s->WinWidth, WH = s->WinHeight;
  s->Dialog[GUIPOS_INPUTBOX].h = ChatFont.Height + 8;
  s->Dialog[GUIPOS_INPUTBOX].y = WH-s->Dialog[GUIPOS_INPUTBOX].h-1;

  s->Dialog[GUIPOS_YOURNAME].w = UnicodeLen((char*)s->Dialog[GUIPOS_YOURNAME].dp, NULL)*ChatFont.Width;
  s->Dialog[GUIPOS_INPUTCHARS].w = 4*ChatFont.Width+1;
  s->Dialog[GUIPOS_INPUTCHARS].x = WW-s->Dialog[GUIPOS_INPUTCHARS].w-1;

  s->Dialog[GUIPOS_YOURNAME].h = s->Dialog[GUIPOS_INPUTBOX].h;
  s->Dialog[GUIPOS_INPUTCHARS].h = s->Dialog[GUIPOS_INPUTBOX].h;
  s->Dialog[GUIPOS_YOURNAME].y = s->Dialog[GUIPOS_INPUTBOX].y-s->Dialog[GUIPOS_INPUTBOX].h;
  s->Dialog[GUIPOS_INPUTCHARS].y = s->Dialog[GUIPOS_INPUTBOX].y;

  s->Dialog[GUIPOS_SPARKICON].y = s->Dialog[GUIPOS_YOURNAME].y;
  s->Dialog[GUIPOS_SPARKICON].h = WH-s->Dialog[GUIPOS_YOURNAME].y;
  s->Dialog[GUIPOS_SPARKICON].x = 1;
  s->Dialog[GUIPOS_SPARKICON].w = s->Dialog[GUIPOS_SPARKICON].h;

  s->Dialog[GUIPOS_INPUTBOX].x = s->Dialog[GUIPOS_SPARKICON].x+s->Dialog[GUIPOS_SPARKICON].w+1;
  s->Dialog[GUIPOS_INPUTBOX].w = s->Dialog[GUIPOS_INPUTCHARS].x-s->Dialog[GUIPOS_INPUTBOX].x;
  s->Dialog[GUIPOS_YOURNAME].x = s->Dialog[GUIPOS_INPUTBOX].x;

  s->Dialog[GUIPOS_CHANNELTAB].x = 1;
  s->Dialog[GUIPOS_CHANNELTAB].y = 1;
  s->Dialog[GUIPOS_CHATVIEW].y = 1;
  s->Dialog[GUIPOS_CHANNELTAB].h = s->Dialog[GUIPOS_SPARKICON].y-s->Dialog[GUIPOS_CHANNELTAB].y-1;

  s->Dialog[GUIPOS_CHATVIEW].x = s->Dialog[GUIPOS_CHANNELTAB].x+1+s->Dialog[GUIPOS_CHANNELTAB].w;
  s->Dialog[GUIPOS_CHATVIEW].w = WW-1-s->Dialog[GUIPOS_CHATVIEW].x;
  s->Dialog[GUIPOS_CHATVIEW].h = s->Dialog[GUIPOS_CHANNELTAB].h;

  RunWidget(MSG_WIN_RESIZE, &s->Dialog[GUIPOS_CHATVIEW], s);
}

void EndMinimalGUI() {
  for(int i=0;i<NUM_GUISTATES;i++) {
    if(GUIStates[i]) {
      if(GUIStates[i]->Flags)
        for(int j=0;GUIStates[i]->Dialog[j].proc;j++)
          RunWidget(MSG_END, &GUIStates[i]->Dialog[j], GUIStates[i]);
    }
  }
  for(int i=0;i<SYSCURSOR_MAX;i++)
    SDL_FreeCursor(SystemCursors[i]);
  SDL_StopTextInput();
  SDL_FreeSurface(WindowIcon);
  SDL_DestroyWindow(window);
  SDL_DestroyRenderer(ScreenRenderer);
  SDL_DestroyTexture(SparklesIcon);
  SDL_DestroyTexture(MainIconSet);
  Free_FontSet(&ChatFont);
  Free_FontSet(&TinyChatFont);
  TTF_Quit();
  IMG_Quit();
}
