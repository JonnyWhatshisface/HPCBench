
#ifndef _hpcbench_h
#define _hpcbench_h

#define _GNU_SOURCE

#include <ctype.h>
#include <dirent.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <memory.h>
#include <pthread.h>
#include <regex.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifdef __LP64__
#define FMT_U64 "ld"
#else
#define FMT_U64 "lld"
#endif

#define MAX_PARALLEL_TESTS 64
#define MAX_CPU_THREADS 16
#define MAX_CPU_COUNT 224
#define MAX_SOCKET_COUNT 64
#define MAX_THREADS_PER_CPU 128
#define MAX_PCI_DEVICE_COUNT 4096
#define PCI_ADDRESS_SPACE 0x100 // 256 bytes

typedef struct cpu cpu;
typedef struct package package;
typedef struct context context;
typedef struct scheduler scheduler;
typedef struct worker worker;
typedef struct test test;
typedef struct plugins plugins;
typedef struct tests tests;
typedef struct arguments arguments;
typedef struct test_tracker test_tracker;
typedef struct vm vm;
typedef struct pci_dev pci_dev;
typedef struct numa_node numa_node;

typedef enum
{
    LINUX,
    WINDOWS,
    SOLARIS,
    BSD,
    MAC
} plugin_target;

typedef enum
{
    SCHED,
    NOSCHED
} testreq;

typedef enum
{
    PRIMARY,
    SIBLING
} THREAD_TYPE;

struct arguments
{
    const char *name;
    int has_arg;
    int *flag;
    int val;
    char *description;
};

struct numa_node {
    unsigned int id;
    package *package;
};

struct cpu
{
    unsigned int id;
    unsigned int numa_node;
    package *package;
    THREAD_TYPE type;
    cpu *primary;
    cpu *siblings[MAX_CPU_COUNT];
};

struct pci_dev {
    package *socket;
    package **cpus;
};

struct package
{
    unsigned int id;
    unsigned int socket;
    numa_node *node;
    cpu *cpus[MAX_CPU_COUNT];
};

struct context
{
    package *packages[MAX_SOCKET_COUNT];
    package *sockets[MAX_SOCKET_COUNT];
    cpu *cpus[MAX_CPU_COUNT];
    pci_dev *pci_devices[MAX_PCI_DEVICE_COUNT];
    cpu **requestedcpus;
    test **tests;
    vm *vm;
    int debug;
    int use_cgroups;
    int skip_affinity_set;
    int main_cpu;
    unsigned int socket_count;
    unsigned int logical_cpu_count;
    unsigned int threads_per_core;
    unsigned int physical_core_count;
    unsigned int ttpc; // Test threadspercore
    unsigned int wpt;  // Workersper test. Cores * ttpc
    pthread_mutex_t context_lock;
    plugins *plugins;
    struct option *params; // getopt params
    char *outputpath;
    int argc;
    char **argv;
    char plugin_path[4096];
    void *custom; // Could be used for cross-plugin chatter (test->ctx->custom)
};

struct scheduler
{
    pthread_t thread;
    char *cgroup;
    test *test;
    void *custom;
    uint64_t start;
    uint64_t end;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int ready;
};

struct worker
{
    int id;
    int cpuid;
    pthread_t thread;
    char *cgroup;
    test *test;
    uint64_t start;
    uint64_t end;
    worker *next;
};

struct test
{
    struct plugins *plugin;
    context *ctx;
    worker **workers;
    scheduler *scheduler;
    test_tracker *tracker;
    uint64_t start;
    uint64_t end;
    void *custom;
    pthread_mutex_t lock;
    pthread_cond_t cond;
};

struct test_tracker
{
    test *test;
    pthread_t thread;
    char *cgroup;
    uint64_t start;
    uint64_t end;
    uint64_t setup_start;
    uint64_t setup_end;
    uint64_t test_start;
    uint64_t test_end;
    uint64_t finalize_start;
    uint64_t finalize_end;
};

struct plugins
{
    const char *name;
    char path[4096];
    plugin_target targetos;
    const char *description;
    void (*usage)(void);
    testreq scheduler;
    scheduler *sched;
    void (*setuptask)(test *);
    void (*workertask)(test *, worker *);
    void (*schedtask)(scheduler *);
    void (*finalizetask)(test *);
    plugins *next;
    void *phandle;
    arguments *options;
    struct option *longopts;
    char **argv;
};

struct vm {
    context *ctx;
};

int init_test(test *);
int create_workers(test *test);
void *worker_setup_function(void *);
void cleanup_and_exit(context *);
int parse_and_run_test(context *);
void _usage(context *);
void _plugins_list(context *);
void *test_worker_function(void *);
test_tracker *initialize_test(test *);
void *test_tracker_function(void *);
void *test_scheduler_function(void *);
void *test_setup_function(void *);
void *test_finalize_function(void *);
void error_and_exit(char *, int);

// Shared functions for plugins
uint64_t hpcb_realtime_ms(void);
uint64_t hpcb_usertime_ms(void);
uint64_t hpcb_tsccycles(void);
unsigned int hpcb_get_thread_cpu(worker *);
unsigned int hpcb_get_thread_node(worker *);
unsigned int hpcb_set_thread_cpu(worker *, unsigned int);
int hpcb_check_if_number(char *);
void hpcb_debug_out(char *);


#endif /* hpcbench_h */
