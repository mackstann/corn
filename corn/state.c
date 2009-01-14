#include "config.h"

#include "main.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <stdio.h>

FILE * state_file_open(const char * name, const char * mode)
{
    gchar * path = g_build_filename(g_get_user_data_dir(), main_instance_name, name, NULL);
    FILE * f = g_fopen(path, mode);
    g_free(path);
    return f;
}

