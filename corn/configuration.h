#ifndef __corn_configuration_h__
#define __corn_configuration_h__

#include <glib.h>

extern gboolean config_loop_at_end;
extern gboolean config_random_order;
extern gboolean config_repeat_track;

void config_load(void);
void config_save(void);

gboolean config_maybe_save_playlist(gpointer data);

#endif
