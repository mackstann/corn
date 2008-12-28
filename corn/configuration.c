#include "config.h"

#include "gettext.h"

#include "main.h"
#include "music.h"
#include "music-control.h"
#include "playlist.h"
#include "configuration.h"
#include "parsefile.h"

#include <glib/gstdio.h>
#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

gboolean config_loop_at_end = FALSE;
gboolean config_random_order = FALSE;
gboolean config_repeat_track = FALSE;

static GThreadPool * pool;

static FILE * open_config_file(const char * name, const char * mode)
{
    gchar * path = g_build_filename(g_get_user_config_dir(), PACKAGE, name, NULL);
    FILE * f = g_fopen(path, mode);
    g_free(path);
    return f;
}

static void save_int_to_config_file(const char * name, gint num)
{
    FILE * f = open_config_file(name, "w");
    if(f)
    {
        fprintf(f, "%d", num);
        fclose(f);
    }
    else
        g_printerr("%s %s (%s).\n", _("Couldn't save to config file"), name, g_strerror(errno));
}

static gint read_int_from_config_file(const char * name, gint min, gint max, gint default_value)
{
    FILE * f = open_config_file(name, "r");
    if(f)
    {
        gchar mrl[20];
        if(fgets(mrl, 20, f))
        {
            mrl[19] = '\0';
            fclose(f);
            gchar * end;
            errno = 0;
            glong val = strtol(mrl, &end, 10);

            if(!errno && end != mrl && val >= min && val <= max)
                return (gint)val;
        }
    }
    return default_value;
}

void save_playlist_threadfunc(gpointer data, gpointer user_data);

void config_load(void)
{
    // playlist
    gchar * playlist_filename = g_build_filename(g_get_user_config_dir(), PACKAGE, "playlist", NULL);
    GnomeVFSURI * pl_uri = gnome_vfs_uri_new(playlist_filename);
    g_free(playlist_filename);
    parse_m3u(pl_uri);
    gnome_vfs_uri_unref(pl_uri);
    // end playlist

    config_loop_at_end = read_int_from_config_file("state.loop", 0, 1, 0);
    config_random_order = read_int_from_config_file("state.random", 0, 1, 0);
    config_repeat_track = read_int_from_config_file("state.repeat", 0, 1, 0);

    playlist_seek(read_int_from_config_file("state.list_position", 0, G_MAXINT, 0));
    gint pos = read_int_from_config_file("state.track_position", 0, G_MAXINT, 0);

    gint playing = read_int_from_config_file("state.playing", 0, 2, MUSIC_STOPPED);

    if(playing == MUSIC_PLAYING)
        music_play();
    else if(playing == MUSIC_PAUSED)
    {
        music_pause();
        music_stream_time = pos;
    }

    // playlist
    GError * error = NULL;
    pool = g_thread_pool_new(save_playlist_threadfunc, NULL, 1, FALSE, &error);
    if(error)
        g_error("%s (%s).\n", _("Couldn't create thread pool"),
                error->message);
    // end playlist
}

void save_playlist(GString * pldata)
{
    FILE * f = open_config_file("playlist", "w");
    if(f)
    {
        fputs(pldata->str, f);
        fclose(f);
    }
    g_string_free(pldata, TRUE);
}

GString * generate_playlist_data(void)
{
    GString * s = g_string_new("");
    for(gint i = 0; i < playlist->len; i++)
    {   
        g_string_append(s, PLAYLIST_ITEM_N(i));
        g_string_append_c(s, '\n');
    }
    return s;
}

void config_save(void)
{
    save_int_to_config_file("state.loop", config_loop_at_end ? 1 : 0);
    save_int_to_config_file("state.random", config_random_order ? 1 : 0);
    save_int_to_config_file("state.repeat", config_repeat_track ? 1 : 0);
    save_int_to_config_file("state.playing", music_playing);
    save_int_to_config_file("state.list_position", playlist_position);
    save_int_to_config_file("state.track_position", music_get_position());
    save_playlist(generate_playlist_data());
}

void save_playlist_threadfunc(gpointer data, gpointer user_data)
{
    save_playlist((GString *)data);
}

gboolean config_maybe_save_playlist(gpointer data)
{
    if(playlist_mtime == -1)
        return TRUE;
    if(main_time_counter - playlist_mtime < playlist_save_wait_time)
        return TRUE;
    if(g_thread_pool_unprocessed(pool))
    {
        g_warning(_("Thread pool is getting backed up!  Attempts to save playlist are hanging?"));
        return TRUE;
    }

    GError * error = NULL;
    g_thread_pool_push(pool, generate_playlist_data(), &error);
    if(error)
        g_error("%s (%s).\n", _("Couldn't push thread to save playlist to disk"),
                error->message);

    playlist_mtime = -1;
    return TRUE;
}
