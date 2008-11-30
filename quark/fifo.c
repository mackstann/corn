#include "config.h"

#include "gettext.h"
#include "main.h"
#include "music.h"
#include "playlist.h"
#include "fifo.h"

#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define FIFO_NAME "fifo"

static GIOChannel *fifo_chan = NULL;

static gboolean
fifo_read (GIOChannel *chan, GIOCondition cond, gpointer data);
static void
fifo_execute (const char *command);

gboolean
fifo_open ()
{
    int fifo_fd;
    char *path;

    path = g_build_filename (g_get_home_dir (), ".quark", FIFO_NAME, NULL);

    if ((fifo_fd = open (path, O_RDONLY|O_NONBLOCK)) == -1) {
        if (mkfifo (path, S_IRWXU|S_IRWXG|S_IRWXO) == -1) {
            g_free (path);
            return FALSE;
        }
        if ((fifo_fd = open (path, O_RDONLY|O_NONBLOCK)) == -1) {
            return FALSE;
            g_free (path);
        }
    }
    g_free (path);
    fifo_chan = g_io_channel_unix_new (fifo_fd);
    g_io_channel_set_close_on_unref (fifo_chan, TRUE);
    g_io_channel_set_encoding (fifo_chan, NULL, NULL);
    g_io_add_watch (fifo_chan, G_IO_IN|G_IO_HUP|G_IO_ERR, fifo_read, NULL);
    return TRUE;
}

void
fifo_close ()
{
    if (fifo_chan) {
        g_io_channel_unref (fifo_chan);
    }
}

void
fifo_destroy()
{
    char *path;

    fifo_close ();

    path = g_build_filename (g_get_home_dir (), ".quark", FIFO_NAME, NULL);
    unlink (path);
    g_free (path);
}

static gboolean
fifo_read (GIOChannel *chan, GIOCondition cond, gpointer data)
{
    char *command;
    gsize term;
    GError *err;
    gboolean ret;

    g_static_mutex_lock (&main_fifo_mutex);

    err = NULL;

    if (cond & G_IO_IN) {
        while (g_io_channel_read_line (chan, &command, NULL, &term, &err) ==
               G_IO_STATUS_NORMAL) {
            command[term] = '\0';
            fifo_execute (command);
            g_free (command);
        }
        if (err) {
            g_warning ("error reading fifo: %s", err->message);
            g_error_free (err);
        }
    }
    
    if (cond & G_IO_HUP || cond & G_IO_ERR) {
        fifo_close ();
        if (!fifo_open ()) {
            g_critical ("failed to reopen fifo");
            main_quit ();
        }
        ret = FALSE; /* remove me from the watch */
    } else
        ret = TRUE;

    g_static_mutex_unlock (&main_fifo_mutex);

    return ret;
}

static void
fifo_execute (const char *command)
{
    if (!g_ascii_strncasecmp (command, "quit", 4))
        main_quit ();
    else if (!g_ascii_strncasecmp (command, "play", 4))
        music_play ();
    else if (!g_ascii_strncasecmp (command, "pause", 5))
        if (music_playing)
            music_pause ();
        else
            music_play ();
    else if (!g_ascii_strncasecmp (command, "stop", 4))
        music_stop ();
    else if (!g_ascii_strncasecmp (command, "next", 4)) {
        playlist_advance (1, TRUE);
    }
    else if (!g_ascii_strncasecmp (command, "prev", 4)) {
        playlist_advance (-1, TRUE);
    }
    else if (!g_ascii_strncasecmp (command, "append", 6)) {
        if (strlen (command) > 7) {
            gchar *u;
            const gchar *path;

            path = (command + 7);
            if (!(u = g_filename_to_utf8 (path, -1, NULL, NULL, NULL))) {
                g_warning (_("Skipping '%s'. Could not convert to UTF-8. "
                             "See the README for a possible solution."), path);
            } else {
                playlist_append_single (u);
                g_free (u);
            }
        }
    }
    else if (!g_ascii_strncasecmp (command, "track", 5)) {
        if (strlen (command) > 6) {
            int i = atoi (command + 6);
            playlist_seek (i - 1); /* first track is '1' */
        }
    }
    else if (!g_ascii_strncasecmp (command, "remove", 6)) {
        if (strlen (command) > 7) {
            int i = atoi (command + 7);
            playlist_remove (i - 1); /* first track is '1' */
        }
    }
    else if (!g_ascii_strncasecmp (command, "clear", 5)) {
        playlist_clear ();
    }
    else if (!g_ascii_strncasecmp (command, "move", 4)) {
        if (strlen (command) > 5) {
            char *p;
            int i = strtol (command + 5, &p, 10);
            if (strlen (command) > (unsigned)(p - command)) {
                int before = atoi (p);
                playlist_move (i - 1, before - 1); /* first track is '1' */
            }
        }
    }
    else if (!g_ascii_strncasecmp (command, "loop", 4)) {
        main_set_loop_at_end(!main_loop_at_end);
    }
    else if (!g_ascii_strncasecmp (command, "random", 6)) {
        main_set_random_order(!main_random_order);
    }
    else if (!g_ascii_strncasecmp (command, "dump", 4)) {
        playlist_dump ();
    }
    else if (!g_ascii_strncasecmp (command, "connect", 7)) {
        if (strlen (command) > 8) {
            fifo_add_notify (command + 8);
        }
    }

}

/**
 A create a per client notification FIFO.

 To be notified of status changes, a client must create a FIFO, then send a
 "connect <filename>" message to quark. quark can then open the supplied fifo
 for reading.
*/
void
fifo_add_notify (const char *filename)
{
    struct stat statbuf;
    GIOChannel *channel;
    
    int fifo_fd = open (filename, O_WRONLY | O_NONBLOCK);
    if (fifo_fd < 0) {
        g_warning ("Unable to open client notify FIFO %s", filename);
        return;
    }

    if (fstat (fifo_fd, &statbuf) < 0) {
        g_warning ("Unable to stat client notify FIFO %s:", strerror(errno));
        return;
    }
    
    if (!S_ISFIFO (statbuf.st_mode)) {
        g_warning ("Client notify file %s not a FIFO", filename);
        return;
    }

    g_message ("Connecting to client FIFO %s", filename);
    channel = g_io_channel_unix_new (fifo_fd);
    
    music_add_notify (channel);
}
