/*
 * SparklesChat
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
 *
 */
#include "chat.h"
#include <math.h>

#include <websocketpp/config/core.hpp>
#include <websocketpp/client.hpp>
#include <iostream>
#include <fstream>

#include <openssl/bio.h>
#include <openssl/evp.h>

extern "C" {
void QueueSockEvent(SqSocket *Socket, int Event, const char *Text);

void WebsocketOpen(SqSocket *Sock) {
  char buf[4096];
  sprintf(buf, "%i", rand());
  snprintf(buf, sizeof(buf),
           "GET / HTTP/1.1\r\n"
           "Host: %s\r\n"
           "Upgrade: websocket\r\n"
           "Connection: Upgrade\r\n"
           "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
           "Sec-WebSocket-Version: 13\r\n"
           "\r\n",
           Sock->Hostname);
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "connecting with a websocket %s", Sock->Hostname);
//  SockSend(Sock, buf, strlen(buf));
}

void WebsocketWrite(SqSocket *Sock, const char *String) {

}
void WebsocketRead(SqSocket *Sock, const char *Read, int Length) {

}

}
