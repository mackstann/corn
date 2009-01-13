#include "gettext.h"
#include "music.h"
#include "music-metadata.h"
#include "main.h"
#include "playlist.h"
#include "state-settings.h"
#include "mpris-player.h"
#include "db.h"
#include "dbus.h"

#include "mpris-tracklist.h"

#include <glib.h>
#include <glib-object.h>

guint track_list_change_signal;

G_DEFINE_TYPE(MprisTrackList, mpris_tracklist, G_TYPE_OBJECT)

static void mpris_tracklist_init(MprisTrackList * obj)
{
}

static void mpris_tracklist_class_init(MprisTrackListClass * klass)
{
    track_list_change_signal =
        g_signal_new("track_list_change",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__BOXED,
                     G_TYPE_NONE, 1, G_TYPE_INT);
}

gboolean mpris_tracklist_del_track(MprisTrackList * obj, gint track, GError ** error)
{
    playlist_remove(track);
    return TRUE;
}

gboolean mpris_tracklist_add_track(MprisTrackList * obj, const gchar * uri,
                                   gboolean playnow, gint * failed, GError ** error)
{
    gchar * u;
    if(!(u = g_filename_to_utf8(uri, -1, NULL, NULL, NULL)))
    {
        g_warning(_("Skipping '%s'. Could not convert to UTF-8. "
                    "See the README for a possible solution."), uri);
        *failed = 1;
    }
    else
    {
        playlist_append(u);
        if(playnow)
            playlist_seek(-1);
        *failed = 0;
    }
    return TRUE;
}

gboolean mpris_tracklist_get_length(MprisTrackList * obj, gint * len, GError ** error)
{
    *len = playlist_length();
    return TRUE;
}

gboolean mpris_tracklist_get_current_track(MprisTrackList * obj, gint * track, GError ** error)
{
    // will be -1 when list is empty, but that's ok because the spec says
    // behavior is undefined when calling this on an empty tracklist.
    *track = playlist_position();
    return TRUE;
}

gboolean mpris_tracklist_set_loop(MprisTrackList * obj, gboolean on, GError ** error)
{
    setting_loop_at_end = on;
    mpris_player_emit_status_change(mpris_player);
    return TRUE;
}

gboolean mpris_tracklist_set_random(MprisTrackList * obj, gboolean on, GError ** error)
{
    setting_random_order = on;
    mpris_player_emit_status_change(mpris_player);
    return TRUE;
}

gboolean mpris_tracklist_get_metadata(MprisTrackList * obj, gint track, GHashTable ** meta, GError ** error)
{
    *meta = db_get(playlist_nth(track));
    return TRUE;
}

void mpris_tracklist_emit_track_list_change(MprisTrackList * obj)
{
    g_signal_emit(obj, track_list_change_signal, 0, playlist_length());
}
