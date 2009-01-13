#include "gettext.h"

#include "music-metadata.h"
#include "music.h"

#include <glib.h>
#include <glib-object.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>

static void free_gvalue_and_its_value(gpointer value)
{
    g_value_unset((GValue *)value);
    g_free((GValue *)value);
}

void add_metadata_from_string(GHashTable * meta, const gchar * name, const gchar * str)
{
    if(!str)
        return;

    gchar * u = g_locale_to_utf8(str, strlen(str), NULL, NULL, NULL);
    if(!u)
    {
        g_critical(_("Skipping %s value '%s'. Could not convert to UTF-8. Bug?"), name, str);
        return;
    }

    GValue * val = g_new0(GValue, 1);
    g_value_init(val, G_TYPE_STRING);
    g_value_set_string(val, u);
    g_hash_table_insert(meta, (gchar *)name, val);
}

void add_metadata_from_int(GHashTable * meta, const gchar * name, gint num)
{
    GValue * val = g_new0(GValue, 1);
    g_value_init(val, G_TYPE_INT);
    g_value_set_int(val, num);
    g_hash_table_insert(meta, (gchar *)name, val);
}

static GHashTable * get_stream_metadata(GHashTable * meta, xine_stream_t * strm)
{
    add_metadata_from_string(meta, "title", xine_get_meta_info(strm, XINE_META_INFO_TITLE));
    add_metadata_from_string(meta, "artist", xine_get_meta_info(strm, XINE_META_INFO_ARTIST));
    add_metadata_from_string(meta, "album", xine_get_meta_info(strm, XINE_META_INFO_ALBUM));
    add_metadata_from_string(meta, "tracknumber", xine_get_meta_info(strm,
                                                XINE_META_INFO_TRACK_NUMBER));

    gint pos, time, length;
    /* length = 0 for streams */
    if(xine_get_pos_length(strm, &pos, &time, &length) && length)
    {
        add_metadata_from_int(meta, "time", length / 1000);
        add_metadata_from_int(meta, "mtime", length);
    }

    add_metadata_from_string(meta, "genre", xine_get_meta_info(strm, XINE_META_INFO_GENRE));

    // "comment"
    // "rating" int [1..5] or [0..5]?

    const gchar * yearstr = xine_get_meta_info(strm, XINE_META_INFO_YEAR);
    if(yearstr)
    {
        gchar * end;
        gint year = (gint)strtol(yearstr, &end, 10);
        if(!errno && end != yearstr)
            add_metadata_from_int(meta, "year", year);
    }

    // "date" - timestamp of original performance - int

    add_metadata_from_int(meta, "audio-bitrate", xine_get_stream_info(strm, XINE_STREAM_INFO_BITRATE));
    add_metadata_from_int(meta, "audio-samplerate", xine_get_stream_info(strm, XINE_STREAM_INFO_AUDIO_SAMPLERATE));
    return meta;
}

GHashTable * music_get_playlist_item_metadata(const gchar * item)
{
    g_assert(item != NULL);

    GHashTable * meta = g_hash_table_new_full(g_str_hash, g_str_equal,
        NULL, // our keys are all static -- no free function for them
        free_gvalue_and_its_value); // XXX this is copy/paste, refactor

    add_metadata_from_string(meta, "location", item);

    xine_audio_port_t * audio = xine_open_audio_driver(xine, "none", NULL);

    g_return_val_if_fail(audio != NULL, meta);

    xine_stream_t * strm = xine_stream_new(xine, audio, NULL);
    if(!strm)
    {
        xine_close_audio_driver(xine, audio);
        g_return_val_if_fail(strm != NULL, meta);
    }

    gchar * path;
    if(!(path = g_filename_from_utf8(item, -1, NULL, NULL, NULL)))
        g_critical(_("Skipping getting track metadata for '%s'. "
                     "Could not convert from UTF-8. Bug?"), item);
    else if(xine_open(strm, path))
        get_stream_metadata(meta, strm);

    xine_dispose(strm);
    xine_close_audio_driver(xine, audio);
    return meta;
}

GHashTable * music_get_track_metadata(gint track)
{
    GHashTable * empty = g_hash_table_new(NULL, NULL);
    g_return_val_if_fail(track >= 0, empty);
    g_return_val_if_fail(track < playlist_length(), empty);
    g_hash_table_unref(empty);
    return music_get_playlist_item_metadata(playlist_nth(track));
}

GHashTable * music_get_current_track_metadata(void)
{
    // try to do it cheaply, using the already loaded stream
    if(music_stream && xine_get_status(music_stream) != XINE_STATUS_IDLE)
    {
        GHashTable * meta = g_hash_table_new_full(g_str_hash, g_str_equal,
            NULL, // our keys are all static -- no free function for them
            free_gvalue_and_its_value); // XXX refactor

        get_stream_metadata(meta, music_stream);
        add_metadata_from_string(meta, "location", playlist_current());
        return meta;
    }
    else if(playlist_position() != -1) // or do it the hard way
        return music_get_playlist_item_metadata(playlist_current());

    return g_hash_table_new(NULL, NULL);
}
