#include "gettext.h"
#include "config.h"

#include "playlist.h"
#include "parsefile.h"
#include "sniff-file.h"

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

static gchar * add_relative_dir(GFile * dir, const gchar * name, gboolean force)
{
    g_assert(dir != NULL);
    g_assert(name != NULL);

    if(!force && (name[0] == '/' || sniff_looks_like_uri(name)))
        return g_strdup(name);

    // else, relative path

    GFile * absolute = g_file_get_child(dir, name);
    gchar * abs_uri = g_file_get_uri(absolute);
    g_object_unref(absolute);

    return abs_uri;
}

static void parse_m3u(GFile * m3u)
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

static void parse_pls(GFile * pls)
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

void parse_file(const gchar * path)
{
    g_return_if_fail(path != NULL);

    GFile * file = sniff_looks_like_uri(path)
        ? g_file_new_for_uri(path)
        : g_file_new_for_path(path);

    FoundFile * ff = sniff_file(path, file);

    if(ff->type & SNIFFED_DIRECTORY)
        parse_dir(file);
    else if(ff->type & SNIFFED_M3U)
        parse_m3u(file);
    else if(ff->type & SNIFFED_PLS)
        parse_pls(file);

    g_object_unref(file);
    g_queue_push_tail(&found_files, ff);
}
