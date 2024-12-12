#ifndef _vm_h
#define _vm_h

#include "vm.h"
#include "hpcbench.h"
#include "utilities.h"
#include "plugins.h"

vm *init_vm(context *ctx);
static int compare_mem_table_structs(const void *, const void *);
void init_Linux_version(void);
void meminfo(void);

#endif