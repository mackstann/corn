#ifndef __parsefile_h
#define __parsefile_h

#include <glib.h>

gboolean parse_file (const gchar *path);
gboolean parse_m3u (const gchar *path);

#endif
