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
static xine_event_queue_t *events;

static gint stream_time;

gboolean music_gapless = FALSE;
gboolean music_playing = FALSE;
gint music_volume = 100; // XXX

void music_events(void *data, const xine_event_t *e)
{
    xine_mrl_reference_data_t *mrl;
    static gboolean mrl_change = FALSE;

    thread_lock();

    if(main_status == CORN_RUNNING)
    {
        g_message("XINE EVENT %d", e->type);

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
            mrl = e->data;
            g_message("MRL REFERENCE %s", mrl->mrl);
            if(playlist_current)
            {
                playlist_replace_path(mrl->alternative, mrl->mrl);
                mrl_change = TRUE;
            }
            break;
        case XINE_EVENT_UI_MESSAGE:
            g_message(_("Message from Xine: %s"),
                       (char*) e->data);
            break;
        }
    }

    thread_unlock();
}

void music_init()
{
    char *configfile;

    if(!xine_check_version(XINE_MAJOR_VERSION, XINE_MINOR_VERSION, XINE_SUB_VERSION))
    {
        g_critical(_("Incompatible version of Xine-lib found."));
        exit(EXIT_FAILURE);
    }

    xine = xine_new();

    xine_config_register_string(xine,
        "audio.driver", "auto", "audio driver", NULL, 0, NULL, NULL);

    configfile = g_build_filename(g_get_home_dir(), ".corn", "xine_config", NULL);
    xine_config_load(xine, configfile);
    g_free(configfile);

    xine_cfg_entry_t driver = {0};
    xine_config_lookup_entry(xine, "audio.driver", &driver);

    xine_init(xine);

    if(!(ao = xine_open_audio_driver(xine, driver.str_value, NULL)))
    {
        g_critical(_("Unable to open audio driver from Xine."));
        exit(EXIT_FAILURE);
    }

    if(!(stream = xine_stream_new(xine, ao, NULL)))
    {
        g_critical(_("Unable to open a Xine stream."));
        exit(EXIT_FAILURE);
    }

    events = xine_event_new_queue(stream);
    xine_event_create_listener_thread(events, music_events, NULL);

#if defined(XINE_PARAM_GAPLESS_SWITCH) && defined(XINE_PARAM_EARLY_FINISHED_EVENT)
    music_gapless = !!xine_check_version(1, 1, 1);
#endif
}


void music_destroy()
{
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

    if(!music_playing && playlist_current)
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
        {
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
        } else
        {
            music_playing = FALSE;
            playlist_fail();
        }
    }
}

static void _do_pause(void)
{
    stream_time = music_get_position();
    if(xine_get_status(stream) != XINE_STATUS_IDLE)
        xine_close(stream);
    music_playing = FALSE;
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
    if(xine_get_status(stream) != XINE_STATUS_IDLE)
    {
        int pos, time, length;
        /* length = 0 for streams */
        if(xine_get_pos_length(stream, &pos, &time, &length) && length)
            return time;
        return 0;
    }
    return stream_time;
}

void music_pause()
{
    _do_pause();
}

void music_stop()
{
    _do_pause();
    stream_time = 0;
}

void music_set_volume(gint vol)
{
    music_volume = CLAMP(vol, 0, 100);
    xine_set_param(stream, XINE_PARAM_AUDIO_VOLUME, music_volume);
}

