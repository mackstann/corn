#include "watch.h"
#include "playlist.h"
#include "music-metadata.h"
#include "db.h"

#include <gio/gio.h>
#include <glib.h>

#include <string.h>

static GQueue event_queue = G_QUEUE_INIT;
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
    GFile * file = g_queue_pop_head(&event_queue);
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
            playlist_append(path);
    }
    return !g_queue_is_empty(&event_queue);
}

void changed_callback(G_GNUC_UNUSED GFileMonitor * monitor,
                                    GFile * file,
                      G_GNUC_UNUSED GFile * other_file,
                                    GFileMonitorEvent event_type,
                      G_GNUC_UNUSED gpointer user_data)
{
    gchar * path = g_file_get_path(file);
    if(event_type == G_FILE_MONITOR_EVENT_CHANGED) g_message("%-40s %s", "G_FILE_MONITOR_EVENT_CHANGED", path);
    if(event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT) g_message("%-40s %s", "G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT", path);
    if(event_type == G_FILE_MONITOR_EVENT_DELETED) g_message("%-40s %s", "G_FILE_MONITOR_EVENT_DELETED", path);
    if(event_type == G_FILE_MONITOR_EVENT_CREATED) g_message("%-40s %s", "G_FILE_MONITOR_EVENT_CREATED", path);
    if(event_type == G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED) g_message("%-40s %s", "G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED", path);
    if(event_type == G_FILE_MONITOR_EVENT_PRE_UNMOUNT) g_message("%-40s %s", "G_FILE_MONITOR_EVENT_PRE_UNMOUNT", path);
    if(event_type == G_FILE_MONITOR_EVENT_UNMOUNTED) g_message("%-40s %s", "G_FILE_MONITOR_EVENT_UNMOUNTED", path);
    g_free(path);

    switch(event_type)
    {
        case G_FILE_MONITOR_EVENT_CHANGED:
        case G_FILE_MONITOR_EVENT_DELETED:
        case G_FILE_MONITOR_EVENT_CREATED:
        case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
            if(g_queue_is_empty(&event_queue) ||
               !g_file_equal((GFile *)g_queue_peek_tail(&event_queue), file))
            {
                g_object_ref(file);
                g_queue_push_tail(&event_queue, file);
                if(g_queue_get_length(&event_queue) == 1)
                    g_idle_add(handle_event_when_idle, NULL);
            }
        default:
            return;
    }
}

void watch_file(const gchar * file_uri)
{
    if(!watches)
        watches = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, watch_free);

    DirWatch * watch = (DirWatch *)g_hash_table_lookup(watches, file_uri);
    if(watch)
        watch->refcount++;
    else
    {
        GError * error = NULL;
        watch = g_new(DirWatch, 1);
        watch->refcount = 1;
        GFile * f = g_file_new_for_uri(file_uri);
        watch->monitor = g_file_monitor_directory(f, G_FILE_MONITOR_NONE, NULL, &error);
        g_object_unref(f);
        if(error)
        {
            g_printerr("Unable to monitor directory %s (%s).\n", file_uri, error->message);
            g_error_free(error);
            g_free(watch);
        }
        else
        {
            // connect to signal
            g_signal_connect(watch->monitor, "changed", (gpointer)changed_callback, NULL);
            g_hash_table_insert(watches, g_strdup(file_uri), watch);
        }
    }
}

void unwatch_file(const gchar * file_uri)
{
    DirWatch * watch = (DirWatch *)g_hash_table_lookup(watches, file_uri);
    if(watch)
    {
        if(--watch->refcount == 0)
            g_hash_table_remove(watches, watch);
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

void watch_parent(const gchar * file_uri)
{
    GFile * parent = get_parent(file_uri);

    if(!parent)
        return;

    gchar * parent_uri = g_file_get_uri(parent);
    watch_file(parent_uri);
    g_free(parent_uri);
    g_object_unref(parent);
}

void unwatch_parent(const gchar * file_uri)
{
    GFile * parent = get_parent(file_uri);

    if(!parent)
        return;

    gchar * parent_uri = g_file_get_uri(parent);
    unwatch_file(parent_uri);
    g_free(parent_uri);
    g_object_unref(parent);
}

