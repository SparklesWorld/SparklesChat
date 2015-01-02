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

extern FontSet TinyChatFont;

SDL_Color IRCColors[IRCCOLOR_PALETTE_SIZE] = {
// IRC, from XChat
  {0xCC,0xCC,0xCC,255}, {0x00,0x00,0x00,255}, {0x36,0x36,0xB2,255}, {0x2A,0x8C,0x2A,255},
  {0xC3,0x3B,0x3B,255}, {0xC7,0x32,0x32,255}, {0x80,0x26,0x7F,255}, {0x66,0x36,0x1F,255},
  {0xD9,0xA6,0x41,255}, {0x3D,0xCC,0x3D,255}, {0x1A,0x55,0x55,255}, {0x2F,0x8C,0x74,255},
  {0x45,0x45,0xE6,255}, {0xB0,0x37,0xB0,255}, {0x4C,0x4C,0x4C,255}, {0x95,0x95,0x95,255},
// F-Chat
  {0xFF,0xFF,0xFF,255}, {0x00,0x00,0x00,255}, {0xC3,0x3B,0x3B,255}, {0x45,0x45,0xE6,255}, 
  {0xD9,0xA6,0x41,255}, {0x3D,0xCC,0x3D,255}, {0xEB,0x98,0x98,255}, {0xD3,0xD3,0xD3,255}, 
  {0xE9,0x8C,0x35,255}, {0xB0,0x37,0xB0,255}, {0x66,0x36,0x1F,255}, {0x45,0xE4,0xE6,255}, 
  {0x00,0x00,0x00,255}, {0x00,0x00,0x00,255}, {0x00,0x00,0x00,255}, {0x00,0x00,0x00,255}, 
// internal stuff
  {220,220,220,255}, {0,0,0,255},    {0,192,0,255}, {128,128,128,255}, {0x35,0x6e,0xc1,255},
  {0,0,255,255},     {255,0,0,255},  {0,255,0,255}, {255,0,0,255},     {0,64,0, 255},
  {255,255,255,255}, {64,64,64,255}, {64,64,64,255}, {135,135,135,255},
};
const char *FChatColorNames[] = {"white", "black", "red", "blue", "yellow", "green", "pink", "gray", "orange", "purple", "brown", "cyan", NULL};

// drawing functions
void rectfill(SDL_Renderer *Bmp, int X1, int Y1, int X2, int Y2) {
  SDL_Rect Temp = {X1, Y1, abs(X2-X1)+1, abs(Y2-Y1)+1};
  SDL_RenderFillRect(Bmp,  &Temp);
}
void sblit(SDL_Surface* SrcBmp, SDL_Surface* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height) {
  SDL_Rect Src = {SourceX, SourceY, Width, Height};
  SDL_Rect Dst = {DestX, DestY};
  SDL_BlitSurface(SrcBmp, &Src, DstBmp, &Dst);
}
void blit(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height) {
  SDL_Rect Src = {SourceX, SourceY, Width, Height};
  SDL_Rect Dst = {DestX, DestY, Width, Height};
  SDL_RenderCopy(DstBmp,  SrcBmp, &Src, &Dst);
}
void blitz(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height, int Width2, int Height2) {
  SDL_Rect Src = {SourceX, SourceY, Width, Height};
  SDL_Rect Dst = {DestX, DestY, Width2, Height2};
  SDL_RenderCopy(DstBmp,  SrcBmp, &Src, &Dst);
}
void blitfull(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int DestX, int DestY) {
  SDL_Rect Dst = {DestX, DestY};
  SDL_RenderCopy(DstBmp,  SrcBmp, NULL, &Dst);
}
void SDL_MessageBox(int Type, const char *Title, SDL_Window *Window, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  char Buffer[512];
  vsprintf(Buffer, fmt, argp);
  SDL_ShowSimpleMessageBox(Type, Title, Buffer, Window);
  va_end(argp);
}

void Free_FontSet(FontSet *Fonts) {
  if(Fonts->Font[0]) TTF_CloseFont(Fonts->Font[0]);
  if(Fonts->Font[1]) TTF_CloseFont(Fonts->Font[1]);
  if(Fonts->Font[2]) TTF_CloseFont(Fonts->Font[2]);
  if(Fonts->Font[3]) TTF_CloseFont(Fonts->Font[3]);
}

int Load_FontSet(FontSet *Fonts, int Size, const char *Font1, const char *Font2, const char *Font3, const char *Font4) {
  Fonts->Font[0] = NULL; Fonts->Font[1] = NULL;
  Fonts->Font[2] = NULL; Fonts->Font[3] = NULL;
  Fonts->Width = 0; Fonts->Height = 0;

  Fonts->Font[0] = TTF_OpenFont(Font1, Size);
  Fonts->Font[1] = TTF_OpenFont(Font2, Size);
  Fonts->Font[2] = TTF_OpenFont(Font3, Size);
  Fonts->Font[3] = TTF_OpenFont(Font4, Size);

  if(!Fonts->Font[0] || !Fonts->Font[1] || !Fonts->Font[2] || !Fonts->Font[3]) {
    SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", NULL, "Error loading one or more fonts - SDL_ttf Error: %s", TTF_GetError());
    Free_FontSet(Fonts);
    return 0;
  }

  TTF_SizeText(Fonts->Font[0], "N", &Fonts->Width, &Fonts->Height);
  return 1;
}

const int TabColors[] = {IRCCOLOR_FG, IRCCOLOR_NEWMSG, IRCCOLOR_NEWDATA, IRCCOLOR_NEWHIGHLIGHT};
int RenderSimpleText(SDL_Renderer *Renderer, FontSet *Font, int X, int Y, int Flags, const char *Text) {
  SDL_Surface *TextSurface2 = TTF_RenderUTF8_Shaded(Font->Font[Flags&3],Text,IRCColors[TabColors[(Flags>>2)&3]],IRCColors[IRCCOLOR_BG]);
  SDL_Surface *TextSurface = SDL_ConvertSurfaceFormat(TextSurface2, SDL_PIXELFORMAT_RGBA8888, 0);

  SDL_Surface *FormatSurface=TTF_RenderUTF8_Shaded(Font->Font[Flags&3],"cbiupr\xe2\x86\xb5",IRCColors[IRCCOLOR_FORMATFG],IRCColors[IRCCOLOR_FORMATBG]);
  SDL_Texture *Texture;

  char FormatCodes[] = {0x03,0x02,0x1d,0x1f,0x0f,0x16,'\n',0};
  unsigned char *Text2 = (unsigned char*)Text;
  int CurChar = 0;
  while(*Text2) {
    char *Which = strchr(FormatCodes, (char)Text2[0]);
    if(Which)
      sblit(FormatSurface, TextSurface, (Which-FormatCodes)*Font->Width, 0, CurChar*Font->Width, 0, Font->Width, Font->Height);
    if(*Text2<0x80||*Text2>0xbf)
      CurChar++;
    Text2++;
  }

  Texture = SDL_CreateTextureFromSurface(Renderer, TextSurface);
  blit(Texture, Renderer, 0, 0, X, Y, TextSurface->w, TextSurface->h);
  SDL_FreeSurface(TextSurface);
  SDL_FreeSurface(TextSurface2);
  SDL_FreeSurface(FormatSurface);
  SDL_DestroyTexture(Texture);
  return 1;
}

int RenderText(SDL_Renderer *Renderer, FontSet *Font, int X, int Y, RenderTextMode *Mode, const char *Text) {
// 02=bold, 1d=italic 1f=underline 0f=plain 16=inverse 04=rrggbb

  SDL_Surface *TextSurface, *RenderSurface;
  int Bold=0, Italic=0, Underline=0, Sup=0, Sub=0, Strike=0, Hidden=0, Link=0;
  int BG=IRCCOLOR_BG, FG=1, Swap;
  SDL_Color FontFG=IRCColors[IRCCOLOR_FG], FontBG=IRCColors[IRCCOLOR_BG];
  int CurDrawX = 0;
  char Text2[strlen(Text)+1], *Poke;
  int SurfaceWidth, SurfaceHeight;
  int WhichFont, TempW, TempH, i;
  char TempHex[7];
  int MaxWidth=0, LineVSpace=0;
  int BGOnly = 0;

  int Flags = 0;
  if(Mode) {
    MaxWidth = Mode->MaxWidth;
    LineVSpace = Mode->LineVSpace;
    //MaxHeight = Mode->MaxHeight;
    Flags = Mode->Flags;
  }

  char *Stripped = NULL;
  if(Mode && Mode->AlreadyStripped) Stripped = Mode->AlreadyStripped;
  else {
    Stripped = (char*)malloc(strlen(Text)+1);
    StripIRCText(Stripped,Text,0);
  }

  TTF_SizeUTF8(Font->Font[0], Stripped, &SurfaceWidth, &SurfaceHeight);
  if(Flags & RENDERMODE_RIGHT_JUSTIFY)
    X -= SurfaceWidth;

  RenderSurface = SDL_CreateRGBSurface(0, SurfaceWidth+8, SurfaceHeight+8, 32, 0, 0, 0, 0);
  if(!RenderSurface) return 0;
  SDL_FillRect(RenderSurface, NULL, SDL_MapRGB(RenderSurface->format, IRCColors[IRCCOLOR_BG].r, IRCColors[IRCCOLOR_BG].g, IRCColors[IRCCOLOR_BG].b));

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
          memcpy(TempHex, Text+1, 6); // only copy 7
          TempHex[6] = 0;             // null terminator
          i = strtol(TempHex, NULL, 16);
          if(BGOnly) {
            FontBG.r = (i >> 16) & 255;
            FontBG.g = (i >> 8)  & 255;
            FontBG.b = i & 255;
          } else {
            FontFG.r = (i >> 16) & 255;
            FontFG.g = (i >> 8)  & 255;
            FontFG.b = i & 255;
          }
          BGOnly = 0;
          Text+=6;
        }
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
        if(BGOnly)
          FontBG=IRCColors[FG&31];
        else {
          FontFG=IRCColors[FG&31];
          FontBG=IRCColors[BG];
        }
        BGOnly = 0;
        break;
      case 0x0f:
        Bold = 0; Italic = 0;
        Underline = 0; Sup = 0;
        Sub = 0; Strike = 0;
        FontFG=IRCColors[1]; FontBG=IRCColors[IRCCOLOR_BG];
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
            FontFG=Sub?IRCColors[IRCCOLOR_FADED]:IRCColors[IRCCOLOR_FG];
            break;
          case FORMAT_SUPERSCRIPT:
            Sup ^= 1;
            FontFG=Sub?IRCColors[IRCCOLOR_FADED]:IRCColors[IRCCOLOR_FG];
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
            FontFG=IRCColors[1]; FontBG=IRCColors[IRCCOLOR_BG];
            break;
          case FORMAT_BG_ONLY:
            BGOnly = 1;
            break;
        }
        break;
      default:
        if(k>=0x20) {
          Poke = Text2;
          while((unsigned char)Text[0]>=0x20 && (unsigned char)Text[0]<0xfe)
            *(Poke++) = *(Text++);
          *Poke = 0;

          WhichFont = Bold|(Italic<<1);
          TTF_SizeUTF8(Font->Font[WhichFont], Text2, &TempW, &TempH);

          TextSurface=TTF_RenderUTF8_Shaded(Font->Font[WhichFont],Text2,FontFG,FontBG);
          int PixelWidth = UnicodeLen(Text2, NULL)*Font->Width;
          if(Underline) {
            SDL_Rect Fill = {0, TextSurface->h-2, TextSurface->w, 1};
            SDL_FillRect(TextSurface, &Fill, SDL_MapRGB(TextSurface->format, 0, 0, 0));
          }
          if(Strike) {
            SDL_Rect Fill = {0, TextSurface->h/2, TextSurface->w, 1};
            SDL_FillRect(TextSurface, &Fill, SDL_MapRGB(TextSurface->format, 0, 0, 0));
          }
          sblit(TextSurface, RenderSurface, 0, 0, CurDrawX, 0, TextSurface->w, TextSurface->h);

          CurDrawX += PixelWidth;
          SDL_FreeSurface(TextSurface);
          Text--; // will ++ after the loop
        }
    }

    if(*Text)
      Text++;
    else break;
  }

  SDL_Texture *Texture;
  Texture = SDL_CreateTextureFromSurface(Renderer, RenderSurface);

  if(Mode)
    Mode->DrawnHeight = Font->Height;
  if(!MaxWidth || (MaxWidth >= RenderSurface->w)) { // no wrap
    blit(Texture, Renderer, 0, 0, X, Y, SurfaceWidth, SurfaceHeight);
  } else { // wrap
    int CharsPerRow = MaxWidth/Font->Width; // floor rounding

    int CurRow = 0, CurCol = 0;
    char *Peek = Stripped;
    int RenderStart = 0;
    while(*Peek) {
      // skip spaces for this word
      char *EndOf = Peek;
      while(*EndOf == ' ')
        EndOf++;
      // find the start of the next word
      char *Temp = strchr(EndOf, ' ');
      if(Temp) EndOf = Temp;
      else EndOf = strrchr(EndOf, 0);

      // determine how many characters need to be drawn
      int RenderEnd = RenderStart + UnicodeLen(Peek, EndOf);
      int RenderLength = RenderEnd - RenderStart;
      int NewCurCol = CurCol + RenderLength;
      if(RenderLength < CharsPerRow/2) { // short word
        if(NewCurCol >= CharsPerRow) {
          CurCol = 0;
          NewCurCol = RenderLength;
          CurRow++;
        }
        blit(Texture, Renderer, RenderStart*Font->Width, 0, X+(Font->Width*CurCol), Y+((Font->Height+LineVSpace)*CurRow), (RenderLength)*Font->Width, SurfaceHeight);
      } else { // long word
        // do the rest of the first line
        blit(Texture, Renderer, RenderStart*Font->Width, 0, X+(Font->Width*CurCol), Y+((Font->Height+LineVSpace)*CurRow), (CharsPerRow-1-CurCol)*Font->Width, SurfaceHeight);
        int RenderPart = RenderStart+(CharsPerRow-1-CurCol);
        CurRow++;
        // handle lines as wide as the chat view
        while((RenderEnd-RenderPart) >= (CharsPerRow-1)) {
          blit(Texture, Renderer, RenderPart*Font->Width, 0, X, Y+((Font->Height+LineVSpace)*CurRow), (CharsPerRow-1)*Font->Width, SurfaceHeight);
          RenderPart += CharsPerRow-1;
          CurRow++;
        }
        // handle last line
        CurCol = 0;
        NewCurCol = 0;
        if(RenderPart != RenderEnd) {
          blit(Texture, Renderer, RenderPart*Font->Width, 0, X, Y+((Font->Height+LineVSpace)*CurRow), (RenderEnd-RenderPart)*Font->Width, SurfaceHeight);
          NewCurCol = RenderEnd-RenderPart;
        }
      }
      RenderStart = RenderEnd;
      CurCol = NewCurCol;

      Peek = EndOf;
    }
    if(Mode)
      Mode->DrawnHeight = (Font->Height+LineVSpace)*(CurRow+1);
  }
  SDL_FreeSurface(RenderSurface);
  SDL_DestroyTexture(Texture);
  if(!Mode || !Mode->AlreadyStripped) free(Stripped);
  return 1;
}

int SimulateWordWrap(FontSet *Font, RenderTextMode *Mode, const char *Text) {
  if(!Mode) return 0; // can't word wrap without a RenderTextMode anyway

  int MaxWidth=Mode->MaxWidth;
  int LineVSpace=Mode->LineVSpace;

  int SurfaceWidth, SurfaceHeight;
  char MakeStripped[strlen(Text)+1];
  char *Stripped = MakeStripped;
  if(Mode->AlreadyStripped) Stripped = Mode->AlreadyStripped;
  else StripIRCText(MakeStripped,Text,0);

  TTF_SizeUTF8(Font->Font[0], Stripped, &SurfaceWidth, &SurfaceHeight);
  if(!MaxWidth || (MaxWidth >= SurfaceWidth)) // no wrapping needed
    return Font->Height;

  int CharsPerRow = MaxWidth/Font->Width; // floor rounding

  int CurRow = 0, CurCol = 0;
  char *Peek = Stripped;
  int RenderStart = 0;
  while(*Peek) {
    // skip spaces for this word
    char *EndOf = Peek;
    while(*EndOf == ' ')
      EndOf++;
    // find the start of the next word
    char *Temp = strchr(EndOf, ' ');
    if(Temp) EndOf = Temp;
    else EndOf = strrchr(EndOf, 0);

    // determine how many characters need to be drawn
    int RenderEnd = RenderStart + UnicodeLen(Peek, EndOf);
    int RenderLength = RenderEnd - RenderStart;
    int NewCurCol = CurCol + RenderLength;
    if(RenderLength < CharsPerRow/2) { // short word
      if(NewCurCol >= CharsPerRow) {
        CurCol = 0;
        NewCurCol = RenderLength;
        CurRow++;
      }
    } else { // long word
      // do the rest of the first line
      int RenderPart = RenderStart+(CharsPerRow-1-CurCol);
      CurRow++;
      // handle lines as wide as the chat view
      while((RenderEnd-RenderPart) >= (CharsPerRow-1)) {
        RenderPart += CharsPerRow-1;
        CurRow++;
      }
      // handle last line
      CurCol = 0;
      NewCurCol = 0;
      if(RenderPart != RenderEnd) {
        NewCurCol = RenderEnd-RenderPart;
      }
    }
    RenderStart = RenderEnd;
    CurCol = NewCurCol;

    Peek = EndOf;
  }

  return Mode->DrawnHeight = (Font->Height+LineVSpace)*(CurRow+1);
}
