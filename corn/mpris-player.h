#ifndef __brunt_mpris_player_h__
#define __brunt_mpris_player_h__

#include <glib-object.h>

#define BRUNT_TYPE_MPRIS_PLAYER                  (mpris_player_get_type ())
#define BRUNT_MPRIS_PLAYER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRUNT_TYPE_MPRIS_PLAYER, MprisPlayer))
#define BRUNT_IS_MPRIS_PLAYER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRUNT_TYPE_MPRIS_PLAYER))
#define BRUNT_MPRIS_PLAYER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), BRUNT_TYPE_MPRIS_PLAYER, MprisPlayerClass))
#define BRUNT_IS_MPRIS_PLAYER_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), BRUNT_TYPE_MPRIS_PLAYER))
#define BRUNT_MPRIS_PLAYER_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), BRUNT_TYPE_MPRIS_PLAYER, MprisPlayerClass))

typedef struct _MprisPlayer MprisPlayer;
typedef struct _MprisPlayerClass MprisPlayerClass;

struct _MprisPlayer
{
    GObject parent;
};

struct _MprisPlayerClass
{
    GObjectClass parent;
};

GType mpris_player_get_type (void);

enum {
    NONE                 = 0,
    CAN_GO_NEXT          = 1 << 0,
    CAN_GO_PREV          = 1 << 1,
    CAN_PAUSE            = 1 << 2,
    CAN_PLAY             = 1 << 3,
    CAN_SEEK             = 1 << 4,
    CAN_PROVIDE_METADATA = 1 << 5,
    CAN_HAS_TRACKLIST    = 1 << 6
} MprisPlayerCapabilities;

gboolean mpris_player_next (MprisPlayer * obj, GError ** error);
gboolean mpris_player_prev (MprisPlayer * obj, GError ** error);
gboolean mpris_player_pause(MprisPlayer * obj, GError ** error);
gboolean mpris_player_stop (MprisPlayer * obj, GError ** error);
gboolean mpris_player_play (MprisPlayer * obj, GError ** error);

gboolean mpris_player_get_caps(MprisPlayer * obj, gint * caps, GError ** error);
gboolean mpris_player_volume_set(MprisPlayer * obj, gint vol, GError ** error);
gboolean mpris_player_volume_get(MprisPlayer * obj, gint * vol, GError ** error);

#endif

