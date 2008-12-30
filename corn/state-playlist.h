#ifndef __corn_state_playlist_h__
#define __corn_state_playlist_h__

#include <glib.h>

void state_playlist_init(void);
void state_playlist_destroy(void);
void state_playlist_launch_save_if_time_has_come(void);

#endif
