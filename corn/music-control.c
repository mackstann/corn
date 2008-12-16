#include "music-control.h"
#include "music.h"
#include "playlist.h"

static void do_pause(void)
{
    music_stream_time = music_get_position();
    if(xine_get_status(music_stream) != XINE_STATUS_IDLE)
        xine_close(music_stream);
}

void music_play(void)
{
    gint orig_pos = playlist_position;
    while(!music_try_to_play() && playlist_fail() && playlist_position != orig_pos);
    // if we keep failing to load files, and loop/repeat are on, we don't want
    // to infinitely keep trying to play the same file(s) that won't play.  so
    // once we fail and eventually get back to the same song, stop.
}

void music_seek(gint ms)
{
    music_pause();
    music_stream_time = MAX(0, ms); // xine is smart.  no need to check upper bound.
    music_play();
}

// position within current song, in ms
gint music_get_position(void)
{
    if(xine_get_status(music_stream) == XINE_STATUS_IDLE)
        return music_stream_time;
    gint pos, time, length;
    if(xine_get_pos_length(music_stream, &pos, &time, &length))
        return time;
    return 0;
}

void music_pause(void)
{
    do_pause();
    music_playing = MUSIC_PAUSED;
}

void music_stop(void)
{
    do_pause();
    music_playing = MUSIC_STOPPED;
    music_stream_time = 0;
}

void music_set_volume(gint vol)
{
    music_volume = CLAMP(vol, 0, 100);
    xine_set_param(music_stream, XINE_PARAM_AUDIO_VOLUME, music_volume);
}
