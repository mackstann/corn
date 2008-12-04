#ifndef __music_h
#define __music_h

#include <glib.h>

#ifndef G_GNUC_PRINTF
#  define G_GNUC_PRINTF()
#endif

extern gboolean music_playing;
extern gint music_volume;

void music_init       ();
void music_destroy    ();
void music_play       ();
void music_pause      ();
void music_stop       ();
void music_set_volume(gint vol);
void music_seek(gint ms);
gint music_get_position(void);
/* this should probably go in some other namespace... */

#endif
