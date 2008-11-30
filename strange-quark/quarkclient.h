#ifndef __fifo_h
#define __fifo_h

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define QUARKCLIENT_TYPE            (quarkclient_get_type ())
#define QUARKCLIENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), QUARKCLIENT_TYPE, QuarkClient))
#define QUARKCLIENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), QUARKCLIENT_TYPE, QuarkClient))
#define IS_QUARKCLIENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), QUARKCLIENT_TYPE))
#define IS_QUARKCLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), QUARKCLIENT_TYPE))

typedef struct _QuarkClient      QuarkClient;
typedef struct _QuarkClientClass QuarkClientClass;
typedef struct _QuarkClientTrack QuarkClientTrack;

typedef void (*QuarkClientErrorFunc)(QuarkClient *client,
                                     GError *error,
                                     gpointer user_data);

#define QUARKCLIENT_PLAY_STATE_TYPE  (quarkclient_play_state_get_type ())

GType quarkclient_play_state_get_type ();

typedef enum
{
    QUARKCLIENT_STATE_PLAYING,
    QUARKCLIENT_STATE_PAUSED,
    QUARKCLIENT_STATE_STOPPED
} QuarkClientPlayState;

struct _QuarkClient {
    GObject               object;

    gchar                *input_path;
    gchar                *output_path;

    GIOChannel           *input;  /* receive data from quark */
    GIOChannel           *output; /* send data to quark */

    GSList               *playlist;

    QuarkClientErrorFunc  error_func;
    gpointer              error_user_data;

    gint                  quark_pid;
};

struct _QuarkClientClass {
    GObjectClass parent_class;

    void (* closed) (QuarkClient *qc); /* connection to the backend closed */

    void (* play_state_changed) (QuarkClient *qc, QuarkClientPlayState state);

    void (* failed) (QuarkClient *qc);

    void (* playlist_position_changed) (QuarkClient *qc, gint pos);
    void (* track_added) (QuarkClient *qc, gint track);
    void (* track_removed) (QuarkClient *qc, gint track);
    void (* track_moved) (QuarkClient *qc, gint from, gint to);
};

#define QUARKCLIENT_TYPE_TRACK (quarkclient_track_get_type ())

struct _QuarkClientTrack {
    gchar *path;
};

GType             quarkclient_get_type       ();

QuarkClient*      quarkclient_new            ();

gboolean          quarkclient_open           (QuarkClient *qc,
                                              const gchar *appname,
                                              GError **error);
void              quarkclient_close          (QuarkClient *qc);

gboolean          quarkclient_is_open        (QuarkClient *qc);

gboolean          quarkclient_play           (QuarkClient *qc, GError **error);
gboolean          quarkclient_pause          (QuarkClient *qc, GError **error);
gboolean          quarkclient_stop           (QuarkClient *qc, GError **error);
gboolean          quarkclient_next_track     (QuarkClient *qc, GError **error);
gboolean          quarkclient_previous_track (QuarkClient *qc, GError **error);
gboolean          quarkclient_skip_to_track  (QuarkClient *qc,
                                              QuarkClientTrack *track,
                                              GError **error);

gboolean          quarkclient_clear          (QuarkClient *qc, GError **error);
gboolean          quarkclient_add_track      (QuarkClient *qc,
                                              const gchar *path,
                                              GError **error);
gboolean          quarkclient_add_tracks     (QuarkClient *qc,
                                              gchar *const*paths,
                                              GError **error);
gboolean          quarkclient_remove_track   (QuarkClient *qc,
                                              QuarkClientTrack *track,
                                              GError **error);
gboolean          quarkclient_reorder_track  (QuarkClient *qc,
                                              QuarkClientTrack *track,
                                              QuarkClientTrack *before,
                                              GError **error);

GSList*           quarkclient_playlist       (QuarkClient *qc);
QuarkClientTrack* quarkclient_playlist_nth   (QuarkClient *qc, gint pos);

QuarkClientErrorFunc quarkclient_get_default_error_handler (QuarkClient *qc);
void                 quarkclient_set_default_error_handler (QuarkClient *qc,
                                                            QuarkClientErrorFunc func,
                                                            gpointer user_data);


GType        quarkclient_track_get_type ();

const gchar* quarkclient_track_path     (QuarkClientTrack *track);

G_END_DECLS

#endif
