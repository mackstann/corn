// CPRIS: the Corn Media Player Interfacing Specification
//
// AKA stuff that should be in MPRIS but isn't yet

#include "main.h"

#include "cpris-root.h"

#include <glib.h>
#include <glib-object.h>

G_DEFINE_TYPE(CprisRoot, cpris_root, G_TYPE_OBJECT);

static void
cpris_root_init(CprisRoot * obj)
{
}

static void
cpris_root_class_init(CprisRootClass * klass)
{
}

gboolean cpris_root_clear(CprisRoot * obj, GError ** error)
{
    playlist_clear();
    return TRUE;
}

