#include "main.h"
#include "music-control.h"
#include "music-metadata.h"
#include "music.h"
#include "playlist.h"
#include "state-settings.h"

#include "mpris-player.h"

#include <dbus/dbus-glib.h>
#include <glib.h>
#include <glib-object.h>

#include <string.h>

gint mpris_player_capabilities = 0;

// much thanks to audacious developers -- some code is inherited from them.

guint track_change_signal;
guint status_change_signal;
guint caps_change_signal;

G_DEFINE_TYPE(MprisPlayer, mpris_player, G_TYPE_OBJECT)

#define DBUS_TYPE_G_STRING_VALUE_HASHTABLE (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))

static void mpris_player_init(MprisPlayer * obj)
{
    // set caps!
    //CAN_GO_NEXT | CAN_GO_PREV | CAN_PAUSE | CAN_PLAY;
    //CAN_SEEK
    //CAN_PROVIDE_METADATA
    //CAN_HAS_TRACKLIST
}

static void mpris_player_class_init(MprisPlayerClass * klass)
{
    caps_change_signal =
        g_signal_new("caps_change",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__INT,
                     G_TYPE_NONE, 1, G_TYPE_INT);
    track_change_signal =
        g_signal_new("track_change",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__BOXED,
                     G_TYPE_NONE, 1, DBUS_TYPE_G_STRING_VALUE_HASHTABLE);
    status_change_signal =
        g_signal_new("status_change",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__INT,
                     G_TYPE_NONE, 1, G_TYPE_INT);
}

// example for calling these on the command line:
// $ dbus-send --session --type=method_call --dest=org.mpris.corn <backslash>
//        /Player org.freedesktop.MediaPlayer.Next

gboolean mpris_player_next(MprisPlayer * obj, GError ** error)
{
    playlist_advance(1);
    return TRUE;
}

gboolean mpris_player_prev(MprisPlayer * obj, GError ** error)
{
    playlist_advance(-1);
    return TRUE;
}

gboolean mpris_player_pause(MprisPlayer * obj, GError ** error)
{
    if(music_playing == MUSIC_PLAYING)
        music_pause();
    else if(music_playing == MUSIC_PAUSED)
        music_play();
    return TRUE;
}

gboolean mpris_player_stop(MprisPlayer * obj, GError ** error)
{
    music_stop();
    return TRUE;
}

gboolean mpris_player_play(MprisPlayer * obj, GError ** error)
{
    music_play();
    return TRUE;
}

gboolean mpris_player_get_caps(MprisPlayer * obj, gint * caps, GError ** error)
{
    *caps = CAN_GO_NEXT | CAN_GO_PREV | CAN_PAUSE | CAN_PLAY;
    //CAN_SEEK
    //CAN_PROVIDE_METADATA
    //CAN_HAS_TRACKLIST
    return TRUE;
}

gboolean mpris_player_volume_set(MprisPlayer * obj, gint vol, GError ** error)
{
    music_set_volume(vol);
    return TRUE;
}

gboolean mpris_player_volume_get(MprisPlayer * obj, gint * vol, GError ** error)
{
    *vol = music_volume;
    return TRUE;
}

gboolean mpris_player_position_set(MprisPlayer * obj, gint ms, GError ** error)
{
    music_seek(ms);
    return TRUE;
}

gboolean mpris_player_position_get(MprisPlayer * obj, gint * ms, GError ** error)
{
    *ms = music_position();
    return TRUE;
}

gboolean mpris_player_repeat(MprisPlayer * obj, gboolean on, GError ** error)
{
    setting_repeat_track = on;
    return TRUE;
}

gboolean mpris_player_get_metadata(MprisPlayer * obj, GHashTable ** meta, GError ** error)
{
    *meta = music_get_current_track_metadata();
    return TRUE;
}

#define DBUS_STRUCT_INT_INT_INT_INT (dbus_g_type_get_struct("GValueArray", G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID))

gpointer get_status_struct(void)
{
    static GValue value;
    memset(&value, 0, sizeof(GValue));
    g_value_init(&value, DBUS_STRUCT_INT_INT_INT_INT);
    g_value_take_boxed(&value, dbus_g_type_specialized_construct(DBUS_STRUCT_INT_INT_INT_INT));
    dbus_g_type_struct_set(&value, 0, music_playing, 1,
                           setting_random_order ? 1 : 0, 2,
                           setting_repeat_track ? 1 : 0, 3,
                           setting_loop_at_end ? 1 : 0, G_MAXUINT);
    return g_value_get_boxed(&value);
}

gboolean mpris_player_get_status(MprisPlayer * obj, GValue ** status, GError ** error)
{
    *status = get_status_struct();
    return TRUE;
}

gboolean mpris_player_emit_caps_change(MprisPlayer * obj)
{
    g_signal_emit(obj, caps_change_signal, 0, mpris_player_capabilities);
    return TRUE;
}

gboolean mpris_player_emit_track_change(MprisPlayer * obj)
{
    g_return_val_if_fail(playlist_position() != -1, TRUE);
    GHashTable * meta = music_get_current_track_metadata();
    g_signal_emit(obj, track_change_signal, 0, meta);
    g_hash_table_destroy(meta);
    return TRUE;
}

gboolean mpris_player_emit_status_change(MprisPlayer * obj)
{
    g_signal_emit(obj, status_change_signal, 0, get_status_struct());
    return TRUE;
}
