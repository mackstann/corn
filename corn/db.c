#include "config.h"

#include "db.h"
#include "main.h"
#include "music-metadata.h"

#include <sqlite3.h>

#include <glib-object.h>
#include <glib.h>

#include <stdlib.h>

static sqlite3 * db = NULL;

static GHashTable * to_update = NULL;
static GHashTable * to_remove = NULL;

static sqlite3_stmt * insert_stmt;
static sqlite3_stmt * delete_stmt;
static sqlite3_stmt * select_stmt;
static sqlite3_stmt * begin_stmt;
static sqlite3_stmt * commit_stmt;

static gint transaction_start_time = 0;

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
    ")";

static const char * sql_item_insert =
    "insert or replace into metadata ("
    "    location, artist, title, album, tracknumber, ms, samplerate, bitrate"
    ") values (?, ?, ?, ?, ?, ?, ?, ?)";

static const char * sql_item_delete =
    "delete from metadata where location = ?";

static const char * sql_item_select =
    "select * from metadata where location = ?";

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
    INIT_TRY(sqlite3_prepare_v2(db, sql_item_select, -1, &select_stmt, NULL), "Couldn't prepare select statement");

    INIT_TRY(sqlite3_prepare_v2(db, "begin",  -1, &begin_stmt,  NULL), "Couldn't prepare begin statement");
    INIT_TRY(sqlite3_prepare_v2(db, "commit", -1, &commit_stmt, NULL), "Couldn't prepare commit statement");

    INIT_TRY(sqlite3_step(begin_stmt), "Couldn't begin transaction");

    to_update = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    to_remove = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    return 0;
}

void db_destroy(void)
{
    sqlite3_reset(commit_stmt);
    sqlite3_step(commit_stmt);

    sqlite3_finalize(insert_stmt);
    sqlite3_finalize(delete_stmt);
    sqlite3_finalize(select_stmt);
    sqlite3_finalize(begin_stmt);
    sqlite3_finalize(commit_stmt);

    if(db)
        sqlite3_close(db);

    g_hash_table_unref(to_update);
    g_hash_table_unref(to_remove);
}

// CRUD

static void update_with_metadata(const gchar * uri, GHashTable * meta)
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

GHashTable * db_get(const gchar * uri)
{
    sqlite3_reset(select_stmt);
    sqlite3_bind_text(select_stmt, 1, uri, -1, SQLITE_STATIC);

    int result;
    do {
        result = sqlite3_step(select_stmt);
    } while(result == SQLITE_BUSY);

    if(result != SQLITE_ROW)
    {
        if(result != SQLITE_DONE)
            printerr("Couldn't step select statement");

        // not in db yet, fetch manually and insert it while we have it
        GHashTable * meta = music_get_playlist_item_metadata(uri);
        update_with_metadata(uri, meta);
        return meta;
    }

    GHashTable * meta = g_hash_table_new(g_str_hash, g_str_equal);
    add_metadata_from_string(meta, "location",    sqlite3_column_text(select_stmt, 0));
    add_metadata_from_string(meta, "artist",      sqlite3_column_text(select_stmt, 1));
    add_metadata_from_string(meta, "title",       sqlite3_column_text(select_stmt, 2));
    add_metadata_from_string(meta, "album",       sqlite3_column_text(select_stmt, 3));
    add_metadata_from_string(meta, "tracknumber", sqlite3_column_text(select_stmt, 4));

    if(sqlite3_column_text(select_stmt, 5))
        add_metadata_from_int(meta, "ms",               sqlite3_column_int(select_stmt, 5));
    if(sqlite3_column_text(select_stmt, 6))
        add_metadata_from_int(meta, "audio-samplerate", sqlite3_column_int(select_stmt, 6));
    if(sqlite3_column_text(select_stmt, 7))
        add_metadata_from_int(meta, "audio-bitrate",    sqlite3_column_int(select_stmt, 7));

    return meta;
}

static void update(const gchar * uri)
{
    GHashTable * meta = music_get_playlist_item_metadata(uri);
    update_with_metadata(uri, meta);
    g_hash_table_unref(meta);
}

static void remove(const gchar * uri)
{
    sqlite3_reset(delete_stmt);
    sqlite3_bind_text(delete_stmt, 1, uri, -1, SQLITE_STATIC);
    TRY(sqlite3_step(delete_stmt), "Couldn't step delete statement");
}

// idle callback functions

static gboolean do_when_idle(GHashTable * table, void (* runfunc)(const gchar *))
{
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, table);
    if(!g_hash_table_iter_next(&iter, &key, &value))
        return FALSE;

    if(main_time_counter - transaction_start_time >= 10)
    {
        sqlite3_reset(commit_stmt);
        sqlite3_reset(begin_stmt);
        sqlite3_step(commit_stmt);
        sqlite3_step(begin_stmt);
        transaction_start_time = main_time_counter;
    }

    const gchar * path = (const gchar *)key;
    runfunc(path);
    g_hash_table_iter_remove(&iter);

    return !!g_hash_table_size(table);
}

static gboolean update_when_idle(G_GNUC_UNUSED gpointer data)
{
    return do_when_idle(to_update, update);
}

static gboolean remove_when_idle(G_GNUC_UNUSED gpointer data)
{
    return do_when_idle(to_remove, remove);
}

// scheduling functions

static void schedule(GHashTable * table, GHashTable * othertable,
        gboolean (* idlefunc)(gpointer), const gchar * path)
{
    g_hash_table_remove(othertable, path);
    gboolean was_empty = !g_hash_table_size(table);
    g_hash_table_insert(table, g_strdup(path), GINT_TO_POINTER(1));
    if(was_empty)
        g_idle_add_full(G_PRIORITY_LOW, idlefunc, NULL, NULL);
}

void db_schedule_update(const gchar * path)
{
    schedule(to_update, to_remove, update_when_idle, path);
}

void db_schedule_remove(const gchar * path)
{
    schedule(to_remove, to_update, remove_when_idle, path);
}

