#include "config.h"

#include "gettext.h"
#include "music.h"
#include "main.h"
#include "playlist.h"

#include <glib-object.h>
#include <xine.h>
#include <glib.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>

static xine_t             *xine;
static xine_stream_t      *stream;
static xine_audio_port_t  *ao;
static xine_event_queue_t *events;

static GHashTable * current_song_meta;

static gint stream_time;

gboolean music_gapless = FALSE;
gint music_playing = MUSIC_STOPPED;
gint music_volume;

void music_events(void *data, const xine_event_t *e)
{
    xine_mrl_reference_data_t *mrl;
    static gboolean mrl_change = FALSE;

    g_static_mutex_lock(&main_mutex);

    if(main_status == CORN_RUNNING)
    {
        switch(e->type)
        {
        case XINE_EVENT_UI_PLAYBACK_FINISHED:
#if defined(XINE_PARAM_GAPLESS_SWITCH) && defined(XINE_PARAM_EARLY_FINISHED_EVENT)
            if(music_gapless)
                xine_set_param(stream, XINE_PARAM_GAPLESS_SWITCH, 1);
#endif
            playlist_advance((mrl_change ? 0 : 1), main_loop_at_end);
            mrl_change = FALSE;
            break;
        case XINE_EVENT_MRL_REFERENCE:
        case XINE_EVENT_MRL_REFERENCE_EXT:
            mrl = e->data;
            g_message("MRL REFERENCE %s", mrl->mrl);
            if(playlist_current)
            {
                playlist_replace_path(mrl->alternative, mrl->mrl);
                mrl_change = TRUE;
            }
            break;
        }
    }

    g_static_mutex_unlock(&main_mutex);
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

    events = xine_event_new_queue(stream);
    xine_event_create_listener_thread(events, music_events, NULL);

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
}

void music_play()
{
    PlaylistItem *item;
    gint state, time;
    gchar *path;

    state = xine_get_status(stream);

    if(music_playing != MUSIC_PLAYING && playlist_current)
    {
        item = LISTITEM(playlist_current);

        if(!(path = g_filename_from_utf8(PATH(item), -1, NULL, NULL, NULL)))
        {
            g_critical(_("Skipping '%s'. Could not convert from UTF-8. Bug?"), PATH(item));
            playlist_fail();
            return;
        }

        if(state != XINE_STATUS_IDLE)
            xine_close(stream);

        if(!xine_open(stream, path))
        {
            playlist_fail();
            return;
        }

#if defined(XINE_PARAM_GAPLESS_SWITCH) && defined(XINE_PARAM_EARLY_FINISHED_EVENT)
        if(music_gapless)
            xine_set_param(stream, XINE_PARAM_EARLY_FINISHED_EVENT, 1);
#endif
        
        time = stream_time;
        stream_time = 0;
        if(xine_play(stream, 0, time))
            music_playing = MUSIC_PLAYING;
        else
        {
            music_playing = MUSIC_STOPPED;
            playlist_fail();
        }
    }
}

static void free_gvalue_and_its_value(gpointer value)
{
    g_value_unset((GValue*)value);
    g_free((GValue*)value);
}

void add_metadata_from_string(GHashTable * meta, const gchar * name, const gchar * str)
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

void add_metadata_from_int(GHashTable * meta, const gchar * name, gint num)
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

GHashTable * music_get_metadata(void)
{
    return get_stream_metadata(stream);
}

GHashTable * music_get_track_metadata(gint track)
{
    GHashTable * empty = g_hash_table_new(NULL, NULL);

    if(track >= g_list_length(playlist) || track < 0)
        return empty;

    xine_audio_port_t * audio = xine_open_audio_driver(xine, "none", NULL);
    if(!audio)
        return empty;

    xine_stream_t * strm = xine_stream_new(xine, audio, NULL);
    if(!strm)
    {
        xine_close_audio_driver(xine, audio);
        return empty;
    }

    gchar * path;
    PlaylistItem * item = LISTITEM(g_list_nth(playlist, track));
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

    xine_dispose(strm);
    xine_close_audio_driver(xine, audio);

    g_hash_table_unref(empty);
    return meta;
}

static void _do_pause(void)
{
    stream_time = music_get_position();
    if(xine_get_status(stream) != XINE_STATUS_IDLE)
        xine_close(stream);
}

void music_seek(gint ms)
{
    // if it goes past end of song, xine will just end the song
    ms = MAX(0, ms);
    _do_pause();
    stream_time = ms;
    music_play();
}

gint music_get_position(void)
{
    if(xine_get_status(stream) == XINE_STATUS_IDLE)
        return stream_time;
    gint pos, time, length;
    if(xine_get_pos_length(stream, &pos, &time, &length))
        return time;
    return 0;
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

