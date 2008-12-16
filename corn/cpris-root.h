// CPRIS: the Corn Media Player Interfacing Specification
//
// AKA stuff that should be in MPRIS but isn't yet

#ifndef __corn_cpris_root_h__
#define __corn_cpris_root_h__

#include <glib-object.h>

typedef struct _CprisRoot { GObject parent; } CprisRoot;
typedef struct _CprisRootClass { GObjectClass parent; } CprisRootClass;

GType cpris_root_get_type(void);

gboolean cpris_root_clear(CprisRoot * obj, GError ** error);
gboolean cpris_root_play_track(CprisRoot * obj, gint track, GError ** error);
gboolean cpris_root_move(CprisRoot * obj, gint from, gint to, GError ** error);

#endif
