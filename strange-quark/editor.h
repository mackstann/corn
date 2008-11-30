#ifndef __EDITOR_H__
#define __EDITOR_H__

#include "quarkclient.h"

#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

G_BEGIN_DECLS

#define EDITOR_TYPE            (editor_get_type ())
#define EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                                            EDITOR_TYPE, \
                                                            Editor))
#define EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                                         EDITOR_TYPE, \
                                                         Esditor))
#define IS_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                                            EDITOR_TYPE))
#define IS_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                                                         EDITOR_TYPE))

typedef struct _Editor      Editor;
typedef struct _EditorClass EditorClass;

struct _Editor {
    GtkWindow     window;

    GtkWidget    *vbox;

    GtkWidget    *menu;
    GtkWidget    *list;
    GtkListStore *data;

    GtkWidget    *buttons;
    GtkWidget    *append;
    GtkWidget    *clear;

    QuarkClient  *qc;
    GConfClient  *gconf;
    GdkPixbuf    *icon;
};

struct _EditorClass {
    GtkWindowClass parent_class;
};

GType editor_get_type ();

GtkWidget *editor_new         (QuarkClient *qc, GConfClient *gconf,
                               GdkPixbuf *icon);

void       editor_add_files   (Editor *ed, gboolean append);
void       editor_clear_files (Editor *ed);

G_END_DECLS

#endif
