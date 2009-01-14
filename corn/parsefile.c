#include "gettext.h"
#include "config.h"

#include "playlist.h"
#include "parsefile.h"

#include <gio/gio.h>
#include <string.h>
#include <glib.h>
#include <stdlib.h>

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
static gchar * add_relative_dir(GFile * dir, const gchar * name)
{
    g_assert(dir != NULL);
    g_assert(name != NULL);

    if(name[0] == '/' || looks_like_uri(name))
        return g_strdup(name);

    // else, relative path

    GFile * absolute = g_file_get_child(dir, name);
    gchar * abs_uri = g_file_get_uri(absolute);
    g_object_unref(absolute);

    return abs_uri;
}

gint parse_m3u(GFile * m3u)
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
            playlist_append(add_relative_dir(dir, lines[i]));
        }
        g_object_unref(dir);
        g_strfreev(lines);
    }
    return 0;
}

gint parse_pls(GFile * pls)
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
                playlist_append(add_relative_dir(dir, file));
            g_free(file_key);
            g_free(file);
        }
        g_object_unref(dir);
    }

    g_key_file_free(keyfile);
    return 0;
}

static gint parse_dir_fail(GFile * dir, GError * error)
{
    gchar * uri = g_file_get_uri(dir);
    g_printerr("Can't list contents of directory %s (%s).\n", uri, error->message);
    g_free(uri);
    return 0;
}

gboolean parse_dir(GFile * dir)
{
    GError * error = NULL;
    GFileEnumerator * fenum = g_file_enumerate_children(dir, G_FILE_ATTRIBUTE_STANDARD_NAME,
            G_FILE_QUERY_INFO_NONE, NULL, &error);

    if(error)
        return parse_dir_fail(dir, error);

    GSList * entries = NULL;
    for(;;)
    {
        GFileInfo * info = g_file_enumerator_next_file(fenum, NULL, &error);
        if(error)
        {
            g_object_unref(fenum);
            return parse_dir_fail(dir, error);
        }

        if(!info)
            break;

        const gchar * name = g_file_info_get_attribute_string(info, G_FILE_ATTRIBUTE_STANDARD_NAME);
        g_object_unref(info);

        if(!name || name[0] == '.')
            continue;

        entries = g_slist_append(entries, add_relative_dir(dir, name));
    }

    g_file_enumerator_close(fenum, NULL, NULL);

    entries = g_slist_sort(entries, (GCompareFunc)g_ascii_strcasecmp);
    for(GSList * it = entries; it; it = g_slist_next(it))
        playlist_append(it->data);

    g_slist_free(entries);

    return PARSE_RESULT_WATCH_FILE;
}

gint parse_file(const gchar * path, gchar ** uri_out)
{
    g_return_val_if_fail(path != NULL, FALSE);

    *uri_out = NULL;

    static GRegex * familiar_extensions = NULL;
    if(!familiar_extensions)
        familiar_extensions = g_regex_new(
            ".\\.(mp3|ogg|flac|m4a|ape|mpc|wav|aiff|pcm|wma|ram)$",
            G_REGEX_CASELESS | G_REGEX_OPTIMIZE, 0, NULL);

    GFile * file = looks_like_uri(path)
        ? g_file_new_for_uri(path)
        : g_file_new_for_path(path);

    // looks like a boring media file with predictable file extension
    if(g_regex_match(familiar_extensions, path, 0, NULL))
    {
        *uri_out = g_file_get_uri(file);
        g_object_unref(file);
        return PARSE_RESULT_WATCH_PARENT;
    }

    GFileInfo * info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
            G_FILE_QUERY_INFO_NONE, NULL, NULL);
    if(info)
    {
        const gchar * mime_type = g_file_info_get_attribute_string(info,
                G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);

        if(mime_type)
        {
            g_debug(mime_type);
            gint ret = -1;
                 if(!strcmp("audio/x-mpegurl",     mime_type)) ret = parse_m3u(file);
            else if(!strcmp("x-directory/normal",  mime_type)) ret = parse_dir(file);
            else if(!strcmp("audio/x-scpls",       mime_type)) ret = parse_pls(file);
            else if(!strcmp("application/pls",     mime_type)) ret = parse_pls(file);
            else if(!strcmp("application/pls+xml", mime_type)) ret = parse_pls(file);
            g_object_unref(info);
            if(ret != -1)
            {
                g_object_unref(file);
                return ret;
            }
        }
    }

    *uri_out = g_file_get_uri(file);
    g_object_unref(file);
    return PARSE_RESULT_WATCH_PARENT;
}
