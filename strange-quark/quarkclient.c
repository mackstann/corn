#undef  G_LOG_DOMAIN
#define G_LOG_DOMAIN "QuarkClient"

#include "config.h"

#include "gettext.h"
#include "quarkclient.h"
#include "marshalers.h"

#include <sys/types.h> /* for getpid() */
#include <unistd.h>    

#include <errno.h>     /* for open()/close()/stat()/strerror() */
#include <sys/types.h> /* for ^ and also for kill() */
#include <sys/stat.h>
#include <unistd.h>    /* for ^ and also for usleep() */
#include <fcntl.h>
#include <string.h>
#include <signal.h>    /* for kill() */
#include <stdlib.h>    /* for strtol() */

enum {
    CLOSED,
    PLAY_STATE_CHANGED,
    PLAYLIST_POSITION_CHANGED,
    SONG_FAILED,
    TRACK_ADDED,
    TRACK_REMOVED,
    TRACK_MOVED,
    QUARKCLIENT_NUM_SIGNALS
};

static guint quarkclient_signals[QUARKCLIENT_NUM_SIGNALS] = { 0 };

static void     quarkclient_class_init (QuarkClientClass *klass);
static void     quarkclient_init       (QuarkClient *ti);
static void     quarkclient_finalize   (GObject *object);
static gboolean quarkclient_event      (GIOChannel *source,
                                        GIOCondition cond,
                                        gpointer data);

GType
quarkclient_get_type ()
{
    static GType type = 0;
    if (!type) { /* cache it */
        static const GTypeInfo info = {
            sizeof(QuarkClientClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc) quarkclient_class_init,
            NULL, /* class_finalize */ 
            NULL, /* class_data */
            sizeof(QuarkClient),
            0, /* n_preallocs */
            (GInstanceInitFunc) quarkclient_init,
        };
        type = g_type_register_static (G_TYPE_OBJECT, "QuarkClient", &info, 0);
    }
    return type;
}

static void
quarkclient_class_init (QuarkClientClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    klass->closed = NULL;
    klass->play_state_changed = NULL;
    klass->playlist_position_changed = NULL;
    klass->track_added = NULL;
    klass->track_removed = NULL;
    klass->track_moved = NULL;

    quarkclient_signals[CLOSED] =
        g_signal_new ("closed", G_TYPE_FROM_CLASS(klass),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (QuarkClientClass, closed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);
    quarkclient_signals[PLAY_STATE_CHANGED] =
        g_signal_new ("play-state-changed", G_TYPE_FROM_CLASS(klass),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (QuarkClientClass, play_state_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__ENUM,
                      G_TYPE_NONE,
                      1,
                      QUARKCLIENT_PLAY_STATE_TYPE |
                      G_SIGNAL_TYPE_STATIC_SCOPE);
    quarkclient_signals[PLAYLIST_POSITION_CHANGED] =
        g_signal_new ("playlist-position-changed", G_TYPE_FROM_CLASS(klass),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (QuarkClientClass,
                                       playlist_position_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__INT,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_INT | G_SIGNAL_TYPE_STATIC_SCOPE);
    quarkclient_signals[SONG_FAILED] =
        g_signal_new ("song-failed", G_TYPE_FROM_CLASS(klass),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (QuarkClientClass, failed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);
    quarkclient_signals[TRACK_ADDED] =
        g_signal_new ("track-added", G_TYPE_FROM_CLASS(klass),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (QuarkClientClass, track_added),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__INT,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_INT | G_SIGNAL_TYPE_STATIC_SCOPE);
    quarkclient_signals[TRACK_REMOVED] =
        g_signal_new ("track-removed", G_TYPE_FROM_CLASS(klass),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (QuarkClientClass, track_removed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__INT,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_INT | G_SIGNAL_TYPE_STATIC_SCOPE);
    quarkclient_signals[TRACK_MOVED] =
        g_signal_new ("track-moved", G_TYPE_FROM_CLASS(klass),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (QuarkClientClass, track_moved),
                      NULL, NULL,
                      quark_cclosure_marshal_VOID__INT_INT,
                      G_TYPE_NONE,
                      2,
                      G_TYPE_INT | G_SIGNAL_TYPE_STATIC_SCOPE,
                      G_TYPE_INT | G_SIGNAL_TYPE_STATIC_SCOPE);

    gobject_class->finalize = quarkclient_finalize;
}

static void
quarkclient_error_handler (QuarkClient *qc, GError *error, gpointer user_data)
{
    g_warning ("%s", error->message);
    g_error_free (error);
}

static void
quarkclient_error(QuarkClient *qc, GError *qcerror, GError **error)
{
    g_assert ((error == NULL) || (*error == NULL));
    if (error) *error = qcerror;
    else       qc->error_func (qc, qcerror, qc->error_user_data);
}

static void
quarkclient_init (QuarkClient *qc)
{
    gchar *dir;

    /* make the directory we use in ~ */
    dir = g_build_filename (g_get_home_dir(), ".quark", NULL);
    mkdir (dir, S_IRWXU|S_IRWXG|S_IRWXO);
    g_free (dir);

    qc->input_path = NULL;
    qc->output_path =
        g_build_filename (g_get_home_dir(), ".quark", "fifo", NULL);

    qc->error_func = quarkclient_error_handler;

    qc->input = qc->output = NULL;

    qc->quark_pid = 0;
}

static void
quarkclient_finalize (GObject *object)
{
    QuarkClient *qc = QUARKCLIENT (object);
    /* kill off the backend if we're the ones who started it */
    if (qc->quark_pid) kill (qc->quark_pid, SIGTERM);
    if (qc->input) g_io_channel_unref (qc->input);
    if (qc->output) g_io_channel_unref (qc->output);
    g_free(qc->input_path);
    g_free(qc->output_path);
}

QuarkClient *
quarkclient_new()
{
    return g_object_new (QUARKCLIENT_TYPE, NULL);
}

static gboolean
quarkclient_write (QuarkClient *qc, const gchar *str, GError **error)
{
    GIOStatus s;
    GError *qcerror = NULL;
    gsize written;
    gchar *sendstr;

    sendstr = g_strdup_printf ("%s\n", str);

    do {
        s = g_io_channel_write_chars (qc->output, sendstr, -1,
                                      &written, &qcerror);
        str += written;
    } while (s == G_IO_STATUS_AGAIN && qcerror == NULL);

    g_free (sendstr);

    if (qcerror) {
        quarkclient_error (qc, qcerror, error);
        quarkclient_close (qc);
    }

    g_io_channel_flush (qc->output, NULL);

    return qcerror == NULL;
}

static gboolean
quarkclient_verify_fifo (QuarkClient *qc,
                         const gchar *path,
                         gint fd,
                         GError **error)
{
    struct stat statbuf;

    if (fstat(fd, &statbuf) < 0) {
        GError *e = g_error_new_literal (G_IO_CHANNEL_ERROR,
                                         g_io_channel_error_from_errno(errno),
                                         strerror(errno));
        quarkclient_error (qc, e, error);
        return FALSE;
    }

    if (!S_ISFIFO(statbuf.st_mode)) {
        GError *e = g_error_new(G_IO_CHANNEL_ERROR, G_IO_CHANNEL_ERROR_PIPE,
                                _("%s is not a FIFO.\n"
                                  "If you have specified the "
                                  "path to the FIFO, please ensure it is "
                                  "actually a FIFO.\n"
                                  "If you have not specified a path, then "
                                  "this is likely a bug in the program. "
                                  "Remove the $HOME/.quark directory, restart "
                                  "the Quark backend, and try again. If this "
                                  "persists, contact the maintainer."), path);
        quarkclient_error (qc, e, error);
        return FALSE;
    }
    return TRUE;
}

static GIOChannel *
quarkclient_open_output (QuarkClient *qc, GError **error)
{
    int fd;
    GIOChannel *chan;

    /* if it fails to open the FIFO, then it will try launch the backend, and
       try open the FIFO again if the launch succeeds */

    if ((fd = open (qc->output_path, O_WRONLY | O_NONBLOCK)) < 0) {
        GError *e = NULL;
        /* XXX pass it options for where to place the fifo if we we're passed
           any! */
        char *argv[] = { "quark", NULL };
        if (!g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                            NULL, NULL, &qc->quark_pid, &e)) {
            char *msg = e->message;
            e->message = g_strdup_printf (_("Unable to launch the Quark "
                                            "backend. Quark was probably "
                                            "installed incorrectly. Check "
                                            "that \"quark\" binary is in your "
                                            "path and is executable by your "
                                            "user. The error given was: %s"),
                                            msg);
            g_free (msg);
            quarkclient_error (qc, e, error);
            return NULL;
        } else {
            /* give it a chance to make the fifo.. */
            gulong time = 0;
            while (time < G_USEC_PER_SEC * 10) {
                if ((fd = open (qc->output_path, O_WRONLY | O_NONBLOCK)) >= 0)
                    break;
                usleep (G_USEC_PER_SEC / 4);
                time += G_USEC_PER_SEC / 4;
            }
        }
        if (fd < 0) {
            GError *e = g_error_new (G_IO_CHANNEL_ERROR,
                                     g_io_channel_error_from_errno(errno),
                                     _("Unable to connect to a running Quark. "
                                       "The error given was '%s'. Run the "
                                       "Quark backend and try again."),
                                     strerror(errno));
            quarkclient_error (qc, e, error);
            return NULL;
        }
    }

    if (!quarkclient_verify_fifo (qc, qc->output_path, fd, error)) {
        close (fd);
        return NULL;
    }

    chan = g_io_channel_unix_new (fd);
    g_io_channel_set_close_on_unref (chan, TRUE);

    return chan;
}

static GIOChannel *
quarkclient_open_input (QuarkClient *qc, GError **error)
{
    int fd;
    GIOChannel *chan;
    gchar *s;

    if ((fd = open (qc->input_path, O_RDONLY | O_NONBLOCK)) < 0) {
        if (mkfifo (qc->input_path, S_IRWXU | S_IRWXG | S_IRWXO) < 0) {
            GError *e = g_error_new (G_IO_CHANNEL_ERROR,
                                     g_io_channel_error_from_errno(errno),
                                     _("Unable to create a FIFO to communicate"
                                     " with the Quark backend. The error "
                                     "given was '%s'"),
                                     strerror(errno));
            quarkclient_error (qc, e, error);
            return NULL;
        } else {
            fd = open (qc->input_path, O_RDONLY | O_NONBLOCK);
        }
    }

    if (fd < 0) {
        GError *e = g_error_new (G_IO_CHANNEL_ERROR,
                                 g_io_channel_error_from_errno(errno),
                                 _("Unable to open client FIFO to communicate "
                                   "with the Quark backend. Try ensuring that "
                                   "your permissions are correct. The error "
                                   "given was '%s'"),
                                 strerror(errno));
        quarkclient_error (qc, e, error);
        return NULL;
    }

    if (!quarkclient_verify_fifo (qc, qc->input_path, fd, error)) {
        close (fd);
        return NULL;
    }

    chan = g_io_channel_unix_new (fd);
    g_io_channel_set_close_on_unref (chan, TRUE);

    s = g_strdup_printf("connect %s", qc->input_path);
    g_message("%s", s);
    if (!quarkclient_write (qc, s, error)) {
        g_io_channel_unref (chan);
        chan = NULL;
    }
    g_free(s);

    return chan;
}

gboolean
quarkclient_open (QuarkClient *qc, const gchar *appname, GError **error)
{
    gchar *pidname;

    g_return_val_if_fail (qc != NULL, FALSE);
    g_return_val_if_fail ((error == NULL) || (*error == NULL), FALSE);

    if (qc->input && qc->output) return TRUE;

    if (appname) {
        pidname = g_strdup_printf("%s.%u", appname, getpid());
        qc->input_path = g_build_filename (g_get_home_dir(), ".quark",
                                           pidname, NULL);
        g_free(pidname);
    }

    g_return_val_if_fail (qc->input_path != NULL, FALSE);

    qc->output = quarkclient_open_output (qc, error);
    if (!qc->output) return FALSE;
    g_io_add_watch (qc->output, G_IO_ERR | G_IO_HUP, quarkclient_event, qc);

    qc->input = quarkclient_open_input (qc, error);
    if (!qc->input) {
        quarkclient_close (qc);
        return FALSE;
    }
    g_io_add_watch (qc->input, G_IO_ERR | G_IO_HUP | G_IO_IN,
                    quarkclient_event, qc);

    return TRUE;
}

void
quarkclient_close (QuarkClient *qc)
{
    gboolean wasopen = qc->input && qc->output;

    g_return_if_fail (qc != NULL);

    if (qc->input) {
        g_io_channel_unref (qc->input);
        qc->input = NULL;
        unlink (qc->input_path);
    }
    if (qc->output) {
        g_io_channel_unref (qc->output);
        qc->output = NULL;
    }

    if (wasopen)
        g_signal_emit (qc, quarkclient_signals[CLOSED], 0);
}

gboolean
quarkclient_is_open (QuarkClient *qc)
{
    g_return_val_if_fail (qc != NULL, FALSE);

    return qc->input && qc->output;
}

static gboolean
quarkclient_event (GIOChannel *source, GIOCondition cond, gpointer data)
{
    QuarkClient *qc = QUARKCLIENT (data);

    if (cond & G_IO_HUP || cond & G_IO_ERR) {
        quarkclient_close (qc);
        return FALSE;
    }

    if (source == qc->input && cond & G_IO_IN) {
        gchar *line;
        
        while (g_io_channel_read_line (qc->input, &line, NULL, NULL, NULL)
               == G_IO_STATUS_NORMAL)
        {
            gchar *data = NULL;
            gchar **lines;
            gchar **it;
            gboolean start = FALSE;
            gboolean read_act = FALSE;

            start = !strcmp (line, "STARTMESSAGE\n");
            g_free(line);
            if (!start)
                continue;

            while (!read_act &&
                   g_io_channel_read_line (qc->input, &line,
                                           NULL, NULL, NULL)
                   == G_IO_STATUS_NORMAL)
            {
                if (!strcmp (line, "ENDMESSAGE\n")) {
                    read_act = TRUE;
                } else {
                    gchar *s;
                    if (data)
                        s = g_strconcat(data, line, NULL);
                    else
                        s = g_strdup(line);
                    g_free(data);
                    data = s;
                }
                g_free(line);
            }

            if (!read_act) {
                g_free(data);
                continue;
            }

            it = lines = g_strsplit (data, "\n", -1);
            g_free(data);

            if (!strcmp (*it, "CURRENT")) {
                gint pos;
                gchar *end;

                if (!*++it) goto malformed_message;
                pos = strtol(*it, &end, 10);
                if (end == *it || *end != '\0') goto malformed_message;

                g_signal_emit
                    (qc, quarkclient_signals[PLAYLIST_POSITION_CHANGED],
                     0, pos);

                read_act = TRUE;
            } else if (!strcmp (*it, "ADD")) {
                QuarkClientTrack *t;
                gchar *path;
                gint pos;
                gchar *end;

                if (!*++it) goto malformed_message;
                path = *it;
                if (!*++it) goto malformed_message;
                pos = strtol(*it, &end, 10);
                if (end == *it || *end != '\0') goto malformed_message;

                t = g_new(QuarkClientTrack, 1);
                t->path = g_strdup(path);

                qc->playlist = g_slist_insert(qc->playlist, t, pos);

                g_signal_emit (qc, quarkclient_signals[TRACK_ADDED], 0, pos);

                read_act = TRUE;
            } else if (!strcmp (*it, "REMOVE")) {
                QuarkClientTrack *t;
                GSList *sit;
                gint pos;
                gchar *end;

                if (!*++it) goto malformed_message;
                pos = strtol(*it, &end, 10);
                if (end == *it || *end != '\0') goto malformed_message;

                sit = g_slist_nth(qc->playlist, pos);
                if (sit) {
                    t = sit->data;

                    qc->playlist = g_slist_delete_link(qc->playlist, sit);

                    g_signal_emit
                        (qc, quarkclient_signals[TRACK_REMOVED], 0, pos);

                    g_free(t->path);
                    g_free(t);
                }

                read_act = TRUE;
            } else if (!strcmp (*it, "MOVE")) {
                QuarkClientTrack *t;
                GSList *sit;
                gint from, to;
                gchar *end;

                if (!*++it) goto malformed_message;
                from = strtol(*it, &end, 10);
                if (end == *it || *end != '\0') goto malformed_message;
                if (!*++it) goto malformed_message;
                to = strtol(*it, &end, 10);
                if (end == *it || *end != '\0') goto malformed_message;

                sit = g_slist_nth(qc->playlist, from);
                if (sit) {
                    t = sit->data;
                    qc->playlist = g_slist_insert(qc->playlist, t, to);
                    qc->playlist = g_slist_delete_link(qc->playlist, sit);

                    g_signal_emit
                        (qc, quarkclient_signals[TRACK_MOVED], 0, from, to);
                }

                read_act = TRUE;
            } else if (!strcmp (*it, "FAILED")) {
                g_signal_emit
                    (qc, quarkclient_signals[SONG_FAILED], 0);

                read_act = TRUE;
            } else if (!strcmp (*it, "PLAYING")) {
                g_signal_emit
                    (qc, quarkclient_signals[PLAY_STATE_CHANGED], 0,
                     QUARKCLIENT_STATE_PLAYING);

                read_act = TRUE;
            } else if (!strcmp (*it, "PAUSED")) {
                g_signal_emit
                    (qc, quarkclient_signals[PLAY_STATE_CHANGED], 0,
                     QUARKCLIENT_STATE_PAUSED);

                read_act = TRUE;
            } else if (!strcmp (*it, "STOPPED")) {
                g_signal_emit
                    (qc, quarkclient_signals[PLAY_STATE_CHANGED], 0,
                     QUARKCLIENT_STATE_STOPPED);

                read_act = TRUE;
            }
            g_strfreev(lines);
            continue;
        malformed_message:
            g_strfreev(lines);
            g_warning("Malformed message");
        }
    }

    return TRUE;
}

static guint
quarkclient_track_position (QuarkClient *qc, QuarkClientTrack *track)
{
    return g_slist_position (qc->playlist, g_slist_find (qc->playlist, track));
}

gboolean
quarkclient_play (QuarkClient *qc, GError **error)
{
    g_return_val_if_fail (qc != NULL, FALSE);
    g_return_val_if_fail ((error == NULL) || (*error == NULL), FALSE);

    if (!quarkclient_open (qc, NULL, error)) return FALSE;

    return quarkclient_write (qc, "play", error);
}

gboolean
quarkclient_pause (QuarkClient *qc, GError **error)
{
    g_return_val_if_fail (qc != NULL, FALSE);
    g_return_val_if_fail ((error == NULL) || (*error == NULL), FALSE);

    if (!quarkclient_open (qc, NULL, error)) return FALSE;

    return quarkclient_write (qc, "pause", error);
}

gboolean
quarkclient_stop (QuarkClient *qc, GError **error)
{
    g_return_val_if_fail (qc != NULL, FALSE);
    g_return_val_if_fail ((error == NULL) || (*error == NULL), FALSE);

    if (!quarkclient_open (qc, NULL, error)) return FALSE;

    return quarkclient_write (qc, "stop", error);
}

gboolean
quarkclient_next_track (QuarkClient *qc, GError **error)
{
    g_return_val_if_fail (qc != NULL, FALSE);
    g_return_val_if_fail ((error == NULL) || (*error == NULL), FALSE);

    if (!quarkclient_open (qc, NULL, error)) return FALSE;

    return quarkclient_write (qc, "next", error);
}

gboolean
quarkclient_previous_track (QuarkClient *qc, GError **error)
{
    g_return_val_if_fail (qc != NULL, FALSE);
    g_return_val_if_fail ((error == NULL) || (*error == NULL), FALSE);

    if (!quarkclient_open (qc, NULL, error)) return FALSE;

    return quarkclient_write (qc, "prev", error);
}

gboolean
quarkclient_skip_to_track (QuarkClient *qc, QuarkClientTrack *track,
                           GError **error)
{
    gchar *str;
    gboolean r;

    g_return_val_if_fail (qc != NULL, FALSE);
    g_return_val_if_fail (track != NULL, FALSE);
    g_return_val_if_fail ((error == NULL) || (*error == NULL), FALSE);

    str = g_strdup_printf ("track %d", quarkclient_track_position (qc, track));
    r = quarkclient_write (qc, str, error);
    g_free (str);
    return r;
}

gboolean
quarkclient_clear (QuarkClient *qc, GError **error)
{
    g_return_val_if_fail (qc != NULL, FALSE);
    g_return_val_if_fail ((error == NULL) || (*error == NULL), FALSE);

    if (!quarkclient_open (qc, NULL, error)) return FALSE;

    return quarkclient_write (qc, "clear", error);
}

gboolean
quarkclient_add_track (QuarkClient *qc, const gchar *path, GError **error)
{
    gchar *str, *utf;
    gboolean r;
    GError *e = NULL;

    g_return_val_if_fail (qc != NULL, FALSE);
    g_return_val_if_fail (path != NULL, FALSE);
    g_return_val_if_fail ((error == NULL) || (*error == NULL), FALSE);

    if (!quarkclient_open (qc, NULL, error)) return FALSE;

    utf = g_filename_to_utf8 (path, -1, NULL, NULL, &e);
    if (utf) {
        str = g_strdup_printf ("append %s", utf);
        r = quarkclient_write (qc, str, error);
        g_free (str);
    } else {
        quarkclient_error (qc, e, error);
        r = FALSE;
    }
    return r;
}

gboolean
quarkclient_add_tracks (QuarkClient *qc, gchar *const*paths, GError **error)
{
    gchar *str, *utf;
    gboolean r = TRUE;
    GError *e = NULL;
    guint i;

    g_return_val_if_fail (qc != NULL, FALSE);
    g_return_val_if_fail (paths != NULL, FALSE);
    g_return_val_if_fail ((error == NULL) || (*error == NULL), FALSE);

    if (!quarkclient_open (qc, NULL, error)) return FALSE;

    for (i = 0; paths[i]; ++i) {
        utf = g_filename_to_utf8 (paths[i], -1, NULL, NULL, &e);
        if (utf) {
            str = g_strdup_printf ("append %s", utf);
            r = quarkclient_write (qc, str, error);
            g_free (str);
        } else {
            quarkclient_error (qc, e, error);
            r = FALSE;
        }
    }
    return r;
}

gboolean
quarkclient_remove_track(QuarkClient *qc, QuarkClientTrack *track,
                         GError **error)
{
    gchar *str;
    gboolean r;

    g_return_val_if_fail (qc != NULL, FALSE);
    g_return_val_if_fail (track != NULL, FALSE);
    g_return_val_if_fail ((error == NULL) || (*error == NULL), FALSE);

    if (!quarkclient_open (qc, NULL, error)) return FALSE;

    str = g_strdup_printf ("remove %d",
                           quarkclient_track_position (qc, track));
    r = quarkclient_write (qc, str, error);
    g_free (str);
    return r;
}

gboolean
quarkclient_reorder_track (QuarkClient *qc, QuarkClientTrack *track,
                           QuarkClientTrack *before, GError **error)
{
    gchar *str;
    gboolean r;

    g_return_val_if_fail (qc != NULL, FALSE);
    g_return_val_if_fail (track != NULL, FALSE);
    g_return_val_if_fail (before != NULL, FALSE);
    g_return_val_if_fail ((error == NULL) || (*error == NULL), FALSE);

    if (!quarkclient_open (qc, NULL, error)) return FALSE;

    str = g_strdup_printf ("move %d %d",
                           quarkclient_track_position (qc, track),
                           quarkclient_track_position (qc, before));
    r = quarkclient_write (qc, str, error);
    g_free (str);
    return r;
}

GSList*
quarkclient_playlist (QuarkClient *qc)
{
    g_return_val_if_fail (qc != NULL, NULL);
    return qc->playlist;
}

QuarkClientTrack*
quarkclient_playlist_nth (QuarkClient *qc, gint pos)
{
    g_return_val_if_fail (qc != NULL, NULL);
    return g_slist_nth_data (qc->playlist, pos);
}


QuarkClientErrorFunc
quarkclient_get_default_error_handler (QuarkClient *qc)
{
    g_return_val_if_fail (qc != NULL, NULL);

    return qc->error_func;
}

void
quarkclient_set_default_error_handler (QuarkClient *qc,
                                       QuarkClientErrorFunc func,
                                       gpointer user_data)
{
    g_return_if_fail (qc != NULL);

    if (func)
        qc->error_func = func;
    else
        qc->error_func = quarkclient_error_handler;
    qc->error_user_data = user_data;
}


static QuarkClientTrack *
quarkclient_track_copy (const QuarkClientTrack *track)
{
  QuarkClientTrack *result = g_new (QuarkClientTrack, 1);
  result->path = g_strdup (track->path);
  return result;
}

void
quarkclient_track_free (QuarkClientTrack *track)
{
    g_free (track->path);
    g_free (track);
}

GType
quarkclient_track_get_type ()
{
    static GType type = 0;
    if (!type) { /* cache it */
        type = g_boxed_type_register_static
            ("QuarkClientTrack",
             (GBoxedCopyFunc) quarkclient_track_copy,
             (GBoxedFreeFunc) quarkclient_track_free);
    }
    return type;
}

const gchar*
quarkclient_track_path (QuarkClientTrack *track)
{
    g_return_val_if_fail (track != NULL, NULL);

    return track->path;
}

GType quarkclient_play_state_get_type ()
{
    static GType t = 0;
    if (t == 0) {
        static const GFlagsValue values[] = {
            { QUARKCLIENT_STATE_PLAYING,
              "QUARKCLIENT_STATE_PLAYING", "playing" },
            { QUARKCLIENT_STATE_PAUSED,
              "QUARKCLIENT_STATE_PAUSED", "paused" },
            { QUARKCLIENT_STATE_STOPPED,
              "QUARKCLIENT_STATE_STOPPED", "stopped" },
            { 0, NULL, NULL }
        };
        t = g_flags_register_static ("QuarkClientPlayState", values);
    }
    return t;
}
