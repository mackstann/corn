#ifndef __corn_mpris_root_h__
#define __corn_mpris_root_h__

#include <glib-object.h>
#include <glib.h>

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
