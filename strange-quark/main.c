#include "config.h"

#include "gettext.h"
#include <locale.h>

#include "applet.h"
#include <gtk/gtk.h>
#include <signal.h>

static void signal_handler(int signal)
{
    switch (signal) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
        gtk_main_quit();
        break;
    }
}

int main(int argc, char **argv)
{
    struct sigaction action;
    sigset_t sigset;

    /* initialize the locale */
    if (!setlocale(LC_ALL, ""))
	g_warning("Couldn't set locale from environment.\n");
    bindtextdomain(PACKAGE_NAME, LOCALEDIR);
    bind_textdomain_codeset(PACKAGE_NAME, "UTF-8");
    textdomain(PACKAGE_NAME);

    /* set up signal handler */
    sigemptyset(&sigset);
    action.sa_handler = signal_handler;
    action.sa_mask = sigset;
    action.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGHUP, &action, (struct sigaction *) NULL);
    sigaction(SIGTERM, &action, (struct sigaction *) NULL);
    sigaction(SIGINT, &action, (struct sigaction *) NULL);

    gtk_init(&argc, &argv);

    applet_startup();

    gtk_main();

    applet_shutdown();

    return 0;
}
