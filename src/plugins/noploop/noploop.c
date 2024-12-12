/*
    Method employed adopted from Brendan Gregg's NOP loop benchmark.

    Compile: gcc -o noploop.so noploop.c -fPIC -shared
*/
#include <stdio.h>
#include <time.h>
#include <getopt.h>

#include "../../hpcbench.h"
#include "noploop.h"

#define PLUGIN_NAME "noploop"
#define PLUGIN_DESCRIPTION "Simple NOP loop test to measure CPU instruction performance."
#define SCHEDULER NOSCHED;
#define TARGETOS LINUX;

plugins *hpcb_plugin_info(context *) __attribute__((used));
void hpcb_plugin_setup(test *) __attribute__((used));
void hpcb_plugin_entry(test *, worker *) __attribute__((used));
void hpcb_plugin_finalize(test *) __attribute__((used));

struct arguments args[] = {
    {"loops", required_argument, 0, 'l', "Amount of loops to run"},
    {NULL, 0, NULL, 0}};

// Pointer to hpcb_realtime_ms symbol to shortcut
uint64_t (*gettime)(void) = hpcb_realtime_ms;

plugins *hpcb_plugin_info(context *ctx)
{
    plugins *self = calloc(1, sizeof(plugins));
    self->name = PLUGIN_NAME;
    self->description = PLUGIN_DESCRIPTION;
    self->scheduler = SCHEDULER;
    self->targetos = TARGETOS;
    self->options = args; // Or NULL if no args...
    return self;
}

void hpcb_plugin_setup(test *test)
{
    optind = 1;
    int opts = 0;

    noploopctx *noploop = calloc(1, sizeof(struct noploopctx));
    noploop->loops = 1;

    test->custom = (void *)noploop; // We use the void* provided by context to bring options in

    while ((opts = getopt_long(test->ctx->argc, test->plugin->argv, "l:", test->plugin->longopts, NULL)) != -1)
    {
        switch (opts)
        {

        case 'l':
            noploop->loops = atoi(optarg);
            break;
            
        }
    }
}

void hpcb_plugin_entry(test *test, worker *thread)
{
    noploopctx *noploop = test->custom; // Access our carried in ctx
    uint64_t start, end, instrate, period;
    uint64_t tscstart, tscend, tscticks, tscfreq;

    start = gettime();
    tscstart = hpcb_tsccycles();
    int i;
    for (i = 1; i <= noploop->loops * NLOOP; i++)
    {
        NOP12;
    }
    tscend = hpcb_tsccycles();
    end = gettime();

    tscticks = tscend - tscstart;
    period = end - start;
    tscfreq = tscticks / period;
    instrate = ((uint64_t)NOPS * NLOOP) / (period);
    if (test->ctx->debug)
        printf("Completed %d passes: CPU: [%d] THREAD: [%d] TIME: [%" FMT_U64 "ms] FREQ: [%" FMT_U64 "] TICKS: [%" FMT_U64 "] INSTR/cycle: [%" FMT_U64 "]\n", noploop->loops, thread->cpuid, thread->id, period, tscfreq, tscticks, (instrate + (tscfreq / 2)) / tscfreq);
}

void hpcb_plugin_finalize(test *test)
{
}

void hpcb_plugin_scheduler_entry(scheduler *scheduler)
{
}