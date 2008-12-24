#include "gettext.h"

#include "config.h"

#include "playlist.h"

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <string.h>
#include <glib.h>
#include <stdlib.h>

static gchar ** read_file(const gchar * path)
{
    gint size;
    gchar * buf, ** lines;

    if(gnome_vfs_read_entire_file(path, &size, &buf)) // nul-terminated only since 2.10
        return NULL;

        buf = g_realloc(buf, size + 1);
            buf[size] = '\0';


    g_strdelimit(buf, "\r", '\n'); // \r is used on some platforms
    lines = g_strsplit(buf, "\n", 0);
    g_free(buf);
    return lines;
}

static GRegex * absolute_path_or_uri_prefix = NULL;

static GnomeVFSURI * add_relative_dir(GnomeVFSURI * dir_uri, const gchar * name)
{
    // WRONG
    //
    // check if name is local or a uri.  then escape or not.
    //
    // or find a better way?
    if(!absolute_path_or_uri_prefix)
        absolute_path_or_uri_prefix = g_regex_new("^(/|[a-z0-9+.-]+://)",
            G_REGEX_CASELESS | G_REGEX_OPTIMIZE, 0, NULL);

    gchar * escaped_name = gnome_vfs_get_uri_from_local_path(name);
    GnomeVFSURI * uri;
    if(g_regex_match(absolute_path_or_uri_prefix, escaped_name, 0, NULL))
        uri = gnome_vfs_uri_new(escaped_name);
    else
        uri = gnome_vfs_uri_append_file_name(dir_uri, escaped_name);
    g_free(escaped_name);
    return uri;
}

gboolean parse_m3u(GnomeVFSURI * uri)
{
    g_message("parsing m3u: %s", uri->text);

    gchar ** lines;
    if((lines = read_file(uri->text)))
    {
        gchar * dir = gnome_vfs_uri_extract_dirname(uri);
        GnomeVFSURI * dir_uri = gnome_vfs_uri_new(dir);
        g_free(dir);
        gint i;
        for(i = 0; lines[i]; ++i)
        {
            lines[i] = g_strstrip(lines[i]);
            if(lines[i][0] == '\0' || lines[i][0] == '#')
                continue;
            GnomeVFSURI * absolute = add_relative_dir(dir_uri, lines[i]);
            playlist_append(absolute->text);
            gnome_vfs_uri_unref(absolute);
        }
        g_message("%d lines", i);
        gnome_vfs_uri_unref(dir_uri);
        g_strfreev(lines);
    }
    return FALSE;
}

gboolean parse_pls(GnomeVFSURI * uri)
{
    g_message("parsing pls: %s", uri->text);

    gint length;
    gchar * buf;

    GKeyFile * keyfile = g_key_file_new();

    if(gnome_vfs_read_entire_file(uri->text, &length, &buf) ||
       !g_key_file_load_from_data(keyfile, buf, length, G_KEY_FILE_NONE, NULL) ||
       !g_key_file_has_group(keyfile, "playlist") ||
       !g_key_file_has_key(keyfile, "playlist", "NumberOfEntries", NULL))
    {
        g_warning("Invalid .pls file: %s", uri->text);
    }
    else
    {
        gint entries = g_key_file_get_integer(keyfile, "playlist", "NumberOfEntries", NULL);
        for(gint i = 1; i <= entries; ++i)
        {
            gchar * file_key = g_strdup_printf("File%d", i);
            gchar * file = g_key_file_get_value(keyfile, "playlist", file_key, NULL);
            if(file)
            {
                GnomeVFSURI * file_uri = gnome_vfs_uri_new(file);
                if(file_uri)
                {
                    playlist_append(file_uri->text);
                    gnome_vfs_uri_unref(file_uri);
                }
                else
                    g_warning("%s %s %s .pls %s %s", _("Could not parse URI"),
                            file, _("in"), _("file"), uri->text);
            }
            g_free(file_key);
        }
    }

    g_key_file_free(keyfile);
    return FALSE;
}

gboolean parse_dir(GnomeVFSURI * uri)
{
    GnomeVFSDirectoryHandle * dirh;
    GnomeVFSFileInfo * info;
    GSList * entries = NULL, * it;

    if(gnome_vfs_directory_open_from_uri(&dirh, uri, GNOME_VFS_FILE_INFO_FOLLOW_LINKS))
        return FALSE;

    info = gnome_vfs_file_info_new();

    while(gnome_vfs_directory_read_next(dirh, info) == GNOME_VFS_OK)
    {
        if(info->name[0] == '.')
            continue;
        GnomeVFSURI * absolute = add_relative_dir(uri, info->name);
        entries = g_slist_append(entries, g_strdup(absolute->text));
        gnome_vfs_uri_unref(absolute);
    }

    entries = g_slist_sort(entries, (GCompareFunc) strcmp);
    for(it = entries; it; it = g_slist_next(it))
    {
        g_message("recursive directory add: %s", (gchar *)it->data);
        playlist_append(it->data);
    }
    g_slist_free(entries);

    gnome_vfs_directory_close(dirh);
    gnome_vfs_file_info_unref(info);

    return FALSE;
}

gboolean parse_file(const gchar * path)
{
    g_return_val_if_fail(path != NULL, FALSE);

    static GRegex * familiar_extensions = NULL;
    if(!familiar_extensions)
        familiar_extensions = g_regex_new(
            ".\\.(mp3|ogg|flac|m4a|ape|mpc|wav|aiff|pcm|wma|ram)$",
            G_REGEX_CASELESS | G_REGEX_OPTIMIZE, 0, NULL);

    // looks like a boring media file with predictable file extension
    if(g_regex_match(familiar_extensions, path, 0, NULL))
        return TRUE;

    gchar * uri_text = gnome_vfs_get_uri_from_local_path(path);
    if(!uri_text)
    {
        g_warning("%s: %s", _("Unknown error when creating URI for path"), path);
        return FALSE;
    }
    g_message("%s", path);

    GnomeVFSURI * uri = gnome_vfs_uri_new(uri_text);
    if(!uri)
    {
        g_free(uri_text);
        g_warning("%s: %s", _("Unknown error when creating URI for path"), path);
        return FALSE;
    }

    GnomeVFSFileInfo * info = gnome_vfs_file_info_new();

    gboolean addme = TRUE;

    if(!gnome_vfs_get_file_info_uri(uri, info,
                                GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
                                GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE |
                                GNOME_VFS_FILE_INFO_FOLLOW_LINKS))
    {
             if(!strcmp("audio/x-mpegurl",        info->mime_type)) addme = parse_m3u(uri);
        else if(!strcmp("x-directory/normal",     info->mime_type)) addme = parse_dir(uri);
        else if(!strcmp("audio/x-scpls",          info->mime_type)) addme = parse_pls(uri);
        else if(!strcmp("application/pls",        info->mime_type)) addme = parse_pls(uri);
        else if(!strcmp("application/pls+xml",    info->mime_type)) addme = parse_pls(uri);
    }

    gnome_vfs_file_info_unref(info);
    gnome_vfs_uri_unref(uri);
    g_free(uri_text);
    return addme;
}
