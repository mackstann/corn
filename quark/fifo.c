#include "config.h"

#include "gettext.h"
#include "main.h"
#include "music.h"
#include "playlist.h"
#include "fifo.h"

#include <json-glib/json-glib.h>
#include <glib-object.h>
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
fifo_execute (JsonObject *command);

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
    gsize len;
    GError *err = NULL, * parse_err = NULL;
    gboolean ret;

    g_static_mutex_lock (&main_fifo_mutex);

    if (cond & G_IO_IN) {
        while (g_io_channel_read_line (chan, &command, NULL, &len, &err) ==
               G_IO_STATUS_NORMAL) {
            command[len] = '\0';
            JsonParser * parser = json_parser_new();
            if(!json_parser_load_from_data(parser, command, len, &parse_err))
            {
                g_warning ("error parsing command: command: %s", command);
                g_warning ("error parsing command: error: %s", parse_err->message);
                g_error_free (parse_err);
            }
            else
            {
                JsonNode * root = json_parser_get_root(parser);
                if(JSON_NODE_TYPE(root) != JSON_NODE_OBJECT)
                    g_warning ("command is not an object: %s", command);
                else
                {
                    JsonObject * obj = json_node_get_object(root);
                    if(!json_object_has_member(obj, "command"))
                        g_warning("command has no \"command\" member: %s", command);
                    else
                        fifo_execute(obj);
                }
            }
            //fifo_execute (command);
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
fifo_execute (JsonObject *command)
{
    const gchar * commandstr = json_node_get_string(json_object_get_member(command, "command"));

    if (!g_strcmp0 (commandstr, "quit"))
        main_quit ();
    else if (!g_strcmp0 (commandstr, "play"))
        music_play ();
    else if (!g_strcmp0 (commandstr, "pause"))
        if (music_playing)
            music_pause ();
        else
            music_play ();
    else if (!g_strcmp0 (commandstr, "stop"))
        music_stop ();
    else if (!g_strcmp0 (commandstr, "next")) {
        playlist_advance (1, TRUE);
    }
    else if (!g_strcmp0 (commandstr, "prev")) {
        playlist_advance (-1, TRUE);
    }
    else if (!g_strcmp0 (commandstr, "append")) {
        if(!json_object_has_member(command, "path"))
            g_warning("append command has no \"path\" member");
        else
        {
            const gchar * path = json_node_get_string(json_object_get_member(command, "path"));
            gchar *u;
            if (!(u = g_filename_to_utf8 (path, -1, NULL, NULL, NULL))) {
                g_warning (_("Skipping '%s'. Could not convert to UTF-8. "
                             "See the README for a possible solution."), path);
            } else {
                playlist_append_single (u);
                g_free (u);
            }
        }
    }
    else if (!g_strcmp0 (commandstr, "track")) {
        if(!json_object_has_member(command, "track"))
            g_warning("track command has no \"track\" member");
        else
        {
            gint track = json_node_get_int(json_object_get_member(command, "track"));
            playlist_seek (track-1); /* first track is '1' */
        }
    }
    else if (!g_strcmp0 (commandstr, "remove")) {
        if(!json_object_has_member(command, "track"))
            g_warning("remove command has no \"track\" member");
        else
        {
            gint track = json_node_get_int(json_object_get_member(command, "track"));
            playlist_remove (track-1); /* first track is '1' */
        }
    }
    else if (!g_strcmp0 (commandstr, "clear")) {
        playlist_clear ();
    }
    else if (!g_strcmp0 (commandstr, "move")) {
        if(!json_object_has_member(command, "from"))
            g_warning("move command has no \"from\" member");
        else
        {
            gint from = json_node_get_int(json_object_get_member(command, "from"));
            if(!json_object_has_member(command, "to"))
                g_warning("move command has no \"to\" member");
            else
            {
                gint to = json_node_get_int(json_object_get_member(command, "to"));
                playlist_move (to - 1, from - 1); /* first track is '1' */
            }
        }
    }
    else if (!g_strcmp0 (commandstr, "loop")) {
        main_set_loop_at_end(!main_loop_at_end);
    }
    else if (!g_strcmp0 (commandstr, "random")) {
        main_set_random_order(!main_random_order);
    }
    else if (!g_strcmp0 (commandstr, "dump")) {
        playlist_dump ();
    }
    else if (!g_strcmp0 (commandstr, "connect")) {
        if(!json_object_has_member(command, "filename"))
            g_warning("connect command has no \"filename\" member");
        else
        {
            const gchar * filename = json_node_get_string(json_object_get_member(command, "filename"));
            fifo_add_notify (filename);
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
