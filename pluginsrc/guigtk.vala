/*
 * SparklesChat
 *
 * Copyright (C) 2014-2015 NovaSquirrel
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
 */

using Gtk;
using Pango;

enum TabTSCols { // column IDs for the tab list tree store
  NAME,
  COLOR,
  STRIKE,
  POINTER,
  TAB_ID
}

public class ChatWindow : Window {
  public string[] tabColorList = {"", "#0000ff", "#ff0000", "#00ff00"};

  private TextView textView;
  private TextView statusView;
  private TextView profileView;
  private Entry inputEntry;
  private Label charCount;
  public TreeView tabList;
  private TreeView userList;
  private ScrolledWindow logScroll;
  public TreeStore tabsStore;
  public ListStore userStore;
  public void *curTab = null; // currently focused tab
  public void *oldTab = null;

  public TreeIter server_iter;
  public TreeIter channel_iter;

  public TextView tabInfoView;
  public TextView notificationsView;
  public TextView notifyVerboseView;

  public void infoAdded(void *Tab) {
    GetTabMessages(curTab, 1);
  }

  public bool findTabById(int id1, int id2, out TreeIter tab) {
    TreeIter iter;
    tabsStore.get_iter_first(out iter);
    while(true) {
      GLib.Value IdValue;
      tabsStore.get_value(iter, TabTSCols.TAB_ID, out IdValue);
      if(id1 == IdValue.get_int()) {
        if(id2 == -1) {
          tab = iter;
          return true;
        } else {
          TreeIter iter2;
          if(!tabsStore.iter_children(out iter2, iter))
            return false;
          while(true) {
            tabsStore.get_value(iter2, TabTSCols.TAB_ID, out IdValue);
            if(id2 == IdValue.get_int()) {
              tab = iter;
              return true;
            }
            if(!tabsStore.iter_next(ref iter2))
              return false;
          }
        }
      }
      if(!tabsStore.iter_next(ref iter))
        return false;
    }
  }

  public void dirtyTabsAttr(int id1, int id2, int flags) {
    TreeIter iter;
    if(mainWindow.findTabById(id1, id2, out iter))
      tabsStore.set(iter, TabTSCols.COLOR, tabColorList[flags & 3], TabTSCols.STRIKE, (flags & TabFlags.NOTINCHAN)!=0, -1);
  }

  public void dirtyTabsList() {
    tabsStore = new TreeStore(5, typeof(string), typeof(string), typeof(bool), typeof(void*), typeof(int));
    tabList.set_model(tabsStore);
    ReloadTabsList();
    tabList.expand_all(); // to do: keep collapsed stuff collapsed maybe?
  }

  public void addLine(string str) {
    bool needScroll = (logScroll.vadjustment.upper - logScroll.get_allocated_height()) == logScroll.vadjustment.value;

    AddLineWithFormatting(str);
    TextIter endIter;
    textView.buffer.get_end_iter(out endIter);
    TextMark scrollMark = textView.buffer.create_mark(null, endIter, false);
    textView.buffer.insert(endIter, "\n", -1);

    if(needScroll) { // scroll to the bottom if needed
      textView.buffer.get_end_iter(out endIter); // get iter at the end
      textView.buffer.move_mark(scrollMark, endIter);
      textView.scroll_mark_onscreen(scrollMark);
    }
    textView.buffer.delete_mark(scrollMark);
  }

  public void addLinePart(string text, string fg, string bg, int flags) {
    bool bold =      (flags&1)!=0;
    bool italic =    (flags&2)!=0;
    bool underline = (flags&4)!=0;
    bool sup =       (flags&8)!=0;
    bool sub =       (flags&16)!=0;
    bool strike =    (flags&32)!=0;
    bool hidden =    (flags&64)!=0;
    TextIter endIter;
    textView.buffer.get_end_iter(out endIter);
    TextMark leftMark = textView.buffer.create_mark(null, endIter, true);
    textView.buffer.insert(endIter, text, -1);

    string anonymous = null;
    TextTag tag = textView.buffer.create_tag(anonymous, "background-full-height", true, "background-full-height_set", true, null);
    tag.foreground = fg;
    tag.background = bg;
    tag.rise = sup?2:0;
    tag.scale = (sup|sub)?0.75:1;
    tag.weight = bold?Pango.Weight.BOLD:Pango.Weight.NORMAL;
    tag.strikethrough = strike;
    tag.style = italic?Pango.Style.ITALIC:Pango.Style.NORMAL;
    tag.underline = underline?Pango.Underline.SINGLE:Pango.Underline.NONE;
    tag.invisible = hidden;

    tag.weight_set = true;
    if(tag.foreground != "")
      tag.foreground_set = true;
    if(tag.background != "")
      tag.background_set = true;
    if(tag.strikethrough)
      tag.strikethrough_set = true;
    if(tag.style != Pango.Style.NORMAL)
      tag.style_set = true;
    if(tag.invisible)
      tag.invisible_set = true;
    if(tag.underline != Pango.Underline.NONE)
      tag.underline_set = true;
    if(tag.rise != 0)
      tag.rise_set = true;
    if(tag.scale != 1)
      tag.scale_set = true;

    TextIter startIter;
    textView.buffer.get_iter_at_mark(out startIter, leftMark);
    textView.buffer.get_end_iter(out endIter);

    textView.buffer.apply_tag(tag, startIter, endIter);
    textView.buffer.delete_mark(leftMark);
  }

  public ChatWindow() {
      this.title = "SparklesChat";
      this.window_position = WindowPosition.CENTER;
      try {
        this.icon = new Gdk.Pixbuf.from_file("data/icon.png");
      } catch(Error e) {
      }
      set_default_size(640, 480);

      // make the menu on the top (TO DO: read menu from the file!)
      MenuBar menuBar = new Gtk.MenuBar();
      MenuItem menuItemMain = new Gtk.MenuItem.with_label("SparklesChat");
      menuBar.add(menuItemMain);
      Menu menuMain = new Gtk.Menu();
      menuItemMain.set_submenu(menuMain);
      MenuItem itemConnect = new Gtk.MenuItem.with_label("Connect");
      itemConnect.activate.connect(() => {
        print("Not done yet");
      });
      menuMain.add(itemConnect);
      MenuItem itemExit = new Gtk.MenuItem.with_label("Exit");
      itemExit.activate.connect(Gtk.main_quit);
      menuMain.add(itemExit);

      // make the text view
      textView = new TextView();
      textView.editable = false;
      textView.cursor_visible = false;
      textView.override_font(FontDescription.from_string("consolas,lucida console,monospace 9"));
      textView.set_wrap_mode(Gtk.WrapMode.WORD);

      // with scrolling
      logScroll = new ScrolledWindow(null, null);
      logScroll.set_policy(PolicyType.AUTOMATIC, PolicyType.ALWAYS);
      logScroll.add(textView);

      inputEntry = new Entry();
      inputEntry.set_width_chars(1);

      charCount = new Label("0");

      inputEntry.activate.connect (() => {
        unowned string str = inputEntry.get_text();
        if(str == "") return;
        DoInputCommand(str, curTab);
        inputEntry.set_text("");
      });

      inputEntry.key_release_event.connect ((event) => {
        charCount.set_text(inputEntry.get_text().length.to_string());
        return false;
      });

      tabList = new TreeView();
      tabList.enable_search = false;
      tabList.key_release_event.connect ((event) => {
        charCount.set_text(inputEntry.get_text().length.to_string());
        return false;
      });

      var tempStore = new TreeStore(1, typeof(string));
      tabList.set_model(tempStore);
      tabList.get_selection().changed.connect (() => {
        TreeModel model;
        TreeIter iter;
        if(!tabList.get_selection().get_selected(out model, out iter))
          return;
        GLib.Value PointerValue;
        tabsStore.get_value(iter, TabTSCols.POINTER, out PointerValue);
        oldTab = curTab;
        curTab = PointerValue.get_pointer();
        QueueFocusTabEvent(oldTab, curTab);
        textView.buffer.text = "";
        tabInfoView.buffer.text = "";
        GetTabMessages(curTab, 0);

        TextIter endIter;
        textView.buffer.get_end_iter(out endIter);
        TextMark scrollMark = textView.buffer.create_mark(null, endIter, false);
        textView.scroll_mark_onscreen(scrollMark);
        textView.buffer.delete_mark(scrollMark);
      });

      var tabCell = new CellRendererText();
      tabCell.set("foreground_set", true);
      tabCell.set("strikethrough_set", true);
      tabList.insert_column_with_attributes(-1, "Name", tabCell, "text", 0, "foreground", 1, "strikethrough", 2);
      tabList.headers_visible = false;
      tabList.enable_tree_lines = true;
      var tabScroll = new ScrolledWindow(null, null);
      tabScroll.set_policy(PolicyType.AUTOMATIC, PolicyType.AUTOMATIC);
      tabScroll.add(tabList);

      // make user list
      userList = new TreeView();
      var listmodel = new ListStore (1, typeof (string));
      userList.set_model(listmodel);
      userList.headers_visible = false;
      userList.fixed_height_mode = true;
      userList.insert_column_with_attributes(-1, "Users", new CellRendererText(), "text", 0);
//      TreeIter userIter;
//      listmodel.append(out userIter);
//      listmodel.set(userIter, 0, "NovaSquirrel");
      var userScroll = new ScrolledWindow(null, null);
      userScroll.set_policy(PolicyType.AUTOMATIC, PolicyType.AUTOMATIC);
      userScroll.add(userList);

      // make boxes that will be filled; pack_start args are expand, fill
      var mainVbox = new Box(Orientation.VERTICAL, 0);
      var chatHbox = new Box(Orientation.HORIZONTAL, 0);
      var middleVbox = new Box(Orientation.VERTICAL, 0);
      var hPane1 = new Paned(Orientation.HORIZONTAL);
      var middleVpane = new Paned(Orientation.VERTICAL);
      var userPane1 = new Paned(Orientation.VERTICAL);
      var userPane2 = new Paned(Orientation.VERTICAL);
      var tabListVbox = new Box(Orientation.VERTICAL, 0);
      var mainHpane = new Paned(Orientation.HORIZONTAL);

      mainVbox.pack_start(menuBar, false, true, 0);
      mainVbox.pack_start(mainHpane, true, true, 0);

      var notebook = new Notebook();
      {
        tabInfoView = new TextView();
        tabInfoView.editable = false;
        tabInfoView.cursor_visible = false;
        tabInfoView.set_wrap_mode(Gtk.WrapMode.WORD);
        var scroll = new ScrolledWindow(null, null);
        scroll.set_policy(PolicyType.NEVER, PolicyType.AUTOMATIC);
        scroll.add(tabInfoView);
        notebook.append_page(scroll, new Label("Tab info"));

        notificationsView = new TextView();
        notificationsView.editable = false;
        notificationsView.cursor_visible = false;
        notificationsView.set_wrap_mode(Gtk.WrapMode.WORD);
        scroll = new ScrolledWindow(null, null);
        scroll.set_policy(PolicyType.NEVER, PolicyType.AUTOMATIC);
        scroll.add(notificationsView);
        notebook.append_page(scroll, new Label("Notifications"));

        notifyVerboseView = new TextView();
        notifyVerboseView.editable = false;
        notifyVerboseView.cursor_visible = false;
        notifyVerboseView.set_wrap_mode(Gtk.WrapMode.WORD);
        scroll = new ScrolledWindow(null, null);
        scroll.set_policy(PolicyType.NEVER, PolicyType.AUTOMATIC);
        scroll.add(notifyVerboseView);
        notebook.append_page(scroll, new Label("Notify (Verbose)"));
        notebook.set_scrollable(true);
      }

      middleVpane.pack1(notebook, false, true);
      middleVpane.pack2(middleVbox, true, true);

      hPane1.pack1(middleVpane, true, false);
      hPane1.pack2(userPane1, false, true);

      {
        statusView = new TextView();
        statusView.editable = false;
        statusView.cursor_visible = false;
        statusView.set_wrap_mode(Gtk.WrapMode.WORD);
        var scroll = new ScrolledWindow(null, null);
        scroll.set_policy(PolicyType.AUTOMATIC, PolicyType.AUTOMATIC);
        scroll.add(statusView);
        userPane1.pack1(scroll, false, true);

        userPane1.pack2(userPane2, true, true);
        userPane2.pack1(userScroll, true, true);

        profileView = new TextView();
        profileView.editable = false;
        profileView.cursor_visible = false;
        profileView.set_wrap_mode(Gtk.WrapMode.WORD);
        scroll = new ScrolledWindow(null, null);
        scroll.set_policy(PolicyType.AUTOMATIC, PolicyType.AUTOMATIC);
        scroll.add(profileView);
        userPane2.pack2(scroll, false, true);
      }

      tabListVbox.pack_start(tabScroll, true, true);

      mainHpane.pack1(tabListVbox, false, true);
      mainHpane.pack2(hPane1, true, true);

      mainVbox.set_size_request(250, -1);
      userList.set_size_request(100, -1);
      userPane1.set_size_request(100, -1);

      chatHbox.pack_start(inputEntry, true, true, 0);
      chatHbox.pack_start(charCount, false, false, 0);

      middleVbox.pack_start(logScroll, true, true, 0);
      middleVbox.pack_start(chatHbox, false, false, 0);
      add(mainVbox);
  }
}

public ChatWindow mainWindow;
extern void PollForMainThreadMessagesAndDelay();
extern void ReloadTabsList();
//extern void PrintDebug(string text);
extern void QueueFocusTabEvent(void *OldTab2, void *NewTab2);
extern void DoInputCommand(string Command, void *Tab);
extern void GetTabMessages(void *Tab2, int UpdateOnly);
extern void AddLineWithFormatting(string text);

[Flags]
enum TabFlags{
  COLOR1,
  COLOR2,
  CHANNEL,   /* is a channel */
  QUERY,     /* is a query */
  SERVER,    /* is a server */
  NOTINCHAN, /* user isn't in the channel, whether through being kicked or having parted */
  NOLOGGING, /* don't log this tab */
  HIDDEN,    /* hidden, detached tab */
  IS_TYPING, /* user is typing */ 
  HAS_TYPED  /* has stuff typed */
}

public void FocusATab(int id1, int id2) {
  TreeIter iter;
  if(mainWindow.findTabById(id1, id2, out iter))
    mainWindow.tabList.get_selection().select_iter(iter);
}

public void AddTabToList(int level, string name, int flags, void *tab, int id) {
  if(level == 0) {
    mainWindow.tabsStore.append(out mainWindow.server_iter, null);
    mainWindow.tabsStore.set(mainWindow.server_iter, TabTSCols.NAME, name, TabTSCols.COLOR, mainWindow.tabColorList[flags & 3], TabTSCols.STRIKE, (flags & TabFlags.NOTINCHAN)!=0, TabTSCols.POINTER, tab, TabTSCols.TAB_ID, id, -1);
  } else {
    mainWindow.tabsStore.append(out mainWindow.channel_iter, mainWindow.server_iter);
    mainWindow.tabsStore.set(mainWindow.channel_iter, TabTSCols.NAME, name,  TabTSCols.COLOR, mainWindow.tabColorList[flags & 3], TabTSCols.STRIKE, (flags & TabFlags.NOTINCHAN)!=0, TabTSCols.POINTER, tab, TabTSCols.TAB_ID, id, -1);
  }
}

public void Sparkles_StartGUI() {
  string[] args2 = {""};
  unowned string[] args = args2;
  Gtk.init(ref args);

  bool Quit = false;
  mainWindow = new ChatWindow();
  mainWindow.destroy.connect(() => {
    Quit = true;
  });
  mainWindow.show_all();

  //taking the place of Gtk.main() because we need SDL too
  while(!Quit) {
    while(Gtk.events_pending())
      Gtk.main_iteration();
    PollForMainThreadMessagesAndDelay();
  }
}

public void DirtyTabsList() {
  mainWindow.dirtyTabsList();
}

public void DirtyTabsAttr(int id1, int id2, int flags) {
  mainWindow.dirtyTabsAttr(id1, id2, flags);
}

public void InfoAdded(void *Context) {
  mainWindow.infoAdded(Context);
}

public void GotTabMessage(string message, void *Tab) {
  mainWindow.addLine(message);
}

public void AddFormattedText(string text, string fg, string bg, int flags) {
  mainWindow.addLinePart(text, fg, bg, flags);
}
