#ifndef __parsefile_h
#define __parsefile_h

#include <glib.h>
#include <gio/gio.h>

#define PARSE_RESULT_WATCH_FILE   (1<<0)
#define PARSE_RESULT_WATCH_PARENT (1<<1)

gint parse_file(const gchar * path, gchar ** uri_out);
gint parse_m3u(GFile * m3u);

#endif
