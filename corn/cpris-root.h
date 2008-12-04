// CPRIS: the Corn Media Player Interfacing Specification
//
// AKA stuff that should be in MPRIS but isn't yet

#ifndef __corn_cpris_root_h__
#define __corn_cpris_root_h__

#include <glib-object.h>

#define CORN_TYPE_CPRIS_ROOT                  (cpris_root_get_type ())
#define CORN_CPRIS_ROOT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CORN_TYPE_CPRIS_ROOT, CprisRoot))
#define CORN_IS_CPRIS_ROOT(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CORN_TYPE_CPRIS_ROOT))
#define CORN_CPRIS_ROOT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CORN_TYPE_CPRIS_ROOT, CprisRootClass))
#define CORN_IS_CPRIS_ROOT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CORN_TYPE_CPRIS_ROOT))
#define CORN_CPRIS_ROOT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CORN_TYPE_CPRIS_ROOT, CprisRootClass))

typedef struct _CprisRoot CprisRoot;
typedef struct _CprisRootClass CprisRootClass;

struct _CprisRoot
{
    GObject parent;
};

struct _CprisRootClass
{
    GObjectClass parent;
};

GType cpris_root_get_type (void);

gboolean cpris_root_clear(CprisRoot * obj, GError ** error);
gboolean cpris_root_play_track(CprisRoot * obj, gint track, GError ** error);
gboolean cpris_root_move(CprisRoot * obj, gint from, gint to, GError ** error);

#endif

