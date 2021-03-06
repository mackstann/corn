#ifndef __corn_db_h__
#define __corn_db_h__

#include <glib.h>

gint db_init(void);
void db_destroy(void);

void db_schedule_update(const gchar * uri);
void db_schedule_remove(const gchar * uri);
GHashTable * db_get(const gchar * uri);
GHashTable * db_get_noadd(const gchar * uri);

#endif
