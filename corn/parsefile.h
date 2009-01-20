#ifndef __parsefile_h
#define __parsefile_h

#include <glib.h>

void parse_file(const gchar * path);

extern GQueue found_files;

#endif
