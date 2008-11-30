#include "config.h"

#include "gettext.h"
#include "editor.h"
#include <gtk/gtk.h>
#include <string.h>

#define PLAYING_COLOR "red"

enum {
    NUMBER_COLUMN,
    FILE_COLUMN,
    PLAYING_COLUMN,
    N_COLUMNS
};

enum {
    EDITOR_NUM_SIGNALS
};

#define QUARK_GCONF_ROOT        "/apps/quark"
#define QUARK_GCONF_ROOT_RECENT "/apps/quark/recent"
#define RECENT_DIR              QUARK_GCONF_ROOT "/recent_dir"
#define NUM_RECENT              QUARK_GCONF_ROOT_RECENT "/num_recent"
#define RECENT_0                QUARK_GCONF_ROOT_RECENT "/0"

//static guint editor_signals[EDITOR_NUM_SIGNALS] = { 0 };
static GtkWidget *parent_class;

static void editor_class_init (EditorClass *klass);
static void editor_init (Editor *ed);
static void editor_finalize (GObject *object);

/* event handlers */
static void editor_open(GtkWidget *widget, gpointer data);
static void editor_save(GtkWidget *widget, gpointer data);
static void editor_clear(GtkWidget *widget, gpointer data);
static void editor_append(GtkWidget *widget, gpointer data);
static void editor_remove(GtkWidget *widget, gpointer data);
static void editor_close(GtkWidget *widget, gpointer data);
static void editor_ascend(GtkWidget *widget, gpointer data);
static void editor_descend(GtkWidget *widget, gpointer data);

GType
editor_get_type ()
{
    static GType type = 0;
    if (!type) { /* cache it */
        static const GTypeInfo info = {
            sizeof(EditorClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc) editor_class_init,
            NULL, /* class_finalize */ 
            NULL, /* class_data */
            sizeof(Editor),
            0, /* n_preallocs */
            (GInstanceInitFunc) editor_init,
        };
        type = g_type_register_static (GTK_TYPE_WINDOW, "Editor", &info, 0);
    }
    return type;
}

static gboolean
editor_delete (GtkWidget *widget, GdkEventAny *event)
{
    gtk_widget_hide (widget);
    return TRUE;
}

static void
editor_class_init (EditorClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);


    parent_class = g_type_class_peek_parent (klass);

    gobject_class->finalize = editor_finalize;

    widget_class->delete_event = editor_delete;
}

static void
editor_init (Editor *ed)
{
    GtkWidget *w, *m;
    GtkAccelGroup *accel;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    accel = gtk_accel_group_new ();

    ed->vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (ed->vbox);
    gtk_container_add (GTK_CONTAINER (ed), ed->vbox);

    /* set up the menu */

    ed->menu = gtk_menu_bar_new();

    w = gtk_menu_item_new_with_mnemonic ("_File");
    gtk_menu_shell_append (GTK_MENU_SHELL (ed->menu), w);

    m = gtk_menu_new ();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (w), m);

    w = gtk_image_menu_item_new_from_stock (GTK_STOCK_OPEN, accel);
    gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
    g_signal_connect (w, "activate", G_CALLBACK (editor_open), ed);
    w = gtk_image_menu_item_new_from_stock (GTK_STOCK_SAVE, accel);
    gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
    g_signal_connect (w, "activate", G_CALLBACK (editor_save), ed);
    w = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
    w = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLOSE, accel);
    gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
    g_signal_connect (w, "activate", G_CALLBACK (editor_close), ed);

    w = gtk_menu_item_new_with_mnemonic ("_Edit");
    gtk_menu_shell_append (GTK_MENU_SHELL (ed->menu), w);

    m = gtk_menu_new ();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (w), m);

    w = gtk_image_menu_item_new_from_stock (GTK_STOCK_ADD, accel);
    gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
    g_signal_connect (w, "activate", G_CALLBACK (editor_append), ed);
    w = gtk_image_menu_item_new_from_stock (GTK_STOCK_REMOVE, accel);
    gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
    g_signal_connect (w, "activate", G_CALLBACK (editor_remove), ed);
    w = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
    w = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLEAR, accel);
    gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
    g_signal_connect (w, "activate", G_CALLBACK (editor_clear), ed);

    w = gtk_menu_item_new_with_mnemonic ("_Sort");
    gtk_menu_shell_append (GTK_MENU_SHELL (ed->menu), w);

    m = gtk_menu_new ();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (w), m);

    w = gtk_image_menu_item_new_from_stock (GTK_STOCK_SORT_ASCENDING, accel);
    gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
    g_signal_connect (w, "activate", G_CALLBACK (editor_ascend), ed);
    w = gtk_image_menu_item_new_from_stock (GTK_STOCK_SORT_DESCENDING, accel);
    gtk_menu_shell_append (GTK_MENU_SHELL (m), w);
    g_signal_connect (w, "activate", G_CALLBACK (editor_descend), ed);
    w = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (m), w);

    gtk_widget_show_all (ed->menu);
    gtk_box_pack_start (GTK_BOX (ed->vbox), ed->menu, FALSE, FALSE, 0);

    /* set up the list */

    ed->data = gtk_list_store_new (N_COLUMNS,
                                   G_TYPE_INT,     /* song number */
                                   G_TYPE_STRING,  /* song's filename */
                                   G_TYPE_STRING); /* song's foreground color*/

    ed->list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (ed->data));

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes
        ("Number", renderer, "text", NUMBER_COLUMN,
         "foreground", PLAYING_COLUMN, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (ed->list), column);
    column = gtk_tree_view_column_new_with_attributes
        ("Name", renderer, "text", FILE_COLUMN, "foreground", PLAYING_COLUMN,
         NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (ed->list), column);

    w = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (w),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_ALWAYS);
    gtk_container_add (GTK_CONTAINER (w), ed->list);
    gtk_widget_show_all (w);

    gtk_box_pack_start (GTK_BOX (ed->vbox), w, TRUE, TRUE, 0);

    /* set up the buttons */

    ed->buttons = gtk_hbutton_box_new ();
    gtk_button_box_set_layout (GTK_BUTTON_BOX (ed->buttons),
                               GTK_BUTTONBOX_START);
    gtk_container_set_border_width (GTK_CONTAINER (ed->buttons), 5);
    gtk_box_set_spacing (GTK_BOX (ed->buttons), 5);
    gtk_widget_show (ed->buttons);

    ed->clear = gtk_button_new_from_stock (GTK_STOCK_CLEAR);
    gtk_widget_show (ed->clear);
    gtk_box_pack_start (GTK_BOX (ed->buttons), ed->clear, FALSE, FALSE, 0);
    g_signal_connect (ed->clear, "clicked", G_CALLBACK (editor_clear), ed);

    ed->append = gtk_button_new_from_stock (GTK_STOCK_ADD);
    gtk_widget_show (ed->append);
    gtk_box_pack_start (GTK_BOX (ed->buttons), ed->append, FALSE, FALSE, 0);
    g_signal_connect (ed->append, "clicked", G_CALLBACK (editor_append), ed);

    gtk_box_pack_end (GTK_BOX (ed->vbox), ed->buttons, FALSE, FALSE, 0);

    /* set up the whole window */
    gtk_window_set_title (GTK_WINDOW (ed), _("Edit Playlist - Quark"));
    gtk_window_set_role (GTK_WINDOW (ed), _("Playlist Editor"));
    gtk_window_set_default_size (GTK_WINDOW (ed), 300, 300);
    gtk_window_add_accel_group (GTK_WINDOW (ed), accel);
}

static void
editor_finalize (GObject *object)
{
}

static void
editor_track_added (QuarkClient *qc, gint pos, gpointer data)
{
    Editor *ed = EDITOR(data);
    GtkTreeIter it;
    gchar *s;
    QuarkClientTrack *track;

    track = quarkclient_playlist_nth (qc, pos);
    g_assert (track);

    gtk_list_store_insert (GTK_LIST_STORE (ed->data), &it, pos);

    s = g_path_get_basename (quarkclient_track_path (track));
    gtk_list_store_set (GTK_LIST_STORE (ed->data), &it,
                        NUMBER_COLUMN, 0,
                        FILE_COLUMN, s,
                        PLAYING_COLUMN, NULL,
                        -1);
    g_free(s);
}

static void
editor_track_removed (QuarkClient *qc, gint pos, gpointer data)
{
    Editor *ed = EDITOR(data);
    GtkTreeIter it;

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (ed->data), &it)) {
        while (pos-- > 0)
            gtk_tree_model_iter_next (GTK_TREE_MODEL (ed->data), &it);
        gtk_list_store_remove (GTK_LIST_STORE (ed->data), &it);
    }
}

static void
editor_track_moved (QuarkClient *qc, gint from, gint to, gpointer data)
{
    Editor *ed = EDITOR(data);
    GtkTreeIter it, n;

    if (to > from) ++to;

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (ed->data), &it)) {
        while (from-- > 0)
            gtk_tree_model_iter_next (GTK_TREE_MODEL (ed->data), &it);
    }

    gtk_list_store_insert (GTK_LIST_STORE (ed->data), &n, to);
    gtk_list_store_move_after (GTK_LIST_STORE (ed->data),
                               &it, &n);
    gtk_list_store_remove (GTK_LIST_STORE (ed->data), &n);
}

static void
editor_song_changed (QuarkClient *qc, gint pos, gpointer data)
{
    Editor *ed = EDITOR(data);
    static GtkTreeIter it;

    if (gtk_list_store_iter_is_valid (GTK_LIST_STORE (ed->data), &it))
        gtk_list_store_set (GTK_LIST_STORE (ed->data), &it,
                            PLAYING_COLUMN, NULL, -1);

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (ed->data), &it)) {
        while (pos-- > 0)
            gtk_tree_model_iter_next (GTK_TREE_MODEL (ed->data), &it);

        /* woot */

        gtk_list_store_set (GTK_LIST_STORE (ed->data), &it,
                            PLAYING_COLUMN, PLAYING_COLOR, -1);
    }    
}

GtkWidget *
editor_new (QuarkClient *qc, GConfClient *gconf, GdkPixbuf *icon)
{
    Editor *ed;

    g_return_val_if_fail (qc != NULL, NULL);
    g_return_val_if_fail (gconf != NULL, NULL);

    ed = g_object_new (EDITOR_TYPE, NULL);
    ed->qc = qc;
    ed->gconf = gconf;
    ed->icon = icon;

    if (icon)
        gtk_window_set_icon (GTK_WINDOW (ed), icon);

    g_signal_connect (qc, "track-added",
                      G_CALLBACK (editor_track_added), ed);
    g_signal_connect (qc, "track-removed",
                      G_CALLBACK (editor_track_removed), ed);
    g_signal_connect (qc, "track-moved",
                      G_CALLBACK (editor_track_moved), ed);
    g_signal_connect (qc, "playlist-position-changed",
                      G_CALLBACK (editor_song_changed), ed);

    return GTK_WIDGET (ed);
}

static void
editor_open(GtkWidget *widget, gpointer data)
{
}

static void
editor_save(GtkWidget *widget, gpointer data)
{
}

static void
editor_clear(GtkWidget *widget, gpointer data)
{
    editor_clear_files (EDITOR (data));
}

static void
editor_append(GtkWidget *widget, gpointer data)
{
    editor_add_files (EDITOR (data), TRUE);
}

static void
editor_remove(GtkWidget *widget, gpointer data)
{
}

static void
editor_close(GtkWidget *widget, gpointer data)
{
    editor_delete (data, NULL);
}

static void
editor_ascend(GtkWidget *widget, gpointer data)
{
}

static void
editor_descend(GtkWidget *widget, gpointer data)
{
}

static void
free_list_data (GSList *list)
{
    GSList *it;
    for (it = list; it; it = g_slist_next (it))
        g_free (it->data);
    g_slist_free (list);
}

static void
editor_save_recent (Editor *ed, gchar **files)
{
    GSList *list = NULL, *it, *it2, *val;
    gchar *path, **p;
    gint i, num, savenum;

    for (p = files; *p; ++p)
        list = g_slist_append (list, *p);

    savenum = num = gconf_client_get_int (ed->gconf, NUM_RECENT, NULL);

    /* remove it if it already exists */
    for (i = 0; i < num; ++i) {
        path = g_strdup_printf ("%s/%i", QUARK_GCONF_ROOT_RECENT, i);
        val = gconf_client_get_list (ed->gconf, path, GCONF_VALUE_STRING,
                                     NULL);
        if (val) {
            for (it = list, it2 = val; it && it2;
                 it = g_slist_next (it), it2 = g_slist_next (it2)) {
                if (g_utf8_collate (it->data, it2->data))
                    break;
            }
            if (!it && !it2) {
                gconf_client_unset (ed->gconf, path, NULL);
                num = i + 1; /* only shift this far below */
                i = num; /* break the for loop */
            }
            free_list_data (val);
        }

        g_free (path);
    }

    for (i = num - 1; i > 0; --i) {
        path = g_strdup_printf ("%s/%i", QUARK_GCONF_ROOT_RECENT, i - 1);
        val = gconf_client_get_list (ed->gconf, path, GCONF_VALUE_STRING,
                                     NULL);
        if (val) {
            g_free (path);
            path = g_strdup_printf ("%s/%i", QUARK_GCONF_ROOT_RECENT, i);
            gconf_client_set_list (ed->gconf, path, GCONF_VALUE_STRING,
                                   val, NULL);
            free_list_data (val);
        }
        g_free (path);
    }

    gconf_client_set_list (ed->gconf, RECENT_0, GCONF_VALUE_STRING, list,
                           NULL);

    gconf_client_suggest_sync (ed->gconf, NULL);

    g_slist_free (list);
}

void
editor_add_files (Editor *ed, gboolean append)
{
    GtkWidget *sel;
    gboolean r = TRUE;
    gchar *p, *p2;

    g_return_if_fail (ed != NULL);

    sel = gtk_file_selection_new (append ?
                                  _("Append Songs - Quark") :
                                  _("Play Songs - Quark"));
    gtk_file_selection_set_select_multiple (GTK_FILE_SELECTION (sel), TRUE);
    if (ed->icon)
        gtk_window_set_icon (GTK_WINDOW (sel), ed->icon);

    if ((p = gconf_client_get_string (ed->gconf, RECENT_DIR, NULL)))
        gtk_file_selection_set_filename (GTK_FILE_SELECTION (sel), p);

    if (gtk_dialog_run (GTK_DIALOG (sel)) == GTK_RESPONSE_OK) {
        gchar **files, **it;

        files = gtk_file_selection_get_selections (GTK_FILE_SELECTION (sel));
        for (it = files; *it; ++it)
            g_strstrip(*it);

        if ((p = g_strrstr (files[0], "://"))) {
            if ((p2 = g_strrstr_len (files[0], p - files[0], "/"))) {
                p = g_strdup (p2 + 1);
                g_free (files[0]);
                files[0] = p;
            }
        }

        if (!append)
            r = quarkclient_clear (ed->qc, NULL);
        if (r) r = quarkclient_add_tracks (ed->qc, files, NULL);
        if (r) quarkclient_play (ed->qc, NULL);

        if (!strstr (files[0], "://")) {
            p = g_path_get_dirname (files[0]);
            /* make it into a dir */
            p2 = g_strconcat (p, G_DIR_SEPARATOR_S, NULL); 
            gconf_client_set_string (ed->gconf, RECENT_DIR, p2, NULL);
            g_free (p2);
            g_free (p);
        }

        editor_save_recent (ed, files);

        g_strfreev (files);
    }

    gtk_widget_destroy (sel);
}

void
editor_clear_files (Editor *ed)
{
    quarkclient_clear (ed->qc, NULL);
}
