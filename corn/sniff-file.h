#ifndef __sniff_file_h__
#define __sniff_file_h__

#include <glib.h>
#include <gio/gio.h>

#define SNIFFED_FILE       (1<<0)
#define SNIFFED_DIRECTORY  (1<<1)
#define SNIFFED_PLAYLIST   (1<<2)
#define SNIFFED_M3U       ((1<<3)|SNIFFED_PLAYLIST)
#define SNIFFED_PLS       ((1<<4)|SNIFFED_PLAYLIST)

typedef struct
{
    gchar * uri;
    guint type;
} FoundFile;

FoundFile * sniff_file(const gchar * path, GFile * file);
gboolean sniff_looks_like_uri(const gchar * path);

#endif
