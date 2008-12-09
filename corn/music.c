#include "config.h"

#include "gettext.h"
#include "music.h"
#include "main.h"
#include "playlist.h"
#include "configuration.h"

#include <glib-object.h>
#include <xine.h>
#include <glib.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

typedef struct _music_socket_pair_t {
    gint reader;
    gint writer;
} music_socket_pair_t;

static xine_t             *xine = NULL;
static xine_stream_t      *stream = NULL;
static xine_audio_port_t  *ao = NULL;
static xine_event_queue_t *events = NULL;

static GHashTable * current_song_meta;

static gint stream_time;

gboolean music_gapless = FALSE;
gint music_playing = MUSIC_STOPPED;
gint music_volume;

music_socket_pair_t event_sockets;

typedef ssize_t (* fd_rw_op_func)(int fd, void * data, size_t len);

void music_event_fd_op(fd_rw_op_func func, int fd, xine_event_t * e)
{
    ssize_t ntransferred = 0;
    do {
        ssize_t ret = func(fd,
                (void *)(((char *)e) + ntransferred),
                sizeof(xine_event_t) - ntransferred);
        if(ret == -1)
            g_assert(ret == EINTR || ret == EAGAIN);
        else
            ntransferred += ret;
    } while(ntransferred < sizeof(xine_event_t));
}

void music_event_send(void *data, const xine_event_t *e)
{
    music_event_fd_op((fd_rw_op_func)&write, event_sockets.writer, (xine_event_t *)e);
}

gboolean music_event_handle(GIOChannel *source, GIOCondition condition, gpointer data)
{
    xine_event_t e;
    music_event_fd_op((fd_rw_op_func)&read, event_sockets.reader, &e);

    xine_mrl_reference_data_t *mrl;
    static gboolean mrl_change = FALSE;

    if(main_status == CORN_RUNNING)
    {
        switch(e.type)
        {
        case XINE_EVENT_UI_PLAYBACK_FINISHED:
#if defined(XINE_PARAM_GAPLESS_SWITCH) && defined(XINE_PARAM_EARLY_FINISHED_EVENT)
            if(music_gapless)
                xine_set_param(stream, XINE_PARAM_GAPLESS_SWITCH, 1);
#endif
            // XXX should lock playlist
            playlist_advance((mrl_change ? 0 : 1), config_loop_at_end);
            mrl_change = FALSE;
            break;
        case XINE_EVENT_MRL_REFERENCE_EXT:
            mrl = e.data;
            g_message("MRL REFERENCE %s", mrl->mrl);
            if(playlist_current)
            {
                playlist_replace_path(mrl->alternative, mrl->mrl);
                mrl_change = TRUE;
            }
            break;
        }
    }

    return TRUE;
}

int music_init()
{
    char *configfile;

    if(!xine_check_version(XINE_MAJOR_VERSION, XINE_MINOR_VERSION, XINE_SUB_VERSION))
    {
        g_critical(_("Incompatible version of Xine-lib found."));
        return 10;
    }

    xine = xine_new();

    xine_config_register_string(xine,
        "audio.driver", "auto", "audio driver", NULL, 0, NULL, NULL);

    configfile = g_build_filename(g_get_user_config_dir(), PACKAGE, "xine_config", NULL);
    xine_config_load(xine, configfile);
    g_free(configfile);

    xine_cfg_entry_t driver = {0};
    xine_config_lookup_entry(xine, "audio.driver", &driver);

    xine_init(xine);

    if(!(ao = xine_open_audio_driver(xine, driver.str_value, NULL)))
    {
        g_critical(_("Unable to open audio driver from Xine."));
        return 11;
    }

    if(!(stream = xine_stream_new(xine, ao, NULL)))
    {
        g_critical(_("Unable to open a Xine stream."));
        return 12;
    }

    // hey, everyone else is doing it
    // http://www.google.com/codesearch?q=xine_set_param\(.*%2C\s*XINE_PARAM_METRONOM_PREBUFFER%2C\s*6000
    xine_set_param(stream, XINE_PARAM_METRONOM_PREBUFFER, 6000);

    xine_set_param(stream, XINE_PARAM_IGNORE_VIDEO, 1);
    xine_set_param(stream, XINE_PARAM_IGNORE_SPU, 1);

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, (int *)&event_sockets))
    {
        g_critical("%s (%s).", _("Unable to open event socket pair"), g_strerror(errno));
        return 13;
    }

    events = xine_event_new_queue(stream);
    xine_event_create_listener_thread(events, music_event_send, NULL);

    GIOChannel * chan = g_io_channel_unix_new(event_sockets.reader);
    g_io_add_watch_full(chan, G_PRIORITY_HIGH, G_IO_IN, music_event_handle, NULL, NULL);
    g_io_channel_unref(chan);

#if defined(XINE_PARAM_GAPLESS_SWITCH) && defined(XINE_PARAM_EARLY_FINISHED_EVENT)
    music_gapless = !!xine_check_version(1, 1, 1);
#endif

    music_volume = xine_get_param(stream, XINE_PARAM_AUDIO_VOLUME);

    return 0;
}

void music_destroy()
{
    music_stop();
    xine_event_dispose_queue(events);
    xine_dispose(stream);
    xine_close_audio_driver(xine, ao);
    xine_exit(xine);
    while(close(event_sockets.reader) == -1 && errno == EINTR);
    while(close(event_sockets.writer) == -1 && errno == EINTR);
}

static gboolean try_to_play(void)
{
    if(music_playing == MUSIC_PLAYING)
        return TRUE;

    if(!playlist_current)
        return TRUE;

    gchar *path;
    if(!(path = g_filename_from_utf8(PATH(playlist_current), -1, NULL, NULL, NULL)))
    {
        g_critical(_("Skipping '%s'. Could not convert from UTF-8. Bug?"), PATH(playlist_current));
        return FALSE;
    }

    if(xine_get_status(stream) != XINE_STATUS_IDLE)
        xine_close(stream);

    if(!xine_open(stream, path))
        return FALSE;

#if defined(XINE_PARAM_GAPLESS_SWITCH) && defined(XINE_PARAM_EARLY_FINISHED_EVENT)
    if(music_gapless)
        xine_set_param(stream, XINE_PARAM_EARLY_FINISHED_EVENT, 1);
#endif

    if(xine_play(stream, 0, stream_time))
    {
        stream_time = 0;
        music_playing = MUSIC_PLAYING;
        return TRUE;
    }
    music_playing = MUSIC_STOPPED;
    return FALSE;
}

////////////////////////////////////////////////////////////////////////
//                             METADATA
////////////////////////////////////////////////////////////////////////

static void free_gvalue_and_its_value(gpointer value)
{
    g_value_unset((GValue*)value);
    g_free((GValue*)value);
}

static void add_metadata_from_string(GHashTable * meta, const gchar * name, const gchar * str)
{
    if(!str)
        return;

    gchar * u = g_locale_to_utf8(str, strlen(str), NULL, NULL, NULL);
    if(!u)
    {
        g_critical(_("Skipping %s value '%s'. Could not convert to UTF-8. Bug?"), name, str);
        return;
    }

    GValue * val = g_new0(GValue, 1);
    g_value_init(val, G_TYPE_STRING);
    g_value_set_string(val, u);
    g_hash_table_insert(meta, name, val);
}

static void add_metadata_from_int(GHashTable * meta, const gchar * name, gint num)
{
    GValue * val = g_new0(GValue, 1);
    g_value_init(val, G_TYPE_INT);
    g_value_set_int(val, num);
    g_hash_table_insert(meta, name, val);
}

static GHashTable * get_stream_metadata(xine_stream_t * strm)
{
    GHashTable * meta = g_hash_table_new_full(g_str_hash, g_str_equal,
        NULL, // our keys are all static -- no free function for them
        free_gvalue_and_its_value);

    add_metadata_from_string(meta, "title", xine_get_meta_info(strm, XINE_META_INFO_TITLE));
    add_metadata_from_string(meta, "artist", xine_get_meta_info(strm, XINE_META_INFO_ARTIST));
    add_metadata_from_string(meta, "album", xine_get_meta_info(strm, XINE_META_INFO_ALBUM));
    add_metadata_from_string(meta, "tracknumber", xine_get_meta_info(strm, XINE_META_INFO_TRACK_NUMBER));

    gint pos, time, length;
    /* length = 0 for streams */
    if(xine_get_pos_length(strm, &pos, &time, &length) && length)
    {
        add_metadata_from_int(meta, "time", length/1000);
        add_metadata_from_int(meta, "mtime", length);
    }

    add_metadata_from_string(meta, "genre", xine_get_meta_info(strm, XINE_META_INFO_GENRE));

    // "comment"
    // "rating" int [1..5] or [0..5]?

    const gchar * yearstr = xine_get_meta_info(strm, XINE_META_INFO_YEAR);
    if(yearstr)
    {
        gchar * end;
        gint year = (gint)strtol(yearstr, &end, 10);
        if(!errno && end != yearstr)
            add_metadata_from_int(meta, "year", year);
    }

    // "date" - timestamp of original performance - int

    add_metadata_from_int(meta, "audio-bitrate", xine_get_stream_info(strm, XINE_STREAM_INFO_BITRATE));
    add_metadata_from_int(meta, "audio-samplerate", xine_get_stream_info(strm, XINE_STREAM_INFO_AUDIO_SAMPLERATE));
    return meta;
}

GHashTable * music_get_playlist_item_metadata(PlaylistItem * item)
{
    GHashTable * empty = g_hash_table_new(NULL, NULL);

    g_return_val_if_fail(item != NULL, empty);

    xine_audio_port_t * audio = xine_open_audio_driver(xine, "none", NULL);

    g_return_val_if_fail(audio != NULL, empty);

    xine_stream_t * strm = xine_stream_new(xine, audio, NULL);
    if(!strm)
    {
        xine_close_audio_driver(xine, audio);
        g_return_val_if_fail(strm != NULL, empty);
    }

    gchar * path;
    if(!(path = g_filename_from_utf8(PATH(item), -1, NULL, NULL, NULL)))
    {
        g_critical(_("Skipping getting track metadata for '%s'. "
                     "Could not convert from UTF-8. Bug?"), PATH(item));
        xine_dispose(strm);
        xine_close_audio_driver(xine, audio);
        return empty;
    }

    if(!xine_open(strm, path))
    {
        xine_dispose(strm);
        xine_close_audio_driver(xine, audio);
    }

    GHashTable * meta = get_stream_metadata(strm);
    add_metadata_from_string(meta, "mrl", PATH(item));

    xine_dispose(strm);
    xine_close_audio_driver(xine, audio);

    g_hash_table_unref(empty);
    return meta;
}

GHashTable * music_get_track_metadata(gint track)
{
    GHashTable * empty = g_hash_table_new(NULL, NULL);
    g_return_val_if_fail(track >= 0, empty);
    g_return_val_if_fail(track < g_queue_get_length(playlist), empty);
    g_hash_table_unref(empty);
    return music_get_playlist_item_metadata(g_queue_peek_nth(playlist, track));
}

GHashTable * music_get_metadata(void)
{
    // try to do it cheaply, using the already loaded stream
    if(stream && xine_get_status(stream) != XINE_STATUS_IDLE)
    {
        GHashTable * meta = get_stream_metadata(stream);
        add_metadata_from_string(meta, "mrl", PATH(playlist_current));
        return meta;
    }
    else if(playlist_position != -1) // or do it the hard way
        return music_get_playlist_item_metadata(playlist_current);

    return g_hash_table_new(NULL, NULL);
}

////////////////////////////////////////////////////////////////////////
//                             CONTROL
////////////////////////////////////////////////////////////////////////

void music_play(void)
{
    gint orig_pos = playlist_position;
    while(!try_to_play() && playlist_fail() && playlist_position != orig_pos);
    // if we keep failing to load files, and loop/repeat are on, we don't want
    // to infinitely keep trying to play the same file(s) that won't play.  so
    // once we fail and eventually get back to the same song, stop.
}

void music_seek(gint ms)
{
    music_pause();
    stream_time = MAX(0, ms); // xine is smart.  no need to check upper bound.
    music_play();
}

// position within current song, in ms
gint music_get_position(void)
{
    if(xine_get_status(stream) == XINE_STATUS_IDLE)
        return stream_time;
    gint pos, time, length;
    if(xine_get_pos_length(stream, &pos, &time, &length))
        return time;
    return 0;
}

static void _do_pause(void)
{
    stream_time = music_get_position();
    if(xine_get_status(stream) != XINE_STATUS_IDLE)
        xine_close(stream);
}

void music_pause()
{
    _do_pause();
    music_playing = MUSIC_PAUSED;
}

void music_stop()
{
    _do_pause();
    music_playing = MUSIC_STOPPED;
    stream_time = 0;
}

void music_set_volume(gint vol)
{
    music_volume = CLAMP(vol, 0, 100);
    xine_set_param(stream, XINE_PARAM_AUDIO_VOLUME, music_volume);
}

