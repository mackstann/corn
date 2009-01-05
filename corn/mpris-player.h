#ifndef __corn_mpris_player_h__
#define __corn_mpris_player_h__

#include "playlist.h"
#include "music.h"
#include "state-settings.h"

#include <glib-object.h>
#include <dbus/dbus-glib.h>

typedef struct _MprisPlayer { GObject parent; DBusGProxy * proxy; } MprisPlayer;
typedef struct _MprisPlayerClass { GObjectClass parent; } MprisPlayerClass;

GType mpris_player_get_type(void);

#define CAP_NONE                 (0)
#define CAP_CAN_GO_NEXT          (1 << 0)
#define CAP_CAN_GO_PREV          (1 << 1)
#define CAP_CAN_PAUSE            (1 << 2)
#define CAP_CAN_PLAY             (1 << 3)
#define CAP_CAN_SEEK             (1 << 4)
#define CAP_CAN_PROVIDE_METADATA (1 << 5)
#define CAP_CAN_HAS_TRACKLIST    (1 << 6)

#define CAP_CURRENTLY_CAN_GO_NEXT() (!playlist_empty() && (setting_loop_at_end || playlist_position() < playlist_length()-1))
#define CAP_CURRENTLY_CAN_GO_PREV() (!playlist_empty() && (setting_loop_at_end || playlist_position() > 0))
#define CAP_CURRENTLY_CAN_PAUSE() (!playlist_empty())
#define CAP_CURRENTLY_CAN_PLAY() (!playlist_empty())
#define CAP_CURRENTLY_CAN_SEEK() (!playlist_empty() && music_stream && xine_get_stream_info(music_stream, XINE_STREAM_INFO_SEEKABLE))
#define CAP_CURRENTLY_CAN_PROVIDE_METADATA() (!playlist_empty())

#define CAP_CURRENT_ALL() ( \
        (CAP_CURRENTLY_CAN_GO_NEXT() ? CAP_CAN_GO_NEXT : 0) | \
        (CAP_CURRENTLY_CAN_GO_PREV() ? CAP_CAN_GO_PREV : 0) | \
        (CAP_CURRENTLY_CAN_PAUSE() ? CAP_CAN_PAUSE : 0) | \
        (CAP_CURRENTLY_CAN_PLAY() ? CAP_CAN_PLAY : 0) | \
        (CAP_CURRENTLY_CAN_SEEK() ? CAP_CAN_SEEK : 0) | \
        (CAP_CURRENTLY_CAN_PROVIDE_METADATA() ? CAP_CAN_PROVIDE_METADATA : 0) | \
        CAP_CAN_HAS_TRACKLIST \
    )

gboolean mpris_player_next (MprisPlayer * obj, GError ** error);
gboolean mpris_player_prev (MprisPlayer * obj, GError ** error);
gboolean mpris_player_pause(MprisPlayer * obj, GError ** error);
gboolean mpris_player_stop (MprisPlayer * obj, GError ** error);
gboolean mpris_player_play (MprisPlayer * obj, GError ** error);

gboolean mpris_player_get_caps    (MprisPlayer * obj, gint * caps,        GError ** error);
gboolean mpris_player_volume_set  (MprisPlayer * obj, gint vol,           GError ** error);
gboolean mpris_player_volume_get  (MprisPlayer * obj, gint * vol,         GError ** error);
gboolean mpris_player_position_set(MprisPlayer * obj, gint ms,            GError ** error);
gboolean mpris_player_position_get(MprisPlayer * obj, gint * ms,          GError ** error);
gboolean mpris_player_repeat      (MprisPlayer * obj, gboolean on,        GError ** error);
gboolean mpris_player_get_metadata(MprisPlayer * obj, GHashTable ** meta, GError ** error);
gboolean mpris_player_get_status  (MprisPlayer * obj, GValue ** status,   GError ** error);

gboolean mpris_player_emit_caps_change  (MprisPlayer * obj);
gboolean mpris_player_emit_track_change (MprisPlayer * obj);
gboolean mpris_player_emit_status_change(MprisPlayer * obj);

extern gint mpris_player_capabilities;

#endif
