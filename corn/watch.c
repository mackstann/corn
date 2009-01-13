#include "watch.h"
#include "playlist.h"
#include "music-metadata.h"
#include "db.h"

#include <gio/gio.h>
#include <glib.h>

#include <string.h>

static GQueue * event_queue = NULL;

static GHashTable * watches = NULL;

typedef struct
{
    GFileMonitor * monitor;
    gint refcount;
} DirWatch;

static void watch_free(gpointer data)
{
    DirWatch * watch = (DirWatch *)data;
    g_file_monitor_cancel(watch->monitor);
    g_object_unref(watch->monitor);
    g_free(watch);
}

// XXX temporary hack because we lack a database
static gint locate(const gchar * path)
{
    for(gint i = 0; i < playlist_length(); i++)
        if(!strcmp(path, playlist_nth(i)))
            return i;
    return -1;
}

gboolean handle_event_when_idle(G_GNUC_UNUSED gpointer data)
{
    GFile * file = g_queue_pop_head(event_queue);
    gchar * path = g_file_get_path(file); // XXX should use uri
    g_object_unref(file);
    if(path)
    {
        GHashTable * meta = music_get_playlist_item_metadata(path);
        gboolean has_meta = g_hash_table_size(meta);
        gint pos = locate(path);
        if(pos > -1)
        {
            if(has_meta)
                db_schedule_update(playlist_nth(pos));
            else
            {
                playlist_remove(pos);
                g_free(path);
            }
        }
        else if(pos == -1 && has_meta)
        {
            playlist_append(path);
        }
    }
    return FALSE;
}

void changed_callback(G_GNUC_UNUSED GFileMonitor * monitor,
                                    GFile * file,
                      G_GNUC_UNUSED GFile * other_file,
                                    GFileMonitorEvent event_type,
                      G_GNUC_UNUSED gpointer user_data)
{
    switch(event_type)
    {
        case G_FILE_MONITOR_EVENT_CHANGED:
        case G_FILE_MONITOR_EVENT_DELETED:
        case G_FILE_MONITOR_EVENT_CREATED:
        case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
            if(g_queue_is_empty(event_queue) ||
               !g_file_equal((GFile *)g_queue_peek_tail(event_queue), file))
            {
                g_object_ref(file);
                g_queue_push_tail(event_queue, file);
                g_idle_add(handle_event_when_idle, NULL);
            }
        default:
            return;
    }
}

static GFile * get_parent(const gchar * uri)
{
    GFile * f = g_file_new_for_path(uri);
    if(!f)
        return NULL;

    GFile * parent = g_file_get_parent(f);
    g_object_unref(f);
    return parent;
}

void watch_file(const gchar * file_uri)
{
    GFile * parent = get_parent(file_uri);

    if(!parent)
        return;

    gchar * parent_uri = g_file_get_uri(parent);

    if(!watches)
    {
        watches = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, watch_free);
        event_queue = g_queue_new();
    }

    DirWatch * watch = (DirWatch *)g_hash_table_lookup(watches, parent_uri);
    if(watch)
        watch->refcount++;
    else
    {
        GError * error = NULL;
        watch = g_new(DirWatch, 1);
        watch->refcount = 1;
        watch->monitor = g_file_monitor_directory(parent, G_FILE_MONITOR_NONE, NULL, &error);
        if(error)
        {
            g_printerr("Unable to monitor directory %s (%s).\n", parent_uri, error->message);
            g_error_free(error);
            g_free(parent_uri);
            g_free(watch);
        }
        else
        {
            // connect to signal
            g_signal_connect(watch->monitor, "changed", (gpointer)changed_callback, NULL);
            g_hash_table_insert(watches, parent_uri, watch);
        }
    }
    g_object_unref(parent);
}

void unwatch_file(const gchar * file_uri)
{
    GFile * parent = get_parent(file_uri);

    if(!parent)
        return;

    gchar * parent_uri = g_file_get_uri(parent);

    DirWatch * watch = (DirWatch *)g_hash_table_lookup(watches, parent_uri);
    if(watch)
    {
        if(--watch->refcount == 0)
            g_hash_table_remove(watches, watch);
    }
    else
        g_warning("Tried to unwatch unknown file: %s", file_uri);

    g_object_unref(parent);
}

