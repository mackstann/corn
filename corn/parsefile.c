#include "gettext.h"
#include "config.h"

#include "playlist.h"
#include "parsefile.h"

#include <gio/gio.h>
#include <string.h>
#include <glib.h>
#include <stdlib.h>

GQueue found_files = G_QUEUE_INIT;

static gchar ** read_file(GFile * file)
{
    gchar * buf;
    if(!g_file_load_contents(file, NULL, &buf, NULL, NULL, NULL))
        return NULL;

    g_strdelimit(buf, "\r", '\n'); // \r is used on some platforms
    gchar ** lines = g_strsplit(buf, "\n", 0);
    g_free(buf);
    return lines;
}

static gboolean looks_like_uri(const gchar * path)
{
    static GRegex * uri_pattern = NULL;
    if(!uri_pattern)
        uri_pattern = g_regex_new("^[a-z0-9+.-]+:",
            G_REGEX_CASELESS | G_REGEX_OPTIMIZE, 0, NULL);
    return g_regex_match(uri_pattern, path, 0, NULL);
}

// if #name is a relative path, then make it an absolute path by prepending
// #dir to it.  if absolute or a uri, return unmodified.  return value may be a
// uri or a unix path, but is always newly allocated and owned by the caller.
static gchar * add_relative_dir(GFile * dir, const gchar * name, gboolean force)
{
    g_assert(dir != NULL);
    g_assert(name != NULL);

    if(!force && (name[0] == '/' || looks_like_uri(name)))
        return g_strdup(name);

    // else, relative path

    GFile * absolute = g_file_get_child(dir, name);
    gchar * abs_uri = g_file_get_uri(absolute);
    g_object_unref(absolute);

    return abs_uri;
}

void parse_m3u(GFile * m3u)
{
    gchar ** lines;
    if((lines = read_file(m3u)))
    {
        GFile * dir = g_file_get_parent(m3u);
        gint i;
        for(i = 0; lines[i]; ++i)
        {
            lines[i] = g_strstrip(lines[i]);
            if(lines[i][0] == '\0' || lines[i][0] == '#')
                continue;
            parse_file(add_relative_dir(dir, lines[i], FALSE));
        }
        g_object_unref(dir);
        g_strfreev(lines);
    }
}

void parse_pls(GFile * pls)
{
    GKeyFile * keyfile = g_key_file_new();

    gchar * buf;
    gsize length;
    if(!g_file_load_contents(pls, NULL, &buf, &length, NULL, NULL) ||
       !g_key_file_load_from_data(keyfile, buf, length, G_KEY_FILE_NONE, NULL) ||
       !g_key_file_has_group(keyfile, "playlist") ||
       !g_key_file_has_key(keyfile, "playlist", "NumberOfEntries", NULL))
    {
        gchar * uri = g_file_get_uri(pls);
        g_warning("Invalid .pls file: %s", uri);
        g_free(uri);
    }
    else
    {
        gint entries = g_key_file_get_integer(keyfile, "playlist", "NumberOfEntries", NULL);
        GFile * dir = g_file_get_parent(pls);
        for(gint i = 1; i <= entries; ++i)
        {
            gchar * file_key = g_strdup_printf("File%d", i);
            gchar * file = g_key_file_get_value(keyfile, "playlist", file_key, NULL);
            if(file && file[0] != '\0')
                parse_file(add_relative_dir(dir, file, FALSE));
            g_free(file_key);
            g_free(file);
        }
        g_object_unref(dir);
    }

    g_key_file_free(keyfile);
}

static void parse_dir_fail(GFile * dir, GError * error)
{
    gchar * uri = g_file_get_uri(dir);
    g_warning("Can't list contents of directory %s (%s).", uri, error->message);
    g_error_free(error);
    g_free(uri);
}

void parse_dir(GFile * dir)
{
    GError * error = NULL;
    GFileEnumerator * fenum = g_file_enumerate_children(dir, G_FILE_ATTRIBUTE_STANDARD_NAME,
            G_FILE_QUERY_INFO_NONE, NULL, &error);

    if(error)
    {
        parse_dir_fail(dir, error);
        return;
    }

    GSList * entries = NULL;
    for(;;)
    {
        GFileInfo * info = g_file_enumerator_next_file(fenum, NULL, &error);
        if(error)
        {
            g_object_unref(fenum);
            parse_dir_fail(dir, error);
            return;
        }

        if(!info)
            break;

        const gchar * name = g_file_info_get_attribute_byte_string(info, G_FILE_ATTRIBUTE_STANDARD_NAME);

        if(name && name[0] != '.')
            entries = g_slist_append(entries, add_relative_dir(dir, name, TRUE));

        g_object_unref(info);
    }

    g_object_unref(fenum);

    entries = g_slist_sort(entries, (GCompareFunc)g_ascii_strcasecmp);
    for(GSList * it = entries; it; it = g_slist_next(it))
        parse_file(it->data);

    g_slist_free(entries);
}

static FoundFile * found_file_new(gchar * uri, gint flags)
{
    FoundFile * ff = g_new(FoundFile, 1);
    ff->uri = uri;
    ff->flags = flags;
    return ff;
}

static FoundFile * _parse_file_fallback_dumb_and_slow(const gchar * path, GFile * file)
{
    GFileInfo * info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_TYPE,
            G_FILE_QUERY_INFO_NONE, NULL, NULL);
    if(info)
    {
        guint type = g_file_info_get_attribute_uint32(info,
                G_FILE_ATTRIBUTE_STANDARD_TYPE);
        g_object_unref(info);
        if(type == G_FILE_TYPE_DIRECTORY)
        {
            parse_dir(file);
            return found_file_new(g_file_get_uri(file), PARSE_RESULT_DIRECTORY);
        }
    }

    // i guess it's just some file.  we'll find out later when we try to play it.
    return found_file_new(g_file_get_uri(file), PARSE_RESULT_FILE);
}

static FoundFile * _parse_file(const gchar * path, GFile * file)
{
    gsize pathlen = strlen(path);

    if(pathlen < 5)
    {
        // screw it, can't guess based on name
        return _parse_file_fallback_dumb_and_slow(path, file);
    }

    // to match these we want at least one character, followed by a dot,
    // followed by the file extension
    if((pathlen >= 5 &&
        (g_str_has_suffix(path+pathlen-4, ".mp3") ||
         g_str_has_suffix(path+pathlen-4, ".ogg") ||
         g_str_has_suffix(path+pathlen-4, ".m4a") ||
         g_str_has_suffix(path+pathlen-4, ".ape") ||
         g_str_has_suffix(path+pathlen-4, ".mpc") ||
         g_str_has_suffix(path+pathlen-4, ".wav") ||
         g_str_has_suffix(path+pathlen-4, ".pcm") ||
         g_str_has_suffix(path+pathlen-4, ".wma") ||
         g_str_has_suffix(path+pathlen-4, ".ram")))
       ||
       (pathlen >= 6 &&
        (g_str_has_suffix(path+pathlen-5, ".flac") ||
         g_str_has_suffix(path+pathlen-5, ".aiff"))))
    {
        // looks like a boring media file with predictable file extension
        return found_file_new(g_file_get_uri(file), PARSE_RESULT_FILE);
    }

    // maybe a playlist?

    if(g_str_has_suffix(path+pathlen-4, ".m3u"))
    {
        parse_m3u(file);
        return found_file_new(NULL, PARSE_RESULT_PLAYLIST);
    }

    if(g_str_has_suffix(path+pathlen-4, ".pls"))
    {
        parse_pls(file);
        return found_file_new(NULL, PARSE_RESULT_PLAYLIST);
    }

    return _parse_file_fallback_dumb_and_slow(path, file);
}

void parse_file(const gchar * path)
{
    g_return_if_fail(path != NULL);

    GFile * file = looks_like_uri(path)
        ? g_file_new_for_uri(path)
        : g_file_new_for_path(path);

    FoundFile * ff = _parse_file(path, file);

    g_object_unref(file);
    g_queue_push_tail(&found_files, ff);
}
