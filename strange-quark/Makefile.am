applicationsdir = $(datadir)/applications
pixmapdir = $(datadir)/pixmaps
localedir=$(datadir)/locale

AM_CPPFLAGS = \
  $(GTK_CFLAGS) \
  $(GCONF_CFLAGS) \
  $(GDK_PIXBUF_CFLAGS) \
  -DGTK_DISABLE_DEPRECATED \
  -DG_DISABLE_DEPRECATED \
  -DG_LOG_DOMAIN=\"strange-quark\" \
  -DPIXMAPDIR=\"$(pixmapdir)\" \
  -DLOCALEDIR=\"$(localedir)\"

bin_PROGRAMS = strange-quark

strange_quark_LDADD = $(GTK_LIBS) $(GCONF_LIBS)
strange_quark_SOURCES = \
  gettext.h \
  main.c \
  applet.h \
  applet.c \
  trayicon.h \
  trayicon.c \
  quarkclient.h \
  quarkclient.c \
  editor.h \
  editor.c \
  marshalers.h \
  marshalers.c

marshalers: marshalers.list
	glib-genmarshal --header --prefix=quark_cclosure_marshal $^ > marshalers.h
	glib-genmarshal --body --prefix=quark_cclosure_marshal $^ > marshalers.c

applications_DATA = \
  quark.desktop

EXTRA_DIST = \
  quark.desktop \
  strange-quark.schemas

install-data-local: strange-quark.schemas
if GCONF_SCHEMAS_INSTALL
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	gconftool-2 --makefile-install-rule $<
endif
	$(INSTALL) -d $(DESTDIR)$(GCONF_SCHEMA_FILE_DIR)
	$(INSTALL) $< $(DESTDIR)$(GCONF_SCHEMA_FILE_DIR)

uninstall-local:
	$(RM) $(DESTDIR)$(GCONF_SCHEMA_FILE_DIR)/strange-quark.schemas
