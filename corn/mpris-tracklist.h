#ifndef __corn_mpris_tracklist_h__
#define __corn_mpris_tracklist_h__

#include <glib-object.h>
#include <glib.h>

#define CORN_TYPE_MPRIS_TRACKLIST                  (mpris_tracklist_get_type ())
#define CORN_MPRIS_TRACKLIST(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CORN_TYPE_MPRIS_TRACKLIST, MprisTrackList))
#define CORN_IS_MPRIS_TRACKLIST(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CORN_TYPE_MPRIS_TRACKLIST))
#define CORN_MPRIS_TRACKLIST_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CORN_TYPE_MPRIS_TRACKLIST, MprisTrackListClass))
#define CORN_IS_MPRIS_TRACKLIST_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CORN_TYPE_MPRIS_TRACKLIST))
#define CORN_MPRIS_TRACKLIST_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CORN_TYPE_MPRIS_TRACKLIST, MprisTrackListClass))

typedef struct _MprisTrackList MprisTrackList;
typedef struct _MprisTrackListClass MprisTrackListClass;

struct _MprisTrackList
{
    GObject parent;
};

struct _MprisTrackListClass
{
    GObjectClass parent;
};

GType mpris_tracklist_get_type (void);

gboolean mpris_tracklist_del_track(MprisTrackList * obj, gint track, GError ** error);

gboolean mpris_tracklist_add_track(MprisTrackList * obj, const gchar * uri,
                              gboolean playnow, gint * failed, GError ** error);

gboolean mpris_tracklist_get_length
(MprisTrackList * obj, gint * len, GError ** error);

gboolean mpris_tracklist_get_current_track
(MprisTrackList * obj, gint * track, GError ** error);

gboolean mpris_tracklist_set_loop
(MprisTrackList * obj, gboolean on, GError ** error);

gboolean mpris_tracklist_set_random
(MprisTrackList * obj, gboolean on, GError ** error);

gboolean mpris_tracklist_get_metadata
(MprisTrackList * obj, gint track, GHashTable ** meta, GError ** error);

#endif

