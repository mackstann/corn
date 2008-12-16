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

xine_t * xine = NULL;
xine_stream_t * music_stream = NULL;

static xine_audio_port_t  *ao = NULL;
static xine_event_queue_t *events = NULL;

gint music_stream_time;
gint music_playing = MUSIC_STOPPED;
gint music_volume;

static gboolean music_gapless = FALSE;

/////// inter-thread i/o ///////
//
// an async queue would be a bit cleaner for this, but the only way to check it
// for events is to poll it.  with a socket pair, we have file descriptors that
// we can select().

static music_socket_pair_t event_sockets;

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
                xine_set_param(music_stream, XINE_PARAM_GAPLESS_SWITCH, 1);
#endif
            // XXX should lock playlist
            playlist_advance((mrl_change ? 0 : 1), config_loop_at_end);
            mrl_change = FALSE;
            break;
        case XINE_EVENT_MRL_REFERENCE_EXT:
            mrl = e.data;
            g_message("MRL REFERENCE %s", mrl->mrl);
            if(PLAYLIST_CURRENT_ITEM())
            {
                playlist_replace_path(mrl->alternative, mrl->mrl);
                mrl_change = TRUE;
            }
            break;
        }
    }

    return TRUE;
}

// end inter-thread i/o stuff

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

    if(!(music_stream = xine_stream_new(xine, ao, NULL)))
    {
        g_critical(_("Unable to open a Xine stream."));
        return 12;
    }

    // hey, everyone else is doing it
    // http://www.google.com/codesearch?q=xine_set_param\(.*%2C\s*XINE_PARAM_METRONOM_PREBUFFER%2C\s*6000
    xine_set_param(music_stream, XINE_PARAM_METRONOM_PREBUFFER, 6000);

    xine_set_param(music_stream, XINE_PARAM_IGNORE_VIDEO, 1);
    xine_set_param(music_stream, XINE_PARAM_IGNORE_SPU, 1);

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, (int *)&event_sockets))
    {
        g_critical("%s (%s).", _("Unable to open event socket pair"), g_strerror(errno));
        return 13;
    }

    events = xine_event_new_queue(music_stream);
    xine_event_create_listener_thread(events, music_event_send, NULL);

    GIOChannel * chan = g_io_channel_unix_new(event_sockets.reader);
    g_io_add_watch_full(chan, G_PRIORITY_HIGH, G_IO_IN, music_event_handle, NULL, NULL);
    g_io_channel_unref(chan);

#if defined(XINE_PARAM_GAPLESS_SWITCH) && defined(XINE_PARAM_EARLY_FINISHED_EVENT)
    music_gapless = xine_check_version(1, 1, 1);
#endif

    music_volume = xine_get_param(music_stream, XINE_PARAM_AUDIO_VOLUME);

    return 0;
}

void music_destroy()
{
    if(xine_get_status(music_stream) != XINE_STATUS_IDLE)
        xine_close(music_stream);
    xine_event_dispose_queue(events);
    xine_dispose(music_stream);
    xine_close_audio_driver(xine, ao);
    xine_exit(xine);
    while(close(event_sockets.reader) == -1 && errno == EINTR);
    while(close(event_sockets.writer) == -1 && errno == EINTR);
}

gboolean music_try_to_play(void)
{
    if(music_playing == MUSIC_PLAYING)
        return TRUE;

    if(!PLAYLIST_CURRENT_ITEM())
        return TRUE;

    gchar *path;
    if(!(path = g_filename_from_utf8(PATH(PLAYLIST_CURRENT_ITEM()), -1, NULL, NULL, NULL)))
    {
        g_critical(_("Skipping '%s'. Could not convert from UTF-8. Bug?"), PATH(PLAYLIST_CURRENT_ITEM()));
        return FALSE;
    }

    if(xine_get_status(music_stream) != XINE_STATUS_IDLE)
        xine_close(music_stream);

    if(!xine_open(music_stream, path))
        return FALSE;

#if defined(XINE_PARAM_GAPLESS_SWITCH) && defined(XINE_PARAM_EARLY_FINISHED_EVENT)
    if(music_gapless)
        xine_set_param(music_stream, XINE_PARAM_EARLY_FINISHED_EVENT, 1);
#endif

    if(xine_play(music_stream, 0, music_stream_time))
    {
        music_stream_time = 0;
        music_playing = MUSIC_PLAYING;
        return TRUE;
    }
    music_playing = MUSIC_STOPPED;
    return FALSE;
}

