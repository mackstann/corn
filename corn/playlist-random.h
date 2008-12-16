#ifndef __corn_playlist_random_h__
#define __corn_playlist_random_h__

#include <glib.h>

void plrand_init(void);
void plrand_destroy(void);
gint plrand_prev(gint current, gint playlist_len);
gint plrand_next(gint current, gint playlist_len);
void plrand_record_past(gint current);
void plrand_shift_track_numbers(gint atleast, gint atmost, gint inc);
void plrand_move_track(gint src, gint dest);
void plrand_forget_track(gint track);
void plrand_clear(void);

#endif
