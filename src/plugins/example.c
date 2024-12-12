
#include <stdio.h>
#include <time.h>
#include <getopt.h>

#include "../hpcbench.h"

#define PLUGIN_NAME "example"
#define PLUGIN_DESCRIPTION "Test plugin for HPCBench"
#define SCHEDULER NOSCHED;
#define TARGETOS LINUX;

plugins *hpcb_plugin_info(context *) __attribute__((used));
void hpcb_plugin_setup(test *) __attribute__((used));
void hpcb_plugin_entry(test *, worker *) __attribute__((used));
void hpcb_plugin_finalize(test *) __attribute__((used));
void hpcb_plugin_scheduler_entry(scheduler *) __attribute__((used));

struct arguments args[] = { 
    // If no args, then make this null
    {"testarg1", required_argument, 0, 'a', "Test argument - arg required"},
    {"testarg2", no_argument, 0, 'b', "Test argument - no arg required"},
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
    int opts = 0;

    printf("    [%s] Entered setup function\n", test->plugin->name);
    while ((opts = getopt_long(test->ctx->argc, test->plugin->argv, "a:b", test->plugin->longopts, NULL)) != -1) {
        switch(opts) {

            case 'a':
                printf("    [%s] Testarg1: %s\n", test->plugin->name, optarg);
                break;

            case 'b':
                printf("    [%s] Option testarg2 received\n", test->plugin->name);
                break;
        }
    }

    sleep(1); // Add some time to the clock for test
    printf("    [%s] Setup complete!\n", test->plugin->name);
}

void hpcb_plugin_entry(test *test, worker *thread) {
    printf("    [%s] Entered worker function...\n", test->plugin->name);
    printf("    [%s] Running on CPU %d\n", test->plugin->name, hpcb_get_thread_cpu(thread));
    sleep(15);
    printf("    [%s] Switching to CPU 3...\n", test->plugin->name);
    hpcb_set_thread_cpu(thread, 3);
    printf("    [%s] Thread is now running on CPU %d\n", test->plugin->name, hpcb_get_thread_cpu(thread));
    sleep(15);
    printf("    [%s] Finished...\n", test->plugin->name);
}

void hpcb_plugin_finalize(test *test) {

}

void hpcb_plugin_scheduler_entry(scheduler *scheduler) {

}