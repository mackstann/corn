#include "config.h"

#include "playlist.h"
#include "playlist-random.h"
#include "music.h"
#include "music-control.h"
#include "main.h"
#include "parsefile.h"
#include "state-settings.h"
#include "dbus.h"

#include "string.h"

// annotations of algorithmic complexity use 'n' to refer
// to the length of the playlist (unless otherwise noted)

GArray * playlist = NULL;

gint playlist_position = -1;

// -1 if current playlist has been saved to disk.  otherwise, the time at which
// the playlist was last modified.  we only trigger a save-to-disk when
// playlist modification activity has died down for a little bit.
gint playlist_mtime = PLAYLIST_MTIME_NEVER;
gint playlist_save_wait_time = 5;

void playlist_init(void)
{
    playlist = g_array_new(FALSE, FALSE, sizeof(gchar *));
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
    else if(playlist->len > 1 && setting_random_order)
        playlist_position = g_random_int_range(0, playlist->len);
    else
        playlist_position = 0;
}

void playlist_append(gchar * path) // takes ownership of the path passed in
{
    g_return_if_fail(path != NULL);
    g_return_if_fail(g_utf8_validate(path, -1, NULL));

    if(parse_file(path))
        g_array_append_val(playlist, path);

    if(playlist_position == -1)
        reset_playlist_position();

    PLAYLIST_TOUCH();
}

void playlist_replace_path(const gchar * path)
{
    g_return_if_fail(playlist->len > 0);
    g_free(PLAYLIST_CURRENT_ITEM());
    PLAYLIST_ITEM_N(playlist_position) = g_strdup(path);
    PLAYLIST_TOUCH();
}

void playlist_advance(gint how)
{
    gboolean looped = FALSE;
    gint wasplaying = music_playing;

    if(!playlist->len || G_UNLIKELY(!how))
        return;

    if(!setting_repeat_track)
    {
        if(setting_random_order)
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
    if((!looped || setting_loop_at_end) && wasplaying == MUSIC_PLAYING)
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
        g_free(PLAYLIST_ITEM_N(i));

    g_array_set_size(playlist, 0);

    plrand_clear();

    playlist_position = -1;

    PLAYLIST_TOUCH();
}

void playlist_remove(gint track)
{
    if(G_UNLIKELY(track < 0)) return;
    if(G_UNLIKELY(track >= playlist->len)) return;

    if(track == playlist_position)
        playlist_advance(1);

    if(track < playlist_position)
        playlist_position--;

    g_free(PLAYLIST_ITEM_N(track));
    g_array_remove_index(playlist, track); // O(n)

    plrand_shift_track_numbers(track + 1, playlist->len - 1, -1);
    plrand_forget_track(track);

    // if we're still in the same spot, there was no track to advance to.  set
    // to track 0, or if we're removing last song, set to -1.
    if(track == playlist_position)
        reset_playlist_position();

    PLAYLIST_TOUCH();
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

    gchar * item = PLAYLIST_ITEM_N(track);
    g_array_insert_val(playlist, (dest > track ? dest+1 : dest), item); // O(n)
    g_array_remove_index(playlist, track); // O(n)

    if(track == playlist_position)
        playlist_position = dest;
    else if(track < playlist_position && dest > playlist_position)
        playlist_position--;
    else if(track > playlist_position && dest <= playlist_position)
        playlist_position++;

    PLAYLIST_TOUCH();
}
