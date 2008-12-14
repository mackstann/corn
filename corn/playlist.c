#include "config.h"

#include "playlist.h"
#include "music.h"
#include "music-control.h"
#include "main.h"
#include "parsefile.h"
#include "configuration.h"
#include "dbus.h"

#include "string.h"

// annotations of algorithmic complexity use 'n' to refer
// to the length of the playlist (unless otherwise noted)

GArray * playlist = NULL;
static GArray * playlist_random = NULL;

PlaylistItem * playlist_current = NULL;
gint playlist_position = -1;

static void listitem_init(PlaylistItem * item, gchar * path, gchar ** p, guint nalts)
{
    item->main_path = path;
    item->paths = p;
    item->use_path = 0; /*they cycle to the end.. g_random_int_range(0, nalts);*/
}

static void listitem_destroy(PlaylistItem * item)
{
    g_strfreev(item->paths);
    g_free(item->main_path);
}

/*
static gboolean
get_file_utf8 (const gchar * path, gchar ** f, gchar ** u)
{
    if(g_utf8_validate(path, -1, NULL))
    {
        if(!(*f = g_filename_from_utf8 (path, -1, NULL, NULL, NULL)))
        {
            g_critical(_("Skipping '%s'. Could not convert from UTF-8. "
                          "Bug?"), path);
            return FALSE;
        }
        *u = g_strdup(path);
    } else {
        if(!(*u = g_filename_to_utf8 (path, -1, NULL, NULL, NULL)))
        {
            g_warning(_("Skipping '%s'. Could not convert to UTF-8. "
                         "See the README for a possible solution."), path);
            return FALSE;
        }
        *f = g_strdup(path);
    }
    return TRUE;
}
*/

void playlist_init(void)
{
    playlist = g_array_new(FALSE, FALSE, sizeof(PlaylistItem));
    playlist_random = g_array_new(FALSE, FALSE, sizeof(gint));
}

void playlist_destroy(void)
{
    // TODO delete elements
    g_array_free(playlist, TRUE);
    g_array_free(playlist_random, TRUE);
}

static void playlist_append(PlaylistItem * item)
{
    gint rand_pos_after_current = g_random_int_range(
        playlist_position+1,
        playlist_random->len+1
    ); // +1 to put it after the chosen index

    g_array_append_val(playlist, *item); // O(1)

    gint last_song_index = playlist->len-1; // need lvalue for insert_val
    g_array_insert_val(playlist_random, rand_pos_after_current, last_song_index); // O(n)

    if(!playlist_current)
    {
        playlist_current = &g_array_index(playlist, PlaylistItem, playlist->len-1);
        playlist_position = 0;
    }

}

void playlist_append_single(const gchar * path)
{
    g_return_if_fail(path != NULL);
    g_return_if_fail(g_utf8_validate(path, -1, NULL));

    if(!parse_file(path)) /* XXX PASS A GERROR**? .. and shit @ client */
        return;

#if 0
#include <xine/compat.h>
    // check against this
        XINE_PATH_MAX
#endif

    gchar ** paths = g_new(gchar*, 2);
    paths[0] = g_strdup(path);
    paths[1] = NULL;

    PlaylistItem item;
    listitem_init(&item, g_strdup(path), paths, 1);
    playlist_append(&item);
}

void playlist_append_alternatives(const gchar * path, gchar * const * alts)
{
    gchar ** paths;
    guint i, nalts = 0;

    g_return_if_fail(alts != NULL && alts[0] != NULL);

    while(alts[nalts]) ++nalts;

    paths = g_new(gchar*, nalts + 1);
    paths[nalts] = NULL;

    for(i = 0; i < nalts; ++i)
    {
        g_return_if_fail(g_utf8_validate(alts[i], -1, NULL));
        paths[i] = g_strdup(alts[i]);
    }

    PlaylistItem item;
    listitem_init(&item, g_strdup(path), paths, nalts);
    playlist_append(&item);
}

void playlist_replace_path(guint track, const gchar * path)
{
    guint i, nalts = 0;
    gchar ** p, ** alts;

    g_return_if_fail(playlist_current != NULL);

    alts = p = playlist_current->paths;

    while(alts[nalts]) ++nalts;

    if(track > nalts - 1)
    {
        g_assert(track == nalts); /* cant be more than one past the end! */
        playlist_current->paths =
            g_renew(gchar*, playlist_current->paths, track + 2);
        alts[nalts - 1] = NULL;
        nalts++;
    }

    for(i = 0; i < track; ++i) ++p;
    g_free(*p);
    *p = g_strdup(path);
    for(++p; *p; ++p)
    {
        g_free(*p);
        *p = NULL;
    }

    if(track < nalts - 1)
        playlist_current->paths =
            g_renew(gchar*, playlist_current->paths, track + 2);
}

gboolean playlist_fail(void)
{
    PlaylistItem * item;
    guint nalts = 0;

    g_return_val_if_fail(playlist_current != NULL, FALSE);

    item = playlist_current;
    while(item->paths[nalts])
        ++nalts;
    if(nalts - 1 > item->use_path)
    {
        ++item->use_path;
        /* try again */
        return TRUE;
    } else {
        /*playlist_remove(g_list_position(playlist, playlist_current));*/
        playlist_advance(1, config_loop_at_end);
        return TRUE;
    }
    return FALSE;
}

#define swap(type, a, b) do { \
        type tmp = a; \
        a = b; \
        b = tmp; \
    } while(0)

void playlist_rerandomize(void) // O(n)
{
    // http://en.wikipedia.org/wiki/Fisher-Yates_shuffle
    for(gint n = playlist->len; n > 1;) // O(n)
    {
        gint k = g_random_int_range(0, n--);
        swap(gint,
            g_array_index(playlist_random, gint, n),
            g_array_index(playlist_random, gint, k));
    }
}

#define array_index_of_value(arr, typ, val) (__extension__({ \
    g_assert(sizeof(val) == sizeof(typ)); \
    gint index = -1; \
    for(gint i = 0; i < arr->len; i++) \
        if(!memcmp(&g_array_index(arr, typ, i), &val, sizeof(val))) { \
            index = i; \
            break; \
        } \
    index; }))

void playlist_advance(gint num, gboolean loop)
{
    static gint rand_cur = -1;
    gboolean looped = FALSE;
    gint wasplaying = music_playing;

    if(!playlist->len || G_UNLIKELY(!num))
        return;

    if(!config_repeat_track)
    {
        if(config_random_order)
        {
            rand_cur = array_index_of_value(playlist_random, gint, playlist_position); // O(n)

            while(num > 0)
            {
                num--;
                if(++rand_cur >= playlist_random->len)
                {
                    playlist_rerandomize();
                    rand_cur = 0;
                    looped = TRUE;
                }
            }
            while(num < 0)
            {
                num++;
                if(--rand_cur < 0)
                {
                    playlist_rerandomize();
                    rand_cur = playlist_random->len - 1;
                    looped = TRUE;
                }
            }
            playlist_position = g_array_index(playlist_random, gint, rand_cur);
            playlist_current = &g_array_index(playlist, PlaylistItem, playlist_position);
        } else {
            while(num-- > 0)
            {
                if(++playlist_position >= playlist->len)
                {
                    playlist_position = 0;
                    looped = TRUE;
                }
            }
            while(num++ < 0)
            {
                if(--playlist_position < 0)
                {
                    playlist_position = playlist->len - 1;
                    looped = TRUE;
                }
            }
            playlist_current = &g_array_index(playlist, PlaylistItem, playlist_position);
        }
    }

    music_stop();
    if((!looped || loop) && wasplaying == MUSIC_PLAYING)
        music_play();

    mpris_player_emit_track_change(mpris_player);
}

void playlist_seek(gint track)
{
    if G_UNLIKELY(track < 0) return;
    if G_UNLIKELY(track >= playlist->len) return;

    gint was_playing = music_playing;

    playlist_current = &g_array_index(playlist, PlaylistItem, track);
    playlist_position = track;

    /* this function is used during load, and we don't want to start
       playing necessarily */
    music_stop();
    if(was_playing == MUSIC_PLAYING)
        music_play();
    else if(was_playing == MUSIC_PAUSED)
        music_playing = MUSIC_PAUSED;

    mpris_player_emit_track_change(mpris_player);
}

void playlist_clear(void)
{
    music_stop();

    while(playlist->len--)
        listitem_destroy(&g_array_index(playlist, PlaylistItem, playlist->len));

    g_array_remove_range(playlist, 0, playlist->len);
    g_array_remove_range(playlist_random, 0, playlist_random->len);
    playlist_current = NULL;
    playlist_position = -1;
}

void playlist_remove(gint track)
{
    if G_UNLIKELY(track < 0) return;
    if G_UNLIKELY(track >= playlist->len) return;

    if(track == playlist_position)
        playlist_advance(1, config_loop_at_end);

    // if we're still in the same spot, there was no track to advance to
    if(track == playlist_position)
        playlist_position = 0;

    if(track < playlist_position)
        playlist_position--;

    PlaylistItem * item = &g_array_index(playlist, PlaylistItem, track); // O(1)

    // this takes the last item in the array, puts it in x's place, and
    // decrements the array size.
    g_array_remove_index_fast(playlist_random, // O(n)
        array_index_of_value(playlist_random, gint, track)
    );

    for(gint i = 0; i < playlist_random->len; i++)
        if(g_array_index(playlist_random, gint, i) > track)
            g_array_index(playlist_random, gint, i)--;

    listitem_destroy(item);

    g_array_remove_index(playlist, track); // O(n)

    if(!playlist->len)
    {
        playlist_current = NULL;
        playlist_position = -1;
    }
    else
        playlist_current = &g_array_index(playlist, PlaylistItem, 0); // O(1)
}

void playlist_move(gint track, gint dest)
{
    if G_UNLIKELY(track == dest) return;
    if G_UNLIKELY(track < 0) return;
    if G_UNLIKELY(track >= playlist->len) return;

    PlaylistItem * item = &g_array_index(playlist, PlaylistItem, track);

    if(dest > track)
        ++dest;

    g_array_insert_val(playlist, dest, *item); // O(n)
    g_array_remove_index(playlist, track); // O(n)

    if(track == playlist_position)
        playlist_position = dest;
    else if(track < playlist_position && dest > playlist_position)
        playlist_position--;
    else if(track > playlist_position && dest <= playlist_position)
        playlist_position++;
}

