#include "config.h"

#include "playlist.h"
#include "playlist-random.h"
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

gint playlist_position = -1;

static void listitem_init(PlaylistItem * item, gchar * path, gchar ** alts)
{
    item->main_path = path;
    item->paths = alts;
    item->use_path = 0;
}

static void listitem_destroy(PlaylistItem * item)
{
    g_strfreev(item->paths);
    g_free(item->main_path);
}

void playlist_init(void)
{
    playlist = g_array_new(FALSE, FALSE, sizeof(PlaylistItem));
    plrand_init();
}

void playlist_destroy(void)
{
    plrand_destroy();
    playlist_clear();
    g_array_free(playlist, TRUE);
}

static inline void reset_playlist_position(void)
{
    if(!playlist || !playlist->len)
        playlist_position = -1;
    else if(playlist->len > 1 && config_random_order)
        playlist_position = g_random_int_range(0, playlist->len);
    else
        playlist_position = 0;
}

void playlist_append(gchar * path, gchar ** alts)
{
    g_return_if_fail(g_utf8_validate(path, -1, NULL));

    if(!parse_file(path))
        return;

    gchar * noalts[] = { path, NULL };
    if(!alts)
        alts = noalts;

    gint nalts = 0;
    while(alts && alts[nalts])
        ++nalts;

    gchar ** paths = g_new(gchar*, nalts + 1);
    paths[nalts] = NULL;

    for(gint i = 0; i < nalts; ++i)
    {
        g_return_if_fail(g_utf8_validate(alts[i], -1, NULL));
        paths[i] = g_strdup(alts[i]);
    }

    PlaylistItem item;
    listitem_init(&item, g_strdup(path), paths);

    g_array_append_val(playlist, item);

    if(playlist_position == -1)
        reset_playlist_position();
}

void playlist_replace_path(guint track, const gchar * path)
{
    guint i, nalts = 0;
    gchar ** p, ** alts;

    g_return_if_fail(PLAYLIST_CURRENT_ITEM() != NULL);

    alts = p = PLAYLIST_CURRENT_ITEM()->paths;

    while(alts[nalts]) ++nalts;

    if(track > nalts - 1)
    {
        g_assert(track == nalts); /* cant be more than one past the end! */
        PLAYLIST_CURRENT_ITEM()->paths =
            g_renew(gchar*, PLAYLIST_CURRENT_ITEM()->paths, track + 2);
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
        PLAYLIST_CURRENT_ITEM()->paths =
            g_renew(gchar*, PLAYLIST_CURRENT_ITEM()->paths, track + 2);
}

gboolean playlist_fail(void)
{
    PlaylistItem * item;
    guint nalts = 0;

    g_return_val_if_fail(PLAYLIST_CURRENT_ITEM() != NULL, FALSE);

    item = PLAYLIST_CURRENT_ITEM();
    while(item->paths[nalts])
        ++nalts;
    if(nalts - 1 > item->use_path)
    {
        ++item->use_path;
        /* try again */
        return TRUE;
    } else {
        /*playlist_remove(g_list_position(playlist, PLAYLIST_CURRENT_ITEM()));*/
        playlist_advance(1, config_loop_at_end);
        return TRUE;
    }
    return FALSE;
}

void playlist_advance(gint num, gboolean loop)
{
    gboolean looped = FALSE;
    gint wasplaying = music_playing;

    if(!playlist->len || G_UNLIKELY(!num))
        return;

    if(!config_repeat_track)
    {
        if(config_random_order)
        {
            if(num > 0)
                playlist_position = plrand_next(playlist_position, playlist->len);
            else if(num < 0)
                playlist_position = plrand_prev(playlist_position, playlist->len);
        }
        else
        {
            looped = TRUE;
            playlist_position += num;
            if(playlist_position < 0)
                playlist_position = playlist->len - 1;
            else if(playlist_position == playlist->len)
                playlist_position = 0;
            else
                looped = FALSE;
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

    plrand_record_past(playlist_position);

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

    for(gint i = 0; i < playlist->len; i++)
        listitem_destroy(&g_array_index(playlist, PlaylistItem, i));

    g_array_set_size(playlist, 0);

    plrand_clear();

    playlist_position = -1;
}

void playlist_remove(gint track)
{
    if G_UNLIKELY(track < 0) return;
    if G_UNLIKELY(track >= playlist->len) return;

    if(track == playlist_position)
        playlist_advance(1, config_loop_at_end);

    if(track < playlist_position)
        playlist_position--;

    listitem_destroy(&g_array_index(playlist, PlaylistItem, track));

    g_array_remove_index(playlist, track); // O(n)

    plrand_shift_track_numbers(track+1, playlist->len-1, -1);
    plrand_forget_track(track);

    // if we're still in the same spot, there was no track to advance to.  set
    // to track 0, or if we're removing last song, set to -1.
    if(track == playlist_position)
        reset_playlist_position();

}

void playlist_move(gint track, gint dest)
{
    if G_UNLIKELY(track == dest) return;
    if G_UNLIKELY(track < 0) return;
    if G_UNLIKELY(track >= playlist->len) return;

    if(dest > track)
        plrand_shift_track_numbers(track+1, dest-1, -1);
    else if(dest < track)
        plrand_shift_track_numbers(dest, track-1, +1);

    plrand_move_track(track, dest);

    PlaylistItem * item = &g_array_index(playlist, PlaylistItem, track);
    g_array_insert_val(playlist, (dest > track ? dest+1 : dest), *item); // O(n)
    g_array_remove_index(playlist, track); // O(n)

    if(track == playlist_position)
        playlist_position = dest;
    else if(track < playlist_position && dest > playlist_position)
        playlist_position--;
    else if(track > playlist_position && dest <= playlist_position)
        playlist_position++;
}

