#ifndef __parsefile_h
#define __parsefile_h

#include <libgnomevfs/gnome-vfs.h>

#include <glib.h>

#define PARSE_RESULT_WATCH_FILE   (1<<0)
#define PARSE_RESULT_WATCH_PARENT (1<<1)
#define PARSE_RESULT_IMPORT_FILE  (1<<2)

gint parse_file(const gchar * path);
gboolean parse_m3u(GnomeVFSURI * uri);

#endif
