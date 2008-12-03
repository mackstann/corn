#include "main.h"
#include "playlist.h"

#include "mpris-tracklist.h"

#include <glib.h>
#include <glib-object.h>

G_DEFINE_TYPE(MprisTrackList, mpris_tracklist, G_TYPE_OBJECT);

static void
mpris_tracklist_init(MprisTrackList * obj)
{
}

static void
mpris_tracklist_class_init(MprisTrackListClass * klass)
{
}

gboolean mpris_tracklist_del_track(MprisTrackList * obj, gint track, GError ** error)
{
    playlist_remove(track);
    return TRUE;
}

