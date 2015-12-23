objlist := chat draw gui tui cJSON text squirrel eventcmd fnmatch sock plugin ipc websocket util
dllobjlist := libtest
program_title = chat

CC := gcc
LD := g++

objdir := obj
srcdir := src
objlisto := $(foreach o,$(objlist),$(objdir)/$(o).o)

dlldir := plugin
dllsrcdir := pluginsrc
dllobjlisto := $(foreach o,$(dllobjlist),$(dlldir)/$(o).o)
dllobjlistdll := $(foreach dll,$(dllobjlist),$(dlldir)/$(dll).dll)

ifdef LINUX
  SQUIRREL=../SQUIRREL3
  WSLAY=../wslay
  CFLAGS := -Wall -O2 -std=gnu99 `sdl2-config --cflags` -I$(SQUIRREL)/include -I$(WSLAY)/lib/includes
  LDLIBS := -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lssl -lcrypto -lsquirrel -lsqstdlib -lncurses -lwslay -lcurl -L. -L$(SQUIRREL)/lib -L$(WSLAY)/lib 
  LDFLAGS := -Wl
else
  CFLAGS := -Wall -O2 -std=gnu99
  LDLIBS := -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lssl -lcrypto -lSDL2_ttf -lsquirrel -lsqstdlib -lpdcurses -lgdi32 -lcomdlg32 -lcurldll -lws2_32 -lwslay
  LDFLAGS := -Wl,-subsystem,windows
endif

chat: $(objlisto)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(objdir)/%.o: $(srcdir)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

ifdef LINUX
#  guisdl.so:
#  guicurses.so:
  guigtk.so: pluginsrc/guigtk.vala pluginsrc/guigtk.c
	valac --pkg=gtk+-3.0 --library=guigtk pluginsrc/guigtk.vala pluginsrc/guigtk.c -X -fPIC -X -shared -o guigtk.so
	rm guigtk.vapi
else
#$(dlldir)/%.o: $(dllsrcdir)/%.c
#	$(CC) $(CFLAGS) -c -o $@ $< -std=gnu99
#$(dlldir)/%.dll: $(dlldir)/%.o
#	$(CC) -o $@ -s -shared $< -Wl,--subsystem,windows
  libtest.dll: pluginsrc/libtest.c
	gcc -c -o obj/libtest.o pluginsrc/libtest.c -std=gnu99
	g++ -o data/libraries/libtest.dll -s -shared obj/libtest.o -Wl,--subsystem,windows

#  guisdl.dll:
#  guicurses.dll:
  guigtk.dll: pluginsrc/guigtk.vala pluginsrc/guigtk.c
	valac --pkg=gtk+-3.0 --library=guigtk pluginsrc/guigtk.vala pluginsrc/guigtk.c -X -shared -o guigtk.dll
	rm guigtk.vapi
endif

.PHONY: clean

clean:
	-rm $(objdir)/*.o
