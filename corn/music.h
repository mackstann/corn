#ifndef __music_h
#define __music_h

#include <glib.h>

#ifndef G_GNUC_PRINTF
#  define G_GNUC_PRINTF()
#endif

extern gboolean music_playing;
extern gint music_volume;

void music_init(void);
void music_destroy(void);
void music_play(void);
void music_pause(void);
void music_stop(void);
void music_set_volume(gint vol);
void music_seek(gint ms);
gint music_get_position(void);

#endif
