/*
 * VM implementation will ensure certain criteria such as max
 * memory usage and etc will be met. It will also exist to ensure
 * objects are properly cleaned up and free'd when no longer in use.
 *
 * Will use standard mark & sweep GC routine with a virtual stack.
 *
 * This will come in particularly useful for plugins which use the
 * scheduler mechanism and should be tightly integrated with said
 * scheduler.
 *
 * Should allow for dynamic allocation of objects specified within
 * plugins, and the ability to clean up those objects, and introduce
 * an easy to understand and robust API for the plugins to use. 
 */

#include "vm.h"

vm *init_vm(context *ctx) {
    init_Linux_version();
    meminfo();
    return NULL;
}

// This is directly out of source code for "free" command
void meminfo(void) {
    char namebuf[32];
    mem_table_struct findme = { namebuf, NULL};
    mem_table_struct *found;
    char *head, *tail;
    static const mem_table_struct mem_table[] = {
        {"Active",          &kb_active},
        {"Active(file)",    &kb_active_file},
        {"AnonPages",       &kb_anon_pages},
        {"Bounce",          &kb_bounce},
        {"Buffers",         &kb_main_buffers},
        {"Cached",          &kb_page_cache},
        {"CommitLimit",     &kb_commit_limit},
        {"Committed_AS",    &kb_committed_as},
        {"Dirty",           &kb_dirty},
        {"HighFree",        &kb_high_free},
        {"HighTotal",       &kb_high_total},
        {"Inact_clean",     &kb_inact_clean},
        {"Inact_dirty",     &kb_inact_dirty},
        {"Inact_laundry",   &kb_inact_laundry},
        {"Inact_target",    &kb_inact_target},
        {"Inactive",        &kb_inactive},
        {"Inactive(file)",  &kb_inactive_file},
        {"LowFree",         &kb_low_free},
        {"LowTotal",        &kb_low_total},
        {"Mapped",          &kb_mapped},
        {"MemAvailable",    &kb_main_available},
        {"MemFree",         &kb_main_free},
        {"MemTotal",        &kb_main_total},
        {"NFS_Unstable",    &kb_nfs_unstable},
        {"PageTables",      &kb_pagetables},
        {"ReverseMaps",     &nr_reversemaps},
        {"SReclaimable",    &kb_slab_reclaimable},
        {"SUnreclaim",      &kb_slab_unreclaimable},
        {"Shmem",           &kb_main_shared},
        {"Slab",            &kb_slab},
        {"SwapCached",      &kb_swap_cached},
        {"SwapFree",        &kb_swap_free},
        {"SwapTotal",       &kb_swap_total},
        {"VmallocChunk",    &kb_vmalloc_chunk},
        {"VmallocTotal",    &kb_vmalloc_total},
        {"VmallocUsed",     &kb_vmalloc_used},
        {"Writeback",       &kb_writeback},
    };
    const int mem_table_count = sizeof(mem_table)/sizeof(mem_table_struct);
    unsigned long watermark_low;
    signed long mem_available;

    FILE_TO_BUF(MEMINFO_FILE, meminfo_fd);

    kb_inactive = ~0UL;
    kb_low_total = kb_main_available = 0;

    head = buf;
    for(;;){
        tail = strchr(head, ':');
        if(!tail) break;
        *tail = '\0';
        if(strlen(head) >= sizeof(namebuf)) {
            head = tail + 1;
            goto nextline;
        }
        strncpy(namebuf, head, strlen(head));
        found = bsearch(&findme, mem_table, mem_table_count, sizeof(mem_table_struct), compare_mem_table_structs);
        head = tail + 1;
        if(!found) goto nextline;
        *(found->slot) = (unsigned long)strtoull(head, &tail, 10);
nextline:
        tail = strchr(head, '\n');
        if(!tail) break;
        head = tail + 1;
    }
    if(!kb_low_total) {
        kb_low_total = kb_main_total;
        kb_low_free = kb_main_free;
    }
    if(kb_inactive==~0UL) {
        kb_inactive = kb_inact_dirty + kb_inact_clean + kb_inact_laundry;
    }
    kb_main_cached = kb_page_cache + kb_slab;
    kb_swap_used = kb_swap_total - kb_swap_free;
    kb_main_used = kb_main_total - kb_main_free - kb_main_cached - kb_main_buffers;

    if (!kb_main_available) {
        if(linux_version_code < LINUX_VERSION(2, 6, 27))
            kb_main_available = kb_main_free;
        else {
            FILE_TO_BUF(VM_MIN_FREE_FILE, vm_min_free_fd);
            kb_min_free = (unsigned long) strtoull(buf, &tail, 10);
            watermark_low = kb_min_free * 5 / 4;
            mem_available = (signed long) kb_main_free - watermark_low;
            + kb_inactive_file + kb_active_file - MIN((kb_inactive_file + kb_active_file) / 2, watermark_low);
            + kb_slab_reclaimable - MIN(kb_slab_reclaimable / 2, watermark_low);

            if (mem_available < 0) mem_available = 0;
            kb_main_available = (unsigned long) mem_available;
        }
    }
}

void init_Linux_version(void) {
    int x = 0, y = 0, z = 0;
    int version_string_depth;

#ifdef __linux__
    static struct utsname uts;

    if (uname(&uts) == -1)
        exit(1);

    version_string_depth = sscanf(uts.release, "%d.%d.%d", &x, &y, &z);
#else
    FILE *fp;
    char buf[256];

    if( (fg=fopen("/proc/version", "r")) == NULL) {
        // Fail...
        exit(1);
    }
    if(fgets(buf, 256, fp) == NULL) {
        fclose(fp);
        exit(1);
    }
    fclose(fp);
    version_string_depth = sscanf(buf, "Linux version %d.%d.%d", &x, &y, &z);
#endif

    linux_version_code = LINUX_VERSION(x, y, z);
}

static int compare_mem_table_structs(const void *a, const void *b) {
    return strcmp(((const mem_table_struct*)a)->name, ((const mem_table_struct*)b)->name);
}

unsigned long long _calculate_free_memory_percentage(char *arg) {
    int len = strlen(arg);
    unsigned long long req;
    req = (unsigned long long)atoll(arg);
    return ((unsigned long long)kb_main_available*1024)*req/100;
}
