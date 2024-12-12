#include "shared.h"
#include "hpcbench.h"
#include "utilities.h"

unsigned int hpcb_set_thread_cpu(worker *worker, unsigned int cpu)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    worker->cpuid = cpu;
    if (pthread_setaffinity_np(worker->thread, sizeof(cpu_set_t), &cpuset))
        return -1;

    return cpu;
}

unsigned int hpcb_get_thread_cpu(worker *worker)
{
    unsigned int cpu;
    cpu = getcpuid();
    return cpu;
}

unsigned int hpcb_get_thread_node(worker *worker)
{
    unsigned int node;
    node = getnodeid();
    return node;
}

uint64_t hpcb_realtime_ms(void)
{
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    return time.tv_sec * 1000L + time.tv_nsec / 1000000LL;
}

uint64_t hpcb_usertime_ms(void)
{
    return clock() / (CLOCKS_PER_SEC / 1000L);
}

uint64_t hpcb_tsccycles(void)
{
    uint32_t a, d;
    uint64_t cycles;
    __asm__ volatile("rdtscp" : "=a"(a), "=d"(d));
    cycles = (((uint64_t)d) << 32L) | (uint64_t)a;
    return cycles;
}
