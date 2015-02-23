/*
 * SparklesChat
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
 *
 */
#include "chat.h"
#include <wslay/wslay.h>
extern int SockId;
SSL_CTX *SSLContext;

#include <errno.h>
#include <fcntl.h>

#define USE_OPENSSL
#define WANTSOCKET
#define WANTARPA

#ifdef WIN32
#include <winbase.h>
#include <io.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

SqSocket *FirstSock = NULL;

const char *SSLErrorString() {
  static char buf[256];
  ERR_error_string(ERR_get_error(), buf);
  return buf;
}

int SockSend(SqSocket *Sock, const char *Send, int Length) {
  if(Sock->Secure) {
    int Return = SSL_write(Sock->Secure, Send, Length);
	switch(SSL_get_error(Sock->Secure, Return)) {
      case SSL_ERROR_SSL:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SSL_ERROR_SSL: %s", SSLErrorString());
        break;
      case SSL_ERROR_SYSCALL:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SSL_ERROR_SYSCALL");
        break;
	  case SSL_ERROR_ZERO_RETURN:
        Sock->Flags |= SQSOCK_CLEANUP;      
        break;
	}
    return Return;
  }
  return send(Sock->Socket, Send, Length, 0);
}

int SockRecv(SqSocket *Sock, char *Buffer, int Length) {
  if(Sock->Secure) {
    int Return = SSL_read(Sock->Secure, Buffer, Length);
    switch(SSL_get_error(Sock->Secure, Return)) {
      case SSL_ERROR_SSL:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SSL_ERROR_SSL: %s", SSLErrorString());
        break;
      case SSL_ERROR_SYSCALL:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SSL_ERROR_SYSCALL");
        break;
      case SSL_ERROR_ZERO_RETURN:
        Sock->Flags |= SQSOCK_CLEANUP;      
        break;
    }
    return Return;
  }
  return recv(Sock->Socket, Buffer, Length, 0);
}

// http://wiki.openssl.org/index.php/BIO
void InitSock() {
  SSL_library_init();
  SSL_load_error_strings();
  ERR_load_BIO_strings();
  ERR_load_crypto_strings();
  OpenSSL_add_all_algorithms();

  SSLContext = SSL_CTX_new(SSLv23_client_method());
  SSL_CTX_set_verify(SSLContext, SSL_VERIFY_NONE, NULL);
}

void EndSock() {
  EVP_cleanup();
  CRYPTO_cleanup_all_ex_data();
  ERR_free_strings();
  SSL_CTX_free(SSLContext);
}

void QueueSockEvent(SqSocket *Socket, int Event, const char *Text) {
  IPC_WriteF(&SocketToEvent, "S%i;%i:%s", Socket->Id, Event, Text);
}

int CreateSqSock(HSQUIRRELVM v, HSQOBJECT Handler, const char *Host, int Flags) {
  struct SqSocket *Socket = (struct SqSocket*)calloc(1,sizeof(struct SqSocket));
  if(!Socket) return 0;
  strlcpy(Socket->Hostname, Host, sizeof(Socket->Hostname));
 
  Socket->Flags = Flags;
  Socket->Bio = NULL;
  Socket->Script = v;
  Socket->Function.Squirrel = Handler;
  Socket->Id = SockId;
  Socket->Socket = -1;
  Socket->Next = FirstSock;
  int BufferSize = 0x10000;
  Socket->Buffer = (char*)malloc(BufferSize);
  Socket->Buffer[0] = 0;
  Socket->BufferSize = BufferSize;
  if(FirstSock)
    FirstSock->Prev = Socket;
  sq_addref(v, &Handler);
  FirstSock = Socket;
  IPC_WriteF(&EventToSocket, "O%i", SockId); // open socket
  return 1;
}
// see also Spark_NetOpen in plugin.c

void DeleteSocketById(int Id) {
  SDL_LockMutex(LockSockets);
  for(SqSocket *Sock = FirstSock; Sock; Sock=Sock->Next)
    if(Sock->Id == Id) {
      if(FirstSock == Sock)
        FirstSock = Sock->Next;
      if(Sock->Prev)
        Sock->Prev->Next = Sock->Next;
      if(Sock->Script)
        sq_release(Sock->Script, &Sock->Function.Squirrel);
      BIO_free(Sock->Bio);
      if(Sock->Websocket)
        wslay_event_context_free(Sock->Websocket);
      if(Sock->Secure) { // from HexChat
        SSL_set_shutdown(Sock->Secure, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
        SSL_free(Sock->Secure);
        ERR_remove_state(0);		  /* free state buffer */
      }
      if(Sock->Socket)
        close(Sock->Socket);
      free(Sock->Buffer);
      free(Sock);
      SDL_UnlockMutex(LockSockets);
      return;
    }
  SDL_UnlockMutex(LockSockets);
}

SqSocket *FindSockById(int Id) {
  SDL_LockMutex(LockSockets);
  for(SqSocket *Sock = FirstSock; Sock; Sock=Sock->Next)
    if(Sock->Id == Id) {
      SDL_UnlockMutex(LockSockets);
      return Sock;
    }
  SDL_UnlockMutex(LockSockets);
  return NULL;
}

SqSocket *FindSockBySocket(int Socket) {
  SDL_LockMutex(LockSockets);
  for(SqSocket *Sock = FirstSock; Sock; Sock=Sock->Next)
    if(Sock->Socket == Socket) {
      SDL_UnlockMutex(LockSockets);
      return Sock;
    }
  SDL_UnlockMutex(LockSockets);
  return NULL;
}

void WebsocketOpen(SqSocket *Sock);
void WebsocketWrite(SqSocket *Sock, const char *String);

int RunSocketThread(void *Data) {
  while(!quit) {
    char *Command, *Temp;
    char TempBuf[60];
    SqSocket *Sock;
    // check for queue commands
    do {
      Command = IPC_Read(100, 1, &EventToSocket);
      if(Command) {
//        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "socket: %s", Command);
        switch(Command[0]) {
          case 'C': // close socket
            DeleteSocketById(strtol(Command+1, NULL, 10));
            break;
          case 'O': // open new socket
            Sock = FindSockById(strtol(Command+1, NULL, 10));
            Sock->Bio = BIO_new_connect(Sock->Hostname);
            if(!Sock->Bio) {
              Sock->Flags = SQSOCK_CLEANUP;
              QueueSockEvent(Sock, SQS_CANT_CONNECT, "Can't create bio");
              break;
            }
            int Result = BIO_do_connect(Sock->Bio);
            switch(Result) {
              case 1:
                if(!(Sock->Flags & SQSOCK_WEBSOCKET))
                  QueueSockEvent(Sock, SQS_CONNECTED, "");
                Sock->Flags |= SQSOCK_CONNECTED;
                break;
              default:
                sprintf(TempBuf, "BIO_do_connect returned %i", Result);
                QueueSockEvent(Sock, SQS_CANT_CONNECT, TempBuf);
                Sock->Flags |= SQSOCK_CLEANUP;
                break;
            }
            BIO_get_fd(Sock->Bio, &Sock->Socket);
            if(Sock->Flags & SQSOCK_SSL) {
              Sock->Secure = SSL_new(SSLContext);
              if(!Sock->Secure) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Can't make new SSL socket");
                Sock->Flags |= SQSOCK_CLEANUP;
              } else {
                SSL_set_fd(Sock->Secure, Sock->Socket);
                SSL_set_connect_state(Sock->Secure);
                SSL_CTX_set_verify(SSLContext, SSL_VERIFY_NONE, NULL);
              }
            }
            if(Sock->Flags & SQSOCK_WEBSOCKET)
              WebsocketOpen(Sock);
            break;
          case 'M': // write message
            Sock = FindSockById(strtol(Command+1,&Temp, 10));
            char *Message = strchr(Command, ' ');
            if(!Message) break;
            Message++;

            if(Sock) {
              if(Sock->Flags & SQSOCK_WEBSOCKET)
                WebsocketWrite(Sock, Message);
              else
                SockSend(Sock, Message, strlen(Message));
            }
            break;
        }
        free(Command);
      }
    } while(Command);

    if(!FirstSock) // no sockets? don't do the check
      continue;

#ifdef _WIN32
    FD_SET ReadSet;
#else
    fd_set ReadSet;
#endif
    FD_ZERO(&ReadSet);

    SDL_LockMutex(LockSockets);
    for(SqSocket *Sock = FirstSock; Sock;) {
      SqSocket *Next = Sock->Next;
      if(Sock->Flags & SQSOCK_CLEANUP)
        DeleteSocketById(Sock->Id);
      else if(Sock->Socket >= 0)
        FD_SET(Sock->Socket, &ReadSet);
      Sock = Next;
    }
    struct timeval timeout = {0, 0};
    int Total = select(0, &ReadSet, NULL, NULL, &timeout);
#ifdef _WIN32
    if(Total == SOCKET_ERROR)
      SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "select error: %i", WSAGetLastError());
#else
    if(Total == -1)
      SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "select error: %s", strerror(errno));
#endif
    else
      for(SqSocket *Sock = FirstSock; Sock; Sock=Sock->Next)
        if(Sock->Socket >= 0 && FD_ISSET(Sock->Socket, &ReadSet)) {
          if(Sock->Flags & SQSOCK_WS_CONNECTED) {
            Sock->Flags &= ~SQSOCK_WS_BLOCKING;
            if(!wslay_event_want_read(Sock->Websocket) || wslay_event_recv(Sock->Websocket))
              Sock->Flags |= SQSOCK_CLEANUP; 
            continue;
          }
          char WriteHere[0x30000];
          int Length = SockRecv(Sock, WriteHere, sizeof(WriteHere));
          if(Length > 0) {
            if(Sock->Flags & SQSOCK_WEBSOCKET) { // websocket in progress
              if(Sock->BytesWritten + Length > Sock->BufferSize) {
                Sock->Flags |= SQSOCK_CLEANUP;
                QueueSockEvent(Sock, SQS_CANT_CONNECT, "Handshake failed");
              } else {
                memcpy(Sock->Buffer + Sock->BytesWritten, WriteHere, Length);
                Sock->BytesWritten += Length;
                Sock->Buffer[Sock->BytesWritten] = 0;
                if(strstr(Sock->Buffer, "\r\n\r\n")) { // handshake results finished
                  if(strstr(Sock->Buffer, "Sec-WebSocket-Accept")) {
                    *Sock->Buffer = 0;
                    Sock->BytesWritten = 0;
                    Sock->Flags |= SQSOCK_WS_CONNECTED;
                    QueueSockEvent(Sock, SQS_CONNECTED, "Handshake succeeded");
                  } else {
                    Sock->Flags |= SQSOCK_CLEANUP;
                    QueueSockEvent(Sock, SQS_CANT_CONNECT, "Handshake failed");
                  }
                }
              }
            } else { // regular connection
              if(Sock->Flags & SQSOCK_NOT_LINED) { // raw
                WriteHere[Length] = 0;
                QueueSockEvent(Sock, SQS_NEW_DATA, WriteHere);
              } else { // lined
                WriteHere[Length] = 0;

                int BufLen = strlen(Sock->Buffer);
                if(BufLen+Length+1 > Sock->BufferSize) {
                  Sock->Buffer = realloc(Sock->Buffer, BufLen+Length+1);
                  Sock->BufferSize = BufLen+Length+1;
                }
                strcat(Sock->Buffer, WriteHere);

                char *Line;
                do {
                  Line = strchr(Sock->Buffer, '\r');
                  if(!Line) Line = strchr(Sock->Buffer, '\n');
                  if(Line) {
                    while(*Line == '\r' || *Line == '\n')
                      *(Line++) = 0;
                    QueueSockEvent(Sock, SQS_NEW_DATA, Sock->Buffer);
                    if(*Line)
                      memmove(Sock->Buffer, Line, strlen(Line)+1);
                    else
                      *Sock->Buffer = 0;
                  }
                } while(Line && *Line);
              }
            }
          }
        }
    for(SqSocket *Sock = FirstSock; Sock; Sock=Sock->Next)
      if(Sock->Flags & SQSOCK_WS_CONNECTED && wslay_event_want_write(Sock->Websocket))
        wslay_event_send(Sock->Websocket);
    SDL_UnlockMutex(LockSockets);
  }
  return 0;
}
