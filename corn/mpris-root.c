#include "main.h"

#include "mpris-root.h"

#include <glib.h>
#include <glib-object.h>

G_DEFINE_TYPE(MprisRoot, mpris_root, G_TYPE_OBJECT);

static void
mpris_root_init(MprisRoot * obj)
{
}

static void
mpris_root_class_init(MprisRootClass * klass)
{
}

gboolean mpris_root_identity(MprisRoot * obj, gchar ** identity, GError ** error)
{
    *identity = g_strdup("brunt");
    return TRUE;
}

gboolean mpris_root_quit(MprisRoot * obj, GError ** error)
{
    main_quit();
    return TRUE;
}

#define DBUS_STRUCT_INT_INT (dbus_g_type_get_struct ("GValueArray", G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID))

gboolean mpris_root_mpris_version(MprisRoot * obj, GValue ** version, GError ** error)
{

    GValue * value = g_new0(GValue, 1);
    g_value_init(value, DBUS_STRUCT_INT_INT);
    g_value_take_boxed(value, dbus_g_type_specialized_construct(DBUS_STRUCT_INT_INT));

    // field number, value, G_MAXUINT at the end
    dbus_g_type_struct_set(value, 0, 1, 1, 0, G_MAXUINT);

    *version = value;

    //GValueArray * arr = g_value_array_new(2);
    //GValue major = {0};
    //GValue minor = {0};
    //g_value_init(&major, G_TYPE_INT);
    //g_value_init(&minor, G_TYPE_INT);
    //g_value_set_int(&major, 1);
    //g_value_set_int(&minor, 0);
    //g_value_array_append(arr, &major);
    //g_value_array_append(arr, &minor);

    //GValue v = {0};
    //g_value_init(version, G_TYPE_BOXED);
    //g_value_take_boxed(&v, arr);

    //g_value_array_free(arr);

    return TRUE;
}



