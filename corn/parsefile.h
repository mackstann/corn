#ifndef __parsefile_h
#define __parsefile_h

#include <glib.h>
#include <gio/gio.h>

#define PARSE_RESULT_FILE      (1<<0)
#define PARSE_RESULT_DIRECTORY (1<<1)
#define PARSE_RESULT_PLAYLIST  (1<<2)

typedef struct
{
    gchar * uri;
    guint flags;
} FoundFile;

void parse_file(const gchar * path);
void parse_m3u(GFile * m3u);

extern GQueue found_files;

#endif
