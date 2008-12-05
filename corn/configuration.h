#ifndef __corn_confiuration_h__
#define __main_corn_confiuration_h__

#include <gconf/gconf-client.h>
#include <glib.h>

#define CORN_GCONF_ROOT "/apps/corn"

extern gboolean config_loop_at_end;
extern gboolean config_random_order;
extern gboolean config_repeat_track;

void config_load(GConfClient * gconf);
void config_save(GConfClient * gconf);
void config_changed(GConfClient * gconf, guint cnxn_id, GConfEntry * entry, gpointer data);

#endif

