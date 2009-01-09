#include "config.h"

#include "gettext.h"

// BUG: i shouldn't have to include mpris-root before mpris-root-glue --
// dbus-glib-tool seems to be broken
#include "cpris-root.h"
#include "cpris-root-glue.h"
#include "mpris-root.h"
#include "mpris-root-glue.h"
#include "mpris-player.h"
#include "mpris-player-glue.h"
#include "mpris-tracklist.h"
#include "mpris-tracklist-glue.h"
#include "dbus.h"
#include "main.h"

#include <glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>

#define CORN_BUS_CROOT_PATH "/Corn"
#define CORN_BUS_ROOT_PATH "/"
#define CORN_BUS_PLAYER_PATH "/Player"
#define CORN_BUS_TRACKLIST_PATH "/TrackList"

#define CORN_BUS_INTERFACE "org.freedesktop.MediaPlayer"

CprisRoot * cpris_root;
MprisRoot * mpris_root;
MprisPlayer * mpris_player;
MprisTrackList * mpris_tracklist;

static DBusGConnection * bus = NULL;

static int mpris_register_objects(DBusGConnection *);

int mpris_init(void)
{
    dbus_g_thread_init();

    GError * error = NULL;
    if(!(bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error)))
    {
        g_printerr("%s (%s).\n", _("Failed to open connection to D-BUS session bus"),
                   error->message);
        g_error_free(error);
        return 30;
    }

    DBusConnection * dbus_conn = dbus_g_connection_get_connection(bus);
    dbus_connection_set_exit_on_disconnect(dbus_conn, FALSE);
    // ok, so how do we get notified when the bus disconnects, to handle it
    // ourselves?

    return mpris_register_objects(bus);
}

void mpris_destroy(void)
{
    if(bus)
        dbus_g_connection_unref(bus);
    bus = NULL;
}

static int mpris_register_objects(DBusGConnection * bus)
{
    DBusGProxy * bus_proxy = dbus_g_proxy_new_for_name(bus,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus");

    // TODO: it is possible to negotiate taking over a name.  this may or may
    // not be desirable.
    // http://dbus.freedesktop.org/doc/dbus-specification.html#message-bus-names

    GError * error = NULL;
    guint request_name_result;
    dbus_g_proxy_call(bus_proxy, "RequestName", &error,
        G_TYPE_STRING, main_service_name,
        G_TYPE_UINT, 0,
        G_TYPE_INVALID,
        G_TYPE_UINT, &request_name_result,
        G_TYPE_INVALID);

    if(error)
    {
        g_printerr("Error during RequestName proxy call (%s).\n", error->message);
        g_error_free(error);
        return 31;
    }

    if(!request_name_result)
    {
        g_printerr("Failed to acquire %s service.\n", main_service_name);
        return 32;
    }

    cpris_root = g_object_new(cpris_root_get_type(), NULL);
    mpris_root = g_object_new(mpris_root_get_type(), NULL);
    mpris_player = g_object_new(mpris_player_get_type(), NULL);
    mpris_tracklist = g_object_new(mpris_tracklist_get_type(), NULL);

    // put this stuff in constructors
    //
    // these object info thingies are generated by dbus-glib-tool from the xml
    // file and put into the glue header.
    //
    dbus_g_object_type_install_info(cpris_root_get_type(), &dbus_glib_cpris_root_object_info);
    dbus_g_object_type_install_info(mpris_root_get_type(), &dbus_glib_mpris_root_object_info);
    dbus_g_object_type_install_info(mpris_player_get_type(), &dbus_glib_mpris_player_object_info);
    dbus_g_object_type_install_info(mpris_tracklist_get_type(), &dbus_glib_mpris_tracklist_object_info);

    // error check!
    dbus_g_connection_register_g_object(bus, CORN_BUS_CROOT_PATH, G_OBJECT(cpris_root));
    dbus_g_connection_register_g_object(bus, CORN_BUS_ROOT_PATH, G_OBJECT(mpris_root));
    dbus_g_connection_register_g_object(bus, CORN_BUS_PLAYER_PATH, G_OBJECT(mpris_player));
    dbus_g_connection_register_g_object(bus, CORN_BUS_TRACKLIST_PATH, G_OBJECT(mpris_tracklist));

#define DBUS_TYPE_G_STRING_VALUE_HASHTABLE (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))

    DBusGProxy * mpris_proxy = dbus_g_proxy_new_for_name(bus,
        "org.mpris.corn",
        "/Player",
        "org.freedesktop.MediaPlayer");
    dbus_g_proxy_add_signal(mpris_proxy, "StatusChange", G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_add_signal(mpris_proxy, "CapsChange", G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_add_signal(mpris_proxy, "TrackChange", DBUS_TYPE_G_STRING_VALUE_HASHTABLE, G_TYPE_INVALID);

    return 0;
}
