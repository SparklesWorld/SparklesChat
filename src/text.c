/*
 * SparklesChat
 *
 * Copyright (C) 2014 NovaSquirrel
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

int __attribute__ ((hot)) VisibleStrLen(const char *Text, int Flags) { // how many characters would be drawn
  int Ignore = 0, IgnoreLink = 0, Count = 0;
  while(*Text) {
    unsigned char k = *Text;
    char *Find;
    if(k<0x80||k>0xbf) { // Unicode continuation bytes don't count
      if((~Flags)&1) { // by default, process IRC control characters
        if(k>=0x20 && !Ignore) Count++;
        else if(k==']') Ignore &= ~8;
        else if(k==0xff) { // escape code
          Text++; // skip the format number too
          unsigned char FormatCode = *Text;
          switch(FormatCode) {
            case FORMAT_HIDDEN:
              Find = strstr(Text, "\xff\x83");
              if(Find) Text = Find+2;
              break;
            case FORMAT_ICON_LINK:
              Count++;
            case FORMAT_PLAIN_ICON:
              Count += 3;
              Ignore ^= 4;
              break;
            case FORMAT_URL_LINK:
              IgnoreLink ^= 1;
              if(IgnoreLink)
                while(*Text && (*Text != ']'))
                  Text++;
              break;
            case FORMAT_COMMAND_LINK:
              IgnoreLink ^= 1;
              if(IgnoreLink)
                while(*Text && (*Text != 0xfe))
                 Text++;
              break;
          }
        } else if(k==0x03) { // color
          if(isdigit(Text[0])) {
            Text++;
            if(Text[0] == ',') {
              Text++;
              if(isdigit(Text[0])) {
                Text++;
                if(isdigit(Text[0])) {
                  Text++;
                }
              }
            } else if(isdigit(Text[0])) {
              Text++;
              if(Text[0] == ',') {
                Text++;
                if(isdigit(Text[0])) {
                  Text++;
                  if(isdigit(Text[0])) {
                    Text++;
                  }
                }
              }
            }
          }
        } else if(k==0x04) { // RGB color
          int x;
          for(x=0;x<6;x++)
            if(!isxdigit(Text[x]))
              break;
          if(x==6)
            Text+=6;
        }
      } else Count++;
    }
    if(*Text)
      Text++;
  }
  return Count;
}

char* strdup_vprintf(const char* format, va_list ap) {
// from http://svn.nomike.com/playground/trunk/mini-c/strdup_printf.c
  va_list ap2;
  int size;
  char* buffer;

  va_copy(ap2, ap);
  size = vsnprintf(NULL, 0, format, ap2)+1;
  va_end(ap2);

  buffer = malloc(size+1);
  if(!buffer) return NULL;

  vsnprintf(buffer, size, format, ap);
  return buffer;
}

char __attribute__ ((hot)) *StripIRCText(char *Out, const char *Text, int Flags) {
  int Ignore = 0, IgnoreLink = 0;
  char *Output = Out;
  while(*Text) {
    unsigned char k = *(Text++);
    char *Find;
    if(k>=0x20 && k<0xfe && !Ignore) *(Out++)=k;
    else if(k==']') Ignore &= ~8;
    else if(k==0xfe) Ignore &= ~8;
    else if(k==0xff) { // escape code
      unsigned char FormatCode = *Text;
      Text++; // skip the format number too
      switch(FormatCode) {
        case FORMAT_HIDDEN:
          Find = strstr(Text, "\xff\x83");
          if(Find) Text = Find+2;
          break;
        case FORMAT_ICON_LINK:
          *(Out++)= 0xfe;
          while(*Text && (*Text != 0xfe))
            Text++;
        case FORMAT_PLAIN_ICON:
          *(Out++)= 0xfe;
          *(Out++)= 0xfe;
          *(Out++)= 0xff;
          Ignore ^= 4;
          while(*Text && (*Text != 0xfe))
            Text++;
          break;
        case FORMAT_URL_LINK:
          IgnoreLink ^= 1;
          if(IgnoreLink) {
            Find = strchr(Text, ']');
            if(Find) Text = Find;
          }
          break;
        case FORMAT_COMMAND_LINK:
          IgnoreLink ^= 1;
          if(IgnoreLink) {
            Find = strchr(Text, 0xfe);
            if(Find) Text = Find;
          }
          break;
      }
    }
    else if(k==0x03) { // color
      if(isdigit(Text[0])) {
        Text++;
        if(Text[0] == ',') {
          Text++;
          if(isdigit(Text[0])) {
            Text++;
            if(isdigit(Text[0])) {
              Text++;
            }
          }
        } else if(isdigit(Text[0])) {
          Text++;
          if(Text[0] == ',') {
            Text++;
            if(isdigit(Text[0])) {
              Text++;
              if(isdigit(Text[0])) {
                Text++;
              }
            }
          }
        }
      }
    } else if(k==0x04) { // RGB color
      int x;
      for(x=0;x<6;x++)
        if(!isxdigit(Text[x]))
          break;
      if(x==6)
        Text+=6;
    }
  }
  *Out = 0;
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", Output);
  return Output;
}

int __attribute__ ((hot)) UnicodeLen(const char *Text2, const char *EndPoint) {
  unsigned char *Text = (unsigned char*)Text2;
  int Amount = 0;
  while(*Text && (Text != (unsigned char*)EndPoint)) {
    if(*Text<0x80||*Text>0xbf)
      Amount++;
    Text++;
  }
  return Amount;
}
char __attribute__ ((hot)) *UnicodeForward(char *Text2, int CharsAhead) {
  unsigned char *Text = (unsigned char*)Text2;
  if(!Text) return NULL;
  if(!CharsAhead) return Text2;
  while(1) {
    Text++;
    if(!*Text)
      return NULL; // CharsAhead went too far
    if(*Text<0x80||*Text>0xbf) {
      CharsAhead--;
      if(!CharsAhead) return (char*)Text;
    }
  }
}

char *UnicodeBackward(char *Text2, int CharsBehind, char *BufferStart) {
  unsigned char *Text = (unsigned char*)Text2;
  if(!Text) return NULL;
  if(!CharsBehind) return Text2;
  if((char*)Text==BufferStart) return NULL;
  while(1) {
    Text--;
    if(*Text<0x80||*Text>0xbf) {
      CharsBehind--;
      if(!CharsBehind) return (char*)Text;
    }
    if((char*)Text==BufferStart)
      return NULL; // CharsBehind went too far
  }
}

char *UnicodeWordStep(char *Text, int Direction, char *Start) { // for ctrl+whatever
  char *Next;
  int GotAlpha = 0;
  // go until isalnum() then go until it isn't
  if(Direction > 0) { // forward
    while(1) {
      Next = UnicodeForward(Text, 1);
      if(!Next) return UnicodeForward(Start, UnicodeLen(Start, NULL)-1);
      if(isalnum(*Next)) GotAlpha = 1;
      if(!isalnum(*Next) && GotAlpha) return Next;
      Text = Next;
    }
  } else { // backward
    if(Text==Start) return Text;
    while(1) {
      Next = UnicodeBackward(Text, 1, Start);
      if(!Next) return Start;
      if(isalnum(*Next)) GotAlpha = 1;
      if(!isalnum(*Next) && GotAlpha) return Text;
      Text = Next;
    }
  }
}

extern const char *FChatColorNames[];

const char __attribute__ ((hot)) *ConvertBBCode(char *Output, const char *Input, int Flags) {
  if(!strchr(Input, '[') && !strchr(Input, '&')) return Input; // no conversion necessary

  const char *TagName[] = {"b]","i]","u]","s]","sub]","sup]","url=","url]","icon]","user]","color=","color]","noparse]",NULL};
  const int TagLen[] = {2, 2, 2, 2, 4, 4, 4, 4, 5, 5, 6, 6, 8}; // 5+ treated specially
  const char *TagEndName[] = {"[/b]","[/i]","[/u]","[/s]","[/sub]","[/sup]","[/url]","[/url]","[/icon]","[/user]","[/color]",NULL,"[/noparse]"};
  int TagCodes[] = {0x02, 0x1d, 0x1f, FORMAT_STRIKEOUT, FORMAT_SUBSCRIPT, FORMAT_SUPERSCRIPT,
    FORMAT_URL_LINK, FORMAT_URL_LINK, FORMAT_ICON_LINK, FORMAT_NICK_LINK,
   FORMAT_CANCEL_COLORS, FORMAT_CANCEL_COLORS};
  int TagEnabled[] = {0,0,0,0,0,0,0,0,0,0,0,0,0};
  unsigned char *Out = (unsigned char*)Output;
  int i; // tag index to try

  while(*Input) {
    char c = *(Input++);
    if(c == '&' && !Flags) {
      if(!memcmp(Input, "lt;", 3)) {
        *(Out)++ = '<';
        Input += 3;
      } else if(!memcmp(Input, "gt;", 3)) {
        *(Out)++ = '>';
        Input += 3;
      } else if(!memcmp(Input, "amp;", 4)) {
        *(Out)++ = '&';
        Input += 4;
      }
    } else if(c != '[') // if not [ just copy it
      *(Out++) = c;
    else
      if(*Input == '/') { // cancel formatting
        for(i=0;TagName[i];i++)
          if(!memcmp(Input+1, TagName[i], TagLen[i])) {
            if(TagEnabled[i]) {
              Input+=TagLen[i]+1;
              if(TagCodes[i] & 128) // need escape code?
                *(Out++) = 0xff;
              *(Out++) = TagCodes[i];
              TagEnabled[i] = 0;
            } else
              *(Out++) = '['; // tag isn't enabled, print it
            break;
          }
        if(!TagName[i]) // put the [ if not a valid tag
          *(Out++) = '[';
      } else { // start formatting
        for(i=0;TagName[i];i++)
          if(!memcmp(Input, TagName[i], TagLen[i])) {
            if(!TagEndName[i] || !strstr(Input, TagEndName[i])) { // if tag isn't closed, tag should be visible
              *(Out++) = '['; // add the [ back
              break;
            }
            Input+=TagLen[i];
            if(TagLen[i] < 6) { // 6+ handle outputting on their own
              if(TagCodes[i] & 128) // need escape code?
                *(Out++) = 0xff;
              *(Out++) = TagCodes[i];
              if(i!=6)
                TagEnabled[i] = 1;
              else // url= has to set url]
                TagEnabled[i+1] = 1;
            }
            // special handling for lengths of at least 5
            if(i==10) { // [color=
              const char *End = strchr(Input, ']');
              char ColorName[End-Input+1];
              strlcpy(ColorName, Input, sizeof(ColorName));
              Input = End+1;
              if(*ColorName == '#') { // hex color
                *(Out++) = 0x04;
                if(strlen(ColorName) == 4) { // convert 3 digit to 6 digit
                  for(int j=1;j<4;j++) { // double every digit
                    *(Out++)=ColorName[j];
                    *(Out++)=ColorName[j];
                  }
                } else { // assume 6 digits
                  strcpy((char*)Out, ColorName+1);
                  Out+=6;
                }
              } else {
                for(int j=0;FChatColorNames[j];j++)
                  if(!strcasecmp(ColorName, FChatColorNames[j])) {
                    sprintf(ColorName, "\3%.2d", j+16); // F-Chat colors start at 16
                    strcpy((char*)Out, ColorName);
                    Out+=3;
                    break;
                  }
              }
              TagEnabled[i+1] = 1;
            } else if(i==12) { // [noparse]
              const char *End = strstr(Input, "[/noparse]");
              memcpy(Out, Input, End-Input);
              Out+=(End-Input)+10;
            }
            break;
          }
        if(!TagName[i]) // put the [ if not a valid tag
          *(Out++) = '[';
      }
  }
  *Out = 0;
  return Output;
}

void TextInterpolate(char *Out, const char *In, char Prefix, const char *ReplaceThis, const char *ReplaceWith[]) {
  while(*In) {
    if(*In != Prefix)
      *(Out++) = *(In++);
    else {
      In++;
      char *Find = strchr(ReplaceThis, *(In++));
      if(Find) {
        int This = Find - ReplaceThis;
        strcpy(Out, ReplaceWith[This]);
        Out += strlen(ReplaceWith[This]);
      } else {
        *(Out++) = Prefix;
        *(Out++) = In[-1];
      }
    }
  }
  *Out = 0;
}

char *FindCloserPointer(char *A, char *B) {
  if(!A) // doesn't matter if B is NULL too, it'll just return the NULL
    return B;
  if(!B || A < B)
    return A;
  return B;
}

char *StringClone(const char *CloneMe) {
  char *Out = (char*)malloc(strlen(CloneMe)+1);
  if(!Out) return NULL;
  strcpy(Out, CloneMe);
  return Out;
}

int WildMatch(const char *TestMe, const char *Wild) {
  char NewWild[strlen(Wild)+1];
  char NewTest[strlen(TestMe)+1];
  const char *Asterisk = strchr(Wild, '*');
  if(Asterisk && !Asterisk[1]) return(!memcasecmp(TestMe, Wild, strlen(Wild)-1));

  strcpy(NewTest, TestMe);
  strcpy(NewWild, Wild);
  int i;
  for(i=0;NewWild[i];i++)
    NewWild[i] = tolower(NewWild[i]);
  for(i=0;NewTest[i];i++)
    NewTest[i] = tolower(NewTest[i]);
  return !fnmatch(NewWild, NewTest, FNM_NOESCAPE);
}

void strlcpy(char *Destination, const char *Source, int MaxLength) {
  // MaxLength is directly from sizeof() so it includes the zero
  int SourceLen = strlen(Source);
  if((SourceLen+1) < MaxLength)
    MaxLength = SourceLen + 1;
  memcpy(Destination, Source, MaxLength-1);
  Destination[MaxLength-1] = 0;
}

int memcasecmp(const char *Text1, const char *Text2, int Length) {
  for(;Length;Length--)
    if(tolower(*(Text1++)) != tolower(*(Text2++)))
      return 1;
  return 0;
}

