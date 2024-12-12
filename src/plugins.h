#ifndef _plugins_h
#define _plugins_h

#include <dirent.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <getopt.h>

int plugins_scan(context *);
int plugin_register_options(context *, plugins *);
int plugin_destroy(context *, plugins *);

#endif