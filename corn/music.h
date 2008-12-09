#ifndef __music_h
#define __music_h

#include <glib.h>
#include <xine.h>

enum {
    MUSIC_PLAYING = 0,
    MUSIC_PAUSED = 1,
    MUSIC_STOPPED = 2
} MusicPlayingStatus;

extern xine_t * xine;
extern xine_stream_t * music_stream;

extern gint music_playing;
extern gint music_volume;
extern gint music_stream_time;

int music_init(void);
void music_destroy(void);

gboolean music_try_to_play(void);

GHashTable * music_get_metadata(void);
GHashTable * music_get_track_metadata(gint track);

#endif
