#include "gettext.h"

#include "music.h"
#include "playlist.h"
#include "configuration.h"

#include <gconf/gconf-client.h>
#include <glib.h>

gboolean config_loop_at_end  = FALSE;
gboolean config_random_order = FALSE;
gboolean config_repeat_track = FALSE;

#define LOOP_PLAYLIST     CORN_GCONF_ROOT "/loop_playlist"
#define RANDOM_ORDER      CORN_GCONF_ROOT "/random_order"
#define REPEAT_TRACK      CORN_GCONF_ROOT "/repeat_track"
#define PLAYING           CORN_GCONF_ROOT "/playing"
#define PAUSED            CORN_GCONF_ROOT "/paused"
#define PLAYLIST_POSITION CORN_GCONF_ROOT "/playlist/position"
#define PLAYLIST          CORN_GCONF_ROOT "/playlist/playlist"

static GConfClient * gconf;

void config_load(void)
{
    gconf = gconf_client_get_default();
    gconf_client_add_dir(gconf, CORN_GCONF_ROOT, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);

    GSList * paths, * it;

    config_loop_at_end = gconf_client_get_bool(gconf, LOOP_PLAYLIST, NULL);
    config_random_order = gconf_client_get_bool(gconf, RANDOM_ORDER, NULL);
    config_repeat_track = gconf_client_get_bool(gconf, REPEAT_TRACK, NULL);

    paths = gconf_client_get_list(gconf, PLAYLIST, GCONF_VALUE_STRING, NULL);
    for(it = paths; it; it = g_slist_next(it))
    {
        // consolidate with code in playlist.c
        GError * e = NULL;
        gchar * p = g_filename_from_utf8(it->data, -1, NULL, NULL, &e);
        if(p)
        {
            playlist_append_single(p);
            g_free(p);
        }
        if(e)
        {
            g_warning("%s (%s).", _("Error loading playlist"), e->message);
            g_error_free(e);
        }
        g_free(it->data);
    }
    g_slist_free(paths);

    /* don't need another copy of the playlist in memory, and
       gconf_client_clear_cache makes a nice segfault when I try save stuff
       later. This value can't be edited while corn is running anyways.
    */
    gconf_client_unset(gconf, PLAYLIST, NULL);

    playlist_seek(gconf_client_get_int(gconf, PLAYLIST_POSITION, NULL));

    if(gconf_client_get_bool(gconf, PLAYING, NULL) &&
      !gconf_client_get_bool(gconf, PAUSED, NULL))
    {
        music_play();
        // should handle all 3 states
    }
}

void config_save(void)
{
    GList * it;
    GSList * paths = NULL;

    gconf_client_set_bool(gconf, LOOP_PLAYLIST, config_loop_at_end, NULL);
    gconf_client_set_bool(gconf, RANDOM_ORDER, config_random_order, NULL);
    gconf_client_set_bool(gconf, REPEAT_TRACK, config_repeat_track, NULL);
    gconf_client_set_bool(gconf, PLAYING, music_playing == MUSIC_PLAYING ? TRUE : FALSE, NULL);
    gconf_client_set_bool(gconf, PAUSED, music_playing == MUSIC_PAUSED ? TRUE : FALSE, NULL);

    gconf_client_set_int(gconf, PLAYLIST_POSITION,
                          g_list_position(playlist, playlist_current), NULL);

    for(it = playlist; it; it = g_list_next(it))
        paths = g_slist_append(paths, MAIN_PATH(it->data));

    gconf_client_set_list(gconf, PLAYLIST, GCONF_VALUE_STRING, paths, NULL);
    g_slist_free(paths);

    gconf_client_suggest_sync(gconf, NULL);
    g_object_unref(G_OBJECT(gconf));
}

