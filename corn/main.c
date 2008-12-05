#include "config.h"

#include "gettext.h"
#include <locale.h>

#include "dbus.h"
#include "music.h"
#include "playlist.h"
#include "main.h"

#include <libgnomevfs/gnome-vfs.h>
#include <glib.h>
#include <gconf/gconf-client.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>

gboolean main_loop_at_end  = FALSE; /* loop at the end of the playlist */
gboolean main_random_order = FALSE; /* play tracks in random order */
gboolean main_repeat_track = FALSE; /* repeat same track continuously */

CornStatus main_status;

GStaticMutex main_mutex        = G_STATIC_MUTEX_INIT;
GStaticMutex main_signal_mutex = G_STATIC_MUTEX_INIT;

static GMainLoop * loop;

#define CORN_GCONF_ROOT          "/apps/corn"
#define CORN_GCONF_ROOT_PLAYLIST "/apps/corn/playlist"

#define LOOP_PLAYLIST CORN_GCONF_ROOT "/loop_playlist"
#define RANDOM_ORDER  CORN_GCONF_ROOT "/random_order"
#define REPEAT_TRACK  CORN_GCONF_ROOT "/repeat_track"
#define PLAYING       CORN_GCONF_ROOT "/playing"
#define PAUSED        CORN_GCONF_ROOT "/paused"

#define PLAYLIST_POSITION CORN_GCONF_ROOT_PLAYLIST "/position"
#define PLAYLIST          CORN_GCONF_ROOT_PLAYLIST "/playlist"

// XXX make config.{c,h}
static void config_load(GConfClient * gconf);
static void config_save(GConfClient * gconf);
static void config_changed(GConfClient * gconf,
        guint cnxn_id, GConfEntry * entry, gpointer data);

void
signal_handler_quit(int signal)
{
    g_static_mutex_lock(&main_signal_mutex);
    main_quit();
    g_static_mutex_unlock(&main_signal_mutex);
}

int main(int argc, char ** argv)
{
    main_status = CORN_STARTING;

    /* initialize the locale */
    if(!setlocale(LC_ALL, ""))
	g_warning("Couldn't set locale from environment.\n");
    bindtextdomain(PACKAGE_NAME, LOCALEDIR);
    bind_textdomain_codeset(PACKAGE_NAME, "UTF-8");
    textdomain(PACKAGE_NAME);

    /* signals */
    sigset_t sigset;
    sigemptyset(&sigset);

    struct sigaction quit_action;
    quit_action.sa_handler = signal_handler_quit;
    quit_action.sa_mask = sigset;
    quit_action.sa_flags = SA_NOCLDSTOP;

    struct sigaction ignore_action;
    ignore_action.sa_handler = SIG_IGN;
    ignore_action.sa_mask = sigset;
    ignore_action.sa_flags = SA_NOCLDSTOP;

    sigaction(SIGTERM, &quit_action, (struct sigaction *)NULL);
    sigaction(SIGINT, &quit_action, (struct sigaction *)NULL);
    sigaction(SIGHUP, &ignore_action, (struct sigaction *)NULL);

    g_type_init();
    gnome_vfs_init();

    GConfClient * gconf = gconf_client_get_default();
    gconf_client_add_dir(gconf, CORN_GCONF_ROOT, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
    gconf_client_notify_add(gconf, CORN_GCONF_ROOT, config_changed, NULL, NULL, NULL);

    /* make sure the config directory exists */
    const gchar * dir = g_build_filename(g_get_user_config_dir(), PACKAGE, NULL);
    g_mkdir_with_parents(dir, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);

    loop = g_main_loop_new(NULL, FALSE);

    music_init();
    if(!mpris_init())
    {
        g_critical(_("Failed to initialize D-BUS."));
        return 2;
    }

    g_static_mutex_lock(&main_mutex);
    playlist_init();
    config_load(gconf);
    main_status = CORN_RUNNING;
    g_static_mutex_unlock(&main_mutex);

    g_message("ready");
    g_main_loop_run(loop);

    g_static_mutex_lock(&main_mutex);
    main_status = CORN_EXITING;
    config_save(gconf);
    playlist_destroy();
    g_static_mutex_unlock(&main_mutex);

    music_destroy();
    mpris_destroy();

    g_main_loop_unref(loop);
    g_object_unref(G_OBJECT(gconf));

    gnome_vfs_shutdown();

    return 0;
}

void main_quit()
{
    g_main_loop_quit(loop);
}

static void config_load(GConfClient * gconf)
{
    GSList * paths, * it;

    main_loop_at_end = gconf_client_get_bool(gconf, LOOP_PLAYLIST, NULL);
    main_random_order = gconf_client_get_bool(gconf, RANDOM_ORDER, NULL);
    main_repeat_track = gconf_client_get_bool(gconf, REPEAT_TRACK, NULL);

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
            g_warning("Error loading playlist: %s", e->message);
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
        music_play(); // should probably set a flag instead
}

static void
config_save(GConfClient * gconf)
{
    GList * it;
    GSList * paths = NULL;

    gconf_client_set_bool(gconf, LOOP_PLAYLIST, main_loop_at_end, NULL);
    gconf_client_set_bool(gconf, RANDOM_ORDER, main_random_order, NULL);
    gconf_client_set_bool(gconf, REPEAT_TRACK, main_repeat_track, NULL);
    gconf_client_set_bool(gconf, PLAYING, music_playing == MUSIC_PLAYING ? TRUE : FALSE, NULL);
    gconf_client_set_bool(gconf, PAUSED, music_playing == MUSIC_PAUSED ? TRUE : FALSE, NULL);

    gconf_client_set_int(gconf, PLAYLIST_POSITION,
                          g_list_position(playlist, playlist_current), NULL);

    for(it = playlist; it; it = g_list_next(it))
        paths = g_slist_append(paths, MAIN_PATH(it->data));

    gconf_client_set_list(gconf, PLAYLIST, GCONF_VALUE_STRING, paths, NULL);
    g_slist_free(paths);

    gconf_client_suggest_sync(gconf, NULL);
}

static void config_changed(GConfClient * gconf,
        guint cnxn_id, GConfEntry * entry, gpointer data)
{
    if(!strcmp(entry->key, LOOP_PLAYLIST))
        main_loop_at_end = gconf_value_get_bool(entry->value);
    else if(!strcmp(entry->key, RANDOM_ORDER))
        main_random_order = gconf_value_get_bool(entry->value);
    else if(!strcmp(entry->key, REPEAT_TRACK))
        main_repeat_track = gconf_value_get_bool(entry->value);
}

