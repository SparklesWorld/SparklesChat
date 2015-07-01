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
#include "../src/chat.h"
#include "xchat-plugin.h"
#include "sparkles-plugin.h"

xchat_plugin *ph;
sparkles_plugin *sph;
char *(*Spark_ContextForTab)(ClientTab *Tab, char *Buffer);

int Sparkles_InitPlugin(xchat_plugin *plugin_handle, sparkles_plugin *sparkles_handle, char **plugin_name, char **plugin_desc, char **plugin_version, char **plugin_author, char *arg) {
  ph = plugin_handle;
  sph = sparkles_handle;

  *plugin_name = "triple deluXe";
  *plugin_desc = "Mimics the XChat GUI";
  *plugin_author = "NovaSquirrel";
  *plugin_version = "0.10";

  Spark_ContextForTab = Spark_FindSymbol("ContextForTab");
  return 1;
}

int Sparkles_DeInitPlugin() {
  return 1;
}

// for the C half to communicate with the Vala half
void DirtyTabsList();
void DirtyTabsAttr(int id1, int id2, int flags);
void AddTabToList(int level, const char *name, int flags, void *tab, int id);
void InfoAdded(void *Context);
void FocusATab(int id1, int id2);
void AddFormattedText(const char *text, const char *fg, const char *bg, int flags);

void PrintDebug(const char *Text) {
  Spark_DebugPrintf("%s", Text);
}

char *MsgRightOrExtended(ClientMessage *Msg) {
  static char Buffer[INPUTBOX_SIZE];
  if(!Msg->RightExtend) return Msg->Right;
  strcpy(Buffer, Msg->Right);
  strcpy(Buffer+strlen(Msg->Right), Msg->RightExtend);
  return Buffer;
}

void CombineTabMessage(ClientMessage *Message, void *Tab) {
  static char Buffer[INPUTBOX_SIZE];
  if(*Message->Left)
    sprintf(Buffer, "%s %s", Message->Left, MsgRightOrExtended(Message));
  else
    sprintf(Buffer, "%s", MsgRightOrExtended(Message));

  GotTabMessage(Buffer, Tab);
}

void GetTabMessages(void *Tab2, int UpdateOnly) {
  if(!Tab2) return;
  ClientTab *Tab = (ClientTab*)Tab2;
  Spark_LockResource(0);
  if(UpdateOnly) {
    if(!Tab->UndrawnLines) {
      Spark_UnlockResource(0); 
      return;
    }
    int msg;
    for(msg = Tab->NumMessages-Tab->UndrawnLines; msg < Tab->NumMessages; msg++)
      CombineTabMessage(&Tab->Messages[msg], Tab);
  } else {
    int msg;
    for(msg=0; msg<Tab->NumMessages; msg++)
      CombineTabMessage(&Tab->Messages[msg], Tab);
  }
  Tab->UndrawnLines = 0;
  Spark_UnlockResource(0); 
}

void DoInputCommand(const char *Text, void *Tab) {
  if(!Tab) return;

  char CopyBuffer[strlen(Text)+1];
  strcpy(CopyBuffer, Text);

  char Context[80];
  Spark_ContextForTab((ClientTab*)Tab, Context);

  char *EditStart = CopyBuffer;

  if(*CopyBuffer == '/' && CopyBuffer[1] == '/')
    EditStart = CopyBuffer + 2;

  if(*CopyBuffer == '/' && CopyBuffer[1] != '/') { // command
    EditStart++;
    char Command[strlen(EditStart)+1];
    strcpy(Command, EditStart);
    char *CmdArgs = strchr(Command, ' ');
    if(CmdArgs)
      *(CmdArgs++) = 0;
    else
      CmdArgs = "";
    Spark_QueueEvent(Command, CmdArgs, Context, 'C');
  } else // say
    Spark_QueueEvent("say", EditStart, Context, 'C');
}

void QueueFocusTabEvent(void *OldTab2, void *NewTab2) {
  if(!NewTab2) return;
  ClientTab *OldTab = (ClientTab*)OldTab2;
  ClientTab *NewTab = (ClientTab*)NewTab2;
  char ContextBuffer[80];
  char ContextBuffer2[80];

  char TabIdBuffer[16];
  sprintf(TabIdBuffer, "%i", NewTab->Id);
  Spark_QueueEvent(TabIdBuffer, "", "", 'F');
  Spark_QueueEvent("focus tab", Spark_ContextForTab(OldTab, ContextBuffer2), Spark_ContextForTab(NewTab, ContextBuffer), 'E');
}

void ReloadTabsList() {
  Spark_LockResource(0); // tabs list
  ClientTab **TabList = (ClientTab**)Spark_FindSymbol("FirstTab");
  ClientTab *Tab = *TabList;
  while(Tab) {
    AddTabToList(Tab->Parent!=NULL, Tab->Name, Tab->Flags, Tab, Tab->Id);
    if(Tab->Child) Tab = Tab->Child;
    else if(Tab->Next) Tab = Tab->Next;
    else if(Tab->Parent) Tab = Tab->Parent->Next; // if NULL, loop terminates
    else break;
  }
  Spark_UnlockResource(0);
}

void GetTabIds(ClientTab *Tab, int *Parent, int *Child) {
  if(!Tab->Parent) {
    *Parent = Tab->Id;
    *Child = -1;
  } else {
    *Parent = Tab->Parent->Id;
    *Child = Tab->Id;
  }
}

int HandleMainThreadMessage(int Type, char *Data1, char *Data2){
//  Spark_DebugPrintf("MTR: %i %s %s", Type, Data1?:"", Data2?:"");
  ClientTab *Tab = NULL;
  int Id1, Id2;
  switch(Type) {
    case MTR_GUI_COMMAND:      // d1 = context, d2 = command name
      Tab = xchat_find_context(ph, Data1, NULL);
      if(!Tab) break;
      if(!strcasecmp(Data2, "focus")) {
        GetTabIds(Tab, &Id1, &Id2);
        FocusATab(Id1, Id2);
      }
      return MTR_OKAY;
    case MTR_DIRTY_TABS_LIST:  // tab list is changed
      DirtyTabsList();
      return MTR_OKAY;
    case MTR_DIRTY_TABS_ATTR:  // d1 = context, tab attributes are changed but list isn't
      Tab = xchat_find_context(ph, Data1, NULL);
      if(!Tab) break;
      GetTabIds(Tab, &Id1, &Id2);
      DirtyTabsAttr(Id1, Id2, Tab->Flags);
      return MTR_OKAY;
    case MTR_INFO_ADDED:       // d1 = context
      Spark_LockResource(0);
      Tab = xchat_find_context(ph, Data1, NULL);
      if(!Tab) break;
      InfoAdded(Tab);
      Tab->UndrawnLines = 0;
      Spark_UnlockResource(0);
      return MTR_OKAY;
    default:
      return MTR_UNIMPLEMENTED;
  }
}

void PollForMainThreadMessagesAndDelay() {
  Spark_PollGUIMessages(HandleMainThreadMessage);
  Spark_Delay(17);
}

 const char *FormatPalette[] = {
  "#CCCCCC", "#000000", "#3636B2", "#2A8C2A",
  "#C33B3B", "#C73232", "#80267F", "#66361F",
  "#D9A641", "#3DCC3D", "#1A5555", "#2F8C74",
  "#4545E6", "#B037B0", "#4C4C4C", "#959595",

  "#FFFFFF", "#000000", "#C33B3B", "#4545E6",
  "#D9A641", "#3DCC3D", "#EB9898", "#D3D3D3",
  "#E98C35", "#B037B0", "#66361F", "#45E4E6",
  "#000000", "#000000", "#000000", "#000000"
};

void AddLineWithFormatting(const char *Text) {
// 02=bold, 1d=italic 1f=underline 0f=plain 16=inverse 04=rrggbb

  int Bold=0, Italic=0, Underline=0, Sup=0, Sub=0, Strike=0, Hidden=0, Link=0;
  const char *BG = NULL, *FG = NULL, *Swap;
  char Text2[strlen(Text)+1], *Poke;
  int i, FGNum = 0, BGNum = 0, BGOnly = 0;
  char FGHex[8], BGHex[8];

  char *Find;
  while(1) {
    unsigned char k = *Text;
    switch(k) {
      case 0xfe:
        break;
      case 0x02:
        Bold ^= 1;
        break;
      case 0x04:
        for(i=1;i<7;i++)
          if(!isxdigit(Text[i])) break;
        if(i==7) {
          if(BGOnly) {
            BGHex[0] = '#';
            memcpy(BGHex+1, Text+1, 6);
            BGHex[7] = 0;
            BG = BGHex;
          } else {
            FGHex[0] = '#';
            memcpy(FGHex+1, Text+1, 6);
            FGHex[7] = 0;
            FG = FGHex;
          }
          BGOnly = 0;
          Text+=6;
        }
        break;
      case 0x03:
        FGNum = 0;
        BGNum = -1;
        if(isdigit(Text[1])) {
          FGNum = Text[1]-'0';
          Text++;
          if(Text[1] == ',') {
            Text++;
            if(isdigit(Text[1])) {
              BGNum = Text[1]-'0';
              Text++;
              if(isdigit(Text[1])) {
                BGNum = ((BGNum*10)+(Text[1]-'0'))&31;
                Text++;
              }
            }
          } else if(isdigit(Text[1])) {
            FGNum = (FGNum*10)+(Text[1]-'0');
            Text++;
            if(Text[1] == ',') {
              Text++;
              if(isdigit(Text[1])) {
                BGNum = Text[1]-'0';
                Text++;
                if(isdigit(Text[1])) {
                  BGNum = ((BGNum*10)+(Text[1]-'0'))&31;
                  Text++;
                }
              }
            }
          }
        }
        if(BGOnly)
          BG = FormatPalette[FGNum&31];
        else {
          FG = FormatPalette[FGNum&31];
          BG = (BGNum != -1)?FormatPalette[BGNum]:NULL;
        }
        BGOnly = 0;
        break;
      case 0x0f:
        Bold = 0; Italic = 0;
        Underline = 0; Sup = 0;
        Sub = 0; Strike = 0;
        FG = NULL; BG = NULL;
        break;
      case 0x16:
        Swap = BG;
        BG = FG;
        FG = Swap;
        break;
      case 0x1d:
        Italic ^= 1;
        break;
      case 0x1f:
        Underline ^= 1;
        break;
      case 0xff:
        switch((unsigned char)*(++Text)) {
          case FORMAT_SUBSCRIPT:
            Sub ^= 1;
            break;
          case FORMAT_SUPERSCRIPT:
            Sup ^= 1;
            break;
          case FORMAT_STRIKEOUT:
            Strike ^= 1;
            break;
          case FORMAT_HIDDEN: // not currently working
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
            FG = NULL; BG = NULL;
            break;
          case FORMAT_BG_ONLY:
            BGOnly = 1;
            break;
        }
        break;
      default:
        if(k>=0x20) {
          Poke = Text2; // make a buffer of just the section we want
          while((unsigned char)Text[0]>=0x20 && (unsigned char)Text[0]<0xfe)
            *(Poke++) = *(Text++);
          *Poke = 0;
          AddFormattedText(Text2, FG?FG:"", BG?BG:"", Bold|(Italic<<1)|(Underline<<2)|(Sup<<3)|(Sub<<4)|(Strike<<5)|(Hidden<<6));
          Text--; // will ++ after the loop
        }
    }

    if(*Text)
      Text++;
    else break;
  }
}
