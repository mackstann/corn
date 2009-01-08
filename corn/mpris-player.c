#include "main.h"
#include "dbus.h"
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

// much thanks to audacious developers -- some code is inherited from them.

#define CAP_NONE                 (0)
#define CAP_CAN_GO_NEXT          (1 << 0)
#define CAP_CAN_GO_PREV          (1 << 1)
#define CAP_CAN_PAUSE            (1 << 2)
#define CAP_CAN_PLAY             (1 << 3)
#define CAP_CAN_SEEK             (1 << 4)
#define CAP_CAN_PROVIDE_METADATA (1 << 5)
#define CAP_CAN_HAS_TRACKLIST    (1 << 6)

#define DBUS_TYPE_G_STRING_VALUE_HASHTABLE (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))

#define DBUS_STRUCT_INT_INT_INT_INT (dbus_g_type_get_struct("GValueArray", G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID))

guint track_change_signal;
guint status_change_signal;
guint caps_change_signal;

static gint capabilities;

G_DEFINE_TYPE(MprisPlayer, mpris_player, G_TYPE_OBJECT)

static void mpris_player_init(MprisPlayer * obj)
{
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
                     G_TYPE_NONE, 1, DBUS_STRUCT_INT_INT_INT_INT);
}

static gint current_capabilities(void)
{
    gint caps = CAP_CAN_HAS_TRACKLIST;
    if(!playlist_empty())
    {
        caps |= CAP_CAN_PAUSE | CAP_CAN_PLAY | CAP_CAN_PROVIDE_METADATA |
                CAP_CAN_GO_NEXT | CAP_CAN_GO_PREV;
        if(!setting_loop_at_end)
        {
            if(playlist_position() >= playlist_length()-1)
                caps &= ~CAP_CAN_GO_NEXT;
            else if(playlist_position() < 0)
                caps &= ~CAP_CAN_GO_PREV;
        }
        if(music_stream && xine_get_stream_info(music_stream, XINE_STREAM_INFO_SEEKABLE))
            caps |= CAP_CAN_SEEK;
    }
    return caps;
}

static gboolean refresh_capabilities(void)
{
    gint newcaps = current_capabilities();
    if(newcaps != capabilities)
    {
        capabilities = newcaps;
        return TRUE;
    }
    return FALSE;
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
    *caps = capabilities;
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
    mpris_player_emit_status_change(mpris_player);
    return TRUE;
}

gboolean mpris_player_get_metadata(MprisPlayer * obj, GHashTable ** meta, GError ** error)
{
    *meta = music_get_current_track_metadata();
    return TRUE;
}

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
    if(main_status == CORN_RUNNING && refresh_capabilities())
        g_signal_emit(obj, caps_change_signal, 0, capabilities);
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
