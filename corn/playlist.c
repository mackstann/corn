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

static void listitem_init(PlaylistItem * item, gchar * path, GList * alts)
{
    item->main_path = path;
    item->paths = alts;
    item->use_path = item->paths;
}

static void listitem_destroy(PlaylistItem * item)
{
    for(GList * it = item->paths; it; it = g_list_next(it))
        g_free(it->data);
    g_list_free(item->paths);
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

void playlist_append(const gchar * path, GList * alts)
{
    g_return_if_fail(g_utf8_validate(path, -1, NULL));

    if(!alts)
    {
        if(!parse_file(path))
            return;
        alts = g_list_append(alts, g_strdup(path)); // XXX
    }

    for(GList * it = alts; it; it = g_list_next(it))
        g_return_if_fail(g_utf8_validate(it->data, -1, NULL));

    PlaylistItem item;
    listitem_init(&item, g_strdup(path), alts);

    g_array_append_val(playlist, item);

    if(playlist_position == -1)
        reset_playlist_position();
}

void playlist_replace_path(guint track, const gchar * path)
{
    g_return_if_fail(PLAYLIST_CURRENT_ITEM() != NULL);

    gint nalts = g_list_length(PLAYLIST_CURRENT_ITEM()->paths);

    if(track > nalts - 1)
    {
        g_assert(track == nalts); /* can't be more than one past the end! */
        PLAYLIST_CURRENT_ITEM()->paths = g_list_append(PLAYLIST_CURRENT_ITEM()->paths, g_strdup(path));
    }
    else
    {
        GList * replaced = g_list_nth(PLAYLIST_CURRENT_ITEM()->paths, track);
        g_free(replaced->data);
        replaced->data = g_strdup(path);
    }
}

void playlist_fail(void)
{
    g_return_if_fail(PLAYLIST_CURRENT_ITEM() != NULL);

    PlaylistItem * item = PLAYLIST_CURRENT_ITEM();
    item->use_path = g_list_next(item->use_path);
    if(!item->use_path)
    {
        g_warning("Couldn't play %s", MAIN_PATH(item));
        item->use_path = item->paths;
        /*playlist_remove(g_list_position(playlist, PLAYLIST_CURRENT_ITEM())); */
        playlist_advance(1);
    }
}

void playlist_advance(gint how)
{
    gboolean looped = FALSE;
    gint wasplaying = music_playing;

    if(!playlist->len || G_UNLIKELY(!how))
        return;

    if(!config_repeat_track)
    {
        if(config_random_order)
        {
            if(how > 0)
                playlist_position = plrand_next(playlist_position, playlist->len);
            else if(how < 0)
                playlist_position = plrand_prev(playlist_position, playlist->len);
        }
        else
        {
            looped = TRUE;
            playlist_position += how;
            if(playlist_position < 0)
                playlist_position = playlist->len - 1;
            else if(playlist_position == playlist->len)
                playlist_position = 0;
            else
                looped = FALSE;
        }
    }

    music_stop();
    if((!looped || config_loop_at_end) && wasplaying == MUSIC_PLAYING)
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
    if(G_UNLIKELY(track < 0)) return;
    if(G_UNLIKELY(track >= playlist->len)) return;

    if(track == playlist_position)
        playlist_advance(1);

    if(track < playlist_position)
        playlist_position--;

    listitem_destroy(&g_array_index(playlist, PlaylistItem, track));

    g_array_remove_index(playlist, track); // O(n)

    plrand_shift_track_numbers(track + 1, playlist->len - 1, -1);
    plrand_forget_track(track);

    // if we're still in the same spot, there was no track to advance to.  set
    // to track 0, or if we're removing last song, set to -1.
    if(track == playlist_position)
        reset_playlist_position();

}

void playlist_move(gint track, gint dest)
{
    if(G_UNLIKELY(track == dest)) return;
    if(G_UNLIKELY(track < 0)) return;
    if(G_UNLIKELY(track >= playlist->len)) return;

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
