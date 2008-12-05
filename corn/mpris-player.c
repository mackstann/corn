#include "main.h"
#include "music.h"
#include "playlist.h"

#include "mpris-player.h"

#include <dbus/dbus-glib.h>
#include <glib.h>
#include <glib-object.h>

G_DEFINE_TYPE(MprisPlayer, mpris_player, G_TYPE_OBJECT)

static void
mpris_player_init(MprisPlayer * obj)
{
}

static void
mpris_player_class_init(MprisPlayerClass * klass)
{
}

// TODO:
// GetStatus

// example for calling these on the command line:
// $ dbus-send --session --type=method_call --dest=org.mpris.corn <backslash>
//        /Player org.freedesktop.MediaPlayer.Next

gboolean mpris_player_next(MprisPlayer * obj, GError ** error)
{
    playlist_advance(1, TRUE);
    return TRUE;
}

gboolean mpris_player_prev(MprisPlayer * obj, GError ** error)
{
    playlist_advance(-1, TRUE);
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
    *ms = music_get_position();
    return TRUE;
}

gboolean mpris_player_repeat(MprisPlayer * obj, gboolean on, GError ** error)
{
    main_repeat_track = on;
    return TRUE;
}

gboolean mpris_player_get_metadata(MprisPlayer * obj, GHashTable ** meta, GError ** error)
{
    *meta = music_get_metadata();
    return TRUE;
}

#define DBUS_STRUCT_INT_INT_INT_INT (dbus_g_type_get_struct ("GValueArray", G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID))

gboolean mpris_player_get_status(MprisPlayer * obj, GValue ** status, GError ** error)
{
    GValue value = {0};
    g_value_init(&value, DBUS_STRUCT_INT_INT_INT_INT);
    g_value_take_boxed(&value, dbus_g_type_specialized_construct(DBUS_STRUCT_INT_INT_INT_INT));
    // index, value, ... G_MAXUINT at the end
    dbus_g_type_struct_set(&value,
        0, music_playing,
        1, main_random_order ? 1 : 0,
        2, main_repeat_track ? 1 : 0,
        3, main_loop_at_end ? 1 : 0,
        G_MAXUINT
    );
    *status = g_value_get_boxed(&value);
    return TRUE;
}

