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

int IPC_New(IPC_Queue *Out[], int Size, int Queues) {
  for(int i=0; i<Queues; i++) {
    Out[i] = (IPC_Queue*)calloc(1,sizeof(IPC_Queue));
    if(!Out[i])
      return 0;
    Out[i]->Size = Size;
    Out[i]->Queue = (IPC_Message*)calloc(Size,sizeof(IPC_Message));
    Out[i]->Semaphore = SDL_CreateSemaphore(Size);
    Out[i]->Mutex = SDL_CreateMutex();
    Out[i]->Condition = SDL_CreateCond();
  }
  return 1;
}

void IPC_Free(IPC_Queue *Holder[], int Queues) {
  for(int i=0; i<Queues; i++) {
    for(int j=0; j<Holder[i]->Size; j++)
      if(Holder[i]->Queue[j].Text)
        free(Holder[i]->Queue[j].Text);
    free(Holder[i]->Queue);
    SDL_DestroyMutex(Holder[i]->Mutex);
    SDL_DestroySemaphore(Holder[i]->Semaphore);
    SDL_DestroyCond(Holder[i]->Condition);
    free(Holder[i]);
  }
}

void IPC_Write(IPC_Queue *Queue, const char *Text) {
  SDL_Delay(1); // remove later once I fix a race condition
  SDL_LockMutex(Queue->Mutex);
  SDL_Delay(1); // remove later once I fix a race condition
  SDL_SemWait(Queue->Semaphore);
  for(int i=0; i<Queue->Size; i++)
    if(!SDL_AtomicGet(&Queue->Queue[i].Used)) {
      SDL_AtomicSet(&Queue->Queue[i].Used, 1);
      // write text
      Queue->Queue[i].Text = (char*)malloc(strlen(Text)+1);
      strcpy(Queue->Queue[i].Text, Text);
      // update id
      Queue->Queue[i].Id = SDL_AtomicGet(&Queue->MakeId);
      SDL_AtomicAdd(&Queue->MakeId, 1);
      SDL_AtomicSet(&Queue->Queue[i].Used, 2);
      SDL_UnlockMutex(Queue->Mutex);
      SDL_CondSignal(Queue->Condition);
      return;
    }
  // we shouldn't be able to get here, but if we do, unlock stuff
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SHOULD NOT GET HERE");
  SDL_SemPost(Queue->Semaphore);
  SDL_UnlockMutex(Queue->Mutex);
}

void IPC_WriteF(IPC_Queue *Queue, const char *format, ...) {
  va_list args;
  char *buf;
  va_start(args, format);
  buf = strdup_vprintf(format, args);
  va_end(args);
  IPC_Write(Queue, buf);
  free(buf);
}

char *IPC_Read(IPC_Queue *Queue, int Timeout) {
  SDL_LockMutex(Queue->Mutex);
  int Size = Queue->Size;
  int MakeId = SDL_AtomicGet(&Queue->MakeId);
  int UseId = SDL_AtomicGet(&Queue->UseId);
  if(MakeId == UseId) {
    int Result = SDL_CondWaitTimeout(Queue->Condition, Queue->Mutex, Timeout);
    if(Result != 0) // zero means success
      goto Fail;
  }
  if(UseId < MakeId) { // are there any items to read?
    for(int i = 0; i < Size; i++)
      if(SDL_AtomicGet(&Queue->Queue[i].Used) && (Queue->Queue[i].Id == UseId)) {
        SDL_AtomicAdd(&Queue->UseId, 1);
        char *Text = Queue->Queue[i].Text;
        SDL_AtomicSet(&Queue->Queue[i].Used, 0);
        SDL_SemPost(Queue->Semaphore);
        SDL_UnlockMutex(Queue->Mutex);
        return Text;
      }
  }

Fail:
  SDL_UnlockMutex(Queue->Mutex);
  return NULL;
}
