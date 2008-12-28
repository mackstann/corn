#ifndef __corn_state_settings_h__
#define __corn_state_settings_h__

#include <glib.h>

extern gboolean setting_loop_at_end;
extern gboolean setting_random_order;
extern gboolean setting_repeat_track;

void state_settings_init(void);
void state_settings_destroy(void);

#endif
