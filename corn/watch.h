#ifndef __corn_dirwatch_h__
#define __corn_dirwatch_h__

#include <gio/gio.h>
#include <glib.h>

void watch_file(const gchar * file_uri);
void unwatch_file(const gchar * file_uri);
void watch_parent(const gchar * file_uri);
void unwatch_parent(const gchar * file_uri);

#endif
