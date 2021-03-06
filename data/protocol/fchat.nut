//print("F-Chat protocol loaded");
AddonInfo <- {
  "Name":"F-Chat"
  "Summary":"F-Chat protocol"
  "Version":"0.80"
  "Author":"NovaSquirrel"
};

local Sockets = {};
const ConfigPrefix = "Networks/FChat/";
Config <- api.LoadTable("fchat");

// config variables
JoinQueueTime <- 200; //api.GetConfigInt(200, ConfigPrefix+"Defaults/JoinQueueTime");
MessageQueueTime <- 2500; //api.GetConfigInt(2500, ConfigPrefix+"Defaults/MessageQueueTime");

class Server {
  Socket = 0;            // socket id
  Connected = false;     // currently connected to server
  Connecting = false;    // currently connecting
  TryReconnect = false;  // try to reconnect if disconnected
  UseSSL = false;
  ReconnectTimer = null;
  ReconnectDelay = 0;

  MyStatus = 0;          // for restoring when reconnecting
  MyStatusMessage = "";

  Tab = "";              // server context
  RawTab = "";           // server raw context
  Character = "";        // character for this connection

  Friends = null;
  Bookmarks = null;
  Channels = null;       // indexed by whatever f-chat calls the channel
  GlobalUsers = null;
  GlobalOps = null;
  WatchList = null;      // list of character to notify for, whether friends, bookmarks or neither
  AutoJoinSaved = false; // automatically join saved channels?

  Ticket = "";           // for site API stuff
  Host = "";             // for reconnecting
  Account = "";          // for reconnecting
  Password = "";         // for reconnecting 
  UseSSL = false;        // for reconnecting

  IgnoreList = null;
  CHA = null;            // public channel list
  ORS = null;            // private channel list
  VAR = null;            // server variables

  TalkTimer = null;
  TalkQueue = null;
  JoinTimer = null;
  JoinQueue = null;
  function Send(Command, Info) {
    if(Info == null) {
      api.NetSend(this.Socket, Command);
      if(Command != "PIN")
        api.AddMessage("<<\t"+Command, this.RawTab, 0, 0);
    } else {
      local SendThis = Command+" "+api.ToJSON(Info);
      api.AddMessage("<<\t"+SendThis, this.RawTab, 0, 0);
      api.NetSend(this.Socket, SendThis);
    }
  }

  function FindChannelWildcard(P) {
    foreach(Channel in this.CHA)
      if(api.WildMatch(Channel.name, P))
        return Channel.name;
    foreach(Channel in this.ORS)
      if(api.WildMatch(Channel.title, P))
        return Channel.name;
    return null;
  }

  function CallAPI(Callback, Name, Form, Extra) {
    function CallAPIGotTicket(Code, Data, Extra2) {
      if(Code != 0) {
        print("Error getting ticket");
        return;
      }
      Data = api.DecodeJSON(Data);
      if(Data["error"] != "") {
        print("Error: "+Data["error"]);
        return;
      }
      Form.account <- this.Account;
      Form.ticket <- Data["ticket"];
      api.CurlPost(Callback, null, API_URL+Name+".php", api.MakeForms(Form));
    }

    local GetTicketForm = api.MakeForms({"account":this.Account, "password":this.Password, "no_characters":true, "no_friends":true, "no_bookmarks":true});
    api.CurlPost(CallAPIGotTicket, null, "https://www.f-list.net/json/getApiTicket.php", GetTicketForm);
  }

  constructor() {
    Friends = [];
    Bookmarks = [];
    Channels = {};
    TalkQueue = [];
    JoinQueue = [];
    CHA = [];
    ORS = [];
    VAR = {
      "chat_max": 4096,
      "priv_max": 50000,
      "lfrp_max": 50000,
      "lfrp_flood": 600,
      "msg_flood": 0.5,
      "permissions": 0
    };
    GlobalUsers = {};
    GlobalOps = [];
    IgnoreList = [];
    WatchList = [];
  }
}

Statuses <- [
  "online", "looking", "idle", "away",
  "busy", "dnd", "crown"
];

Genders <- [
  "none", "female", "shemale", "male",
  "cunt-boy", "herm", "male-herm", "transgender"
];

class GlobalUser {
  Name = "";
  Gender = 0;
  Status = 0;
  StatusMessage = "";
  constructor(name, gender, status, message) {
    this.Name = name;
    this.Gender = Genders.find(gender.tolower());
    this.Status = Statuses.find(status.tolower());
    this.StatusMessage = api.ConvertBBCode(message);
  }
}

class ChannelStatus {
  Joined = false;
  Tab = "";
  Topic = "";    // description
  ShowName = ""; // display name
  RealName = ""; // protocol name for the channel
  AdsAllowed = true;
  ChatAllowed = true;

  Users = null;
  Ops = null;      // first name is owner
  constructor(Show, Real) {
    Users = [];
    Ops = [];
    ShowName = Show;
    RealName = Real;
  }
};

const API_URL = "http://www.f-list.net/json/api/";

function ConnectGotTicket(Code, Data, Extra) {
  if(Code != 0) {
    print("Error getting the initial ticket");
    return;
  }
  Data = api.DecodeJSON(Data);
  if(Data["error"] != "") {
    print("Error: "+Data["error"]);
    return;
  }

  local Options = Extra[0];
  local Host = Extra[1];
  local ServerName = Extra[2];
  local UseSSL = Extra[3];
  local AutoJoin = Extra[4];
  local Character = Extra[5];
  if(Character == "")
    Character = Data["default_character"];

  local NewServer = Server();
  NewServer.AutoJoinSaved = AutoJoin;
  NewServer.Character = Character;
  NewServer.Friends = Data["friends"];
  NewServer.Bookmarks = Data["bookmarks"];
  print(NewServer.Friends.len() + " friends, "+NewServer.Bookmarks.len()+ " bookmarks");
  NewServer.Tab = api.TabCreate("FC."+Character, "", TabFlags.SERVER);
  NewServer.RawTab = api.TabCreate("Raw", NewServer.Tab, 0);
  NewServer.Host = Host;
  NewServer.Ticket = Data["ticket"];
  NewServer.Account = Config["Account"];
  NewServer.Password = Config["Password"];
  NewServer.UseSSL = UseSSL;
  NewServer.Connecting = true;
  local Socket = api.NetOpen(FChat_Socket, Host, "websocket"+(UseSSL?" ssl":""));
  NewServer.Socket = Socket;
  api.TabSetInfo(NewServer.Tab, "Socket", Socket.tostring());
  api.TabSetInfo(NewServer.Tab, "Ticket", Data["ticket"]);
  api.TabSetInfo(NewServer.Tab, "Nick", NewServer.Character);
  Sockets[Socket] <- NewServer;
}

function Connect(Options) { // called by /connect
  if(!("Account" in Config)) {
    api.TempMessage("No account set, use /fchataccount Account Password", null);
    return;
  }
  print(Options);
  Options = api.CmdParamSplit(Options);

  local ServerName = Options[0][0];
  local UseSSL = false;
  local AutoJoin = true;
  local Character = "";
  local HostOverride = false;
  // parse options list
  for(local i=1;i<Options[0].len();i++) {
    local Option = Options[0][i];
    if(Option == "-ssl")
      UseSSL = true;
    if(Option == "-noautojoin")
      AutoJoin = false;
    local Split = split(Option, "=");
    if(Split.len() > 1) {
      if(Split[0] == "-char")
        Character = Split[1];
      else if(Split[0] == "-host")
        HostOverride = Split[1];
    }
  }

  local Host;
  if(!HostOverride)
    Host = api.GetConfigStr(ConfigPrefix+ServerName+"/Host", "");
  else
    Host = HostOverride;
  if(Host == "") {
    api.TempMessage("No host specified", null);
    return;
  }

  local SSLPort = api.GetConfigInt(ConfigPrefix+ServerName+"/Secure", 0);
  if(UseSSL && SSLPort)
    Host = api.SetHostnamePort(Host, SSLPort);

  local Form = api.MakeForms({"account":Config["Account"], "password":Config["Password"]});
  api.CurlPost(ConnectGotTicket, [Options, Host, ServerName, UseSSL, AutoJoin, Character], "https://www.f-list.net/json/getApiTicket.php", Form);
}

function GetOrMakeChannel(S, Show, Real) {
  if(Real in S.Channels)
    return S.Channels[Real];
  local NewChannel = ChannelStatus(Show, Real);
  NewChannel.Tab = api.TabCreate(Show, S.Tab, TabFlags.CHANNEL)
  NewChannel.Joined = true;
  S.Channels[Real] <- NewChannel;
  return NewChannel;
}

function HandleServerMessage(S, Command, P, Raw) {
// see https://wiki.f-list.net/F-Chat_Server_Commands
  function FixChannel(Id) {
    if(Id.len() > 4 && Id.slice(0,4) == "adh-")
      return Id.slice(0,4).toupper()+Id.slice(4);
    else
      return Id;
  }
  if(Command != "NLN" && Command != "FLN" && Command != "STA") {
    if(api.TabExists(S.RawTab) && Raw.len() < 700)
      api.AddMessage(">>\t"+Command+" "+Raw, S.RawTab, 0, 0);
    print("Command received: "+Command);//+" "+api.ToJSON(P));
  }
//  api.Event("fchat raw", {"Command":Command, "Info":Raw}, S.Tab);
  local Channel;
  switch(Command) {
    case "IDN": // identified
      S.Connected = true;
      S.Connecting = false;
      api.TabRemoveFlags(S.Tab, TabFlags.NOTINCHAN);
      S.TryReconnect = true;
      S.Send("CHA", null);
      S.Send("ORS", null);
      api.Event("server connected", {"Nick":S.Character}, S.Tab);
      if(S.MyStatus || (S.MyStatusMessage != ""))
        S.Send("STA", {"status":Statuses[S.MyStatus], "statusmsg":S.MyStatusMessage});
      foreach(key,val in S.Channels)
        if(val.Joined)
          api.Command("join", key, S.Tab);
      break;
    case "VAR": // change variable
      S.VAR[P.variable] <- P.value;
      break;
    case "ADL": // list of chatops
      S.GlobalOps = P.ops;
      break;
    case "AOP": // someone has been promoted
      break;
    case "BRO": // broadcast
      api.Event("user notice", {"Nick":"Global", "Text":api.ConvertBBCode(P.description)}, S.Tab);
      break;
    case "CDS": // change channel description
      Channel = S.Channels[P.channel];
      P.description = api.ConvertBBCode(P.description);
      if(Channel.Topic == "")
        api.Event("channel topic", {"Text":P.description}, Channel.Tab);
      else
        api.Event("channel topic change", {"Text":P.description}, Channel.Tab);
      Channel.Topic = P.description;
      break;
    case "CHA": // public channels list
      S.CHA = P.channels;
      break;
    case "ORS": // private channels list
      S.ORS = P.channels;
      break;
    case "CIU": // channel invite
      api.Event("you invite", {"Nick":P.sender, "Channel":P.title, "JoinName":P.channel}, S.Tab);
      break;
    case "CBU": // channel ban
      Channel = S.Channels[P.channel];
      api.Event("channel ban+", {"Nick":P.operator, "Target":P.character}, Channel.Tab);
      break;
    case "CKU": // channel kick
      Channel = S.Channels[P.channel];
      api.Event("channel kick", {"Nick":P.operator, "Target":P.character}, Channel.Tab);
      if(P.character == S.Character) {
        api.TabAddFlags(Channel.Tab, TabFlags.NOTINCHAN);
        Channel.Joined = false;
      }
      break;
    case "COA": // channel promote
      Channel = S.Channels[P.channel];
      api.Event("channel status+", {"Target":P.character, "Status":"operator"}, Channel.Tab);
      break;
    case "COR": // channel demote
      Channel = S.Channels[P.channel];
      api.Event("channel status-", {"Target":P.character, "Status":"operator"}, Channel.Tab);
      break;
    case "COL": // channel operator list
      Channel = S.Channels[P.channel];
      Channel.Ops = P.oplist;
      break;
    case "CON": // number of users connected
      api.AddMessage("There are "+P.count+" users connected", S.Tab, 0, 0);
      break;
    case "CSO": // change owner
      Channel = S.Channels[P.channel];
      Channel.Ops[0] = P.character;      
      api.Event("channel status+", {"Target":P.character, "Status":"owner"}, Channel.Tab);
      break;
    case "CTU": // timeout user
      Channel = S.Channels[P.channel];
      api.Event("channel ban+", {"Nick":P.operator, "Target":P.character, "Timeout":P.length}, Channel.Tab);
      break;
    case "DOP": // demote global op
      api.Event("channel status-", {"Target":P.character, "Status":"operator"}, S.Tab);
      break;
    case "ERR": // error; todo: make more specific
      api.Event("generic error", {"Text":P.message, "Number":P.number}, S.Tab);
      break;
    case "FKS": // search results
      api.Event("fchat fks", {"Nicks":P.characters, "Kinks":P.kinks}, Channel.Tab);
      break;
    case "FLN": // a character went offline
      // todo: remove the user from all channels too
      delete S.GlobalUsers[P.character.tolower()];
      api.Event("fchat offline", {"Nick":P.character}, S.Tab);
      break;
    case "HLO": // hello
      api.AddMessage(P.message, S.Tab, 0, 0);
      break;
    case "JCH": // join
      Channel = GetOrMakeChannel(S, P.title, P.channel);
      api.Event("channel join", api.ToJSON({"Nick":P.character.identity}), Channel.Tab);
      break;
    case "ICH": // initial channel data
      local NewChannel = S.Channels[P.channel];
      if(P.mode == "ads")
        NewChannel.ChatAllowed = false;
      else if(P.mode == "chat")
        NewChannel.AdsAllowed = false;
      NewChannel.Users = [];
      foreach(key,value in P.users)
        if(key=="identity")
          NewChannel.Users.append(value);
      NewChannel.Joined = true
      api.TabRemoveFlags(NewChannel.Tab, TabFlags.NOTINCHAN);
      break;
    case "KID": // reply to KIN
      break;
    case "LCH": // left channel
      // remove person from list
      Channel = S.Channels[P.channel];
      if(P.character == S.Character) {
        Channel.Joined = false;
        api.Event("you part", {"Nick":P.character}, Channel.Tab);
      } else
        api.Event("channel part", {"Nick":P.character}, Channel.Tab);
      break;
    case "LIS": // list of all users
      foreach(User in P.characters)
        S.GlobalUsers[User[0].tolower()] <- GlobalUser(User[0], User[1], User[2], User[3]);
      break;
    case "NLN": // user came online
      S.GlobalUsers[P.identity.tolower()] <- GlobalUser(P.identity, P.gender, P.status, "");
      api.Event("fchat online", {"Nick":P.identity, "Gender":P.gender, "Status":P.status}, S.Tab);
      break;
    case "IGN": // ignore
      // todo: acknowledge add and delete?
      if(P.action == "init")
        S.IgnoreList = P.characters;
      break;
    case "FRL": // friends list and bookmarks list combined
      S.WatchList.extend(P.characters);
      break;
    case "PRD": // response to a PRO
      break;
    case "MSG": // channel message
    case "PRI": // private message
      local C = "";
      if(Command == "MSG")
        C = S.Channels[P.channel].Tab;
      else {
        if(!api.MakeContextExists(S.Tab, P.character))
          C = api.TabCreate(P.character, S.Tab, TabFlags.QUERY)
        else
          C = api.MakeContext(S.Tab, P.character);
      }
      P.message = api.ConvertBBCode(P.message);
      if(P.message.len() > 4 && (P.message.tolower().slice(0, 4) == "/me "))
        api.Event("user action", {"Nick":P.character, "Text":P.message.slice(4)}, C);
      else
        api.Event("user message", {"Nick":P.character, "Text":P.message}, C);
      break;
    case "LRP": // RP ad
      Channel = S.Channels[P.channel];
      api.Event("user advertisement", {"Nick":P.character, "Text":api.ConvertBBCode(P.message)}, Channel.Tab);
      break;
    case "RLL": // roll, or bottle spin
      // todo
      break;
    case "RMO": // change room mode
      Channel = S.Channels[P.channel];
      Channel.ChatAllowed = true;
      Channel.AdsAllowed = true;
      if(P.mode == "chat")
        Channel.AdsAllowed = false;
      else if(P.mode == "ads")
        Channel.ChatAllowed = false;
      break;
    case "RTB": // real-time bridge
      switch(P.type) {
        case "note":
          api.Event("user note", {"Subject":P.subject, "Nick":P.sender}, S.Tab);
          break;
        case "comment":
          // implement later
          break;
        case "trackadd":
          api.Event("bookmark+", {"Nick":P.name}, S.Tab);
          break;
        case "trackrem":
          api.Event("bookmark-", {"Nick":P.name}, S.Tab);
          break;
        case "friendadd":
          api.Event("friends+", {"Nick":P.name}, S.Tab);
          break;
        case "friendremove":
          api.Event("friends-", {"Nick":P.name}, S.Tab);
          break;
        case "friendrequest":
          api.Event("friends?", {"Nick":P.name}, S.Tab);
          break;
      }
      break;
    case "SFC": // reporting someone
      break;
    case "STA": // status change
      local Character = S.GlobalUsers[P.character.tolower()];
      Character.Status = Statuses.find(P.status.tolower());
      Character.StatusMessage = api.ConvertBBCode(P.statusmsg);
      api.Event("fchat status", {"Nick":P.character, "Gender":Genders[Character.Gender], "Status":P.status, "Text":P.statusmsg}, S.Tab);
      break;
    case "SYS": // system message
      if("channel" in P) {
        api.AddMessage(api.ConvertBBCode(P.message), S.Channels[FixChannel(P.channel)].Tab, 0, 0);
        api.Event("fchat sys", {"Channel":P.channel, "Text":P.message}, S.Tab);
      } else {
        api.AddMessage(api.ConvertBBCode(P.message), S.Tab, 0, 0);
        api.Event("fchat sys", {"Text":P.message}, S.Tab);
      }
      break;
    case "TPN": // typing status
      api.Event("fchat tpn", {"Nick":P.character, "Status":P.status}, S.Tab);
      break;
    case "UPT": // uptime and other info
      api.AddMessage("Started at:"+P.startstring+" Accepted:"+P.accepted+" Channels:"+P.channels+" Users:"+P.users+" MaxUsers:"+P.maxusers, S.Tab, 0, 0);
      break;
  }
}

function ReconnectTimer(S) {
  if(S.Connected || S.Connecting)
    return false;
  api.Command("reconnect", "", S.Tab);
  S.ReconnectDelay += 15;
  api.AddMessage("Retrying reconnect in " +S.ReconnectDelay, S.Tab, 0, 0);
  return S.ReconnectDelay * 1000;
}

function FChat_Socket(Socket, Event, Text) {
  switch(Event) {
    case SockEvents.CANT_CONNECT:
      print("Can't connect: "+Text);
      break;
    case SockEvents.DISCONNECTED:
      print("Disconnected: "+Text);
      local Server = Sockets[Socket];
      api.AddMessage("Disconnected ("+Text+")", Server.Tab, 0, 0);
      Server.Connected = false;
      Server.Connecting = false;
      if(Server.TryReconnect) {
        Server.ReconnectDelay = 0;
        Server.ReconnectTimer = api.AddTimer(0, ReconnectTimer, Server);
      }
      break;
    case SockEvents.CONNECTED:
      print("Connected: "+Text);
      local TheServer = Sockets[Socket];
      TheServer.Connected = false; // wait until IDN to know it's actually connected
      api.TabAddFlags(TheServer.Tab, TabFlags.NOTINCHAN);
      TheServer.Send("IDN", {"method": "ticket", "account": TheServer.Account, "ticket": TheServer.Ticket, "character":  TheServer.Character, "cname": api.GetInfo("ClientName"), "cversion": AddonInfo.Version+" (on "+api.GetInfo("ClientVersion")+")"});
      break;
    case SockEvents.NEW_DATA:
      local TheServer = Sockets[Socket];
      if(Text == "PIN")
        break;
      if(Text.len() == 3)
        HandleServerMessage(Sockets[Socket], Text, {}, "{}");
      else
        HandleServerMessage(Sockets[Socket], Text.slice(0,3), api.DecodeJSON(Text.slice(4)), Text.slice(4));
      break;
  }
}

function KeepAlive(Extra) {
  foreach(Id,Server in Sockets)
    Server.Send("PIN", null);
  return 35000;
}
api.AddTimer(35000, KeepAlive, null);

function IdForChannelName(S, Name) {
  foreach(key,value in S.Channels)
    if(value.ShowName == Name)
      return value.RealName;
  return null;
}
function MsgDelayTimer(S) {
  if(S.TalkQueue.len()) {
    local Command = S.TalkQueue.pop();
    S.Send(Command[0], Command[1]);
    return (S.VAR.msg_flood * MessageQueueTime).tointeger();
  }
  S.TalkTimer = null;
  return false;
}
function SayMe(S, Text, C) {
  function DoQueue(Command, Param) {
    if(S.TalkTimer)
      S.TalkQueue.push([Command, Param]);
    else {
      S.Send(Command, Param);
      if(MessageQueueTime) // only use the queue if a time is set
        S.TalkTimer = api.AddTimer((S.VAR.msg_flood * MessageQueueTime).tointeger(), MsgDelayTimer, S);
    }
  }
  local Channel = api.TabGetInfo(C, "channel");
  if(api.TabHasFlags(C, TabFlags.QUERY))
    DoQueue("PRI", {"recipient":Channel, "message":Text});
  else
    DoQueue("MSG", {"channel":IdForChannelName(S, Channel), "message":Text});
}
function FailIfNotChannel(T, C) {
  if(!(api.TabHasFlags(C, TabFlags.CHANNEL))) {
    api.TempMessage("\""T+"\" command is only usable in a channel", C);
    return 1;
  }
  return 0;
}
function SayCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  api.Event("you message", {"Nick":api.TabGetInfo(C, "Nick"),"Text":api.ConvertBBCode(P, 1)}, C);
  SayMe(S, P, C);
  return EventReturn.HANDLED;
}
function MeCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  api.Event("you action", {"Nick":api.TabGetInfo(C, "Nick"),"Text":api.ConvertBBCode(P, 1)}, C);
  SayMe(S, "/me "+P, C);
  return EventReturn.HANDLED;
}
function BBTestCmd(T, P, C) {
  api.TempMessage(api.ConvertBBCode(P), C);
  return EventReturn.HANDLED;
}
function RawCmd(T, P, C) {
  api.NetSend(api.TabGetInfo(C, "Socket"), P);
  return EventReturn.HANDLED;
}
function QueryCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  if(!api.MakeContextExists(S.Tab, P))
    api.TabCreate(P, S.Tab, TabFlags.QUERY)
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
  local NewSockId = api.NetOpen(FChat_Socket, Host, "websocket"+(OldSock.UseSSL?" ssl":""));
  OldSock.Socket = NewSockId;
  api.TabSetInfo(OldSock.Tab, "Socket", NewSockId.tostring());
 
  Sockets[NewSockId] <- OldSock;
  delete Sockets[OldSockId];

  return EventReturn.HANDLED;
}
function QuitCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  MarkNotInChannels(C);
  S.Connected = false;
  S.TryReconnect = false;
  return EventReturn.HANDLED;
}
function PartCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local Channel = IdForChannelName(S, api.TabGetInfo(C, "channel"));
  S.Send("LCH", {"channel":Channel});
  api.TabAddFlags(C, TabFlags.NOTINCHAN);
  S.Channels[Channel].Joined = false;
  return EventReturn.HANDLED;
}
function CloseCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  if(api.TabHasFlags(C, TabFlags.CHANNEL))
    api.Command("part", "", C);
  return EventReturn.NORMAL;
}
function AdCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  S.Send("LRP", {"channel":IdForChannelName(S, api.TabGetInfo(C, "channel")), "message":P});
  return EventReturn.HANDLED;
}
function StatusCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  P = api.CmdParamSplit(P);
  local StatusId = Statuses.find(P[0][0].tolower()); // allow only valid statuses
  if(StatusId == null) {
    api.TempMessage("Status \""+P[0][0]+"\" not valid", C);
    return EventReturn.HANDLED;
  }
  local Message = "";
  if(1 in P[1])
    Message = P[1][1];
  S.MyStatus = StatusId; // save status so it can be restored when reconnecting
  S.MyStatusMessage = Message;
  S.Send("STA", {"status":P[0][0], "statusmsg":Message});
  return EventReturn.HANDLED;
}
function AwayCmd(T, P, C) {
  api.Command("status", "away"+P, C);
  return EventReturn.HANDLED;
}
function BackCmd(T, P, C) {
  api.Command("status", "online", C);
  return EventReturn.HANDLED;
}
function CodeCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local Id = IdForChannelName(S, api.TabGetInfo(C, "channel"));
  api.TempMessage("Channel ID ("+Id+") copied to clipboard", C);
  api.SetClipboard(Id);
  return EventReturn.HANDLED;
}
function MakeRoomCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  S.Send("CCR", {"channel":P});
  return EventReturn.HANDLED;
}
function OpenRoomCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local Channel = IdForChannelName(S, api.TabGetInfo(C, "channel"));
  S.Send("RST", {"channel":Channel, "status":"public"});
  return EventReturn.HANDLED;
}
function CloseRoomCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local Channel = IdForChannelName(S, api.TabGetInfo(C, "channel"));
  S.Send("RST", {"channel":Channel, "status":"private"});
  return EventReturn.HANDLED;
}
function SetModeCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local Channel = IdForChannelName(S, api.TabGetInfo(C, "channel"));
  S.Send("RMO", {"channel":Channel, "mode":P});
  return EventReturn.HANDLED;
}
function ModeCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  P = api.CmdParamSplit(P);
  switch(P[0][0]) {
    case "+o": api.Command("op", P[1][1], C); break;
    case "-o": api.Command("deop", P[1][1], C); break;
    case "+s": case "+i": api.Command("openroom", "", C); break;
    case "-s": case "-i": api.Command("closeroom", "", C); break;
    case "+b": api.Command("ban", P[1][1], C); break;
    case "-b": api.Command("unban", P[1][1], C); break;
  }
  return EventReturn.HANDLED;
}
function OpCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local Channel = IdForChannelName(S, api.TabGetInfo(C, "channel"));
  S.Send("COA", {"channel":Channel, "character":P});
  return EventReturn.HANDLED;
}
function DeopCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local Channel = IdForChannelName(S, api.TabGetInfo(C, "channel"));
  S.Send("COR", {"channel":Channel, "character":P});
  return EventReturn.HANDLED;
}
function SetOwnerCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local Channel = IdForChannelName(S, api.TabGetInfo(C, "channel"));
  S.Send("CSO", {"channel":Channel, "character":P});
  return EventReturn.HANDLED;
}
function CycleCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local Channel = IdForChannelName(S, api.TabGetInfo(C, "channel"));
  S.Send("LCH", {"channel":Channel});
  S.Send("JCH", {"channel":Channel});
  return EventReturn.HANDLED;
}
function TopicCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local Channel = IdForChannelName(S, api.TabGetInfo(C, "channel"));
  if(P != "")
    S.Send("CDS", {"channel":Channel, "description":P});
  else {
    // find channel topic
  }
  return EventReturn.HANDLED;
}
function WarnCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];  
  return EventReturn.HANDLED;
}
function KickCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local Channel = IdForChannelName(S, api.TabGetInfo(C, "channel"));
  S.Send("CKU", {"channel":Channel, "characer":P});
  return EventReturn.HANDLED;
}
function BanCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local Channel = IdForChannelName(S, api.TabGetInfo(C, "channel"));
  S.Send("CBU", {"channel":Channel, "character":P});
  return EventReturn.HANDLED;
}
function UnbanCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local Channel = IdForChannelName(S, api.TabGetInfo(C, "channel"));
  S.Send("CUB", {"channel":Channel, "character":P});
  return EventReturn.HANDLED;
}
function BanlistCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local Channel = IdForChannelName(S, api.TabGetInfo(C, "channel"));
  S.Send("CBL", {"channel":Channel});
  return EventReturn.HANDLED;
}
function TimeoutCmd(T, P, C) {
// todo: allow comma, instead of requiring minutes first
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  P = api.CmdParamSplit(P);
  local Channel = IdForChannelName(S, api.TabGetInfo(C, "channel"));
  S.Send("CTU", {"channel":Channel, "character":P[1][1], "length":P[0][0]});
  return EventReturn.HANDLED;
}
function UptimeCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  S.Send("UPT", null);
  return EventReturn.HANDLED;
}
function InviteCmd(T, P, C) {
  if(FailIfNotChannel(T, C)) return EventReturn.HANDLED;
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local Channel = IdForChannelName(S, api.TabGetInfo(C, "channel"));
  S.Send("CIU", {"channel":Channel, "character":P});
  return EventReturn.HANDLED;
}
function WhoisCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  return EventReturn.HANDLED;
}

function JoinDelayTimer(S) {
  if(S.JoinQueue.len()) {
    S.Send("JCH", {"channel":S.JoinQueue.pop()});   
    return JoinQueueTime;
  }
  S.JoinTimer = null;
  return false;
}
function JoinCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  local Channel = null;
  if(P.len() > 4 && P.slice(0,4) == "ADH-")
    Channel = P;
  else { // wildcard search
    local Find = S.FindChannelWildcard(P);
    if(Find)
      Channel = Find;
  }

  if(!Channel) {
    api.TempMessage("Can't find channel \""+P+"\"", C);
    return EventReturn.HANDLED;
  }

  if(S.JoinTimer)
    S.JoinQueue.push(Channel);
  else {
    S.Send("JCH", {"channel":Channel});
    S.JoinTimer = api.AddTimer(JoinQueueTime, JoinDelayTimer, S);
  }
  return EventReturn.HANDLED;
}
function UsersCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  if(api.MakeContextExists(S.Tab,"!Users"))
    api.TabRemove(api.MakeContext(S.Tab,"!Users"));
  C = api.TabCreate("!Users", S.Tab, TabFlags.NOLOGGING)

  if(P == "") P = "*";

  foreach(User in S.GlobalUsers)
    if(api.WildMatch(User.Name, P))
      api.AddMessage(format("%s (%s) is %s: %s", User.Name, Genders[User.Gender], Statuses[User.Status], User.StatusMessage), C, 0, 0);
  return EventReturn.HANDLED;
}

function ListCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  if(api.MakeContextExists(S.Tab,"!List"))
    api.TabRemove(api.MakeContext(S.Tab,"!List"));
  C = api.TabCreate("!List", S.Tab, TabFlags.NOLOGGING)

  if(P == "") P = "*";

  api.AddMessage("\x02Public:", C, 0, 0);
  foreach(Channel in S.CHA)
    if(api.WildMatch(Channel.name, P))
      api.AddMessage(api.Button("Join", "join "+Channel.name)+format(" %3i ", Channel.characters)+Channel.name, C, 0, 0);

  api.AddMessage("\x02Private:", C, 0, 0);
  foreach(Channel in S.ORS)
    if(api.WildMatch(Channel.title, P))
      api.AddMessage(api.Button("Join", "join "+Channel.name)+format(" %3i ", Channel.characters)+Channel.title, C, 0, 0);
  return EventReturn.HANDLED;
}
function FChatAccountCmd(T, P, C) {
  P = api.CmdParamSplit(P);
  Config["Account"] <- P[0][0];
  Config["Password"] <- P[1][1];
  api.TempMessage("F-Chat account set to be "+P[0][0], C);
  api.SaveTable("fchat", Config);
  return EventReturn.HANDLED;
}

api.AddCommandHook("say",      SayCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("me",       MeCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("quote",    RawCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("query",    QueryCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("quit",     QuitCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("ad",       AdCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("join",     JoinCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("close",    CloseCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("part",     PartCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("uptime",   UptimeCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("status",   StatusCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("away",     AwayCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("back",     BackCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("code",     CodeCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("makeroom", MakeRoomCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("openroom", OpenRoomCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("closeroom",CloseRoomCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("setmode",  SetModeCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("mode",     ModeCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("op",       OpCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("deop",     DeopCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("setowner", SetOwnerCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("cycle",    CycleCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("topic",    TopicCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("invite",   InviteCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("warn",     WarnCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("kick",     KickCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("ban",      BanCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("kickban",  BanCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("unban",    UnbanCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("banlist",  BanlistCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("timeout",  TimeoutCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("whois",    WhoisCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("list",     ListCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("users",    UsersCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("preview",  BBTestCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);
api.AddCommandHook("bbtest",   BBTestCmd, Priority.NORMAL, null, null);
api.AddCommandHook("fchataccount", FChatAccountCmd, Priority.NORMAL, null, null);
api.AddCommandHook("reconnect",  ReconnectCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);

function ApiTestCmd(T, P, C) {
  local S = Sockets[api.TabGetInfo(C, "Socket")];
  P = api.CmdParamSplit(P);
  function Display(Code, Data, Extra) {
    print(Data);
  };
  if(1 in P[1]) { // has argument?
    local Params = compilestring("return "+P[1][1])();
    S.CallAPI(Display, P[0][0], Params, null);
  } else
    S.CallAPI(Display, P[0][0], {}, null);
  return EventReturn.HANDLED;
}
api.AddCommandHook("jsonapi",  ApiTestCmd, Priority.NORMAL|PROTOCOL_CMD, null, null);

function SaveConfig(T, P, C) {
  api.SaveTable("fchat", Config);
  return EventReturn.NORMAL;
}
api.AddEventHook("saveall",  SaveConfig, Priority.NORMAL, 0, 0, 0);

function Focus(T, P, C) {
  local OldProtocol = api.TabGetInfo(P,"Protocol");
  local NewProtocol = api.TabGetInfo(C,"Protocol")
  return EventReturn.NORMAL;
}
api.AddEventHook("focus tab",  Focus, Priority.NORMAL, 0, 0, 0);
