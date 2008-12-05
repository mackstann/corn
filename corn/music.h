#ifndef __music_h
#define __music_h

#include <glib.h>

enum {
    MUSIC_PLAYING = 0,
    MUSIC_PAUSED = 1,
    MUSIC_STOPPED = 2
} MusicPlayingStatus;

extern gint music_playing;
extern gint music_volume;

void music_init(void);
void music_destroy(void);
void music_play(void);
void music_pause(void);
void music_stop(void);
void music_set_volume(gint vol);
void music_seek(gint ms);
gint music_get_position(void);
GHashTable * music_get_metadata(void);
GHashTable * music_get_track_metadata(gint track);

#endif
