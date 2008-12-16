#ifndef __corn_mpris_player_h__
#define __corn_mpris_player_h__

#include <glib-object.h>
#include <dbus/dbus-glib.h>

typedef struct _MprisPlayer { GObject parent; DBusGProxy * proxy; } MprisPlayer;
typedef struct _MprisPlayerClass { GObjectClass parent; } MprisPlayerClass;

GType mpris_player_get_type(void);

enum
{
    NONE = 0,
    CAN_GO_NEXT = 1 << 0,
    CAN_GO_PREV = 1 << 1,
    CAN_PAUSE = 1 << 2,
    CAN_PLAY = 1 << 3,
    CAN_SEEK = 1 << 4,
    CAN_PROVIDE_METADATA = 1 << 5,
    CAN_HAS_TRACKLIST = 1 << 6
} MprisPlayerCapabilities;

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
