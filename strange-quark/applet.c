#include "config.h"

#include "gettext.h"
#include "trayicon.h"
#include "quarkclient.h"
#include "editor.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gconf/gconf-client.h>
#include <string.h>    /* for strcmp() */
#include <unistd.h>    /* for sleep() */

#define QUARK_ICON "quark-icon"

#define QUARK_GCONF_ROOT        "/apps/quark"
#define QUARK_GCONF_ROOT_RECENT "/apps/quark/recent"
#define LOOP_PLAYLIST            QUARK_GCONF_ROOT "/loop_playlist"
#define RANDOM_ORDER             QUARK_GCONF_ROOT "/random_order"
#define PLAY_ON_RUN              QUARK_GCONF_ROOT "/play_at_startup"
#define RECENT_DIR               QUARK_GCONF_ROOT "/recent_dir"
#define NUM_RECENT               QUARK_GCONF_ROOT_RECENT "/num_recent"
#define RECENT_0                 QUARK_GCONF_ROOT_RECENT "/0"

static GtkWidget *trayicon;
static GtkWidget *menu;
static GtkWidget *editor;
#if 0
static GtkWidget *bookmark_menu;
#endif
static GtkWidget *recent_menu;
static GtkCheckMenuItem *loop_playlist_item;
static GtkCheckMenuItem *random_order_item;
static GtkCheckMenuItem *play_on_run_item;

static QuarkClient *qc;
static GConfClient *gconf;

static GdkPixbuf *applet_icon = NULL;

static gboolean
menu_quit(GtkWidget *item, gpointer data)
{
    gtk_main_quit();
    return FALSE;
}

static void
applet_error (QuarkClient *qc, GError *error, gpointer data)
{
    GtkWidget *dia;

    dia = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR,
                                  GTK_BUTTONS_CLOSE,
                                  "%s", error->message);
    gtk_dialog_run (GTK_DIALOG (dia));
    gtk_widget_destroy (dia);
    g_error_free (error);
}

static void
applet_play (GtkMenuItem *item, gpointer data)
{
    QuarkClient *qc = QUARKCLIENT (data);
    quarkclient_play (qc, NULL);
}

static void
applet_pause (GtkMenuItem *item, gpointer data)
{
    QuarkClient *qc = QUARKCLIENT (data);
    quarkclient_pause (qc, NULL);
}

static void
applet_stop (GtkMenuItem *item, gpointer data)
{
    QuarkClient *qc = QUARKCLIENT (data);
    quarkclient_stop (qc, NULL);
}

static void
applet_next_track (GtkMenuItem *item, gpointer data)
{
    QuarkClient *qc = QUARKCLIENT (data);
    quarkclient_next_track (qc, NULL);
}

static void
applet_previous_track (GtkMenuItem *item, gpointer data)
{
    QuarkClient *qc = QUARKCLIENT (data);
    quarkclient_previous_track (qc, NULL);
}

static void
applet_clear (GtkMenuItem *item, gpointer data)
{
    editor_clear_files (EDITOR (editor));
}

static void
applet_append_files (GtkMenuItem *item, gpointer data)
{
    editor_add_files (EDITOR (editor), TRUE);
}

static void
applet_replace_files (GtkMenuItem *item, gpointer data)
{
    editor_add_files (EDITOR (editor), FALSE);
}

static void
applet_edit_playlist (GtkMenuItem *item, gpointer data)
{
    gtk_widget_show (editor);
}

static void
applet_help (GtkMenuItem *item, gpointer data)
{
}

static void
applet_about (GtkMenuItem *item, gpointer data)
{
}

static void
applet_save_bookmark (GtkMenuItem *item, gpointer data)
{
    GtkWidget *dia, *text, *label, *box;

    dia = gtk_dialog_new_with_buttons (_("Save bookmark - Quark"),
                                       NULL,
                                       0,
                                       GTK_STOCK_CANCEL,
                                       GTK_RESPONSE_CANCEL,
                                       GTK_STOCK_SAVE,
                                       GTK_RESPONSE_OK,
                                       NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (dia), GTK_RESPONSE_OK);
    gtk_window_set_icon (GTK_WINDOW (dia), applet_icon);
    gtk_window_set_role (GTK_WINDOW (dia), "save bookmark");

    box = gtk_vbox_new (FALSE, 4);
    gtk_widget_show (box);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dia)->vbox), box, TRUE, TRUE,0);
    gtk_container_set_border_width (GTK_CONTAINER (box), 6);

    label = gtk_label_new (_("Choose name for bookmark:"));
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE,0);

    text = gtk_entry_new ();
    gtk_widget_show (text);
    gtk_box_pack_start (GTK_BOX (box), text, TRUE, TRUE, 0);

    switch (gtk_dialog_run (GTK_DIALOG (dia))) {
    case GTK_RESPONSE_OK:
        g_message ("saving as %s", gtk_entry_get_text (GTK_ENTRY (text)));
        break;
    }
    gtk_widget_destroy (dia);
}

static void
applet_loop (GtkCheckMenuItem *item, gpointer data)
{
    gconf_client_set_bool (gconf, LOOP_PLAYLIST,
                           gtk_check_menu_item_get_active (item), NULL);
}

static void
applet_random (GtkCheckMenuItem *item, gpointer data)
{
    gconf_client_set_bool (gconf, RANDOM_ORDER,
                           gtk_check_menu_item_get_active (item), NULL);
}

static void
applet_play_on_run (GtkCheckMenuItem *item, gpointer data)
{
    gconf_client_set_bool (gconf, PLAY_ON_RUN,
                           gtk_check_menu_item_get_active (item), NULL);
}

static gboolean
applet_user_event (GtkWidget *widget, GdkEvent *event, gpointer data)
{
    QuarkClient *qc = QUARKCLIENT (data);

    g_message ("event");

    switch (event->type) {
    case GDK_BUTTON_PRESS:
        switch (event->button.button) {
        case 1: /* left */
            quarkclient_pause (qc, NULL);
            break;
        case 2: /* middle */
            editor_add_files (EDITOR (editor),
                              event->button.state & GDK_SHIFT_MASK);
            break;
        case 3: /* right */
            /* note: if you're gunna bind this, then you should return TRUE
               so the menu doesn't pop up probably.. */
            break;
        }
        break;
    case GDK_SCROLL:
        switch (event->scroll.direction) {
        case GDK_SCROLL_LEFT:
        case GDK_SCROLL_UP:
            quarkclient_previous_track (qc, NULL);
            break;
        case GDK_SCROLL_RIGHT:
        case GDK_SCROLL_DOWN:
            quarkclient_next_track (qc, NULL);
            break;
        }
        break;
    default:
        break;
    }
    return FALSE;
}

static void
applet_load_stock (QuarkClient *qc)
{
    GtkIconFactory *factory;
    GError *e = NULL;

    gtk_icon_factory_add_default (factory = gtk_icon_factory_new ());

    applet_icon = gdk_pixbuf_new_from_file (PIXMAPDIR G_DIR_SEPARATOR_S
                                            "quark.png", &e);
    if (!applet_icon) {
        gchar *msg = g_strdup_printf 
            (_("Failed to load the Quark icon, quark is probably not "
               "installed correctly. The error given was '%s'."),
             e->message);
        g_free (e->message);
        e->message = msg;
        applet_error (qc, e, NULL);
    } else {
        GtkIconSet *set;

        set = gtk_icon_set_new_from_pixbuf (applet_icon);
        gtk_icon_factory_add (factory, QUARK_ICON, set);
        gtk_icon_set_unref (set);
    }
}

static void
applet_recent_item (GtkMenuItem *item, gpointer data)
{
    QuarkClient *qc = QUARKCLIENT (data);
    GList *ch;
    GSList *val, *it;
    gboolean r;
    int pos, len, i;
    gchar *path;

    ch = gtk_container_get_children (GTK_CONTAINER (recent_menu));
    pos = g_list_position (ch, g_list_find (ch, item));

    if (pos >= 0) {
        path = g_strdup_printf ("%s/%i", QUARK_GCONF_ROOT_RECENT, pos);
        val = gconf_client_get_list (gconf, path, GCONF_VALUE_STRING,
                                     NULL);
        g_free (path);

        if (val) {
            gchar **files;

            len = g_slist_length (val);
            files = g_new (gchar*, len + 1);
            files[len] = NULL;

            for (i = 0, it = val; it; ++i, it = g_slist_next (it))
                files[i] = it->data;

            r = quarkclient_clear (qc, NULL);
            if (r) r = quarkclient_add_tracks (qc, files, NULL);
            if (r) quarkclient_play (qc, NULL);

            g_strfreev (files); /* frees the strings in 'val' too */
            g_slist_free (val);
        }
    }
}

static void
applet_rebuild_recent_menu ()
{
    GList *ch, *it;
    GSList *val, *sit;
    gint i, num;
    gchar *path;
    GString *slabel;
    GtkWidget *item;

    ch = gtk_container_get_children (GTK_CONTAINER (recent_menu));
    for (it = ch; it; it = g_list_next (it))
        gtk_container_remove (GTK_CONTAINER (recent_menu), it->data);

    num = gconf_client_get_int (gconf, NUM_RECENT, NULL);

    for (i = 0; i < num; ++i) {
        path = g_strdup_printf ("%s/%i", QUARK_GCONF_ROOT_RECENT, i);
        val = gconf_client_get_list (gconf, path, GCONF_VALUE_STRING,
                                     NULL);
        g_free (path);

        if (val) {
            slabel = g_string_sized_new (0);
            g_string_printf (slabel, "%02d.", i + 1);
            for (sit = val; sit; sit = g_slist_next (sit)) {
                g_strdelimit (sit->data, "_", ' ');
                g_string_append_printf (slabel, " %s", (gchar*)sit->data);
                g_free (sit->data);
            }
            g_slist_free (val);

#define LABEL_SIZE 100

            if (slabel->len > LABEL_SIZE) {
                slabel = g_string_erase (slabel, (LABEL_SIZE-3) / 2,
                                         slabel->len - (LABEL_SIZE-3));
                slabel = g_string_insert (slabel, (LABEL_SIZE-3) / 2 + 1,
                                          "...");
            }

            item = gtk_menu_item_new_with_label (slabel->str);
            gtk_menu_shell_append (GTK_MENU_SHELL (recent_menu), item);
            g_signal_connect(item, "activate", G_CALLBACK(applet_recent_item),
                             qc);
            gtk_widget_show (item);
            
            g_string_free (slabel, TRUE);
        }
    }
}

static void
applet_gconf_changed (GConfClient *gconf,
                      guint cnxn_id,
                      GConfEntry *entry,
                      gpointer data)
{
    g_message ("notify %s", entry->key);
    if (!strcmp (entry->key, LOOP_PLAYLIST)) {
        gtk_check_menu_item_set_active (loop_playlist_item,
                                        gconf_value_get_bool (entry->value));
    } else if (!strcmp (entry->key, RANDOM_ORDER)) {
        gtk_check_menu_item_set_active (random_order_item,
                                        gconf_value_get_bool (entry->value));
    } else if (!strcmp (entry->key, PLAY_ON_RUN)) {
        gtk_check_menu_item_set_active (play_on_run_item,
                                        gconf_value_get_bool (entry->value));
    } else if (!strcmp (entry->key, NUM_RECENT)) {
        applet_rebuild_recent_menu ();
    } else if (!strcmp (entry->key, RECENT_0)) {
        applet_rebuild_recent_menu ();
    }
}

static void
applet_song_change (QuarkClient *qc, gint pos, gpointer data)
{
    TrayIcon *ti = TRAYICON (data);
    QuarkClientTrack *curtrack = quarkclient_playlist_nth (qc, pos);

    if (curtrack) {
        gchar *name = g_path_get_basename (quarkclient_track_path (curtrack));
        gchar *s = g_strdup_printf(_("Playing: %s"), name);
        trayicon_set_tooltip (ti, s);
        g_free(s);
        g_free (name);
    } else
        trayicon_set_tooltip (ti, "Quark");
}

void
applet_startup ()
{
    GtkWidget *item, *image;

    qc = quarkclient_new ();
    quarkclient_set_default_error_handler (qc, applet_error, trayicon);

    quarkclient_open (qc, "strange", NULL);

    gconf = gconf_client_get_default ();
    gconf_client_add_dir (gconf, QUARK_GCONF_ROOT,
                          GCONF_CLIENT_PRELOAD_NONE, NULL);
    gconf_client_notify_add (gconf, QUARK_GCONF_ROOT, applet_gconf_changed,
                             NULL, NULL, NULL);

    applet_load_stock (qc);

    editor = editor_new(qc, gconf, applet_icon);

    menu = gtk_menu_new();

    item = gtk_image_menu_item_new_with_mnemonic (_("_Play"));
    image = gtk_image_new_from_stock (GTK_STOCK_YES, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(applet_play), qc);

    item = gtk_image_menu_item_new_with_mnemonic (_("Pa_use"));
    image = gtk_image_new_from_stock (GTK_STOCK_NO, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(applet_pause), qc);

    item = gtk_image_menu_item_new_with_mnemonic (_("_Stop"));
    image = gtk_image_new_from_stock (GTK_STOCK_STOP, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(applet_stop), qc);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                           gtk_separator_menu_item_new());

    item = gtk_image_menu_item_new_with_mnemonic (_("_Next track"));
    image = gtk_image_new_from_stock (GTK_STOCK_GO_FORWARD,
                                      GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(applet_next_track), qc);

    item = gtk_image_menu_item_new_with_mnemonic (_("Pre_vious track"));
    image = gtk_image_new_from_stock (GTK_STOCK_GO_BACK, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(applet_previous_track), qc);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                           gtk_separator_menu_item_new());

    item = gtk_menu_item_new_with_mnemonic (_("_Recent"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    recent_menu = gtk_menu_new ();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), recent_menu);

    applet_rebuild_recent_menu ();
    item = gtk_image_menu_item_new_with_mnemonic (_("_Load songs..."));
    image = gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(applet_replace_files), qc);

    item = gtk_image_menu_item_new_with_mnemonic (_("_Append songs..."));
    image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(applet_append_files), qc);

    item = gtk_image_menu_item_new_with_mnemonic (_("_Clear playlist"));
    image = gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(applet_clear), qc);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                           gtk_separator_menu_item_new());

    item = gtk_check_menu_item_new_with_mnemonic (_("Play on startup"));
    play_on_run_item = GTK_CHECK_MENU_ITEM (item);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
                                    gconf_client_get_bool (gconf,
                                                           PLAY_ON_RUN,
                                                           NULL));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect(item, "toggled", G_CALLBACK(applet_play_on_run), qc);

    item = gtk_check_menu_item_new_with_mnemonic (_("Loop playlist"));
    loop_playlist_item = GTK_CHECK_MENU_ITEM (item);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
                                    gconf_client_get_bool (gconf,
                                                           LOOP_PLAYLIST,
                                                           NULL));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect(item, "toggled", G_CALLBACK(applet_loop), qc);

    item = gtk_check_menu_item_new_with_mnemonic (_("Random order"));
    random_order_item = GTK_CHECK_MENU_ITEM (item);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
                                    gconf_client_get_bool (gconf,
                                                           RANDOM_ORDER,
                                                           NULL));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect(item, "toggled", G_CALLBACK(applet_random), qc);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                           gtk_separator_menu_item_new());

#if 0
    item = gtk_menu_item_new_with_mnemonic (_("_Bookmarks"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    bookmark_menu = gtk_menu_new ();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), bookmark_menu);

    item = gtk_image_menu_item_new_with_mnemonic
        (_("_Save playlist as bookmark"));
    image = gtk_image_new_from_stock (GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    gtk_menu_shell_append (GTK_MENU_SHELL (bookmark_menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(applet_save_bookmark), qc);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                           gtk_separator_menu_item_new());
#endif

#if 0
    item = gtk_image_menu_item_new_with_mnemonic (_("_Edit playlist..."));
    image = gtk_image_new_from_stock (GTK_STOCK_JUSTIFY_FILL,
                                      GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(applet_edit_playlist), qc);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                           gtk_separator_menu_item_new());
#endif


/*
    item = gtk_image_menu_item_new_from_stock(GTK_STOCK_HELP, NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(applet_help), qc);

    item = gtk_menu_item_new_with_mnemonic (_("About..."));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(applet_about), qc);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                           gtk_separator_menu_item_new());
*/

    item = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(menu_quit), NULL);

    gtk_widget_show_all (GTK_WIDGET (menu));

    trayicon = trayicon_new ();

    g_signal_connect(trayicon, "button-press-event",
                     G_CALLBACK(applet_user_event), qc);
    g_signal_connect(trayicon, "scroll-event",
                     G_CALLBACK(applet_user_event), qc);

    g_signal_connect (qc, "playlist-position-changed",
                      G_CALLBACK (applet_song_change), trayicon);

    trayicon_set_name (TRAYICON (trayicon), "strange-quark");
    trayicon_set_menu (TRAYICON (trayicon), menu);
    trayicon_set_image (TRAYICON (trayicon), QUARK_ICON);
    /* XXX set this with the song */
    trayicon_set_tooltip (TRAYICON (trayicon), "Quark");

    trayicon_show (TRAYICON (trayicon));

    if (gconf_client_get_bool (gconf, PLAY_ON_RUN, NULL))
        quarkclient_play (qc, NULL);
}

void
applet_shutdown ()
{
    g_object_unref (G_OBJECT (qc));
    g_object_unref (G_OBJECT (gconf));
    gtk_object_destroy (GTK_OBJECT (trayicon));
}
