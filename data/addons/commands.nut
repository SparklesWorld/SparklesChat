AddonInfo <- {
  "Name":"Commands"
  "Summary":"Default command handler"
  "Version":"0.01"
  "Author":"NovaSquirrel"
};

local CmdAliases = {
  "my":"me 's &2",
  "me's":"me 's &2",
  "ame":"allchan me &2",
  "amsg":"allchan say &2",
  "anick":"allserv nick &2",
  "dialog":"query &2",
  "priv":"query &2",
  "setdescription":"topic", // close enough
  "getdescription":"topic",
  "j":"join &2",
  "raw":"quote &2",
  "leave":"part &2",
  "promote":"op &2",
  "demote":"deop &2",
  "logout":"quit"
};
function EchoCmd(T, P, C) {
  api.AddMessage(P, C, 0, 0);
  return EventReturn.HANDLED;
}
function CmdAliasCmd(T, P, C) {
  P = api.CmdParamSplit(P);
  switch((P[0][0]).tolower()) {
    case "add":
      CmdAliases[P[0][1]] <- P[1][2];
      break;
    case "del":
      delete CmdAliases[P[0][1]];
      break;
    case "save":
      break;
    case "list":
      local All = "";
      foreach(Cmd,Val in CmdAliases)
        All += Cmd+" ";
      api.TempMessage("All aliases: "+All, C);
      break;
  }
  return EventReturn.HANDLED;
}
function SaveAllCmd(T, P, C) {
  api.Event("saveall","","");
  api.TempMessage("Saved preferences",C);
  return EventReturn.HANDLED;
}
function CloseCmd(T, P, C) {
  api.TabRemove(C);
  return EventReturn.HANDLED;
}
function NativeCmd(T, P, C) {
  // allow /connect with just the network name
  if(T.tolower() == "connect" || T.tolower() == "sslconnect") {
     local NetworkList = api.GetConfigNames("Networks");
     foreach(Protocol in NetworkList) {
       local ServerList = api.GetConfigNames("Networks/"+Protocol);
       foreach(Server in ServerList)
         if(Server.tolower() == P.tolower()) {
           api.Command(T, Protocol+" "+Server, C);
           return EventReturn.HANDLED;
         }
     }
  }
  if(api.NativeCommand(T, P, C) == EventReturn.HANDLED)
    return EventReturn.HANDLED;
  else if(T in CmdAliases) {
    local I = api.CmdParamSplit(api.Interpolate(CmdAliases[T], T+" "+P, ["c"+api.TabGetInfo(C, "channel")]));
    api.Command(I[0][0], I[1][1], C);
    return EventReturn.HANDLED;
  } else {
    if(T.tolower() != "menu")
      print("Unrecognized command: "+T);
  }
  return EventReturn.NORMAL;
}

api.AddCommandHook("echo", EchoCmd, Priority.NORMAL, "Prints text to the window", "");
api.AddCommandHook("saveall", SaveAllCmd, Priority.NORMAL, "Save all preferences for addons", "");
api.AddCommandHook("cmdalias", CmdAliasCmd, Priority.NORMAL, "Manage aliases list", "");
api.AddCommandHook("close", CloseCmd, Priority.LOWEST, null, null);
api.AddCommandHook("*", NativeCmd, Priority.LOWEST, null, null);
