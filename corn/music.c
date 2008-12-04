#include "config.h"

#include "gettext.h"
#include "music.h"
#include "main.h"
#include "playlist.h"

#include <json-glib/json-glib.h>
#include <glib-object.h>
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

static gint stream_time;

static GList *notify_channels = NULL;

gboolean music_playing = FALSE;
gint music_volume = 100; // XXX

void
music_events (void *data, const xine_event_t *e)
{
    xine_mrl_reference_data_t *mrl;
    static gboolean mrl_change = FALSE;

    thread_lock ();

    if (main_status == CORN_RUNNING) {

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
    gint state, time;
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
        
        time = stream_time;
        stream_time = 0;
        if (xine_play (stream, 0, time)) {
            const gchar * meta;
            meta = xine_get_meta_info(stream, XINE_META_INFO_TITLE);          g_message("XINE_META_INFO_TITLE:          %s", meta);
            meta = xine_get_meta_info(stream, XINE_META_INFO_COMMENT);        g_message("XINE_META_INFO_COMMENT:        %s", meta);
            meta = xine_get_meta_info(stream, XINE_META_INFO_ARTIST);         g_message("XINE_META_INFO_ARTIST:         %s", meta);
            meta = xine_get_meta_info(stream, XINE_META_INFO_GENRE);          g_message("XINE_META_INFO_GENRE:          %s", meta);
            meta = xine_get_meta_info(stream, XINE_META_INFO_ALBUM);          g_message("XINE_META_INFO_ALBUM:          %s", meta);
            meta = xine_get_meta_info(stream, XINE_META_INFO_YEAR);           g_message("XINE_META_INFO_YEAR:           %s", meta);
            meta = xine_get_meta_info(stream, XINE_META_INFO_VIDEOCODEC);     g_message("XINE_META_INFO_VIDEOCODEC:     %s", meta);
            meta = xine_get_meta_info(stream, XINE_META_INFO_AUDIOCODEC);     g_message("XINE_META_INFO_AUDIOCODEC:     %s", meta);
            meta = xine_get_meta_info(stream, XINE_META_INFO_SYSTEMLAYER);    g_message("XINE_META_INFO_SYSTEMLAYER:    %s", meta);
            meta = xine_get_meta_info(stream, XINE_META_INFO_INPUT_PLUGIN);   g_message("XINE_META_INFO_INPUT_PLUGIN:   %s", meta);
            meta = xine_get_meta_info(stream, XINE_META_INFO_CDINDEX_DISCID); g_message("XINE_META_INFO_CDINDEX_DISCID: %s", meta);
            meta = xine_get_meta_info(stream, XINE_META_INFO_TRACK_NUMBER);   g_message("XINE_META_INFO_TRACK_NUMBER:   %s", meta);
            music_playing = TRUE;
            music_notify_playing ();
        } else {
            music_playing = FALSE;
            playlist_fail ();
        }
    }
}

static void _do_pause(void)
{
    stream_time = music_get_position();
    if (xine_get_status(stream) != XINE_STATUS_IDLE)
        xine_close (stream);
    music_playing = FALSE;
}

void music_seek(gint ms)
{
    _do_pause();
    // XXX DO SANITY CHECK!
    stream_time = ms;
    music_play();
}

gint music_get_position(void)
{
    if (xine_get_status(stream) != XINE_STATUS_IDLE) {
        int pos, time, length;
        /* length = 0 for streams */
        if (!(xine_get_pos_length (stream, &pos, &time, &length) && length))
        {
            time = 0;
        } else
        {
            g_message("pos: %d", pos);
            g_message("time: %d", time);
            g_message("len: %d", length);
        }
        return time;
    }
    return stream_time;
}

void
music_pause ()
{
    _do_pause();
}

void
music_stop ()
{
    _do_pause();
    stream_time = 0;
}

void music_set_volume(gint vol)
{
    music_volume = CLAMP(vol, 0, 100);
    xine_set_param(stream, XINE_PARAM_AUDIO_AMP_LEVEL, music_volume);
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
    GList *it = notify_channels;

    g_message ("%s", message);

    gchar *s = g_strdup_printf ("%s\n", message);

    while (it != NULL) {
        if (music_list_notify (it->data, s)) it = it->next;
        else {
            // i think this may be leaky/buggy, or at least awkward
            GList *tmp = it->next;
            g_list_remove_link (notify_channels, it);
            g_io_channel_shutdown (it->data, FALSE, NULL);
            g_io_channel_unref (it->data);
            if (it == notify_channels)
                notify_channels = NULL;
            it = tmp;
        }
    }
    g_free(s);
}

gchar *
serialize_json_object (JsonObject * obj)
{
    JsonNode * root_node = json_node_new(JSON_NODE_OBJECT);
    json_node_set_object(root_node, obj);

    JsonGenerator * json = json_generator_new();
    json_generator_set_root(json, root_node);

    gsize _;
    gchar * output = json_generator_to_data(json, &_);

    g_object_unref(json);
    json_node_free(root_node);

    return output;
}

typedef struct {
    gint type;
    const gchar * name;
    union {
        gint i;
        const gchar * s;
    } val;
} notify_attr_t;

void notify_with_object (notify_attr_t * attrs)
{
    JsonObject * obj = json_object_new();

    for(notify_attr_t * attr = attrs; attr->name; ++attr)
    {
        JsonNode * val_node = json_node_new(JSON_NODE_VALUE);

        if(attr->type == G_TYPE_STRING)
            json_node_set_string(val_node, attr->val.s);
        else if(attr->type == G_TYPE_INT)
            json_node_set_int(val_node, attr->val.i);
        else
            g_return_if_reached();

        json_object_add_member(obj, attr->name, val_node);
    }

    gchar * output = serialize_json_object(obj);

    music_notify_msg (output);

    json_object_unref(obj);
    g_free(output);
}

void
music_notify_add_song (const gchar *song, gint pos)
{
    notify_attr_t attrs[] = {
        { .type = G_TYPE_STRING, .name = "event",    .val = { .s = "add" } },
        { .type = G_TYPE_STRING, .name = "song",     .val = { .s = song } },
        { .type = G_TYPE_INT,    .name = "position", .val = { .i = pos } },
        { 0 }
    };
    notify_with_object(attrs);
}

void
music_notify_remove_song (gint pos)
{
    notify_attr_t attrs[] = {
        { .type = G_TYPE_STRING, .name = "event",    .val = { .s = "remove" } },
        { .type = G_TYPE_INT,    .name = "position", .val = { .i = pos } },
        { 0 }
    };
    notify_with_object(attrs);
}

void
music_notify_move_song (gint from, gint to)
{
    notify_attr_t attrs[] = {
        { .type = G_TYPE_STRING, .name = "event", .val = { .s = "move" } },
        { .type = G_TYPE_INT,    .name = "from",  .val = { .i = from } },
        { .type = G_TYPE_INT,    .name = "to",    .val = { .i = to } },
        { 0 }
    };
    notify_with_object(attrs);
}

void
music_notify_current_song (gint pos)
{
    notify_attr_t attrs[] = {
        { .type = G_TYPE_STRING, .name = "event",    .val = { .s = "current" } },
        { .type = G_TYPE_INT,    .name = "position", .val = { .i = pos } },
        { 0 }
    };
    notify_with_object(attrs);
}

void
music_notify_song_failed ()
{
    notify_attr_t attrs[] = {
        { .type = G_TYPE_STRING, .name = "event", .val = { .s = "failed" } },
        { 0 }
    };
    notify_with_object(attrs);
}

void
music_notify_playing ()
{
    notify_attr_t attrs[] = {
        { .type = G_TYPE_STRING, .name = "event", .val = { .s = "playing" } },
        { 0 }
    };
    notify_with_object(attrs);
}

void
music_notify_paused ()
{
    notify_attr_t attrs[] = {
        { .type = G_TYPE_STRING, .name = "event", .val = { .s = "paused" } },
        { 0 }
    };
    notify_with_object(attrs);
}

void
music_notify_stopped ()
{
    notify_attr_t attrs[] = {
        { .type = G_TYPE_STRING, .name = "event", .val = { .s = "stopped" } },
        { 0 }
    };
    notify_with_object(attrs);
}

void
music_notify_clear ()
{
    notify_attr_t attrs[] = {
        { .type = G_TYPE_STRING, .name = "event", .val = { .s = "clear" } },
        { 0 }
    };
    notify_with_object(attrs);
}

