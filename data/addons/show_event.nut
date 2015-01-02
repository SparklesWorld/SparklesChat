AddonInfo <- {
  "Name":"Show Event"
  "Summary":"Displays events"
  "Version":"0.01"
  "Author":"NovaSquirrel"
};

function HSL2RGB(h, s, l) {
// from http://stackoverflow.com/questions/2353211/hsl-to-rgb-color-conversion
  local r, g, b;

  if(!s){
    r = g = b = l; // achromatic
  } else {
    function hue2rgb(p, q, t) {
      if(t < 0) t++;
      if(t > 1) t--;
      if(t < 1.0/6) return p + (q - p) * 6.0 * t;
      if(t < 1.0/2) return q;
      if(t < 2.0/3) return p + (q - p) * (2.0/3 - t) * 6.0;
      return p;
    }

    local q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
    local p = 2.0 * l - q;
    r = hue2rgb(p, q, h + 1.0/3);
    g = hue2rgb(p, q, h);
    b = hue2rgb(p, q, h - 1.0/3);
  }

  return [(r * 255).tointeger(), (g * 255).tointeger(), (b * 255).tointeger()];
}

function ColorizeNick(Nick) {
  local hash1 = api.Num(api.MD5(Nick+Nick+Nick).slice(0, 8),16);
  local hash2 = api.Num(api.MD5(Nick+Nick.len()).slice(8, 16),16);
  local hash3 = api.Num(api.MD5(Nick+Nick).slice(16, 24),16);
  local hash4 = api.Num(api.MD5(Nick+Nick+Nick.len()).slice(24, 32),16);
  local hash = abs(hash1+hash2+hash3+hash4)/2;
  local max = 1073741823;
  local sat = 0.8 + (abs(hash1&max).tofloat()/(max)*0.2);
  local light = 0.3 + (abs(hash4&max).tofloat()/(max)*0.05);
  local hue = (hash&max).tofloat()/max;
  local rgb = HSL2RGB(hue, sat, light);
  return format("\x0004%.2x%.2x%.2x%s\x000f", rgb[0], rgb[1], rgb[2], Nick);
}

function IsActionMy(Text) {
  if(Text.len() < 2) return " ";
  if(Text.slice(0,2)=="'s") return "";
  return " ";
}

function EventString(T, P, C) {
  const IsAction = "*\t";
  const IsNotice = "!\t";
  const IsNewMsg = 1;
  const IsNewData = 2;
  const IsNewHilight = 3;
  if("Nick" in P)
    P["Nick"] <- ColorizeNick(P["Nick"]);
  if("Target" in P)
    P["Target"] <- ColorizeNick(P["Target"]);

  switch(T) { // important: don't capitalize event names, they won't match here
    case "generic message":
      return P.Nick+"\t"+P.Text;
    case "notify+":
      return IsNotice+P.Nick+" added to your notify list";
    case "notify-":
      return IsNotice+P.Nick+" removed from your notify list";

    case "notify away":
      return IsNotice+"Notify: "+P.Nick+" is now away";
    case "notify back":
      return IsNotice+"Notify: "+P.Nick+" is now back";
    case "notify status":
      if("Text" in P)
        return IsNotice+"Notify: "+P.Nick+" set status to "+P.Status+" ("+P.Text+")";
      else
        return IsNotice+"Notify: "+P.Nick+" set status to "+P.Status;

    case "notify online":
      return IsNotice+"Notify: "+P.Nick+" is now online";
    case "notify offline":
      return IsNotice+"Notify: "+P.Nick+" is now offline";

    case "say fail banned":
      api.TabSetColor(C, IsNewData);
      return "Can't send (You're banned)";
    case "say fail mute":
      api.TabSetColor(C, IsNewData);
      return "Can't send (Don't have voice)";
    case "say fail ignored":
      api.TabSetColor(C, IsNewData);
      return "Can't send (User has you ignored)";
    case "say fail generic":
      api.TabSetColor(C, IsNewData);
      return "Can't send ("+P.Text+")";

    case "bookmark+":
      api.TabSetColor(C, IsNewData);
      return IsNotice+P.Nick+" added to your bookmark list";
    case "bookmark-":
      api.TabSetColor(C, IsNewData);
      return IsNotice+P.Nick+" removed from your bookmark list";
    case "watch+":
      api.TabSetColor(C, IsNewData);
      return IsNotice+P.Nick+" added to your name watch list";
    case "watch-":
      api.TabSetColor(C, IsNewData);
      return IsNotice+P.Nick+" removed from your name watch list";
    case "friends+":
      api.TabSetColor(C, IsNewData);
      return IsNotice+P.Nick+" added to your friends list";
    case "friends-":
      api.TabSetColor(C, IsNewData);
      return IsNotice+P.Nick+" removed from your friends list";
    case "friends?":
      api.TabSetColor(C, IsNewData);
      return IsNotice+P.Nick+" wants to be your friend";
    case "nick clash":
      api.TabSetColor(C, IsNewData);
      return IsNotice+P.Nick+" already in use, retrying at"+P.Target;
    case "nick fail":
      api.TabSetColor(C, IsNewData);
      return IsNotice+P.Nick+" already in use. Try another with /nick";

    case "user change nick":
      api.TabSetColor(C, IsNewData);
      return IsNotice+P.Nick+ " is now known as "+P.NewNick;
    case "you change nick":
      return IsNotice+"You are now known as "+P.NewNick;

    case "join fail banned":
      api.TabSetColor(C, IsNewData);
      return IsNotice+"Cannot join "+api.TabGetInfo(C, "Channel")+" (You are banned)";
    case "join fail keyword":
      api.TabSetColor(C, IsNewData);
      return IsNotice+"Cannot join "+api.TabGetInfo(C, "Channel")+" (Requires keyword)";
    case "join fail full":
      api.TabSetColor(C, IsNewData);
      return IsNotice+"Cannot join "+api.TabGetInfo(C, "Channel")+" (Channel is full)";
    case "join fail invite":
      api.TabSetColor(C, IsNewData);
      return IsNotice+"Cannot join "+api.TabGetInfo(C, "Channel")+" (Channel is invite-only)";

    case "user message hilight":
      api.TabSetColor(C, IsNewHilight);
      return "<"+P.Nick+">\t"+P.Text;
    case "user action hilight":
      api.TabSetColor(C, IsNewHilight);
      return IsAction+P.Nick+IsActionMy(P.Text)+P.Text;
    case "user message":
      api.TabSetColor(C, IsNewMsg);
      return "<"+P.Nick+">\t"+P.Text;
    case "user action":
      api.TabSetColor(C, IsNewMsg);
      return IsAction+P.Nick+IsActionMy(P.Text)+P.Text;
    case "user notice":
      api.TabSetColor(C, IsNewMsg);
      return "-"+P.Nick+"-\t"+P.Text;

    case "you message":
      return "<"+P.Nick+">\t"+P.Text;
    case "you action":
      return IsAction+P.Nick+IsActionMy(P.Text)+P.Text;
    case "you msg message":
    case "you notice":
      return ">"+P.Target+"<\t"+P.Text;
    case "you CTCP":
      return ">"+P.Target+"<\tCTCP "+P.Text;
    case "user CTCP":
      api.TabSetColor(C, IsNewData);
      if("Channel" in P)
        return "Received CTCP "+P.Text+" (to "+api.TabGetInfo(C, "Channel")+")";
      else
        return "Received CTCP "+P.Text;

    case "server connecting":
      return IsNotice+"Connecting to "+P.Host+" ("+P.IP+") "+" port "+P.Port;
    case "server connected":
      return IsNotice+"Connected!";
    case "server disconnected":
      return IsNotice+"Disconnected";
    case "server connection failed":
      return IsNotice+"Connection failed!";

    case "user advertisement":
      api.TabSetColor(C, IsNewMsg);
      return "!"+P.Nick+"!\t"+P.Text;
    case "user note":
      api.TabSetColor(C, IsNewMsg);
      return "New note from "+P.Nick+" ("+P.Subject+")";
    case "roll":
      return ""; // todo

    case "generic error":
      return IsNotice+"Error: "+P.Text;

    case "you invite":
      api.TabSetColor(C, IsNewData);
      if("JoinName" in P)
        return IsNotice+P.Nick+" has invited you to "+P.Channel+" "+api.Button("Join", "join "+P.JoinName);
      else
        return IsNotice+P.Nick+" has invited you to "+P.Channel+" "+api.Button("Join", "join "+P.Channel);
    case "channel invite":
      api.TabSetColor(C, IsNewData);
      return IsNotice+P.Nick+" has invited "+P.Target;

    case "channel topic":
      return "Topic for "+api.TabGetInfo(C, "Channel")+" is: "+P.Text;
    case "channel topic change":
      api.TabSetColor(C, IsNewData);
      if("Nick" in P)
        return IsNotice+P.Nick+" changes "+api.TabGetInfo(C, "Channel")+"'s topic to: "+P.Text;
      else
        return IsNotice+"Topic for "+api.TabGetInfo(C, "Channel")+" changed to: "+P.Text;

    case "channel ban+":
      api.TabSetColor(C, IsNewData);
      if("Timeout" in P)
        return IsNotice+P.Nick+" has banned "+P.Target+" for "+P.Timeout+" minutes";
      else
        return IsNotice+P.Nick+" has banned "+P.Target;
    case "channel ban-":
      return IsNotice+P.Nick+" has unbanned "+P.Target;

    case "channel join":
      api.TabSetColor(C, IsNewData);
      if("Host" in P)
        return IsNotice+P.Nick+" ("+P.Host+") has joined "+api.TabGetInfo(C, "Channel");
      else
        return IsNotice+P.Nick+" has joined "+api.TabGetInfo(C, "Channel");

    case "channel kick":
      api.TabSetColor(C, IsNewData);
      if("Text" in P)
        return IsNotice+P.Nick+" has kicked "+P.Target+" ("+P.Text+")";
      else
        return IsNotice+P.Nick+" has kicked "+P.Target;

    case "channel you kick":
      api.TabSetColor(C, IsNewData);
      if("Text" in P)
        return IsNotice+P.Nick+" has kicked you ("+P.Text+")";
      else
        return IsNotice+P.Nick+" has kicked you";

    case "user disconnect":
    case "channel disconnect":
      api.TabSetColor(C, IsNewData);
      if("Text" in P)
        return IsNotice+P.Nick+" has disconnected ("+P.Text+")";
      else
        return IsNotice+P.Nick+" has disconnected";

    case "channel part":
      api.TabSetColor(C, IsNewData);
      if("Text" in P)
        return IsNotice+P.Nick+" has left ("+P.Text+")";
      else
        return IsNotice+P.Nick+" has left";
    case "you part":
      api.TabSetColor(C, IsNewData);
      if("Text" in P)
        return IsNotice+"You have left ("+P.Text+")";
      else
        return IsNotice+"You have left";

    case "channel status+":
      api.TabSetColor(C, IsNewData);
      if("Nick" in P)
        return IsNotice+P.Nick+" gives \""+P.Status+"\" status to "+P.Target;
      else
        return IsNotice+"Someone gives \""+P.Status+"\" status to "+P.Target;
    case "channel status-":
      api.TabSetColor(C, IsNewData);
      if("Nick" in P)
        return IsNotice+P.Nick+" removes \""+P.Status+"\" status from "+P.Target;
      else
        return IsNotice+"Someone removes \""+P.Status+"\" status from "+P.Target;

    case "channel mode+":
      api.TabSetColor(C, IsNewData);
      return IsNotice+P.Nick+" sets modes "+P.Mode;
    case "channel mode-":
      api.TabSetColor(C, IsNewData);
      return IsNotice+P.Nick+" removes modes "+P.Mode;

    case "conversation+": // for Pesterchum mostly
      return IsNotice+P.Nick+" starts discussion with "+P.Target;
    case "conversation-":
      return IsNotice+P.Nick+" ends discussion with "+P.Target;

    default:
      return EventReturn.NORMAL;
  }
}

function ShowEvent(Type, Params, Context) {
  Params = api.DecodeJSON(Params);
  local Message = EventString(Type, Params, Context);
  if(Message == EventReturn.NORMAL)
    return EventReturn.NORMAL;
  api.AddMessage(Message, Context, 0, 0);
  return EventReturn.HANDLED;
}

foreach(T in ["generic message", "notify*", "say fail *", "bookmark*", "watch*", "friends*", "ignore*", "nick*", "you *", "user *", "join *", "server *", "channel *"])
  api.AddEventHook(T, ShowEvent, Priority.LOWEST, 0, 0, 0);
