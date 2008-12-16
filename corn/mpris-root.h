#ifndef __corn_mpris_root_h__
#define __corn_mpris_root_h__

#include <glib-object.h>

#define CORN_TYPE_MPRIS_ROOT                  (mpris_root_get_type ())
#define CORN_MPRIS_ROOT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CORN_TYPE_MPRIS_ROOT, MprisRoot))
#define CORN_IS_MPRIS_ROOT(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CORN_TYPE_MPRIS_ROOT))
#define CORN_MPRIS_ROOT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CORN_TYPE_MPRIS_ROOT, MprisRootClass))
#define CORN_IS_MPRIS_ROOT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CORN_TYPE_MPRIS_ROOT))
#define CORN_MPRIS_ROOT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CORN_TYPE_MPRIS_ROOT, MprisRootClass))

typedef struct _MprisRoot { GObject parent; } MprisRoot;
typedef struct _MprisRootClass { GObjectClass parent; } MprisRootClass;

GType mpris_root_get_type(void);

typedef struct
{
    gshort major;
    gshort minor;
} mpris_version_t;

gboolean mpris_root_identity(MprisRoot * obj, gchar ** identity, GError ** error);
gboolean mpris_root_quit(MprisRoot * obj, GError ** error);
gboolean mpris_root_mpris_version(MprisRoot * obj, GValue ** version, GError ** error);

#endif
