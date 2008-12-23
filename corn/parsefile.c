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

    if(gnome_vfs_read_entire_file(path, &size, &buf))
        return NULL;

    buf = g_realloc(buf, size + 1);
    buf[size] = '\0';

    g_strdelimit(buf, "\r", '\n'); // \r is used on some platforms
    lines = g_strsplit(buf, "\n", 0);
    g_free(buf);
    return lines;
}

static gchar * add_relative_dir(const gchar * name, const gchar * dir)
{
    gchar * slashdir, * ret;

    if(name[0] == G_DIR_SEPARATOR)
        return g_strdup(name);

    // make sure the dir ends in a separator
    slashdir = g_strconcat(dir, (dir[strlen(dir) - 1] == G_DIR_SEPARATOR ?
                                 "" : G_DIR_SEPARATOR_S), NULL);
    ret = gnome_vfs_uri_make_full_from_relative(slashdir, name);
    g_free(slashdir);

    return ret;
}

gboolean parse_m3u(const gchar * path)
{
    g_message("parsing m3u: %s", path);

    gchar ** lines;
    if((lines = read_file(path)))
    {
        gchar * dir = g_path_get_dirname(path);
        for(gint i = 0; lines[i]; ++i)
        {
            lines[i] = g_strstrip(lines[i]);
            if(lines[i][0] == '\0' || lines[i][0] == '#')
                continue;
            playlist_append(g_list_append(NULL, add_relative_dir(lines[i], dir)));
        }
        g_free(dir);
        g_strfreev(lines);
    }
    return FALSE;
}

gboolean parse_pls(const gchar * path)
{
    g_message("parsing pls: %s", path);

    gint length;
    gchar * buf;

    GKeyFile * keyfile = g_key_file_new();

    if(gnome_vfs_read_entire_file(path, &length, &buf) ||
       !g_key_file_load_from_data(keyfile, buf, length, G_KEY_FILE_NONE, NULL) ||
       !g_key_file_has_group(keyfile, "playlist") ||
       !g_key_file_has_key(keyfile, "playlist", "NumberOfEntries", NULL))
    {
        g_warning("Invalid .pls file: %s", path);
    }
    else
    {
        gint entries = g_key_file_get_integer(keyfile, "playlist", "NumberOfEntries", NULL);
        for(gint i = 1; i <= entries; ++i)
        {
            gchar * file_key = g_strdup_printf("File%d", i);
            gchar * file = g_key_file_get_value(keyfile, "playlist", file_key, NULL);
            if(file)
                playlist_append(g_list_append(NULL, file));
            g_free(file_key);
        }
    }

    g_key_file_free(keyfile);
    return FALSE;
}

gboolean parse_dir(const gchar * path)
{
    GnomeVFSDirectoryHandle * dirh;
    GnomeVFSFileInfo * info;
    GSList * entries = NULL, * it;

    if(gnome_vfs_directory_open
       (&dirh, path, GNOME_VFS_FILE_INFO_FOLLOW_LINKS))
        return FALSE;

    info = gnome_vfs_file_info_new();

    while(gnome_vfs_directory_read_next(dirh, info) == GNOME_VFS_OK)
    {
        if(info->name[0] == '.')
            continue;
        gchar * fullpath = add_relative_dir(info->name, path);
        entries = g_slist_append(entries, fullpath);
    }

    entries = g_slist_sort(entries, (GCompareFunc) strcmp);
    for(it = entries; it; it = g_slist_next(it))
    {
        g_message("recursive directory add: %s", (gchar *)it->data);
        playlist_append(g_list_append(NULL, it->data));
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

    if(g_str_has_prefix(path, "file://"))
        path += 7;

    // looks like a boring media file with predictable file extension
    if(g_regex_match(familiar_extensions, path, 0, NULL))
        return TRUE;

    GnomeVFSFileInfo * info = gnome_vfs_file_info_new();

    gboolean addme = TRUE;

    if(!gnome_vfs_get_file_info(path, info,
                                GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
                                GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE |
                                GNOME_VFS_FILE_INFO_FOLLOW_LINKS))
    {
             if(!strcmp("audio/x-mpegurl",        info->mime_type)) addme = parse_m3u(path);
        else if(!strcmp("x-directory/normal",     info->mime_type)) addme = parse_dir(path);
        else if(!strcmp("audio/x-scpls",          info->mime_type)) addme = parse_pls(path);
        else if(!strcmp("application/pls",        info->mime_type)) addme = parse_pls(path);
        else if(!strcmp("application/pls+xml",    info->mime_type)) addme = parse_pls(path);
    }

    gnome_vfs_file_info_unref(info);
    return addme;
}
