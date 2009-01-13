#include "config.h"

#include "db.h"
#include "main.h"
#include "music-metadata.h"

#include <sqlite3.h>

#include <glib-object.h>
#include <glib.h>

#include <stdlib.h>

static sqlite3 * db = NULL;

static GQueue * to_update = NULL;

sqlite3_stmt * insert_stmt;
sqlite3_stmt * delete_stmt;

static const char * sql_create_table =
    "create table if not exists metadata ("
    "    location text not null primary key,"
    "    artist text,"
    "    title text,"
    "    album text,"
    "    tracknumber text,"
    "    ms int,"
    "    samplerate int,"
    "    bitrate int"
    ");";

static const char * sql_item_insert =
    "insert or replace into metadata ("
    "    location, artist, title, album, tracknumber, ms, samplerate, bitrate"
    ") values (?, ?, ?, ?, ?, ?, ?, ?)";

static const char * sql_item_delete =
    "delete from metadata where location = ?";

static void printerr(const char * msg)
{
    g_printerr("%s (%s).\n", msg, db ? sqlite3_errmsg(db) : "?");
}

#define INIT_TRY(code, errmsg) do { \
        int result; \
        do { result = (code); } while(result == SQLITE_BUSY); \
        if(result != SQLITE_OK && result != SQLITE_DONE) \
        { \
            printerr(errmsg); \
            if(db) sqlite3_close(db); \
            return 40; \
        } \
    } while(0)

#define TRY(code, errmsg) do { \
        int result; \
        do { result = (code); } while(result == SQLITE_BUSY); \
        if(result != SQLITE_OK && result != SQLITE_DONE) \
        { \
            printerr(errmsg); \
            return; \
        } \
    } while(0)

gint db_init(void)
{
    gchar * db_path = g_build_filename(g_get_user_data_dir(), main_instance_name, "metadata.db", NULL);

    INIT_TRY(sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL),
             "Couldn't open metadata db file");

    g_free(db_path);

    sqlite3_stmt * create_stmt;

    INIT_TRY(sqlite3_prepare_v2(db, sql_create_table, -1, &create_stmt, NULL), "Couldn't prepare create statement");
    INIT_TRY(sqlite3_step(create_stmt), "Couldn't step create statement");
    INIT_TRY(sqlite3_finalize(create_stmt), "Couldn't finalize create statement");

    INIT_TRY(sqlite3_prepare_v2(db, sql_item_insert, -1, &insert_stmt, NULL), "Couldn't prepare insert statement");
    INIT_TRY(sqlite3_prepare_v2(db, sql_item_delete, -1, &delete_stmt, NULL), "Couldn't prepare delete statement");

    to_update = g_queue_new();

    return 0;
}

void db_destroy(void)
{
    sqlite3_finalize(insert_stmt);
    if(db)
        sqlite3_close(db);
    g_queue_free(to_update);
}

static void update(GHashTable * meta)
{
    sqlite3_reset(insert_stmt);
    sqlite3_clear_bindings(insert_stmt);

    GValue * location    = g_hash_table_lookup(meta, "location");
    GValue * artist      = g_hash_table_lookup(meta, "artist");
    GValue * title       = g_hash_table_lookup(meta, "title");
    GValue * album       = g_hash_table_lookup(meta, "album");
    GValue * tracknumber = g_hash_table_lookup(meta, "tracknumber");
    GValue * ms          = g_hash_table_lookup(meta, "mtime");
    GValue * samplerate  = g_hash_table_lookup(meta, "audio-samplerate");
    GValue * bitrate     = g_hash_table_lookup(meta, "audio-bitrate");

    if(location)    sqlite3_bind_text(insert_stmt, 1, g_value_get_string(location),    -1, SQLITE_STATIC);
    if(artist)      sqlite3_bind_text(insert_stmt, 2, g_value_get_string(artist),      -1, SQLITE_STATIC);
    if(title)       sqlite3_bind_text(insert_stmt, 3, g_value_get_string(title),       -1, SQLITE_STATIC);
    if(album)       sqlite3_bind_text(insert_stmt, 4, g_value_get_string(album),       -1, SQLITE_STATIC);
    if(tracknumber) sqlite3_bind_text(insert_stmt, 5, g_value_get_string(tracknumber), -1, SQLITE_STATIC);
    if(ms)          sqlite3_bind_int (insert_stmt, 6, g_value_get_int(ms));
    if(samplerate)  sqlite3_bind_int (insert_stmt, 7, g_value_get_int(samplerate));
    if(bitrate)     sqlite3_bind_int (insert_stmt, 8, g_value_get_int(bitrate));

    TRY(sqlite3_step(insert_stmt), "Couldn't step insert statement");
}

static gboolean update_when_idle(G_GNUC_UNUSED gpointer data)
{
    update(music_get_playlist_item_metadata(g_queue_peek_head(to_update)));
    g_free(g_queue_pop_head(to_update));
    return !g_queue_is_empty(to_update);
}

void db_schedule_update(const gchar * path)
{
    g_queue_push_tail(to_update, g_strdup(path));
    if(g_queue_get_length(to_update) == 1)
        g_idle_add(update_when_idle, NULL);
}

void db_remove(const gchar * uri)
{
    sqlite3_reset(delete_stmt);
    sqlite3_clear_bindings(delete_stmt);
    sqlite3_bind_text(delete_stmt, 1, uri, -1, SQLITE_STATIC);
    TRY(sqlite3_step(delete_stmt), "Couldn't step delete statement");
}
