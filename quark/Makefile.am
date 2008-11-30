localedir=$(datadir)/locale

AM_CPPFLAGS=$(GLIB_CFLAGS) $(GTHREAD_CFLAGS) $(GNOMEVFS_CFLAGS) $(GCONF_CFLAGS) $(XINE_CFLAGS) \
  -DG_DISABLE_DEPRECATED -DGCONF_DISABLE_DEPRECATED \
  -DLOCALEDIR=\"$(localedir)\"

bin_PROGRAMS = quark

quark_LDADD = $(GLIB_LIBS) $(GTHREAD_LIBS) $(GNOMEVFS_LIBS) $(GCONF_LIBS) $(XINE_LIBS)
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
  parsefile.c

EXTRA_DIST = \
  quark.schemas

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
