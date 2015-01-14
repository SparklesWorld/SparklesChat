AddonInfo <- {
  "Name":"Startup"
  "Summary":"Code to run upon starting the client"
  "Version":"0.01"
  "Author":"NovaSquirrel"
};

function DisplayStartup() {
  api.TabCreate("Startup", "", TabFlags.NOLOGGING);

  local Quotes = ["Squirrel powered!", "Cross platform!", "Modular design!", "Huge backlogs!", "Powerful scripting system!", "Flexible notification system!", "Supports XChat/HexChat plugins!", "The client for nerds!", "The client for power users!"];
  if(api.GetConfigStr("Client/GUI", "") != "Text") {
    // the coolest banner
    api.AddMessage("\x000309▄▀▀▄ █▀▀▄ ▄▀▀▄ █▀▀▄ █  █ █    █▀▀▀ ▄▀▀▄", "Startup", 0, 0);
    api.AddMessage("\x000309▀▄   █▄▄▀ █▄▄█ █▄▄▀ █▄▀  █    █▄▄▄ ▀▄  ", "Startup", 0, 0);
    api.AddMessage("\x000309  ▀▄ █    █  █ █ ▀▄ █ ▀▄ █    █      ▀▄", "Startup", 0, 0);
    api.AddMessage("\x000309▀▄▄▀ █    █  █ █  █ █  █ █▄▄▄ █▄▄▄ ▀▄▄▀ "+api.GetInfo("ClientVersion"), "Startup", 0, 0);
    api.AddMessage("\x000302"+Quotes[rand()%Quotes.len()], "Startup", 0, 0);
  } else {
    api.AddMessage("\x000309\x0002Sparkles\x0002 "+api.GetInfo("ClientVersion"), "Startup", 0, 0);
    api.AddMessage("\x000302\x001d"+Quotes[rand()%Quotes.len()], "Startup", 0, 0);
  }
  api.AddMessage("", "Startup", 0, 0);

  local NetworkList = api.GetConfigNames("Networks");
  foreach(Protocol in NetworkList) {
    local ServerList = api.GetConfigNames("Networks/"+Protocol);
    foreach(Server in ServerList) {
      if(Server.tolower() == "default") continue;
      local Search = "Networks/"+Protocol+"/"+Server+"/";

      api.AddMessage(Protocol+"\t\x0002"+format("%-16s",Server)+"\x000f - "+api.GetConfigStr(Search+"Host", "no host"),"Startup", 0, 0);
      local ActionRow = "Connect: ";
      if(api.GetConfigInt(Search+"Secure", 0))
        ActionRow += api.Button("SSL/TLS", "sslconnect "+Server)+", ";
      ActionRow += api.Button("Plain", "connect "+Server)+", ";

      ActionRow += "More: ";
      ActionRow += api.Button("Edit", "startup edit "+Server) + ", "
      ActionRow += api.Button("Clone", "startup clone "+Server) + ", "
      ActionRow += api.Button("Delete", "startup delete "+Server) + ", "
      api.AddMessage(ActionRow,"Startup", 0, 0);

      api.AddMessage("","Startup", 0, 0);
    }
  }
}
DisplayStartup();
api.Command("gui", "focus", "Startup");
