#include "config.h"

#include "gettext.h"

#include "music.h"
#include "playlist.h"
#include "configuration.h"
#include "parsefile.h"

#include <glib/gstdio.h>
#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

gboolean config_loop_at_end  = FALSE;
gboolean config_random_order = FALSE;
gboolean config_repeat_track = FALSE;

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
    } else
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

void config_load(void)
{
    gchar * playlist_filename = g_build_filename(g_get_user_config_dir(), PACKAGE, "playlist", NULL);
    parse_m3u(playlist_filename);
    g_free(playlist_filename);

    config_loop_at_end =  read_int_from_config_file("state.loop",   0, 1, 0);
    config_random_order = read_int_from_config_file("state.random", 0, 1, 0);
    config_repeat_track = read_int_from_config_file("state.repeat", 0, 1, 0);

    playlist_seek(read_int_from_config_file("state.list_position", 0, G_MAXINT, 0));

    gint playing = read_int_from_config_file("state.playing", 0, 2, MUSIC_STOPPED);
    if(playing != MUSIC_STOPPED)
    {
        music_play();
        if(playing == MUSIC_PAUSED)
            music_pause(); // not sure if this works
    }
}

void config_save(void)
{
    save_int_to_config_file("state.loop", config_loop_at_end ? 1 : 0);
    save_int_to_config_file("state.random", config_random_order ? 1 : 0);
    save_int_to_config_file("state.repeat", config_repeat_track ? 1 : 0);
    save_int_to_config_file("state.playing", music_playing);
    save_int_to_config_file("state.list_position", g_list_position(playlist, playlist_current));

    FILE * f = open_config_file("playlist", "w");
    if(f)
    {
        for(GList * it = playlist; it; it = g_list_next(it))
        {
            fputs(MAIN_PATH(it->data), f);
            fputc('\n', f);
        }
        fclose(f);
    }
}

