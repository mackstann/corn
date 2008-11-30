#ifndef __TRAYICON_H__
#define __TRAYICON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TRAYICON_TYPE            (trayicon_get_type ())
#define TRAYICON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                                              TRAYICON_TYPE, \
                                                              TrayIcon))
#define TRAYICON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                                           TRAYICON_TYPE, \
                                                           TrayIcon))
#define IS_TRAYICON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                                              TRAYICON_TYPE))
#define IS_TRAYICON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                                                           TRAYICON_TYPE))

typedef struct _TrayIcon      TrayIcon;
typedef struct _TrayIconClass TrayIconClass;

struct _TrayIcon {
    GtkPlug    plug;

    gchar     *stock_image;
    GtkWidget *menu;

    GdkAtom    tray_atom; /* _NET_SYSTEM_TRAY_S%d */

    gboolean   showing;
    gboolean   embedded;

    GtkTooltips          *tooltips;
};

struct _TrayIconClass {
    GtkPlugClass parent_class;
};

GType trayicon_get_type ();

GtkWidget *trayicon_new ();

void trayicon_show (TrayIcon *ti);
void trayicon_hide (TrayIcon *ti);

void trayicon_set_name          (TrayIcon *ti, const gchar *name);
void trayicon_set_image         (TrayIcon *ti, const gchar *stock_image);
void trayicon_set_menu          (TrayIcon *ti, GtkWidget   *menu);
void trayicon_set_tooltip       (TrayIcon *ti, const gchar *tip);

G_END_DECLS

#endif
