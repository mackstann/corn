#ifndef __corn_mpris_tracklist_h__
#define __corn_mpris_tracklist_h__

#include <glib-object.h>
#include <glib.h>

typedef struct _MprisTrackList { GObject parent; } MprisTrackList;
typedef struct _MprisTrackListClass { GObjectClass parent; } MprisTrackListClass;

GType mpris_tracklist_get_type(void);

gboolean mpris_tracklist_del_track(MprisTrackList * obj, gint track, GError ** error);
gboolean mpris_tracklist_add_track(MprisTrackList * obj, const gchar * uri,
                                   gboolean playnow, gint * failed,
                                   GError ** error);

gboolean mpris_tracklist_get_length       (MprisTrackList * obj, gint * len, GError ** error);
gboolean mpris_tracklist_get_current_track(MprisTrackList * obj, gint * track, GError ** error);
gboolean mpris_tracklist_set_loop         (MprisTrackList * obj, gboolean on, GError ** error);
gboolean mpris_tracklist_set_random       (MprisTrackList * obj, gboolean on, GError ** error);
gboolean mpris_tracklist_get_metadata     (MprisTrackList * obj, gint track, GHashTable ** meta, GError ** error);

#endif
