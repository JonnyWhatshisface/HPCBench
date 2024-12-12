#ifndef _utilities_h
#define _utilities_h

#include <fcntl.h>
#include <stdint.h>
#include <sys/utsname.h>

#ifdef __LP64__
#define FMT_U64 "ld"
#else
#define FMT_U64 "lld"
#endif

#define CPU_REGEX "^cpu[[:digit:]]+$"
#define PHYSICAL_PACKAGE_ID_PATH "/sys/devices/system/cpu/cpu%d/topology/physical_package_id"
#define THREAD_SIBLING_LIST_PATH "/sys/devices/system/cpu/cpu%d/topology/thread_siblings_list"

#define STAT_FILE "/proc/stat"
#define UPTIME_FILE "/proc/uptime"
#define LOADAVG_FILE "/proc/loadavg"
#define MEMINFO_FILE "/proc/meminfo"
#define VMINFO_FILE "/proc/vmstat"
#define VM_MIN_FREE_FILE "/proc/sys/vm/min_free_kbytes"


#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define BAD_OPEN_MESSAGE "ERROR: /proc must be mounted!\n"

#define FILE_TO_BUF(filename, fd) do                           \
    {                                                          \
        static int local_n;                                    \
        if (fd == -1 && (fd = open(filename, O_RDONLY)) == -1) \
        {                                                      \
            fputs(BAD_OPEN_MESSAGE, stderr);                   \
            fflush(NULL);                                      \
            _exit(102);                                        \
        }                                                      \
        lseek(fd, 0L, SEEK_SET);                               \
        if ((local_n = read(fd, buf, sizeof buf - 1)) < 0)     \
        {                                                      \
            perror(filename);                                  \
            fflush(NULL);                                      \
            _exit(103);                                        \
        }                                                      \
        buf[local_n] = '\0';                                   \
    }                                                          \
    while (0)

static char buf[8192]; // CLEAN


typedef struct mem_table_struct
{
    const char *name;
    unsigned long *slot;
} mem_table_struct;

int check_if_percentage(char *);
int check_if_number(char *);
unsigned long long _calculate_free_memory_percentage(char *);
void build_cpu_map(context *);
void build_pci_dev_map(context *);
char *get_pci_dev_affinity(pci_dev *);
int set_pci_dev_affinity(pci_dev *, char *);
void print_cpu_info(context *);
void free_cpu_map(context *);
uint64_t realtime_ms(void);
uint64_t usertime_ms(void);
uint64_t tsccyles(void);
cpu **build_requested_cpu_map(context *, char *);
test **build_requested_tests(context *, char *);
int check_output_path(context *, char *);
char *get_binary_path(void);
void configure_cgroups(context *, char *);

static inline int getcpuid()
{
#ifdef SYS_getcpu
    int cpu, status;
    status = syscall(SYS_getcpu, &cpu, NULL, NULL);
    return (status == -1) ? status : cpu;
#else
    return -1;
#endif
}

static inline int getnodeid()
{
#ifdef SYS_getcpu
    int node, status;
    status = syscall(SYS_getcpu, NULL, &node, NULL);
    return (status == -1) ? status : node;
#else
    return -1;
#endif
}

#endif