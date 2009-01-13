#include "config.h"

#include "gettext.h"
#include <locale.h>

#include "mpris-player.h"
#include "dbus.h"
#include "music.h"
#include "playlist.h"
#include "state-settings.h"
#include "state-playlist.h"
#include "db.h"
#include "main.h"

#include <unique/unique.h>
#include <libgnomevfs/gnome-vfs.h>
#include <glib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

CornStatus main_status;
guint main_time_counter = 0;
gchar * main_instance_name = PACKAGE_NAME;
gchar * main_service_name;

static GMainLoop * loop;

void main_quit() { g_main_loop_quit(loop); }

static void signal_handler_quit(int signal) { main_quit(); }

static gboolean increment_time_counter(gpointer data)
{
    // calling time() or gettimeofday() all the time is kind of wasteful since
    // we don't need the actual time, just relative time.  so we schedule this
    // to be called every second.  it won't be 100% precise but we don't need
    // 100% precision.
    main_time_counter++;
    return TRUE;
}

static gboolean playlist_save_timeout_func(gpointer data)
{
    state_playlist_launch_save_if_time_has_come();
    return TRUE;
}

static void init_locale(void)
{
    if(!setlocale(LC_ALL, ""))
        g_warning("Couldn't set locale from environment.\n");
    bindtextdomain(main_instance_name, LOCALEDIR);
    bind_textdomain_codeset(main_instance_name, "UTF-8");
    textdomain(main_instance_name);
}

static void init_signals(void)
{
    sigset_t sigset;
    sigemptyset(&sigset);

    struct sigaction ignore_action = {
        .sa_handler = SIG_IGN,
        .sa_mask = sigset,
        .sa_flags = SA_NOCLDSTOP
    };

    sigaction(SIGPIPE, &ignore_action, (struct sigaction *)NULL);
    sigaction(SIGHUP, &ignore_action, (struct sigaction *)NULL);

    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGINT);

    struct sigaction quit_action = {
        .sa_handler = signal_handler_quit,
        .sa_mask = sigset,
        .sa_flags = SA_NOCLDSTOP
    };

    sigaction(SIGTERM, &quit_action, (struct sigaction *)NULL);
    sigaction(SIGINT, &quit_action, (struct sigaction *)NULL);

}

int main(int argc, char **argv)
{
    main_status = CORN_STARTING;

    init_locale();
    init_signals();
    g_type_init();
    g_thread_init(NULL);

    if(argc > 1)
    {
        GRegex * instance_pattern = g_regex_new("^[a-zA-Z][a-zA-Z0-9]*$", 0, 0, NULL);
        if(g_regex_match(instance_pattern, argv[1], 0, NULL))
            main_instance_name = argv[1];
        else
        {
            fprintf(stderr, _("instance name must start with a letter "
                              "and only contain alphanumeric characters.\n"));
            return 1;
        }
        g_regex_unref(instance_pattern);
    }

    main_service_name = g_strdup_printf("org.mpris.%s", main_instance_name);

    // hack to work around bug in libunique that segfaults when X display
    // doesn't exist and no startup id is passed in
    gchar * startup_id = g_strdup_printf("%s.%d_TIME%d", g_get_host_name(), getpid(), (int)time(NULL));
    g_setenv("DESKTOP_STARTUP_ID", startup_id, FALSE);

    UniqueApp * app = unique_app_new(main_service_name, startup_id);
    g_free(startup_id);

    if(unique_app_is_running(app))
    {
        fprintf(stderr,
            "An instance of %s named %s is already running.  If you would really\n"
            "like to run two instances at once (for example, to play to two different audio\n"
            "devices), then run %s with a single argument, which will be the instance\n"
            "name, for example:\n"
            "\n"
            "$ %s foo\n"
            "\n"
            "All config data for that instance will be under the name foo (config directory,\n"
            "etc.), it will use that name on D-BUS, and it will not interact with other\n"
            "instances in any way.  You can quit it and re-start it with the same instance\n"
            "name and it will remember everything about itself just like running %s\n"
            "without an instance name normally would.\n",
            PACKAGE_NAME, main_instance_name, PACKAGE_NAME, argv[0], PACKAGE_NAME);
        return 2;
    };

    loop = g_main_loop_new(NULL, FALSE);
    g_timeout_add_seconds_full(G_PRIORITY_HIGH, 1, increment_time_counter, NULL, NULL);
    g_timeout_add_seconds_full(G_PRIORITY_DEFAULT_IDLE, 1, playlist_save_timeout_func, NULL, NULL);

    /* make sure the config/data dirs exist */
    gchar * dir = g_build_filename(g_get_user_config_dir(), main_instance_name, NULL);
    g_mkdir_with_parents(dir, S_IRWXU | S_IRWXG | S_IRWXO);
    g_free(dir);

    dir = g_build_filename(g_get_user_data_dir(), main_instance_name, NULL);
    g_mkdir_with_parents(dir, S_IRWXU | S_IRWXG | S_IRWXO);
    g_free(dir);

    int failed = 0;

    if(!(failed = gnome_vfs_init() ? 0 : 5))
    {
        if(!(failed = db_init()))
        {
            if(!(failed = music_init()))
            {
                if(!(failed = mpris_init()))
                {
                    playlist_init();
                    state_playlist_init();
                    state_settings_init();

                    main_status = CORN_RUNNING;
                    mpris_player_emit_caps_change(mpris_player);
                    g_message("ready");
                    g_main_loop_run(loop);
                    main_status = CORN_EXITING;

                    state_playlist_destroy();
                    state_settings_destroy();
                    playlist_destroy();
                    mpris_destroy();
                }
                music_destroy();
            }
            db_destroy();
        }
        gnome_vfs_shutdown();
    }
    g_main_loop_unref(loop);

    return failed;
}
