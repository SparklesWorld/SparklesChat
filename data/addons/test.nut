AddonInfo <- {
  "Name":"Test"
  "Summary":"Test Squirrel code"
  "Version":"0.01"
  "Author":"NovaSquirrel"
};

function SqRunCmd(T, P, C) {
  compilestring(P)();
  return EventReturn.HANDLED;
}
function SqPrintCmd(T, P, C) {
  print(compilestring("return "+P)());
  return EventReturn.HANDLED;
}

api.AddCommandHook("sqrun",    SqRunCmd, Priority.NORMAL, null, null);
api.AddCommandHook("sqprint",    SqPrintCmd, Priority.NORMAL, null, null);
