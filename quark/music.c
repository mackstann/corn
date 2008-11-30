#include "config.h"

#include "gettext.h"
#include "music.h"
#include "main.h"
#include "playlist.h"

#include <xine.h>
#include <glib.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

static xine_t             *xine;
static xine_stream_t      *stream;
static xine_audio_port_t  *ao;
static xine_video_port_t  *vo;
static xine_event_queue_t *events;

static int                 stream_pos;

static GList *notify_channels = NULL;
gboolean music_playing = FALSE;

void
music_events (void *data, const xine_event_t *e)
{
    xine_mrl_reference_data_t *mrl;
    static gboolean mrl_change = FALSE;

    thread_lock ();

    if (main_status == QUARK_RUNNING) {

        g_message ("XINE EVENT %d", e->type);

        switch (e->type) {
        case XINE_EVENT_UI_PLAYBACK_FINISHED:
            playlist_advance ((mrl_change ? 0 : 1), main_loop_at_end);
            mrl_change = FALSE;
            break;
        case XINE_EVENT_MRL_REFERENCE:
            mrl = e->data;
            g_message ("MRL REFERENCE %s", mrl->mrl);
            if (playlist_current) {
                playlist_replace_path (mrl->alternative, mrl->mrl);
                mrl_change = TRUE;
            }
            break;
        case XINE_EVENT_UI_MESSAGE:
            g_message (_("Message from Xine: %s"),
                       (char*) e->data);
            break;
        }
    }

    thread_unlock ();
}

void
music_init ()
{
    char *configfile;

    if (!xine_check_version (XINE_MAJOR_VERSION, XINE_MINOR_VERSION,
                             XINE_SUB_VERSION)) {
        g_critical (_("Incompatible version of Xine-lib found."));
        exit (EXIT_FAILURE);
    }

    xine = xine_new ();

    configfile = g_build_filename (g_get_home_dir (), ".xine", "config", NULL);
    xine_config_load (xine, configfile);
    g_free (configfile);

    xine_init (xine);

    if (!(ao = xine_open_audio_driver (xine, NULL, NULL))) {
        g_critical (_("Unable to open audio driver from Xine."));
        exit (EXIT_FAILURE);
    }

    if (!(vo = xine_open_video_driver (xine, NULL, XINE_VISUAL_TYPE_NONE,
                                       NULL))) {
        g_critical (_("Unable to open video driver from Xine."));
        exit (EXIT_FAILURE);
    }

    if (!(stream = xine_stream_new (xine, ao, vo))) {
        g_critical (_("Unable to open a Xine stream."));
        exit (EXIT_FAILURE);
    }

    events = xine_event_new_queue (stream);
    xine_event_create_listener_thread (events, music_events, NULL);
}


void
music_destroy ()
{
    xine_event_dispose_queue (events);
    xine_dispose (stream);
    xine_close_video_driver (xine, vo);
    xine_close_audio_driver (xine, ao);
    xine_exit (xine);
}

void
music_play ()
{
    PlaylistItem *item;
    gint state, pos;
    gchar *path;

    state = xine_get_status (stream);

    if (!music_playing && playlist_current) {
        item = LISTITEM (playlist_current);

        if (!(path = g_filename_from_utf8 (PATH (item), -1,
                                           NULL, NULL, NULL))) {
            g_critical (_("Skipping '%s'. Could not convert from UTF-8. "
                          "Bug?"), PATH (item));
            playlist_fail ();
            return;
        }

        if (state != XINE_STATUS_IDLE)
            xine_close (stream);

        music_notify_current_song (g_list_position
                                   (playlist, playlist_current));

        if (!xine_open (stream, path)) {
            playlist_fail ();
            return;
        }
        
        pos = stream_pos;
        stream_pos = 0;
        if (xine_play (stream, pos, 0)) {
            music_playing = TRUE;
            music_notify_playing ();
        } else {
            music_playing = FALSE;
            playlist_fail ();
        }
    }
}

void
music_pause ()
{
    int state;

    state = xine_get_status (stream);
    if (state != XINE_STATUS_IDLE) {
        int pos, time, length;
        /* length = 0 for streams */
        if (!(xine_get_pos_length (stream, &pos, &time, &length) && length))
            pos = 0;
        stream_pos = pos;
        g_message ("stream_pos: %d", pos);
        xine_close (stream);
    }
    music_playing = FALSE;
    music_notify_paused ();
}

void
music_stop ()
{
    int state;

    state = xine_get_status (stream);
    if (state != XINE_STATUS_IDLE)
        xine_close (stream);
    stream_pos = 0;
    music_playing = FALSE;
    music_notify_stopped ();
}

void
music_add_notify (GIOChannel *output)
{
    GList *it;
    gint i;

    GList *all = g_list_prepend (notify_channels, output);
    GList *only_new = g_list_prepend(NULL, output);

    notify_channels = only_new;

    for (it = playlist, i = 0; it; it = g_list_next(it), ++i)
        music_notify_add_song(MAIN_PATH(LISTITEM(it)), i);
    if (playlist_current)
        music_notify_current_song(g_list_position(playlist, playlist_current));

    g_list_free(only_new);
    notify_channels = all;
}

gboolean
music_list_notify (GIOChannel *channel, const char *message)
{
    gsize written;
    GIOStatus status;
    gboolean retval = TRUE;

    while ( (status = g_io_channel_write_chars (channel, message,
                                                strlen (message),
                                                &written, NULL))
            == G_IO_STATUS_AGAIN) {
        message += written;
    }
    
    if (status == G_IO_STATUS_ERROR) {
        retval = FALSE;
    }
    while ( (status = g_io_channel_flush(channel, NULL)) == G_IO_STATUS_AGAIN);

    if (status == G_IO_STATUS_ERROR)
        retval = FALSE;

    return retval;
}
    
static void
music_notify_msg (const char *message)
{
    char *fullmessage;
    GList *it;

    g_message ("%s", message);
    fullmessage = g_strdup_printf ("STARTMESSAGE\n%sENDMESSAGE\n", message);
    
    it = notify_channels;

    while (it != NULL) {
        if (music_list_notify (it->data, fullmessage))
            it = it->next;
        else {
            // i think this may be leaky/buggy
            GList *tmp = it->next;
            g_list_remove_link (notify_channels, it);
            g_io_channel_shutdown (it->data, FALSE, NULL);
            g_io_channel_unref (it->data);
            if (it == notify_channels)
                notify_channels = NULL;
            it = tmp;
        }
    }
    g_free (fullmessage);
}

void
music_notify_add_song (const gchar *song, gint pos)
{
    gchar *s = g_strdup_printf ("ADD\n%s\n%d\n", song, pos);
    music_notify_msg (s);
    g_free (s);
}

void
music_notify_remove_song (gint pos)
{
    gchar *s = g_strdup_printf ("REMOVE\n%d\n", pos);
    music_notify_msg (s);
    g_free (s);
}

void
music_notify_move_song (gint from, gint to)
{
    gchar *s = g_strdup_printf ("MOVE\n%d\n%d\n", from, to);
    music_notify_msg (s);
    g_free (s);
}

void
music_notify_current_song (gint pos)
{
    gchar *s = g_strdup_printf ("CURRENT\n%d\n", pos);
    music_notify_msg (s);
    g_free (s);
}

void
music_notify_song_failed ()
{
    gchar *s = g_strdup_printf ("FAILED\n");
    music_notify_msg (s);
    g_free (s);
}

void
music_notify_playing ()
{
    gchar *s = g_strdup_printf ("PLAYING\n");
    music_notify_msg (s);
    g_free (s);
}

void
music_notify_paused ()
{
    gchar *s = g_strdup_printf ("PAUSED\n");
    music_notify_msg (s);
    g_free (s);
}

void
music_notify_stopped ()
{
    gchar *s = g_strdup_printf ("STOPPED\n");
    music_notify_msg (s);
    g_free (s);
}

