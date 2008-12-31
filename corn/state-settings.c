#include "config.h"

#include "gettext.h"

#include "music.h"
#include "playlist.h"
#include "music-control.h"
#include "state.h"

#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

gboolean setting_loop_at_end;
gboolean setting_random_order;
gboolean setting_repeat_track;

static void save(const char * name, gint num)
{
    FILE * f = state_file_open(name, "w");
    if(f)
    {
        fprintf(f, "%d", num);
        fclose(f);
    }
    else
        g_printerr("%s %s (%s).\n", _("Couldn't save to settings file"), name, g_strerror(errno));
}

static gint load(const char * name, gint min, gint max, gint default_value)
{
    FILE * f = state_file_open(name, "r");
    if(f)
    {
        const gint bufsize = 20;
        gchar text[bufsize];
        if(fgets(text, bufsize, f))
        {
            text[bufsize-1] = '\0';
            fclose(f);
            gchar * end;
            errno = 0;
            glong val = strtol(text, &end, 10);

            if(!errno && end != text && val >= min && val <= max)
                return (gint)val;
        }
        else
            fclose(f);
    }
    return default_value;
}

void state_settings_init(void)
{
    setting_loop_at_end = load("state.loop", 0, 1, 0);
    setting_random_order = load("state.random", 0, 1, 0);
    setting_repeat_track = load("state.repeat", 0, 1, 0);

    playlist_seek(load("state.list_position", -1, playlist_length()-1, -1));
    gint pos = load("state.track_position", 0, G_MAXINT, 0);

    gint playing = load("state.playing", 0, 2, MUSIC_STOPPED);

    if(playing == MUSIC_PLAYING)
        music_play();
    else if(playing == MUSIC_PAUSED)
    {
        music_pause();
        music_stream_time = pos;
    }
}

void state_settings_destroy(void)
{
    save("state.loop", setting_loop_at_end ? 1 : 0);
    save("state.random", setting_random_order ? 1 : 0);
    save("state.repeat", setting_repeat_track ? 1 : 0);
    save("state.playing", music_playing);
    save("state.list_position", playlist_position());
    save("state.track_position", music_position());
}
