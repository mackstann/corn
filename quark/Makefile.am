localedir=$(datadir)/locale

AM_CPPFLAGS=$(GLIB_CFLAGS) $(GTHREAD_CFLAGS) $(JSON_CFLAGS) \
    $(DBUS_GLIB_CFLAGS) $(GNOMEVFS_CFLAGS) $(GCONF_CFLAGS) $(XINE_CFLAGS) \
    -std=gnu99 \
    -DG_DISABLE_DEPRECATED -DGCONF_DISABLE_DEPRECATED \
    -DLOCALEDIR=\"$(localedir)\"

bin_PROGRAMS = quark

quark_LDADD = $(GLIB_LIBS) $(GTHREAD_LIBS) $(JSON_LIBS) $(DBUS_GLIB_CFLAGS) \
    $(GNOMEVFS_LIBS) $(GCONF_LIBS) $(XINE_LIBS)

quark_SOURCES = \
  gettext.h \
  main.h \
  main.c \
  fifo.h \
  fifo.c \
  music.h \
  music.c \
  playlist.h \
  playlist.c \
  parsefile.h \
  parsefile.c \
  dbus.h \
  dbus.c \
  mpris-root.h \
  mpris-root.c \
  mpris-root-glue.h \
  mpris-player.h \
  mpris-player.c \
  mpris-player-glue.h

EXTRA_DIST = quark.schemas mpris-root.xml mpris-player.xml

BUILT_SOURCES = mpris-root-glue.h mpris-player-glue.h

mpris-root-glue.h: mpris-root.xml
	$(LIBTOOL) --mode=execute dbus-binding-tool --prefix=mpris_root --mode=glib-server --output=mpris-root-glue.h $(srcdir)/mpris-root.xml

mpris-player-glue.h: mpris-player.xml
	$(LIBTOOL) --mode=execute dbus-binding-tool --prefix=mpris_player --mode=glib-server --output=mpris-player-glue.h $(srcdir)/mpris-player.xml

if USE_GCONF
install-data-local: quark.schemas
if GCONF_SCHEMAS_INSTALL
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	gconftool-2 --makefile-install-rule $<
endif
	$(INSTALL) -d $(DESTDIR)$(GCONF_SCHEMA_FILE_DIR)
	$(INSTALL) $< $(DESTDIR)$(GCONF_SCHEMA_FILE_DIR)

uninstall-local:
	$(RM) $(DESTDIR)$(GCONF_SCHEMA_FILE_DIR)/quark.schemas
endif
