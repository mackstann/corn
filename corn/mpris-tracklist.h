#ifndef __brunt_mpris_tracklist_h__
#define __brunt_mpris_tracklist_h__

#include <glib-object.h>

#define BRUNT_TYPE_MPRIS_TRACKLIST                  (mpris_tracklist_get_type ())
#define BRUNT_MPRIS_TRACKLIST(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRUNT_TYPE_MPRIS_TRACKLIST, MprisTrackList))
#define BRUNT_IS_MPRIS_TRACKLIST(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRUNT_TYPE_MPRIS_TRACKLIST))
#define BRUNT_MPRIS_TRACKLIST_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), BRUNT_TYPE_MPRIS_TRACKLIST, MprisTrackListClass))
#define BRUNT_IS_MPRIS_TRACKLIST_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), BRUNT_TYPE_MPRIS_TRACKLIST))
#define BRUNT_MPRIS_TRACKLIST_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), BRUNT_TYPE_MPRIS_TRACKLIST, MprisTrackListClass))

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

#endif

