/*
 * SparklesChat
 * Copyright (C) 2014-2015 NovaSquirrel
 *
 * wslay example client
 * Copyright (c) 2011, 2012 Tatsuhiro Tsujikawa
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
#include <math.h>
#include <wslay/wslay.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
void QueueSockEvent(SqSocket *Socket, int Event, const char *Text);

ssize_t wslay_recv(wslay_event_context_ptr ctx, uint8_t *data, size_t len, int flags, void *user_data) {
  SqSocket *Sock = (SqSocket*)user_data;
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "recv %i", len);
  if(Sock->Flags & SQSOCK_WS_BLOCKING)
    return WSLAY_ERR_WOULDBLOCK;
  Sock->Flags |= SQSOCK_WS_BLOCKING;
  return SockRecv(Sock, (char*)data, len);
}

ssize_t wslay_send(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int flags, void *user_data) {
  SqSocket *Sock = (SqSocket*)user_data;
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "send %i", len);
  return SockSend(Sock, (char*)data, len);
}

int wslay_genmask(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, void *user_data) {
  for(int i=0; i<len; i++)
    buf[i] = (rand()^SDL_GetTicks())&255;
  return 0;
}

void wslay_message(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg *arg, void *user_data) {
  SqSocket *Sock = (SqSocket*)user_data;
  if(arg->opcode == WSLAY_TEXT_FRAME) {
    char Line[arg->msg_length + 1];
    memcpy(Line, arg->msg, arg->msg_length);
    Line[arg->msg_length] = 0;
    QueueSockEvent(Sock, SQS_NEW_DATA, Line);
  } else if(arg->opcode == WSLAY_CONNECTION_CLOSE) {
    QueueSockEvent(Sock, SQS_DISCONNECTED, "Websocket closed");
    Sock->Flags |= SQSOCK_CLEANUP;
  }
}

struct wslay_event_callbacks wslay_callbacks = {
  wslay_recv,
  wslay_send,
  wslay_genmask,
  NULL,
  NULL,
  NULL,
  wslay_message,
};

void WebsocketOpen(SqSocket *Sock) {
  if(wslay_event_context_client_init(&Sock->Websocket, &wslay_callbacks, Sock))
    Sock->Flags |= SQSOCK_CLEANUP;
  wslay_event_config_set_max_recv_msg_length(Sock->Websocket, 0x80000);

  char buf[4096];
  snprintf(buf, sizeof(buf),
           "GET / HTTP/1.1\r\n"
           "Host: %s\r\n"
           "Upgrade: websocket\r\n"
           "Connection: Upgrade\r\n"
           "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
           "Sec-WebSocket-Version: 13\r\n"
           "\r\n",
           Sock->Hostname);
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "connecting with a websocket %s", Sock->Hostname);
  SockSend(Sock, buf, strlen(buf));
}

void WebsocketWrite(SqSocket *Sock, const char *String) {
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "writing websocket %s", String);
  int Length = strlen(String);

  struct wslay_event_msg EventMessage;
  EventMessage.opcode = WSLAY_TEXT_FRAME;
  EventMessage.msg = (uint8_t*)String;
  EventMessage.msg_length = Length;

  wslay_event_queue_msg(Sock->Websocket, &EventMessage);
}
