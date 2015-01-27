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
#ifdef _WIN32
  _pipe(IPC->Pipe, Size, 0);
#else
  pipe(IPC->Pipe);
  //fcntl(Pipe[0], F_SETPIPE_SZ, Size);
  //fcntl(Pipe[1], F_SETPIPE_SZ, Size);
#endif
  SDL_AtomicSet(&IPC->Ready, 0);
}

void IPC_Free(IPC_Holder *IPC) {
  close(IPC->Pipe[0]);
  close(IPC->Pipe[1]);
}

void IPC_Write(IPC_Holder *IPC, const char *Text) {
  int Length = strlen(Text)+1;
  write(IPC->Pipe[PIPE_WRITE], &Length, sizeof(int));
  write(IPC->Pipe[PIPE_WRITE], Text, Length);
  SDL_AtomicAdd(&IPC->Ready, 1);
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

  int Delay = Timeout / 10;
  for(int Tries = 0; Tries < 10; Tries++) {
    for(i=0; i<Count; i++)
      if(SDL_AtomicGet(&IPC[i]->Ready)) {
        int Length;
        read(IPC[i]->Pipe[PIPE_READ], &Length, sizeof(int));
        if(Length > 0x10000) {
          SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "bad message length: %i", Length);
          exit(0);
        }
        char *String = (char*)malloc(Length * sizeof(char));
        read(IPC[i]->Pipe[PIPE_READ], String, Length);
        SDL_AtomicAdd(&IPC[i]->Ready, -1);
        return String;
      }
    SDL_Delay(Delay);
  }
  return NULL;
}
