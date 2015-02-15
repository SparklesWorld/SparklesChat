//print("IRC protocol loaded");
AddonInfo <- {
  "Name":"IRC"
  "Summary":"IRC protocol"
  "Version":"0.70"
  "Author":"NovaSquirrel"
};

local Sockets = {};
const ConfigPrefix = "Networks/IRC/";

class Server {
  Socket = 0;
  Host = null;
  MyHost = "";
  Tab = "";
  RawTab = "";
  Channels = [];
  Connected = false;
  TryReconnect = false;

  UserName = ""; // for reconnects
  RealName = "";
  function Send(Text) {
    api.NetSend(this.Socket, Text+"\r\n")
  }
}
class Channel {
  Joined = false;
  Tab = "";
  Topic = "";
  Users = [];
}

function Connect(Options) { // called by /connect
  Options = split(Options, " ");
  local ServerName = Options[0];
  local Host = api.GetConfigStr(ConfigPrefix+ServerName+"/Host", "");
  if(Host == "") {
    print("No host specified");
    return;
  }
  local HostConnect = Host+":6667";
  local Socket = api.NetOpen(IRC_Socket, HostConnect, "");

  local NewServer = Server();
  NewServer.Tab = api.TabCreate(ServerName, "", TabFlags.SERVER);
  NewServer.RawTab = api.TabCreate("Raw", NewServer.Tab, 0);
  NewServer.Socket = Socket;
  NewServer.Host = HostConnect;
  api.TabSetInfo(NewServer.Tab, "Socket", Socket.tostring());
  api.TabSetInfo(NewServer.Tab, "Nick", "SparklesChat"); // will get updated

  Sockets[Socket] <- NewServer;
}

function FixColon(Name) {
  if(!Name.len()) return Name;
  if(Name[0] == ':') return Name.slice(1);
  return Name;
}
function IsChannel(Name) {
  return Name[0] == '#';
}
function isdigit(Char) {
  return (Char >= '0') && (Char <= '9');
}
function SplitHost(Host) {
  local Split1 = split(Host, "@");
  local Split2 = split(Split1[0], "!");
  return [Split2[0], Split2[1], Split1[1]];
}

function SayCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local EParams = {"Nick":api.TabGetInfo(C, "Nick"),"Text":P};
  api.Event("you message", EParams, C);

  local Channel = api.TabGetInfo(C, "Channel");
  S.Send("privmsg "+Channel+" :"+P);
  return EventReturn.HANDLED;
}
function MeCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local EParams = {"Nick":api.TabGetInfo(C, "Nick"),"Text":P};
  api.Event("you action", EParams, C);

  local Channel = api.TabGetInfo(C, "Channel");
  S.Send("privmsg "+Channel+" :\x0001ACTION "+P+"\x0001");
  return EventReturn.HANDLED;
}
function MsgCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  P = api.CmdParamSplit(P);
  local Target = P[0][0];
  local Text = P[1][1];
  S.Send("privmsg "+Target+" :"+Text);

  local EParams = {"Nick":api.TabGetInfo(C, "Nick"),"Text":Text, "Target":Target};
  api.Event("you msg message", EParams, C);
  return EventReturn.HANDLED;
}
function NoticeCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  P = api.TextParamSplit(P);
  local Target = P[0][0];
  local Text = P[1][1];
  S.Send("notice "+Target+" :"+Text);

  local EParams = {"Nick":api.TabGetInfo(C, "Nick"),"Text":Text, "Target":Target};
  api.Event("you notice", EParams, C);
  return EventReturn.HANDLED;
}
function WhoisCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  S.Send("whois "+P);
  return EventReturn.HANDLED;
}
function JoinCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  S.Send("join "+P);
  return EventReturn.HANDLED;
}
function PartCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  S.Send("part "+P);
  return EventReturn.HANDLED;
}
function QuitCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  S.TryReconnect = false;
  S.Connected = false;
  S.Send("quit :"+P);
  return EventReturn.HANDLED;
}
function NickCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  S.Send("nick :"+P);
  return EventReturn.HANDLED;
}
function MarkNotInChannels(C) {
  local ServerTab = api.TabGetInfo(C, "ServerContext");
  api.TabAddFlags(ServerTab, TabFlags.NOTINCHAN);
  foreach(Channel in api.TabGetInfo(ServerTab, "List"))
    if(api.TabHasFlags(Channel, TabFlags.CHANNEL))
      api.TabAddFlags(Channel, TabFlags.NOTINCHAN);
}
function ReconnectCmd(T, P, C) {
  MarkNotInChannels(C);
  local OldSockId = api.TabGetInfo(C, "Socket");
  local OldSock = Sockets[OldSockId];
  local Host = OldSock.Host;
  api.NetClose(OldSockId);

  // open a new one
  local NewSockId = api.NetOpen(IRC_Socket, Host, "");
  OldSock.Socket = NewSockId;
  api.TabSetInfo(OldSock.Tab, "Socket", NewSockId.tostring());
 
  Sockets[NewSockId] <- OldSock;
  delete Sockets[OldSockId];

  return EventReturn.HANDLED;
}
function RawCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  S.Send(P);
  return EventReturn.HANDLED;
}
function QueryCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  if(!api.MakeContextExists(S.Tab, P))
    api.TabCreate(P, S.Tab, TabFlags.QUERY);
  return EventReturn.HANDLED;
}
function CloseCmd(T, P, C) {
  if(api.TabHasFlags(C, TabFlags.SERVER))
    api.Command("quit", "", C);
  return EventReturn.NORMAL;
}

api.AddCommandHook("say",   SayCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("me",    MeCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("msg",   MsgCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("msg",   NoticeCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("join",  JoinCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("part",  PartCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("quit",  QuitCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("quote", RawCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("query", QueryCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("whois", WhoisCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("nick",  NickCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("reconnect",  ReconnectCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("close",  CloseCmd, Priority.HIGH|PROTOCOL_CMD, null, null);

function IRC_Socket(Socket, Event, Text) {
  switch(Event) {
    case SockEvents.CANT_CONNECT:
      print("Can't connect: "+Text);
      break;
    case SockEvents.CONNECTED:
      local Server = Sockets[Socket];
      Server.Connected = false; // not fully connected until you get the initial ping
      local MyNick = "";
      if(!Server.TryReconnect) {
        MyNick = "SparklesChat"; // pull default from somewhere useful later
        Server.UserName = "Sparkles";
        Server.RealName = "SparklesChat user";
      } else
        MyNick = api.TabGetInfo(Server.Tab, "Nick");
      api.NetSend(Socket, format("NICK %s\r\nUSER %s 8 * :%s\r\n", MyNick, Server.UserName, Server.RealName))
      print("Connected");
      break;
    case SockEvents.NEW_DATA:
      local SplitRN = split(Text, "\r\n");
      if(SplitRN.len() > 1) {
        foreach(Line in SplitRN)
          IRC_Socket(Socket, Event, Line);
        return;
      }

      local Server = Sockets[Socket];
      local Tab = Server.Tab;
      local MyNick = api.TabGetInfo(Tab, "Nick");
      local Split = api.TextParamSplit(Text);
      if(Split[0][0] == "PING") {
        print("PING!")
        api.NetSend(Socket, "PONG "+Split[0][1]+"\r\n");
        if(!Server.Connected && Server.TryReconnect) {
          // restore state; todo: restore /away if it was set
          local ChannelList = "";
          foreach(Channel in api.TabGetInfo(Server.Tab, "List"))
            if(api.TabHasFlags(Channel, TabFlags.CHANNEL)) {
              if(ChannelList != "")
                ChannelList += ",";
              ChannelList += api.TabGetInfo(Channel, "Channel");
            }
          api.Command("join", ChannelList, Server.Tab);
        }
        Server.Connected = true;
        Server.TryReconnect = true;
        return;
      }
      Split[0].apply(FixColon);
      Split[1].apply(FixColon);

      if(Text.len() < 512)
        api.AddMessage(Text, Server.RawTab, 0, 0);
      if(Split[0].len() < 2)
        return;
      local Source = Split[0][0];
      local Command = Split[0][1];
      switch(Command.toupper()) {
        case "PRIVMSG":
          local HostParts = SplitHost(Source);
          local Destination = Split[0][2];
          local Text = Split[1][3];
          local Nick = HostParts[0];
          local C = api.MakeContext(Tab,Destination);
          if(Destination == MyNick) {
            if(!api.MakeContextExists(Tab, Nick))
              C = api.TabCreate(Nick, Tab, TabFlags.QUERY)
            else
              C = api.MakeContext(Tab, Nick);
          }
          if((Text.len() >= 8) && (Text.slice(0, 7) == "\x0001ACTION")) {
            Text = Text.slice(8, -1);
            local EParams = {"Nick":HostParts[0], "Host":HostParts[1]+"@"+HostParts[2], "Text":Text};
            api.Event("user action", EParams, C);
          } else {
            local EParams = {"Nick":HostParts[0], "Host":HostParts[1]+"@"+HostParts[2], "Text":Text};
            api.Event("user message", EParams, C);
          }
          break;
        case "JOIN":
          local HostParts = SplitHost(Source);
          local ChannelName = Split[0][2];
          if(HostParts[0] == MyNick) {
            if(!api.MakeContextExists(Server.Tab,ChannelName))
              Server.Channels.append(api.TabCreate(ChannelName, Tab, TabFlags.CHANNEL));
            else
              api.TabRemoveFlags(api.MakeContext(Server.Tab,ChannelName), TabFlags.NOTINCHAN);
          } else {
            local EParams = {"Nick":HostParts[0], "Host":HostParts[1]+"@"+HostParts[2]};
            api.Event("channel join", EParams, api.MakeContext(Tab,ChannelName));
          }
          break;
        case "PART":
          local HostParts = SplitHost(Source);
          local ChannelName = Split[0][2];
          if(HostParts[0] == MyNick) {
            // also remove from list
            api.TabAddFlags(api.MakeContext(Server.Tab, ChannelName), TabFlags.NOTINCHAN);
          } else {
            local EParams = {"Nick":HostParts[0], "Host":HostParts[1]+"@"+HostParts[2]};
            if(Split[0].len() >= 3) EParams.Text <- Split[0][3];
            api.Event("channel part", EParams, api.MakeContext(Tab, ChannelName));
          }
          break;
        case "NICK":
          local HostParts = SplitHost(Source);
          local NewNick = Split[0][2];
          if(HostParts[0] == MyNick) {
            api.TabSetInfo(Tab, "Nick", NewNick);
          } else {
            if(!api.MakeContextExists(Tab, HostParts[0]))
              api.TabSetInfo(api.MakeContext(Tab,HostParts[0]), "Channel", NewNick);
          }
          break;
        case "TOPIC":
          break;
        case "KICK":
          break;
        case "QUIT":
          break;
        default:
          if(Command.len() == 3 && isdigit(Command[0])) {
            local Numeric = api.Num(Command, 10);
            switch(Numeric) {
              case 1:
                api.TabSetInfo(Tab, "Nick", Split[0][2]);
                Server.MyHost = Split[0][Split[0].len()-1];
                break;
              case 372: // skip motd
                return;
            }
          }
          break;
      }
      if(Split[0].len() >= 4)
        api.AddMessage(Split[1][3], Server.Tab, 0, 0);
      break;
  }
}

function KeepAlive(Extra) {
//  print("still going")
  foreach(Id,Server in Sockets)
    api.NetSend(Server.Socket, "PING :"+rand()+"\r\n");
  return 30000;
}

api.AddTimer(30000, KeepAlive, null);
