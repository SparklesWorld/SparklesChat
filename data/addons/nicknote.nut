AddonInfo <- {
  "Name":"NickNote"
  "Summary":"Associate notes with names"
  "Version":"0.01"
  "Author":"NovaSquirrel"
};

NickNoteTable <- api.LoadTable("nicknote");

function NoteGetCmd(T, P, C) {
  P=P.tolower();
  if(P in NickNoteTable)
    api.TempMessage("Note for \""+P+"\":"+NickNoteTable[P],C);
  else
    api.TempMessage("No note found for \""+P+"\"",C);
  return EventReturn.HANDLED;
}
function NoteSetCmd(T, P, C) {
  local Split = api.CmdParamSplit(P);
  if(Split[0].len() < 2) return EventReturn.BADSYNTAX;
  local Nick = Split[0][0].tolower();
  local Note = Split[1][1];
  NickNoteTable[Nick] <- Note;
  api.TempMessage("Note for \""+Nick+"\" set",C);
  return EventReturn.HANDLED;
}
function NoteDelCmd(T, P, C) {
  P=P.tolower();
  if(P in NickNoteTable) {
    delete NickNoteTable[P];
    api.TempMessage("Note for \""+P+"\" deleted",C);
  } else
    api.TempMessage("No note found for \""+P+"\"",C);
  return EventReturn.HANDLED;
}
function SaveConfig(T, P, C) {
  api.SaveTable("nicknote", NickNoteTable);
  return EventReturn.NORMAL;
}

api.AddCommandHook("noteget",    NoteGetCmd, Priority.NORMAL, "Get a note associated with a name", "Name");
api.AddCommandHook("noteset",    NoteSetCmd, Priority.NORMAL, "Set a note associated with a name", "Name Note");
api.AddCommandHook("notedel",    NoteDelCmd, Priority.NORMAL, "Delete a note associated with a name", "Name");
api.AddEventHook("saveall",      SaveConfig, Priority.NORMAL, 0, 0, 0);
