#include "config.h"

#include "trayicon.h"
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

enum {
    TRAYICON_NUM_SIGNALS
};

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

//static guint trayicon_signals[TRAYICON_NUM_SIGNALS] = { 0 };

static void trayicon_class_init (TrayIconClass *klass);
static void trayicon_init (TrayIcon *ti);
static void trayicon_finalize (GObject *object);
static gboolean trayicon_button_press (GtkWidget *widget,
                                       GdkEventButton *button);
static void trayicon_unrealize (GtkWidget *widget);
static void trayicon_embed (TrayIcon *ti, GdkNativeWindow tray);

GType
trayicon_get_type ()
{
    static GType type = 0;
    if (!type) { /* cache it */
        static const GTypeInfo info = {
            sizeof(TrayIconClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc) trayicon_class_init,
            NULL, /* class_finalize */ 
            NULL, /* class_data */
            sizeof(TrayIcon),
            0, /* n_preallocs */
            (GInstanceInitFunc) trayicon_init,
        };
        type = g_type_register_static (GTK_TYPE_PLUG, "TrayIcon", &info, 0);
    }
    return type;
}

static void
trayicon_class_init (TrayIconClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->finalize = trayicon_finalize;
    widget_class->button_press_event = trayicon_button_press;
    widget_class->unrealize = trayicon_unrealize;
}

static void
trayicon_init (TrayIcon *ti)
{
    gchar *s;

    s = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d",
                         gdk_screen_get_number (gdk_screen_get_default ()));
    ti->tray_atom = gdk_atom_intern (s, FALSE);
    g_free (s);

    ti->showing = FALSE;
    ti->embedded = FALSE;
    ti->menu = NULL;
    ti->stock_image = NULL;

    ti->tooltips = gtk_tooltips_new ();

    gtk_widget_add_events (GTK_WIDGET (ti), GDK_BUTTON_PRESS_MASK);

    g_object_ref (G_OBJECT (ti));
    gtk_object_sink (GTK_OBJECT (ti));

    trayicon_set_image (ti, NULL);
}

static void
trayicon_finalize (GObject *object)
{
    TrayIcon *ti = TRAYICON (object);
    if (ti->menu) g_object_unref (G_OBJECT (ti->menu));
    g_free(ti->stock_image);
}

static gboolean
trayicon_button_press (GtkWidget *widget, GdkEventButton *button)
{
    TrayIcon *ti = TRAYICON (widget);

    /* right clicks pop up the menu */
    if (button->type == GDK_BUTTON_PRESS && button->button == 3) {
        if (ti->menu) {
            gtk_menu_popup (GTK_MENU (ti->menu), NULL, NULL, NULL, NULL,
                            button->button, button->time);
        }
    }

    g_message ("BUTTON PRESS");

    return TRUE;
}

GtkWidget *
trayicon_new ()
{
    return GTK_WIDGET (g_object_new (TRAYICON_TYPE, NULL));
}

static GdkFilterReturn
trayicon_manager_filter (GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
    XEvent *e = (XEvent*) xevent;
    TrayIcon *ti = TRAYICON (data);

    if (ti->showing &&
        (Atom)e->xclient.data.l[1] == gdk_x11_atom_to_xatom (ti->tray_atom)) {
        trayicon_embed (ti, (GdkNativeWindow) e->xclient.data.l[2]);
        return GDK_FILTER_REMOVE;
    }
    return GDK_FILTER_CONTINUE;
}

static void
trayicon_show_image (TrayIcon *ti)
{
    GtkWidget *image, *child;

    gtk_widget_show (GTK_WIDGET (ti));

    if ((child = gtk_bin_get_child (GTK_BIN (ti))))
        gtk_container_remove (GTK_CONTAINER (ti), child);

    image = gtk_image_new_from_stock (ti->stock_image,
                                      GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_show (GTK_WIDGET (image));
    gtk_container_add (GTK_CONTAINER (ti), image);
}

static void
trayicon_embed (TrayIcon *ti, GdkNativeWindow tray)
{
    GdkWindow *tray_window;
    GdkEvent *dock_event;

    tray_window = gdk_window_foreign_new (tray);

    dock_event = gdk_event_new (GDK_CLIENT_EVENT);
    dock_event->client.window = tray_window;
    dock_event->client.send_event = TRUE;
    dock_event->client.message_type =
        gdk_atom_intern ("_NET_SYSTEM_TRAY_OPCODE", FALSE);
    dock_event->client.data_format = 32;
    dock_event->client.data.l[0] = GDK_CURRENT_TIME;
    dock_event->client.data.l[1] = SYSTEM_TRAY_REQUEST_DOCK;
    dock_event->client.data.l[2] = gtk_plug_get_id (GTK_PLUG (ti));
    dock_event->client.data.l[3] = 0;
    dock_event->client.data.l[4] = 0;
    gdk_event_send_client_message (dock_event, tray);

    g_object_unref (G_OBJECT (tray_window));

    gtk_plug_construct (GTK_PLUG (ti), 0);

    trayicon_show_image (ti);

    ti->embedded = TRUE;
}

static void trayicon_unrealize (GtkWidget *widget)
{
    TrayIcon *ti = TRAYICON (widget);
    ti->embedded = FALSE;
}

void
trayicon_show (TrayIcon *ti)
{
    GdkNativeWindow tray;

    g_return_if_fail(ti != NULL);

    gdk_add_client_message_filter (gdk_atom_intern ("MANAGER", FALSE),
                                   trayicon_manager_filter, ti);

    /* if the selection exists, we use it, otherwise we'll just hang around
       and wait for the MANAGER client message (trayicon_manager_filter catches
       them */
    if ((tray = XGetSelectionOwner (GDK_DISPLAY_XDISPLAY 
                                    (gdk_display_get_default ()),
                                    gdk_x11_atom_to_xatom (ti->tray_atom)))) {
        trayicon_embed (ti, tray);
    }

    ti->showing = TRUE;
}

void
trayicon_hide (TrayIcon *ti)
{
    ti->showing = FALSE;
    gtk_widget_hide_all (GTK_WIDGET (ti)); /* XXX this doesn't work */
}

void
trayicon_set_name(TrayIcon *ti, const gchar *name)
{
    g_return_if_fail(ti != NULL);
    g_return_if_fail(name != NULL);

    gtk_window_set_title (GTK_WINDOW (ti), name);
}

void
trayicon_set_image(TrayIcon *ti, const gchar *stock_image)
{
    g_return_if_fail(ti != NULL);

    g_free(ti->stock_image);

    ti->stock_image = g_strdup(stock_image ?
                               stock_image : GTK_STOCK_DIALOG_WARNING);

    if (ti->embedded)
        trayicon_show_image(ti);
}

void
trayicon_set_menu(TrayIcon *ti, GtkWidget *menu)
{
    g_return_if_fail(ti != NULL);
    g_return_if_fail(menu != NULL);
    g_return_if_fail(GTK_IS_MENU(menu));

    if (ti->menu)
        g_object_unref (G_OBJECT (ti->menu));

    ti->menu = menu;

    if (ti->menu) {
        g_object_ref (G_OBJECT (ti->menu));
        gtk_object_sink (GTK_OBJECT (ti->menu));

        gtk_widget_show (GTK_WIDGET (ti->menu));
    }
}

void
trayicon_set_tooltip (TrayIcon *ti, const gchar *tip)
{
    if (!tip)
        gtk_tooltips_disable (ti->tooltips);
    else {
        gtk_tooltips_enable (ti->tooltips);
        gtk_tooltips_set_tip (ti->tooltips, GTK_WIDGET (ti), tip, NULL);
    }
}
