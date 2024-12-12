/*
 * Memory Benchmark Test plugin for HPCBench. 
 *
 * Compile with cc -o memtest.so memtest.c -shared -fPIC -lnuma
*/
#define _GNU_SOURCE

#include <stdio.h>
#include <time.h>
#include <getopt.h>
#include <numaif.h>
#include <numa.h>
#include <x86intrin.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "../../hpcbench.h"

#define PLUGIN_NAME "memtest"
#define PLUGIN_DESCRIPTION "Test plugin for memory tinkering"
#define SCHEDULER NOSCHED;
#define TARGETOS LINUX;

#define NOP0 __asm__ __volatile__ ("nop" : : : "eax");
#define NOP1 NOP0 NOP0
#define NOP2 NOP1 NOP1
#define NOP3 NOP2 NOP2
#define NOP4 NOP3 NOP3
#define NOP5 NOP4 NOP4
#define NOP6 NOP5 NOP5
#define NOP7 NOP6 NOP6
#define NOP8 NOP7 NOP7
#define NOP9 NOP8 NOP8
#define NOP10 NOP9 NOP9
#define NOP11 NOP10 NOP10
#define NOP12 NOP11 NOP11

#define NOPS 4096
#define NLOOP 1000

#define MAX_NODES 8

typedef enum memtest_alloc_type {
    ALLOC_GLIBC,
    ALLOC_LIBNUMA
} ALLOC_TYPE;

typedef enum memtest_alloc_locale {
    ALLOC_LOCALE_DEFAULT,
    ALLOC_LOCALE_LOCAL,
    ALLOC_LOCALE_LOCAL_ONNODE,
    ALLOC_LOCALE_REMOTE,
    ALLOC_LOCALE_ONNODE
} ALLOC_LOCALE;

typedef struct cpmemtest cpmemtest;
typedef struct memlat memlat;
typedef struct memlat_cpu memlat_cpu;
typedef struct memtest memtest;
typedef struct nodeobj nodeobj;
typedef struct memobj memobj;

struct memtest {
    unsigned int cpuid;
    void **allocated_memories;
};

struct memobj {
    unsigned int size;
    unsigned int node;
    unsigned int allocated_by_cpu;
    unsigned int allocated_by_node;
    void *data;
};

struct nodeobj {
    unsigned int node;
    memobj **objects;
};

struct memlat_cpu {
    unsigned int cpu;
    unsigned int node;
    unsigned int local_write_highest;
    unsigned int local_write_lowest;
    unsigned int local_read_highest;
    unsigned int local_read_lowest;
    unsigned int remote_write_highest;
    unsigned int remote_write_lowest;
    unsigned int remote_read_highest;
    unsigned int remote_read_lowest;
};

struct memlat {
    unsigned int node;
    unsigned int local_write_highest;
    unsigned int local_write_lowest;
    unsigned int local_read_highest;
    unsigned int local_read_lowest;
    unsigned int remote_write_highest;
    unsigned int remote_write_lowest;
    unsigned int remote_read_highest;
    unsigned int remote_read_lowest;
    memlat_cpu **cpus;
};

struct cpmemtest {
    unsigned int total_local;
    unsigned int total_remote;
    unsigned int min_size;
    unsigned int max_size;
    unsigned int min_size_local;
    unsigned int max_size_local;
    long long int total_size_local;
    unsigned int min_size_remote;
    unsigned int max_size_remote;
    long long int total_size_remote;
    long long int total_size;
    unsigned int total_pages;
    unsigned int split_pages;
    nodeobj **nodes;
    memobj **memobjects;
    long long int node_allocation_count[MAX_NODES];
    // Control
    unsigned int check_latencies;
    unsigned int random_delay;
    unsigned int random_delay_min;
    unsigned int random_delay_max;
    unsigned int alloccount;
    unsigned int free_after_each;
    unsigned int clflush;
    unsigned int do_noploop;
    unsigned int noploop_count;
    unsigned int check_split_pages;
    unsigned int min_alloc_size;
    unsigned int max_alloc_size;
    ALLOC_TYPE alloc_type;
    ALLOC_LOCALE alloc_locale;
    unsigned int alloc_onnode_id;
};

// Forward declarations for plugin functionality
plugins *hpcb_plugin_info(context *) __attribute__((used));
void hpcb_plugin_setup(test *) __attribute__((used));
void hpcb_plugin_entry(test *, worker *) __attribute__((used));
void hpcb_plugin_finalize(test *) __attribute__((used));
void hpcb_plugin_scheduler_entry(scheduler *) __attribute__((used));

// Declarations for plugin functions
int get_node_of_address(void *);
void alloc_test(test *, worker *);
void test_latencies(test *, worker *);
void do_noploop(int);
int do_count_split_pages(void *, int);
void do_clflush(test *);

static inline int getcpuid() {
    #ifdef SYS_getcpu
    int cpu, status;
    status = syscall(SYS_getcpu, &cpu, NULL, NULL);
    return (status == -1) ? status : cpu;
    #else
    return -1; // Unavailable...
    #endif
}

static inline int getnodeid() {
    #ifdef SYS_getcpu
    int node, status;
    status = syscall(SYS_getcpu, NULL, &node, NULL);
    return (status == -1) ? status : node;
    #else
    return -1; // Unavailable...
    #endif
}



struct arguments args[] = { 
    // If no args, then make this null
    {"alloc-count", required_argument, 0, 'a', "Number of allocations per thread"},
    {"clflush-all", no_argument, 0, 'b', "Flush all allocations from cache before latency benchmarks"},
    {"free-after-each", no_argument, 0, 'c', "Free each allocation immediately after initializing"},
    {"min-size", required_argument, 0, 'e', "Number of allocations per thread"},
    {"max-size", required_argument, 0, 'f', "Number of allocations per thread"},
    {"alloc-type", required_argument, 0, 'g', "Number of allocations per thread"},
    {"alloc-locale", required_argument, 0, 'G', "Number of allocations per thread"},
    {"check-latencies", no_argument, 0, 'Y', "Perform memory latency benchmarks"},
    {"check-splits", no_argument, 0, 'z', "Number of allocations per thread"},
    {"nop-loops-after-each", required_argument, 0, 'Z', "Number of allocations per thread"},
    {"random-delay", no_argument, 0, 'u', "Number of allocations per thread"},
    {"random-delay-min", required_argument, 0, 'U', "Number of allocations per thread"},
    {"random-delay-max", required_argument, 0, 'v', "Number of allocations per thread"},
    {NULL, 0, NULL, 0} // Very important - need this to find the end!
};

plugins *hpcb_plugin_info(context *ctx) {
    plugins *self = calloc(1, sizeof(plugins));
    self->name = PLUGIN_NAME;
    self->description = PLUGIN_DESCRIPTION;
    self->scheduler = SCHEDULER;
    self->targetos = TARGETOS;
    self->options = args; // Or NULL if no args...
    return self;
}

void hpcb_plugin_setup(test *test) {
    optind = 1; // Always need to reset this
    int opts = 0, i = 0;

    cpmemtest *mt = calloc(1, sizeof(struct cpmemtest));
    test->custom = (void *) mt;

    mt->free_after_each = 0;
    mt->total_pages = 0;
    mt->total_size = 0;
    mt->total_pages = 0;
    mt->clflush = 0;
    mt-> do_noploop = 0;
    mt->min_alloc_size = 4096;
    mt->max_alloc_size= 950000;
    mt->alloccount = 100;
    mt->noploop_count = 2;
    mt->check_split_pages = 0;
    mt->total_size_local = 0;
    mt->total_size_remote = 0;
    mt->random_delay = 0;
    mt->random_delay_min = 0;
    mt->random_delay_max = 5;
    mt->check_latencies = 0;
    mt->alloc_type = ALLOC_GLIBC;
    mt->alloc_locale = ALLOC_LOCALE_DEFAULT;

    for (i = 0; i <= MAX_NODES - 1; i++) {
        mt->node_allocation_count[i] = 0;
    }

    if (test->ctx->debug)
        printf("[memtest] Setting up test...\n");

    while ((opts = getopt_long(test->ctx->argc, test->plugin->argv, "a:bce:f:g:G:uU:v:YzZ:", test->plugin->longopts, NULL)) != -1) {
        switch(opts) {

            case 'a':
                mt->alloccount = atoi(optarg);
                break;
            case 'b':
                mt->clflush = 1;
                break;

            case 'c':
                mt->free_after_each = 1;
                break;

            case 'e':
                mt->min_alloc_size = atoi(optarg);
                break;

            case 'f':
                mt->max_alloc_size = atoi(optarg);
                break;

            case 'g':
                // Allocation type / library
                if (strcmp(optarg, "glibc") == 0) {
                    mt->alloc_type = ALLOC_GLIBC;
                } else if (strcmp(optarg, "libnuma") == 0) {
                    mt->alloc_type = ALLOC_LIBNUMA;
                } else {
                    printf("[memtest] Unknown allocation library... Exiting!\n");
                    exit(1);
                }
                break;

            case 'G':
                // Allocation locale
                if (strcmp(optarg, "default") == 0) {
                    mt->alloc_locale = ALLOC_LOCALE_DEFAULT;
                } else if (strcmp(optarg, "local") == 0) {
                    mt->alloc_locale = ALLOC_LOCALE_LOCAL;
                } else if (strcmp(optarg, "local-onnode") == 0) {
                    mt->alloc_locale = ALLOC_LOCALE_LOCAL_ONNODE;
                } else if (strcmp(optarg, "remote") == 0) {
                    mt->alloc_locale = ALLOC_LOCALE_REMOTE;
                } else {
                    // Should else if and check if digit!
                    mt->alloc_locale = ALLOC_LOCALE_ONNODE;
                    mt->alloc_onnode_id = atoi(optarg);
                }
                break;

            case 'u':
                mt->random_delay = 1;
                break;

            case 'U':
                mt->random_delay_min = atoi(optarg);
                break;

            case 'v':
                mt->random_delay_max = atoi(optarg);
                break;

            case 'Y':
                // Memory benchmark...
                mt->check_latencies = 1;
                break;

            case 'z':
                mt->split_pages = 0;
                mt->check_split_pages = 1;
                break;

        }
    }
    // Allocate enough room for all of the pointers to track the memory objects
    mt->memobjects = (memobj **) calloc((mt->alloccount * test->ctx->wpt), sizeof(memobj *));
    mt->nodes = (nodeobj **) calloc((MAX_NODES), sizeof(struct nodeobj *));
    for (i = 0; i <= MAX_NODES -1; i++) {
        nodeobj *n = calloc(1, sizeof(struct nodeobj));
        mt->nodes[i] = n;
        mt->nodes[i]->objects = (memobj **) calloc((mt->alloccount * test->ctx->wpt), sizeof(memobj *));
    }

    printf("[memtest] Will perform %d allocations (%d per thread, %d threads)\n[memtest] Allocation sizes (min/max): %d bytes / %d bytes\n", mt->alloccount * test->ctx->wpt, mt->alloccount, test->ctx->wpt, mt->min_alloc_size, mt->max_alloc_size);
    if (mt->random_delay)
        printf("[memtest] Using random delay of %d to %d seconds per thread...\n", mt->random_delay_min, mt->random_delay_max);
}

void hpcb_plugin_entry(test *test, worker *thread) {
    // Make allocations
    cpmemtest *mt = test->custom;
    alloc_test(test, thread);
    // Run benchmarks if desired
    if(mt->check_latencies)
        test_latencies(test, thread);
}

void hpcb_plugin_finalize(test *test) {
    cpmemtest *mt = test->custom;
    int i = 0, j = 0;

    printf("[memtest] Allocations and initialization complete...\n");
    printf("[memtest] Totals: %lld allocations, %lld bytes, %lld pages\n", (mt->total_local + mt->total_remote), mt->total_size, (mt->total_size / 4096));
    for (i = 0; i <= MAX_NODES - 1; i++) {
        if (mt->node_allocation_count[i] > 0)
            printf("[memtest] Total allocations on NODE %d: %lld\n", i, mt->node_allocation_count[i]);
        // Should print the bytes on each node...!
    }
    printf("[memtest] Total local allocations (count/min_size/max_size/total_bytes): %d/%d/%d/%lld\n[memtest] Total remote allocations (count/min_size/max_size/total_bytes): %d/%d/%d/%lld\n", mt->total_local, mt->min_size_local, mt->max_size_local, mt->total_size_local, mt->total_remote, mt->min_size_remote, mt->max_size_remote, mt->total_size_remote);
    if (mt->check_split_pages)
        printf("[memtest] Number of cross-node split pages: %d\n", mt->split_pages);
    printf("[memtest] Min/Max Allocation size: %d/%d\n", mt->min_size, mt->max_size);

    for (i = 0; i <= (mt->total_local + mt->total_remote -1); i++) {
        // Print statement if needed for debug purposes
        //if(test->ctx->debug)
            //printf("[memtest] Freeing %d bytes on node %d at %p\n", mt->memobjects[i]->size, mt->memobjects[i]->node, &mt->memobjects[i]->data);
        if (mt->alloc_type = ALLOC_LIBNUMA) {
            numa_free(mt->memobjects[i]->data, mt->memobjects[i]->size);
        } else {
            free(mt->memobjects[i]->data);
        }
        free(mt->memobjects[i]);
    }
}

void hpcb_plugin_scheduler_entry(scheduler *scheduler) {
    /*
        Not in use for this plugin but must exist for
        the plugin scan!
    */
}

/*
    Everything below here should be functions for the plugin
*/

int get_node_of_address(void *ptr) {
    int status[1], ret;
    status[0] = -1;
    ret = move_pages(0, 1, &ptr, NULL, status, 0);
    return (status[0] != -1) ? status[0] : -1;
}

void do_nopnoop(int loops) {
    int i, j;
    for (i = 0; i <= loops; ++i) {
        for (j = 0; j <= 4096; ++j) {
            NOP12;
        }
    }
}

int do_count_split_pages(void *ptr, int size) {
    /* 
        Gets node of start allocation and walks every
        4096 bytes checking locality of each page.
    */
    int i = 0, split_count = 0;
    int page_size = getpagesize();
    int memnode = get_node_of_address(&ptr);
    void *p = &ptr;

    for (i = 0; i <= size; i += (page_size / sizeof(void *))) {
        if (get_node_of_address(&p) != memnode)
            ++split_count;
        p += i;
    }

    return split_count;
}

void alloc_test(test *test, worker *thread) {
    cpmemtest *mt = test->custom;
    int cpu, node, delaytime = 0, i = 0;
    uint64_t ticks;
    struct timeval tm;
    pthread_mutex_t *lock = &test->ctx->context_lock;
    cpu = getcpuid();
    node = getnodeid();
    int page_size = getpagesize();
    unsigned int allocation_started_on = 0, numa_avail = 0, min_size = 0, max_size = 0;
    unsigned int remote_start = 0, local_start = 0, remote_pages = 0, size = 0, other_node = 0;
    // Hack for now... Change this!
    if (node == 0)
        other_node = 1;

    numa_avail = numa_available();

        if (mt->random_delay) {
        ticks = hpcb_tsccycles();
        gettimeofday(&tm, NULL);
        srand(tm.tv_sec + tm.tv_usec * (unsigned int) ticks);
        delaytime = rand() % (mt->random_delay_max - mt->random_delay_min + 1) + mt->random_delay_min;
        sleep(delaytime);
    }

    for (i = 0; i <= mt->alloccount - 1; ++i) {
        ticks = hpcb_tsccycles();
        gettimeofday(&tm, NULL);
        srand(tm.tv_sec + tm.tv_usec * (unsigned int) ticks);
        size = rand() % (mt->max_alloc_size - mt->min_alloc_size + 1) + mt->min_alloc_size;

        // Check size and track smallest/largest allocation
        if (size < min_size || min_size == 0)
            min_size = size;
        if (size > max_size || max_size == 0)
            max_size = size;

        void *test;
        switch (mt->alloc_type) {
            case ALLOC_GLIBC:
                // Allocate with glibc
                test = malloc(size);
                break;

            // TODO: Make all allocations page_size bytes aligned!
            // Should allocate (page_size * ((size + (page_size - 1)) / page_size);
            case ALLOC_LIBNUMA:
                // Allocate using libnuma
                switch (mt->alloc_locale) {
                    case ALLOC_LOCALE_DEFAULT:
                        //bm = numa_parse_nodestring(nodestr);
                        //numa_bind(bm);
                        test = numa_alloc_local(size);
                        break;

                    case ALLOC_LOCALE_LOCAL:
                        test = numa_alloc_onnode(size, node);
                        break;

                    case ALLOC_LOCALE_REMOTE:
                        test = numa_alloc_onnode(size, other_node);
                        break;

                    case ALLOC_LOCALE_ONNODE:
                        test = numa_alloc_onnode(size, mt->alloc_onnode_id);
                        break;
                }
                break;
        }

        // Allocation done. Add logic to initialize now or later!
        memset(test, 1, size);
        int node_placed = get_node_of_address(test);
        memobj *obj = calloc(1, sizeof(struct memobj));
        obj->size = size;
        obj->allocated_by_cpu = cpu;
        obj->allocated_by_node = node;
        obj->data = &test;
        obj->node = node_placed;

        if (node_placed != node) {
            pthread_mutex_lock(lock);
            ++remote_start;
            ++mt->total_remote;
            mt->total_size_remote += size;

            if (size < mt->min_size_remote || mt->min_size_remote == 0)
                mt->min_size_remote = size;
            if (size > mt->max_size_remote || mt->max_size_remote == 0)
                mt->max_size_remote = size;

            mt->nodes[node_placed]->objects[mt->total_remote] = obj;

        } else {
            pthread_mutex_lock(lock);
            ++local_start;
            ++mt->total_local;
            mt->total_size_local += size;

            if (size < mt->min_size_local || mt->min_size_local == 0)
                mt->min_size_local = size;
            if (size > mt->max_size_local || mt->max_size_local == 0)
                mt->max_size_local = size;

            mt->nodes[node_placed]->objects[mt->total_local] = obj;
        }

        mt->total_size += size;
        mt->node_allocation_count[node_placed]++;

        if (min_size < mt->min_size || mt->min_size == 0)
            mt->min_size = min_size;
        if (max_size > mt->max_size || mt->max_size == 0)
            mt->max_size = max_size;

        // Allocate an object to track later for the benchmarks

        mt->memobjects[mt->total_local + mt->total_remote - 1] = obj;

        pthread_mutex_unlock(lock);

        if (mt->check_split_pages) {
            int split_page_count = do_count_split_pages(test, size);
            if (split_page_count > 0) {
                pthread_mutex_lock(lock);
                mt->split_pages += split_page_count;
                remote_pages += split_page_count;
                pthread_mutex_unlock(lock);
            }
        }

        //if (mt->cl_flush)
            //_mm_clflush(&test);

        if (mt->free_after_each) {
            if (mt->alloc_type = ALLOC_GLIBC) {
                free(test);
            } else if (mt->alloc_type = ALLOC_LIBNUMA) {
                numa_free(test, size);
            }
        }
        if (mt->do_noploop)
            do_noploop(mt ->noploop_count);
    }

    if (test->ctx->debug)
        printf("[memtest][debug] CPU %d NODE %d: local[%d] - remote[%d] - split[%d] - delay[%d]\n", cpu, node, local_start, remote_start, remote_pages, delaytime);
}

void test_latencies(test *test, worker *thread) {
    cpmemtest *mt = test->custom;
    int cpu, node, delaytime = 0, i = 0, j = 0, other_node = 0;
    uint64_t ticks;
    struct timeval tm;
    pthread_mutex_t *lock = &test->ctx->context_lock;
    cpu = getcpuid();
    node = getnodeid();

    if (node == 0)
        other_node = 1;

    // Execute cache flush of all objects if desired
    if (mt->clflush)
        do_clflush(test);

    // First, local memory access times


}

void do_clflush(test *test) {
    // Flush every allocation out of cache line
    cpmemtest *mt = test->custom;
    int i, j;
    for (i = 0; i <= MAX_NODES -1; i++) {
        for (j = 0; j <= (mt->total_local + mt->total_remote - 1); j++) {
            if(mt->nodes[i]->objects[j] != NULL)
                _mm_clflush(&mt->nodes[i]->objects[j]->data);
        }
    }
}