#ifndef __corn_playlist_random_h__
#define __corn_playlist_random_h__

#include <glib.h>

void plrand_init(void);
void plrand_destroy(void);
gint plrand_prev(gint current, gint playlist_len);
gint plrand_next(gint current, gint playlist_len);

#endif

