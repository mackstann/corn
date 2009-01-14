#include "config.h"

#include "gettext.h"

#include "main.h"
#include "playlist.h"
#include "state.h"
#include "parsefile.h"

#include <glib.h>

#include <stdio.h>
#include <errno.h>

static GThreadPool * pool;

static void save_playlist(GString * pldata)
{
    FILE * f = state_file_open("playlist.m3u", "w");
    if(f)
    {
        fputs(pldata->str, f);
        fclose(f);
    }
    else
        g_printerr("%s (%s).\n", _("Couldn't open playlist file for writing"), g_strerror(errno));
    g_string_free(pldata, TRUE);
}

static void save_playlist_threadfunc(gpointer data, gpointer user_data)
{
    save_playlist((GString *)data);
}

static GString * generate_playlist_data(void)
{
    GString * s = g_string_new("");
    for(gint i = 0; i < playlist_length(); i++)
    {   
        g_string_append(s, playlist_nth(i));
        g_string_append_c(s, '\n');
    }
    return s;
}

void state_playlist_init(void)
{
    gchar * playlist_filename = g_build_filename(g_get_user_data_dir(), main_instance_name, "playlist.m3u", NULL);
    GFile * file = g_file_new_for_path(playlist_filename);
    parse_m3u(file);
    g_object_unref(file);
    g_free(playlist_filename);

    GError * error = NULL;
    pool = g_thread_pool_new(save_playlist_threadfunc, NULL, 1, FALSE, &error);
    if(error)
        g_error("%s (%s).\n", _("Couldn't create thread pool"), error->message);
}

void state_playlist_destroy(void)
{
    g_thread_pool_free(pool, FALSE, TRUE);
    if(playlist_modified())
        save_playlist(generate_playlist_data());
}

void state_playlist_launch_save_if_time_has_come(void)
{
    if(!playlist_flush_due())
        return;

    if(g_thread_pool_unprocessed(pool))
    {
        g_warning(_("Thread pool is getting backed up!  Attempts to save playlist are hanging?"));
        return;
    }

    GError * error = NULL;
    g_thread_pool_push(pool, generate_playlist_data(), &error);
    if(error)
        g_error("%s (%s).\n", _("Couldn't push thread to save playlist to disk"), error->message);

    playlist_mark_as_flushed();
}
