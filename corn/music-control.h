#ifndef __corn_music_control_h__
#define __corn_music_control_h__

#include <glib.h>

void music_play(void);
void music_pause(void);
void music_stop(void);
void music_seek(gint ms);
void music_set_volume(gint vol);

#endif
