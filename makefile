objlist := chat draw gui tui cJSON text squirrel eventcmd fnmatch sock plugin ipc websocket
dllobjlist := libvisual
program_title = chat

CC := gcc
LD := g++
CFLAGS := -Wall -O2 -std=gnu99

ifdef _linux
# todo: fix?
  LDLIBS := -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lssl -lcrypto -lsquirrel -lsqstdlib -lpdcurses
  LDFLAGS := -Wl,-subsystem,windows
else
  LDLIBS := -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lssl -lcrypto -lSDL2_ttf -lsquirrel -lsqstdlib -lpdcurses -lgdi32 -lcomdlg32 -lcurldll -lws2_32 -lwslay
  LDFLAGS := -Wl,-subsystem,windows
endif

objdir := obj
srcdir := src
objlisto := $(foreach o,$(objlist),$(objdir)/$(o).o)

dlldir := plugin
dllsrcdir := pluginsrc
dllobjlisto := $(foreach o,$(dllobjlist),$(dlldir)/$(o).o)
dllobjlistdll := $(foreach dll,$(dllobjlist),$(dlldir)/$(dll).dll)

.PHONY: clean

chat: $(objlisto) $(dllobjlistdll) $(objlistopp)                        src/spark.res
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(objdir)/%.o: $(srcdir)/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(dlldir)/%.o: $(dllsrcdir)/%.c
	$(CC) $(CFLAGS) -DWIN32 -c $< -o $@
$(dlldir)/%.dll: $(dlldir)/%.o
	dllwrap --def pluginsrc/plugin.def --dllname $@ $<

clean:
	-rm $(objdir)/*.o
