localedir=$(datadir)/locale

AM_CPPFLAGS=$(GLIB_CFLAGS) $(GTHREAD_CFLAGS) $(GIO_CFLAGS) $(SQLITE_CFLAGS) \
    $(DBUS_GLIB_CFLAGS) $(XINE_CFLAGS) \
    $(UNIQUE_CFLAGS) -DG_DISABLE_DEPRECATED -DLOCALEDIR=\"$(localedir)\" \
    -DSQLITE_THREADSAFE=0

bin_PROGRAMS = corn

corn_LDADD = $(GLIB_LIBS) $(GTHREAD_LIBS) $(DBUS_GLIB_LIBS) \
    $(XINE_LIBS) $(UNIQUE_LIBS) $(GIO_LIBS) $(SQLITE_LIBS)

corn_SOURCES = \
  gettext.h \
  main.h \
  main.c \
  dbus.h \
  dbus.c \
  db.h \
  db.c \
  music.h \
  music.c \
  music-control.h \
  music-control.c \
  music-metadata.h \
  music-metadata.c \
  playlist.h \
  playlist.c \
  playlist-random.h \
  playlist-random.c \
  parsefile.h \
  parsefile.c \
  sniff-file.h \
  sniff-file.c \
  state.h \
  state.c \
  state-settings.h \
  state-settings.c \
  state-playlist.h \
  state-playlist.c \
  sockqueue.h \
  sockqueue.c \
  watch.h \
  watch.c \
  cpris-root.h \
  cpris-root.c \
  cpris-root-glue.h \
  mpris-root.h \
  mpris-root.c \
  mpris-root-glue.h \
  mpris-player.h \
  mpris-player.c \
  mpris-player-glue.h \
  mpris-tracklist.h \
  mpris-tracklist.c \
  mpris-tracklist-glue.h

EXTRA_DIST = corn.schemas cpris-root.xml mpris-root.xml mpris-player.xml mpris-tracklist.xml

BUILT_SOURCES = cpris-root-glue.h mpris-root-glue.h mpris-player-glue.h mpris-tracklist-glue.h

cpris-root-glue.h: cpris-root.xml
	$(LIBTOOL) --mode=execute dbus-binding-tool --prefix=cpris_root --mode=glib-server --output=cpris-root-glue.h $(srcdir)/cpris-root.xml

mpris-root-glue.h: mpris-root.xml
	$(LIBTOOL) --mode=execute dbus-binding-tool --prefix=mpris_root --mode=glib-server --output=mpris-root-glue.h $(srcdir)/mpris-root.xml

mpris-player-glue.h: mpris-player.xml
	$(LIBTOOL) --mode=execute dbus-binding-tool --prefix=mpris_player --mode=glib-server --output=mpris-player-glue.h $(srcdir)/mpris-player.xml

mpris-tracklist-glue.h: mpris-tracklist.xml
	$(LIBTOOL) --mode=execute dbus-binding-tool --prefix=mpris_tracklist --mode=glib-server --output=mpris-tracklist-glue.h $(srcdir)/mpris-tracklist.xml

