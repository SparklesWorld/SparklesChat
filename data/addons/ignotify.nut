AddonInfo <- {
  "Name":"IgnoreNotify"
  "Summary":"Ignore and notify features"
  "Version":"0.01"
  "Author":"NovaSquirrel"
};

IgnoreTable <- api.LoadTable("ignore");
NotifyTable <- api.LoadTable("notify");

TriggerTypes <- {
  "chan": 1,       // user sent a channel message
  "priv": 2,       // user sent a private message
  "notice": 4,     // user sent a /notice
  "ctcp": 8,       // user used CTCP
  "filesend": 16,  // user attempted to send a file
  "invite": 32,    // user invited you to a channel
  "ad": 64,        // user posted an ad
  "color": 128,    // if ignored, will strip color from the user's posts
  "status": 256,   // user changed their status
  "join": 512,     // user joined a channel
  "part": 1024,    // user disconnected or parted
  "context":2048,  // context it has to occur in
  "custom": 4096,  // custom Squirrel code to run that must be true
  "action": 8192   // what action to take instead of the default, will define later
};
MultiTypes <- {
  "channel": 1|64,
  "private": 2|4|8|16|32,
  "joinpart": 512|1024,
  "all": 1|2|4|8|16|32|64
};

function FilterMessage(T, P, C) {
  P = api.DecodeJSON(P);
  local TFlags = api.TabGetFlags(C);

  foreach(Table in [IgnoreTable,NotifyTable]) {
    function Match(Entry) {
      if("context" in Entry && !api.WildMatch(C, Entry.context))
        return EventReturn.NORMAL;
      if("custom" in Entry && !api.RunString("return function(T, P, C){"+Entry.custom+"}")(T, P, C))
        return EventReturn.NORMAL;
      if(Table == IgnoreTable)
        return EventReturn.DELETE;
      return EventReturn.NORMAL;
    }

    switch(T) {
      case "user message":
      case "user message hilight":
      case "user action":
      case "user action hilight":
        foreach(Mask,MaskEntry in Table)
          if(api.WildMatch(P.Nick, Mask) || ("FullHost" in P && api.WildMatch(P.FullHost, Mask)))
            if(((TFlags & TABFLAGS.CHANNEL) && (MaskEntry.f & 1) && (!("chan" in MaskEntry)||api.WildMatch(P.Text, MaskEntry.chan)))
            || ((TFlags & TABFLAGS.QUERY) && (MaskEntry.f & 2) && (!("priv" in MaskEntry)||api.WildMatch(P.Text, MaskEntry.priv))))
              if(Match(MaskEntry) == EventReturn.DELETE)
                return EventReturn.DELETE;
        break;
      case "notify status":
        break;
    }
  }
  return EventReturn.NORMAL;
}

function ManageCmd(T, P, C) {
  local Split = api.CmdParamSplit(P);
  local FlagTable = (T=="ignore"?IgnoreTable:NotifyTable);

  function LineOfIgnore(Mask, MaskTable) {
    local ThisLine = "|"+api.ForceMinStrLen(Mask,30) + "|";
    foreach(Type,Value in TriggerTypes) {
      if(MaskTable.f & Value) {
        ThisLine += Type;
        if(Type in MaskTable)
          ThisLine += "["+MaskTable[Type]+"]"
        ThisLine += ", ";
      }
    }
    return ThisLine;
  }

  if(P.tolower()=="help") {
    api.TempMessage("Syntax: Name +type -type",C);
    api.TempMessage("chan (channel) priv (private)",C);
    api.TempMessage("notice, ctcp, filesend, invite",C);
    api.TempMessage("ad, color, status, join, part, all",C);
    return EventReturn.HANDLED;
  } else if(P.tolower()=="clear") {
    FlagTable.clear();
    api.TempMessage("List cleared",C);
  } else if(P.tolower()=="list") {
    api.TempMessage(".-"+T+" list------------------.", C);
    foreach(Mask,MaskTable in FlagTable) {
      api.TempMessage(LineOfIgnore(Mask, MaskTable), C);
    }
    api.TempMessage("'------------------------------'", C);
  }
  if(Split[0].len() < 2) return EventReturn.BADSYNTAX;

  local NumOptions = Split[0].len()-1;
  local Mask = Split[0][0];
  if(!(Mask in FlagTable))
    FlagTable[Mask] <- {"f":0};
  local MaskEntry = FlagTable[Mask];

  for(local i=1;i<=NumOptions;i++) {
    local Option = Split[0][i];
    local Action = Option.slice(1).tolower();
    if(Option[0] == '+') { // add option
      if(Action in TriggerTypes) {
        MaskEntry.f = MaskEntry.f | TriggerTypes[Action];
        if(i!=NumOptions && (Split[0][i+1][0]!='+') && (Split[0][i+1][0]!='-'))
          MaskEntry[Action] <- Split[0][i+1];
      } else if(Action in MultiTypes) { // multitype shortcuts don't support meta data
        MaskEntry.f = MaskEntry.f | MultiTypes[Action];        
      }
      api.TempMessage(".--added-----------------------.", C);
      api.TempMessage(LineOfIgnore(Mask, MaskEntry), C);
    } else if(Option[0] == '-') { // remove option
      local Flags = 0;
      if(Action in TriggerTypes) MaskEntry.f = MaskEntry.f & ~TriggerTypes[Action];
      if(Action in MultiTypes) MaskEntry.f = MaskEntry.f & ~MultiTypes[Action];
      api.TempMessage(".--removed---------------------.", C);
      api.TempMessage(LineOfIgnore(Mask, MaskEntry), C);

      // clean up any meta data no longer needed
      foreach(Type,Value in TriggerTypes)
        if(!(MaskEntry.f & Value) && (Type in MaskEntry))
          delete MaskEntry[Type];
      if(!MaskEntry.f)
        delete FlagTable[Mask];
    }
  }

  return EventReturn.HANDLED;
}

function SaveConfig(T, P, C) {
  api.SaveTable("ignore", IgnoreTable);
  api.SaveTable("notify", NotifyTable);
  return EventReturn.NORMAL;
}

foreach(T in ["user *", "channel *", "notify *"])
  api.AddEventHook(T, FilterMessage, Priority.HIGHER, 0, 0, 0);

api.AddCommandHook("ignore", ManageCmd, Priority.NORMAL, "Manage ignore list", "Name +option -option");
api.AddCommandHook("notify", ManageCmd, Priority.NORMAL, "Manage notify list", "Name +option -option");
api.AddEventHook("saveall",  SaveConfig, Priority.NORMAL, 0, 0, 0);
