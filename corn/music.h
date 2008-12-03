#ifndef __music_h
#define __music_h

#include <glib.h>

#ifndef G_GNUC_PRINTF
#  define G_GNUC_PRINTF()
#endif

extern gboolean music_playing;

void music_init       ();
void music_destroy    ();
void music_play       ();
void music_pause      ();
void music_stop       ();
/* this should probably go in some other namespace... */
void music_add_notify (GIOChannel *output);

void music_notify_add_song     (const gchar *song, gint pos);
void music_notify_remove_song  (gint pos);
void music_notify_move_song    (gint from, gint to);
void music_notify_current_song (gint pos);
void music_notify_song_failed  ();
void music_notify_playing      ();
void music_notify_paused       ();
void music_notify_stopped      ();
void music_notify_clear        ();

#endif
