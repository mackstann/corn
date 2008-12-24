#ifndef __parsefile_h
#define __parsefile_h

#include <libgnomevfs/gnome-vfs.h>

#include <glib.h>

gboolean parse_file(const gchar * path);
gboolean parse_m3u(GnomeVFSURI * uri);

#endif
