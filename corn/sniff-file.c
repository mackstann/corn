#include "gettext.h"
#include "config.h"

#include "sniff-file.h"

#include <gio/gio.h>
#include <string.h>
#include <glib.h>
#include <stdlib.h>

gboolean sniff_looks_like_uri(const gchar * path)
{
    static GRegex * uri_pattern = NULL;
    if(!uri_pattern)
        uri_pattern = g_regex_new("^[a-z0-9+.-]+:",
            G_REGEX_CASELESS | G_REGEX_OPTIMIZE, 0, NULL);
    return g_regex_match(uri_pattern, path, 0, NULL);
}

static FoundFile * found_file_new(gchar * uri, gint type)
{
    FoundFile * ff = g_new(FoundFile, 1);
    ff->uri = uri;
    ff->type = type;
    return ff;
}

static FoundFile * _sniff_fallback_dumb_and_slow(const gchar * path, GFile * file)
{
    GFileInfo * info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_TYPE,
            G_FILE_QUERY_INFO_NONE, NULL, NULL);
    if(info)
    {
        guint type = g_file_info_get_attribute_uint32(info,
                G_FILE_ATTRIBUTE_STANDARD_TYPE);
        g_object_unref(info);
        if(type == G_FILE_TYPE_DIRECTORY)
            return found_file_new(g_file_get_uri(file), SNIFFED_DIRECTORY);
    }

    // i guess it's just some file.  we'll find out later when we try to play it.
    return found_file_new(g_file_get_uri(file), SNIFFED_FILE);
}

FoundFile * sniff_file(const gchar * path, GFile * file)
{
    gsize pathlen = strlen(path);

    if(pathlen < 5)
    {
        // screw it, can't guess based on name
        return _sniff_fallback_dumb_and_slow(path, file);
    }

    // to match these we want at least one character, followed by a dot,
    // followed by the file extension
    if((pathlen >= 5 &&
        (!g_ascii_strcasecmp(path+pathlen-4, ".mp3") ||
         !g_ascii_strcasecmp(path+pathlen-4, ".ogg") ||
         !g_ascii_strcasecmp(path+pathlen-4, ".m4a") ||
         !g_ascii_strcasecmp(path+pathlen-4, ".ape") ||
         !g_ascii_strcasecmp(path+pathlen-4, ".mpc") ||
         !g_ascii_strcasecmp(path+pathlen-4, ".wav") ||
         !g_ascii_strcasecmp(path+pathlen-4, ".pcm") ||
         !g_ascii_strcasecmp(path+pathlen-4, ".wma") ||
         !g_ascii_strcasecmp(path+pathlen-4, ".ram")))
       ||
       (pathlen >= 6 &&
        (!g_ascii_strcasecmp(path+pathlen-5, ".flac") ||
         !g_ascii_strcasecmp(path+pathlen-5, ".aiff"))))
    {
        // looks like a boring media file with predictable file extension
        return found_file_new(g_file_get_uri(file), SNIFFED_FILE);
    }

    // maybe a playlist?

    if(!g_ascii_strcasecmp(path+pathlen-4, ".m3u"))
        return found_file_new(NULL, SNIFFED_M3U);

    if(!g_ascii_strcasecmp(path+pathlen-4, ".pls"))
        return found_file_new(NULL, SNIFFED_PLS);

    return _sniff_fallback_dumb_and_slow(path, file);
}
