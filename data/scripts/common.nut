enum Priority {
  LOWEST =  0,
  LOWER =   1,
  LOW =     2,
  NORMAL =  3,
  HIGH =    4,
  HIGHER =  5,
  HIGHEST = 6
};
const PROTOCOL_CMD = 8;

enum EventFlags {
  EF_ALREADY_HANDLED = 1,
  EF_DONT_SHOW = 2, // not implemented yet
};

enum EventReturn {
  NORMAL,    // don't affect the event
  HANDLED,   // message is already handled
  DELETE,    // delete event, stop processing it entirely
  BADSYNTAX, // syntax for command is bad
// you can also return the info string
};

enum TabFlags {
  COLOR_NORM = 0,  /* normal channel color */
  COLOR_TALK = 1,  /* someone talked */
  COLOR_DATA = 2,  /* other data, not talking*/
  COLOR_NAME = 3,  /* name said */
  COLOR_MASK = 3,  /* mask to only get colors */
  CHANNEL = 4,     /* is a channel */
  QUERY = 8,       /* is a query */
  SERVER = 16,     /* is a server */
  NOTINCHAN = 32,  /* user isn't in the channel, whether through being kicked or having parted */
  NOLOGGING = 64,  /* don't log this tab */
  HIDDEN = 128,    /* hidden, detached tab */
  IS_TYPING = 256, /* user is typing */ 
  HAS_TYPED = 512, /* has stuff typed */
}

enum MessageFlags {
  CLASS0 = 0,      // default
  CLASS1 = 1,      // people talking
  CLASS2 = 2,      // leaving/entering
  CLASS3 = 3,      // misc non-chat messages like modes
  CLASS4 = 4,      // ads
  CLASS5 = 5,
  CLASS6 = 6,
  CLASS7 = 7,
  CLASSMASK = 7,
  HIDDEN = 8,      // message is hidden for any reason
  IGNORED = 16,    // message source is ignored
  SCROLLBACK = 32, // message loaded from scrollback
  TEMP = 64        // temp alert message, not logged, cleared away easily
}

enum FF_Escapes {
  SUBSCRIPT = 0x80,
  SUPERSCRIPT = 0x81,
  STRIKEOUT = 0x82,
  HIDDEN = 0x83,        // only appears when copying and pasting the text
  PLAIN_ICON = 0x84,    // just an icon
  ICON_LINK = 0x85,     // icon, then link
  URL_LINK = 0x86,      //
  COMMAND_LINK = 0x87,  //
  NICK_LINK = 0x88,     // someone's name link
  CANCEL_COLORS = 0x89, //
  BG_ONLY = 0x8a,       //
};

enum SockEvents {
  CANT_CONNECT,         // can't connect to the socket
  CONNECTED,            // connected to the socket successfully
  NEW_DATA,             // new data from the socket
};

api.FFEscape <- function (Code) {
  return "\x00ff"+api.Chr(Code);
};
api.ToJSON <- function (Table) {
  if(typeof(Table)!="array" && typeof(Table)!="table")
    return Table.tostring();
  local Out = "";
  function AsString(Item) {
    switch(typeof(Item)) {
      case "table":
      case "array":
        return ToJSON(Item);
      case "string":
        local Len = Item.len();
        local Str = "";
        for(local i=0;i<Len;i++) {
          if(Item[i]=='\\' || Item[i]=='\"')
            Str+="\\";
          Str+=format("%c", Item[i])
        }
        return "\""+Str+"\"";
      default:
        return Item.tostring();
    }
  }
  if(typeof(Table) == "table") {
    if(Table.len() == 0) return "{}";
    Out = "{";
    foreach(Key,Val in Table)
      Out += "\""+Key+"\":"+AsString(Val)+", ";
    return Out.slice(0,-2) + "}";
  }
  if(typeof(Table) == "array") {
    if(Table.len() == 0) return "[]";
    Out = "[";
    foreach(Val in Table)
      Out += AsString(Val)+", ";
    return Out.slice(0,-2) + "]";
  }
}
api.LoadTable <- function (Filename) {
  local Stored = api.LoadTextFile(Filename);
  if(!Stored) return {};
  local Out = api.DecodeJSON(Stored);
  return Out?Out:{};
}
api.TryLoadTable <- function (Filename) {
  local Stored = api.LoadTextFile(Filename);
  if(!Stored) return null;
  return api.DecodeJSON(Stored);
}
api.SaveTable <- function (Filename, Contents) {
  api.SaveTextFile(Filename, api.ToJSON(Contents))
}
api.TabSetColor <- function (Context, Color) {
  local Ranks = [0, 2, 1, 3];
  local OldColor = api.TabGetFlags(Context)&TabFlags.COLOR_MASK;
  if(Ranks[Color] < Ranks[OldColor]) return;
  api.TabSetFlags(Context, (api.TabGetFlags(Context)&~TabFlags.COLOR_MASK)|Color)
}
api.TempMessage <- function (Message, Context) {
  if(Context == null) Context = api.GetInfo("Context");
  api.AddMessage(Message, Context, 0, MessageFlags.TEMP);
}
api.RunString <- function (Code) {
  return compilestring(Code)();
}
api.RunStringPublic <- function (Code) {
  return api.ToJSON(compilestring(Code)());
}
api.EvalAddon <- function (Addon, Code) {
  return api.DecodeJSON(EvalAddonJSON(Addon, Code));
}
api.ForceMinStrLen <- function (String, Length) {
  return format("%"+(-Length)+"s",String);
}
api.Button <- function (Text, Action) { // todo: make friendlier
  return "\x1f\x000302"+api.FFEscape(FF_Escapes.COMMAND_LINK)+Action+"\xfe"+Text+api.FFEscape(FF_Escapes.COMMAND_LINK)+"\x000f";
}
api.MakeForms <- function (Forms) {
  local Out = "";
  foreach(key,value in Forms) {
    if(Out!="")
      Out += "&";
    Out += key+"="+api.URLEncode(value);
  }
  return Out;
}
api.Event <- function (T, P, C) {
  if(typeof(P)=="table")
    api._Event(T, api.ToJSON(P), C);
  else
    api._Event(T, P, C);
}