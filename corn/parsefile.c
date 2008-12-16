#include "config.h"

#include "playlist.h"

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <string.h>
#include <glib.h>
#include <stdlib.h>

/* this function stolen from gxine.. thanks guys ;) */
static gint read_ini_line_int(gchar ** lines, const gchar * key)
{
    gint retval = -1;
    gint i;

    g_return_val_if_fail(lines != NULL && key != NULL, -1);

    for(i = 0; (lines[i] != NULL && retval == -1); i++)
    {
        if(strncasecmp(lines[i], key, strlen(key)) == 0)
        {
            gchar ** bits;

            bits = g_strsplit(lines[i], "=", 2);
            if(bits[0] == NULL || bits[1] == NULL)
            {
                g_strfreev(bits);
                return -1;
            }

            retval = atoi(bits[1]);
            g_strfreev(bits);
        }
    }

    return retval;
}

/* this function stolen from gxine.. thanks guys ;) */
static gchar * read_ini_line_string(gchar ** lines, const gchar * key)
{
    gchar * retval = NULL;
    int i;

    g_return_val_if_fail(lines != NULL && key != NULL, NULL);

    for(i = 0; (lines[i] != NULL && retval == NULL); i++)
    {
        if(strncasecmp(lines[i], key, strlen(key)) == 0)
        {
            gchar ** bits;

            bits = g_strsplit(lines[i], "=", 2);
            if(bits[0] == NULL || bits[1] == NULL)
            {
                g_strfreev(bits);
                return NULL;
            }

            retval = g_strdup(bits[1]);

            g_strfreev(bits);
        }
    }

    return retval;
}


static gchar ** read_file(const gchar * path)
{
    guint size;
    gchar * buf, ** lines;

    if(gnome_vfs_read_entire_file(path, &size, &buf))
        return NULL;

    /* null terminate that shit */
    buf = g_realloc(buf, size + 1);
    buf[size] = '\0';

    g_strdelimit(buf, "\r", '\n');      /* \r is used on some platforms */
    lines = g_strsplit(buf, "\n", 0);
    g_free(buf);
    return lines;
}

static gchar * add_relative_dir(const gchar * name, const gchar * dir)
{
    gchar * slashdir, * ret;

    if(name[0] == G_DIR_SEPARATOR)
        return g_strdup(name);

    /* make sure the dir ends in a separator */
    slashdir = g_strconcat(dir, (dir[strlen(dir) - 1] == G_DIR_SEPARATOR ?
                                 "" : G_DIR_SEPARATOR_S), NULL);
    ret = gnome_vfs_uri_make_full_from_relative(slashdir, name);
    g_free(slashdir);

    return ret;
}

gboolean parse_ram(const gchar * path)
{
    gchar ** lines;
    guint i;
    GnomeVFSHandle * h;
    char buf[4];
    GnomeVFSFileSize read, left;

    g_message("parsing ram: %s", path);

    if(gnome_vfs_open(&h, path, GNOME_VFS_OPEN_READ))
        return FALSE;

    left = 4;
    while(left &&
          gnome_vfs_read(h, buf + (4 - left), left, &read) == GNOME_VFS_OK)
        left -= read;

    gnome_vfs_close(h);

    if(left)
        return FALSE;

    /* from gxine:
       if this is true, then its an actual stream, not a playlist file */
    if(buf[0] == '.' && buf[1] == 'R' && buf[2] == 'M' && buf[3] == 'F')
        return TRUE;

    if((lines = read_file(path)))
    {
        GList * alternatives = NULL;

        for(i = 0; lines[i]; ++i)
        {
            lines[i] = g_strstrip(lines[i]);
            if(strlen(lines[i]))
            {

                /* comment */
                if(lines[i][0] == '#')
                    continue;

                /* --stop-- lines */
                if(strstr(lines[i], "--stop--"))
                    break;

                /* from gxine:
                   Either it's a rtsp, or a pnm mrl, but we also match http
                   mrls here.
                 */
                if(g_str_has_prefix(lines[i], "rtsp://") ||
                   g_str_has_prefix(lines[i], "pnm://") ||
                   g_str_has_prefix(lines[i], "http://"))
                {
                    alternatives = g_list_append(alternatives, g_strdup(lines[i]));
                }
            }
        }

        if(alternatives)
            playlist_append(path, alternatives);

        g_strfreev(lines);
    }
    return FALSE;
}

gboolean parse_m3u(const gchar * path)
{
    gchar ** lines;
    guint i;
    gchar * dir = g_path_get_dirname(path);

    g_message("parsing m3u: %s", path);

    if((lines = read_file(path)))
    {
        for(i = 0; lines[i]; ++i)
        {
            lines[i] = g_strstrip(lines[i]);
            if(strlen(lines[i]) && lines[i][0] != '#')
            {
                if(lines[i][0] == '/')  // absolute
                    playlist_append(lines[i], NULL);
                else            // relative
                {
                    gchar * fullpath = add_relative_dir(lines[i], dir);
                    playlist_append(fullpath, NULL);
                    g_free(fullpath);
                }
            }
        }
        g_strfreev(lines);
    }
    g_free(dir);
    return FALSE;
}

/* this function heavily based on code from gxine */
gboolean parse_pls(const gchar * path)
{
    int entries, i;
    gchar ** lines;

    g_message("parsing pls: %s", path);
    if((lines = read_file(path)))
    {
        if(strncasecmp("[playlist]", lines[0], strlen("[playlist]")) != 0)
        {
            g_warning("Invalid .pls file '%s'", path);
            goto pls_fail;
        }
        if((entries = read_ini_line_int(lines, "NumberOfEntries")) < 0)
        {
            g_warning("Invalid .pls file '%s'", path);
            goto pls_fail;
        }

        GList * alternatives = NULL;

        for(i = 1; i <= entries; ++i)
        {
            char * file /*, * title */ ;
            char * file_key /*, * title_key */ ;

            file_key = g_strdup_printf("file%d", i);
            /* title_key = g_strdup_printf ("title%d", i); */

            file = read_ini_line_string(lines, file_key);
            /* title = read_ini_line_string (lines, title_key); */

            g_free(file_key);
            /* g_free (title_key); */

            if(file != NULL)
                alternatives = g_list_append(alternatives, g_strstrip(file));
            else
                g_free(file);
            /* g_free (title); */
        }

        if(alternatives)
            playlist_append(path, alternatives);
    }
  pls_fail:
    if(lines)
        g_strfreev(lines);
    return FALSE;
}

gboolean parse_dir(const gchar * path)
{
    GnomeVFSDirectoryHandle * dirh;
    GnomeVFSFileInfo * info;
    GSList * entries = NULL, * it;

    g_message("adding dir: %s", path);

    if(gnome_vfs_directory_open
       (&dirh, path, GNOME_VFS_FILE_INFO_FOLLOW_LINKS))
        return FALSE;

    g_message("really adding dir: %s", path);

    info = gnome_vfs_file_info_new();

    while(gnome_vfs_directory_read_next(dirh, info) == GNOME_VFS_OK
          && info->name[0] != '.')
    {
        g_message("read dir entry: %s", info->name);
        gchar * fullpath = add_relative_dir(info->name, path);
        entries = g_slist_append(entries, fullpath);
    }

    entries = g_slist_sort(entries, (GCompareFunc) strcmp);
    for(it = entries; it; it = g_slist_next(it))
    {
        playlist_append(it->data, NULL);
        g_free(it->data);
    }
    g_slist_free(entries);

    gnome_vfs_directory_close(dirh);
    gnome_vfs_file_info_unref(info);

    return FALSE;
}

gboolean parse_file(const gchar * path)
{
    static GRegex * ext_re = NULL;
    if(!ext_re)
        ext_re = g_regex_new(
            ".\\.(mp3|ogg|flac|m4a|ape|mpc|wav|aiff|pcm|wma|ram)$",
            G_REGEX_CASELESS | G_REGEX_OPTIMIZE, 0, NULL);

    gint len = strlen(path);

    if(!strncmp(path, "file://", MIN(len, 7)))
    {
        path += 7;
        len -= 7;
    }

    if(g_file_test(path, G_FILE_TEST_IS_REGULAR) &&
       g_regex_match(ext_re, path, 0, NULL))
        return TRUE;

    GnomeVFSFileInfo * info = gnome_vfs_file_info_new();

    gboolean ret = TRUE;

    if(!gnome_vfs_get_file_info(path, info,
                                GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
                                GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE |
                                GNOME_VFS_FILE_INFO_FOLLOW_LINKS))
    {
        if(strcmp("x-directory/normal", info->mime_type) == 0)
            ret = parse_dir(path);
        else if(strcmp("audio/x-scpls", info->mime_type) == 0)
            ret = parse_pls(path);
        else if(strcmp("audio/x-mpegurl", info->mime_type) == 0)
            ret = parse_m3u(path);
        else if(strcmp("audio/x-pn-realaudio", info->mime_type) == 0)
            ret = parse_ram(path);
    }

    gnome_vfs_file_info_unref(info);
    return ret;
}
