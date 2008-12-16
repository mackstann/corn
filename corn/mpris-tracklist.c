#include "gettext.h"
#include "music.h"
#include "music-metadata.h"
#include "main.h"
#include "playlist.h"
#include "configuration.h"

#include "mpris-tracklist.h"

#include <glib.h>
#include <glib-object.h>

G_DEFINE_TYPE(MprisTrackList, mpris_tracklist, G_TYPE_OBJECT)

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

gboolean mpris_tracklist_add_track(MprisTrackList * obj, const gchar * uri,
                              gboolean playnow, gint * failed, GError ** error)
{
    gchar *u;
    if (!(u = g_filename_to_utf8 (uri, -1, NULL, NULL, NULL))) {
        g_warning (_("Skipping '%s'. Could not convert to UTF-8. "
                    "See the README for a possible solution."), uri);
        *failed = 1;
    } else {
        playlist_append(u, NULL);
        g_free (u);
        if(playnow)
            playlist_seek(playlist->len - 1);
        *failed = 0;
    }
    return TRUE;
}

gboolean mpris_tracklist_get_length
(MprisTrackList * obj, gint * len, GError ** error)
{
    *len = playlist->len;
    return TRUE;
}

gboolean mpris_tracklist_get_current_track
(MprisTrackList * obj, gint * track, GError ** error)
{
    // will be -1 when list is empty, but that's ok because the spec says
    // behavior is undefined when calling this on an empty tracklist.
    *track = playlist_position;
    return TRUE;
}

gboolean mpris_tracklist_set_loop
(MprisTrackList * obj, gboolean on, GError ** error)
{
    config_loop_at_end = on;
    return TRUE;
}

gboolean mpris_tracklist_set_random
(MprisTrackList * obj, gboolean on, GError ** error)
{
    config_random_order = on;
    return TRUE;
}

gboolean mpris_tracklist_get_metadata
(MprisTrackList * obj, gint track, GHashTable ** meta, GError ** error)
{
    *meta = music_get_track_metadata(track);
    return TRUE;
}

