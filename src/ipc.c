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

void IPC_New(IPC_Holder *IPC, int Size) {
  IPC->Message = NULL;
  IPC->Lock = SDL_CreateMutex();
  SDL_AtomicSet(&IPC->Ready, 0);
}

void IPC_Free(IPC_Holder *IPC) {
  IPC_Message *Msg = IPC->Message;
  while(Msg) {
    IPC_Message *Next = Msg->Next;
    free(Msg->Data);
    free(Msg);
    Msg = Next;
  }
  SDL_DestroyMutex(IPC->Lock);
}

void IPC_Write(IPC_Holder *IPC, const char *Text) {
  SDL_LockMutex(IPC->Lock);
  char *Clone = strdup(Text);
  if(!Clone)
    abort();

  // find where to add the new message
  IPC_Message *AddTo = IPC->Message;
  while(AddTo && AddTo->Next)
    AddTo = AddTo->Next;

  // make a new message and stick it on the end
  IPC_Message *New = (IPC_Message*)malloc(sizeof(IPC_Message));
  if(New) {
    if(AddTo)
      AddTo->Next = New;
    else
      IPC->Message = New;
    New->Data = Clone;
    New->Next = NULL;
  } else
    free(Clone);

  SDL_UnlockMutex(IPC->Lock);
  SDL_AtomicAdd(&IPC->Ready, 1);
  return;
}

void IPC_WriteF(IPC_Holder *IPC, const char *format, ...) {
  va_list args;
  char *buf;
  va_start(args, format);
  buf = strdup_vprintf(format, args);
  va_end(args);
  IPC_Write(IPC, buf);
  free(buf);
}

char *IPC_Read(int Timeout, int Count, ...) {
  int i;
  IPC_Holder *IPC[Count];

  va_list ap;
  va_start(ap, Count);
  for(i=0; i<Count; i++)
    IPC[i] = va_arg(ap, IPC_Holder*);
  va_end(ap);

  // to do: use a condition variable
  int Delay = Timeout / 10;
  for(int Tries = 0; Tries < 10; Tries++) {
    for(i=0; i<Count; i++)
      if(SDL_AtomicGet(&IPC[i]->Ready)) {
        SDL_LockMutex(IPC[i]->Lock);
        char *String = 0;
        if(IPC[i]->Message) {
          String = IPC[i]->Message->Data;
          IPC_Message *Next = IPC[i]->Message->Next;
          free(IPC[i]->Message);
          IPC[i]->Message = Next;
        }
        SDL_UnlockMutex(IPC[i]->Lock);
        SDL_AtomicAdd(&IPC[i]->Ready, -1);
        return String;
      }
    SDL_Delay(Delay);
  }
  return NULL;
}
