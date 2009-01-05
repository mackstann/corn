#include "music-control.h"
#include "music.h"
#include "playlist.h"
#include "mpris-player.h"
#include "dbus.h"

static void do_pause(void)
{
    music_stream_time = music_position();
    if(xine_get_status(music_stream) != XINE_STATUS_IDLE)
        xine_close(music_stream);
}

static void do_play(void)
{
    gint orig_pos = playlist_position();
    while(!music_try_to_play())
    {
        g_warning("Couldn't play %s", playlist_current());
        /*playlist_remove(g_list_position(playlist, playlist_current())); */
        playlist_advance(1);

        // if we keep failing to load files, and loop/repeat are on, we don't
        // want to infinitely keep trying to play the same file(s) that won't
        // play.  so once we fail and eventually get back to the same song,
        // stop.
        if(playlist_position() == orig_pos)
            return;
    }
}

void music_play(void)
{
    do_play();
    mpris_player_emit_caps_change(mpris_player); // new song, seekability may have changed
    mpris_player_emit_status_change(mpris_player);
}

void music_pause(void)
{
    do_pause();
    music_playing = MUSIC_PAUSED;
    mpris_player_emit_status_change(mpris_player);
}

void music_stop(void)
{
    do_pause();
    music_playing = MUSIC_STOPPED;
    music_stream_time = 0;
    mpris_player_emit_status_change(mpris_player);
}

void music_seek(gint ms)
{
    do_pause();
    music_stream_time = MAX(0, ms); // xine is smart.  no need to check upper bound.
    do_play();
}

void music_set_volume(gint vol)
{
    music_volume = CLAMP(vol, 0, 100);
    xine_set_param(music_stream, XINE_PARAM_AUDIO_VOLUME, music_volume);
}
