#include "playlist.h"
#include "playlist-random.h"
#include "music.h"
#include "music-control.h"
#include "music-metadata.h"
#include "mpris-player.h"
#include "mpris-tracklist.h"
#include "main.h"
#include "parsefile.h"
#include "sniff-file.h"
#include "state-settings.h"
#include "dbus.h"
#include "watch.h"
#include "db.h"

#define playlist_mtime_never -1
#define playlist_save_wait_time 5

static GArray * playlist = NULL;
static gint position = -1;

// this is set to playlist_mtime_never if current playlist has been saved to
// disk.  otherwise, it's set to the time at which the playlist was last
// modified.  we only trigger a save-to-disk when playlist modification
// activity has died down for a little bit (determined by
// playlist_save_wait_time)
static gint playlist_mtime = playlist_mtime_never;

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

inline gint     playlist_position(void) { return position; }
inline gint     playlist_length(void)   { return playlist ? playlist->len : 0; }
inline gboolean playlist_empty(void)    { return !playlist || !playlist->len; }
inline gchar *  playlist_nth(gint i)    { return g_array_index(playlist, gchar *, i); }
inline gchar *  playlist_current(void)  { return g_array_index(playlist, gchar *, position); }
inline gboolean playlist_modified(void) { return playlist_mtime != playlist_mtime_never; }
inline void     playlist_mark_as_flushed(void) { playlist_mtime = playlist_mtime_never; }

gboolean playlist_flush_due(void)
{
    return playlist_modified() &&
        main_time_counter - playlist_mtime >= playlist_save_wait_time;
}

static void touch()
{
    if(main_status == CORN_RUNNING)
        playlist_mtime = main_time_counter;
    mpris_player_emit_caps_change(mpris_player);
    mpris_tracklist_emit_track_list_change(mpris_tracklist);
}

static inline void reset_position(void)
{
    if(playlist_empty())
        position = -1;
    else if(position == -1)
        position = 0;
}

void playlist_append(gchar * path) // takes ownership of the path passed in
{
    g_return_if_fail(path != NULL);
    g_return_if_fail(g_utf8_validate(path, -1, NULL));

    parse_file(path);

    gint len = g_queue_get_length(&found_files);
    for(gint i = 0; i < len; i++)
    {
        FoundFile * ff = g_queue_pop_head(&found_files);
        if(ff->type & SNIFFED_FILE)
        {
            g_array_append_val(playlist, ff->uri);
            db_schedule_update(ff->uri);
        }
        else if(ff->type & SNIFFED_DIRECTORY)
        {
            watch_dir(ff->uri);
            g_free(ff->uri);
        }

        g_free(ff);
    }

    reset_position();
    touch();
}

void playlist_replace_path(const gchar * path)
{
    g_return_if_fail(!playlist_empty());
    g_free(playlist_current());
    g_array_index(playlist, gchar *, position) = g_strdup(path);
    touch();
}

void playlist_advance(gint how)
{
    gboolean looped = FALSE;
    gint wasplaying = music_playing;

    if(playlist_empty() || G_UNLIKELY(!how))
        return;

    if(!setting_repeat_track)
    {
        if(setting_random_order)
        {
            if(how > 0)
                position = plrand_next(position, playlist_length());
            else if(how < 0)
                position = plrand_prev(position, playlist_length());
        }
        else
        {
            looped = TRUE;
            position += how;
            if(position < 0)
                position = playlist_length() - 1;
            else if(position == playlist_length())
                position = 0;
            else
                looped = FALSE;
        }
    }

    music_stop();
    if((!looped || setting_loop_at_end) && wasplaying == MUSIC_PLAYING)
        music_play();

    mpris_player_emit_track_change(mpris_player);
    mpris_player_emit_caps_change(mpris_player);
}

void playlist_seek(gint track)
{
    if(G_UNLIKELY(track < 0)) return;
    if(G_UNLIKELY(track >= playlist_length())) return;

    gint was_playing = music_playing;

    plrand_record_past(position);

    position = track;

    /* this function is used during load, and we don't want to start
       playing necessarily */
    music_stop();
    if(was_playing == MUSIC_PLAYING)
        music_play();
    else if(was_playing == MUSIC_PAUSED)
        music_playing = MUSIC_PAUSED;

    mpris_player_emit_track_change(mpris_player);
    mpris_player_emit_caps_change(mpris_player);
}

void playlist_clear(void)
{
    music_stop();

    for(gint i = 0; i < playlist_length(); i++)
    {
        db_schedule_remove(playlist_nth(i));
        g_free(playlist_nth(i));
    }

    g_array_set_size(playlist, 0);

    plrand_clear();

    reset_position();
    touch();
}

void playlist_remove(gint track)
{
    if(G_UNLIKELY(track < 0)) return;
    if(G_UNLIKELY(track >= playlist_length())) return;

    if(track == position)
        playlist_advance(1);

    if(track < position)
        position--;

    db_schedule_remove(playlist_nth(track));

    g_free(playlist_nth(track));
    g_array_remove_index(playlist, track); // O(n)

    plrand_shift_track_numbers(track + 1, playlist_length() - 1, -1);
    plrand_forget_track(track);

    // if we're still in the same spot, there was no track to advance to.  set
    // to track 0, or if we're removing last song, set to -1.
    if(track == position)
        reset_position();

    touch();
}

void playlist_move(gint track, gint dest)
{
    // XXX segfaults
    if(G_UNLIKELY(track == dest)) return;
    if(G_UNLIKELY(track < 0)) return;
    if(G_UNLIKELY(track >= playlist_length())) return;

    if(dest > track)
        plrand_shift_track_numbers(track+1, dest-1, -1);
    else if(dest < track)
        plrand_shift_track_numbers(dest, track-1, +1);

    plrand_move_track(track, dest);

    gchar * item = playlist_nth(track);
    g_array_insert_val(playlist, (dest > track ? dest+1 : dest), item); // O(n)
    g_array_remove_index(playlist, track); // O(n)

    if(track == position)
        position = dest;
    else if(track < position && dest > position)
        position--;
    else if(track > position && dest <= position)
        position++;

    touch();
}
