#include "hpcbench.h"
#include "plugins.h"
#include "shared.h"
#include "utilities.h"
#include "vm.h"

#define HPCB_VERSION "0.01b"

struct option long_options[] =
    {
        {"debug", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {"cpu-info", no_argument, 0, 'c'},
        {"pcimap", no_argument, 0, 'p'},
        {"run-tests", required_argument, 0, 'R'},
        {"threads-per-cpu", required_argument, 0, 'T'},
        {"plugin-dir", required_argument, 0, 'P'},
        {"list-plugins", no_argument, 0, 'l'},
        {"skip-affinity-set", no_argument, 0, 'S'},
        {"cpu-set", required_argument, 0, 'C'},
        {"output-path", required_argument, 0, 'w'},
        {"main-thread-cpu", required_argument, 0, 'M'},
        //{"vm-max-memory", required_argument, 0, 'M'},
        //{"vm-max-objects", required_argument, 0, 'O'},
        {NULL, 0, NULL, 0}};

int debug = 0;

int main(int argc, char *argv[])
{

    int opts, somethingtodo, i, pluginlist;
    extern int opterr;

    opterr = i = somethingtodo = opts = pluginlist = 0;

    context *ctx = (context *)calloc(1, sizeof(context));

    if (ctx == NULL)
        error_and_exit("Failed to allocate memory for context", 1);

    ctx->context_lock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    ctx->debug = 0;
    ctx->main_cpu = -1;
    ctx->use_cgroups = 0;
    ctx->skip_affinity_set = 0;
    ctx->params = long_options;
    ctx->argc = argc;
    ctx->argv = malloc((argc + 1) * sizeof(*ctx->argv));

    for (i = 0; i < argc; ++i)
    {
        size_t length = strlen(argv[i]) + 1;
        ctx->argv[i] = calloc(1, length);
        memcpy(ctx->argv[i], argv[i], length);
    }
    ctx->argv[argc] = NULL;

    ctx->ttpc = 1;
    //ctx->vm = init_vm(ctx); // Not quite there yet
    pthread_mutex_init(&ctx->context_lock, NULL);
    build_cpu_map(ctx);
    plugins_scan(ctx);


    // Initialize memory info here

    printf("\nHPCBench %s - Jon@JonathanDavidHall.com\n\n", HPCB_VERSION);

    while ((opts = getopt_long(argc, argv, "lC:G:hcdM:P:T:R:SO:", long_options, NULL)) != -1)
    {

        switch (opts)
        {

        case 'l':
            //pluginlist = 1;
            _plugins_list(ctx);
            exit(0);
            break;

        case 'O':
            break;

        case 'd':
            debug = 1;
            ctx->debug = 1;
            break;

        case 'h':
            somethingtodo = 1;
            _usage(ctx);
            exit(0);
            break;

        case 'c':
            print_cpu_info(ctx);
            exit(1);
            break;

        case 'C':
            ctx->requestedcpus = build_requested_cpu_map(ctx, optarg);
            if (ctx->requestedcpus == NULL)
            {
                printf("[HPCBench] Invalid CPU's requested for tests... \n\n");
                print_cpu_info(ctx);
                printf("[HPCBench] Please specify valid CPU's to run tests on.\n\n");
                exit(1);
            }
            break;

        case 'G':
            configure_cgroups(ctx, optarg);
            break;

        case 'M':
            if (check_if_number(optarg))
                ctx->main_cpu = atoi(optarg);
            break;

        case 'P':
            strncpy(ctx->plugin_path, optarg, sizeof(ctx->plugin_path));
            break;

        case 'T':
            if ((check_if_number(optarg)) && ((atoi(optarg) > 0)))
                ctx->ttpc = atoi(optarg);
            else
                error_and_exit("[HPCBench] You must specify either one (1) or more threads per CPU... Exiting!\n", 1);
            break;

        case 'R':
            // Should do a better check here
            ctx->tests = build_requested_tests(ctx, optarg);
            somethingtodo = (ctx->tests) ? 1 : 0;
            break;

        case 'S':
            // Skip affinity set and let the plugin handle it
            ctx->skip_affinity_set = 1;

        }
    }

    /*
    if(pluginlist) {
        _plugins_list(ctx);
        exit(0);
    }*/

    if (!ctx->outputpath)
        check_output_path(ctx, NULL);
    // Should be a better way than this...
    if (!somethingtodo) {
        _usage(ctx);
        exit(0);
    }

    // Ensure requested CPU map is created
    if(ctx->requestedcpus == NULL) {
        printf("[HPCBench] No CPU's selected... Running on all CPU's!\n");
        ctx->requestedcpus = build_requested_cpu_map(ctx, "all");
    }

    // Set the main thread CPU if desired
    if (ctx->main_cpu >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(ctx->main_cpu, &cpuset);
        sched_setaffinity(getpid(), sizeof(cpuset), &cpuset);
        printf("[HPCBench] Setting main thread CPU to %d\n", ctx->main_cpu);
    }

    // Let's start the threads
    int thread_counter = 0, j;
    for (j = 0; j < MAX_CPU_COUNT - 1; j++)
    {
        if (ctx->requestedcpus[j] != NULL)
            thread_counter++;
    }
    ctx->wpt = thread_counter * ctx->ttpc;

    if (ctx->tests != NULL)
    {
        int i;
        for (i = 0; i < MAX_PARALLEL_TESTS; i++)
        {
            if (ctx->tests[i] != NULL)
            {
                somethingtodo = 1;
                initialize_test(ctx->tests[i]);
            }
        }
        for (i = 0; i <= MAX_PARALLEL_TESTS; i++)
        {
            if (ctx->tests[i] != NULL)
            {
                somethingtodo = 1;
                pthread_join((*ctx).tests[i]->tracker->thread, NULL);
            }
        }
    }

    return 0;
}

void _usage(context *ctx)
{
    // usage...

    printf("Usage: \n\n");
    printf("  Information:\n\n");
    printf("    -h | --help               Display usage information\n");
    printf("    -c | --cpu-info           Display CPU topology information\n");
    printf("    -l | --list-plugins       Display available plugins and their usage\n");
    printf("    -d | --debug              Enable additional debug output for core and plugins\n");
    printf("\n");
    printf("  Test Configuration:\n\n");
    printf("    -C | --cpu-set            Set CPU(s) to run plugin(s) on\n");
    printf("                              all       - Run on all logical CPU's\n");
    printf("                              primary   - Bypass sibling threads\n");
    printf("                              siblings  - Run only on sibling threads\n");
    printf("                              <CPU's>   - Comma and/or range list of CPU's to run on [Example: 0-1,3-5,8,10]\n");
    printf("    -M | --main-thread-cpu    CPU to pin the main parent and tracker threads to\n");
    printf("    -G | --cgroups            Run with cgroups. Contain main threads and worker threads independently, or all in one cgroup\n");
    printf("                              all:<cgroup>\n");
    printf("                              main:<cgroup>,scheduler:<cgroup>,0:<cgroup>,1:<cgroup> [..]\n");
    printf("    -T | --threads-per-cpu    POSIX Thread count per CPU\n");
    printf("    -P | --plugin-dir         Path to scan for available plugins [Default: ./plugins]\n");
    printf("    -R | --run-tests          List of tests to run, comma separated, in order\n");
    printf("    -S | --skip-affinity-sets Skip setting affinity (plugin can do it based on cpuid on thread)\n");
    printf("    -O | --output-path        Path for any log files or output from core or plugins\n");
    printf("\n");

}

void _plugins_list(context *ctx) {
    plugins *current;
    arguments *args;
    // Available plugins
    printf("  Available Plugins/Tests:\n\n");

    current = ctx->plugins;
    while (current != NULL)
    {
        printf("    [%s]\n    %s\n        Arguments:\n", current->name, current->description);
        args = current->options;

        if (args != NULL)
            while (args->name != NULL)
            {
                printf("            --%s %s\n              %s\n", args->name, (args->has_arg ? "[arg]" : ""), args->description);
                args++;
            }

        printf("\n");
        current = current->next;
    }
}

test_tracker *initialize_test(test *test)
{
    test_tracker *tracker = calloc(1, sizeof(test_tracker));
    tracker->test = test;
    test->tracker = tracker;
    test->tracker->test->lock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    test->workers = calloc(test->ctx->wpt, sizeof(worker *));
    if (pthread_create(&tracker->thread, NULL, test_tracker_function, tracker))
        error_and_exit("Failed to create tracker thread... Exiting!\n", 1);
    return tracker;
}

void *test_tracker_function(void *ptr)
{
    uint64_t (*gettime)(void) = hpcb_realtime_ms;
    test_tracker *tracker = ptr;
    pthread_t setup;
    uint64_t total;
    float total_s;
    pthread_t finalize;

    if (tracker->test->ctx->main_cpu >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(tracker->test->ctx->main_cpu, &cpuset);
        sched_setaffinity(getpid(), sizeof(cpuset), &cpuset);
        printf("[HPCBench] Setting tracker thread to cpu %d\n", tracker->test->ctx->main_cpu);
    }

    tracker->start = gettime();
    printf("[HPCBench] Starting [%s]\n", tracker->test->plugin->name);
    pthread_mutex_init(&tracker->test->lock, NULL);
    pthread_cond_init(&tracker->test->cond, NULL);

    // Plugin has a producer/consumer model
    if (tracker->test->plugin->scheduler == SCHED)
    {
        tracker->test->scheduler = calloc(1, sizeof(struct scheduler));
        tracker->test->scheduler->lock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
        tracker->test->scheduler->test = tracker->test;
        if (pthread_create(&tracker->test->scheduler->thread, NULL, test_scheduler_function, tracker->test->scheduler))
            error_and_exit("Failed to create scheduler thread... Exiting!\n", 1);
    }

    tracker->setup_start = gettime();
    if (pthread_create(&setup, NULL, test_setup_function, tracker))
        error_and_exit("Failed to create setup thread... Exiting!\n", 1);

    pthread_join(setup, NULL);
    tracker->setup_end = gettime();
    if (debug)
        printf("[HPCBench] Setup of test [%s] completed in %" FMT_U64 "ms !\n", tracker->test->plugin->name, tracker->setup_end - tracker->setup_start);
    tracker->test_start = gettime();
    int j, worker_count = 0;
    for (j = 0; j < MAX_CPU_COUNT - 1; j++)
    {
        if (tracker->test->ctx->requestedcpus[j] != NULL)
        {
            int k;
            for (k = 1; k <= tracker->test->ctx->ttpc; k++)
            {
                tracker->test->workers[worker_count] = calloc(1, sizeof(worker));
                tracker->test->workers[worker_count]->id = worker_count;
                tracker->test->workers[worker_count]->test = tracker->test;
                tracker->test->workers[worker_count]->cpuid = j;
                if (pthread_create(&tracker->test->workers[worker_count]->thread, NULL, test_worker_function, tracker->test->workers[worker_count]))
                    error_and_exit("Failed to create tracker thread... Exiting!\n", 1);
                worker_count++;
            }
        }
    }
    // join threads
    for (j = 0; j < worker_count; j++)
    {
        pthread_join(tracker->test->workers[j]->thread, NULL);
    }
    if (tracker->test->plugin->scheduler == SCHED)
    {
        pthread_join(tracker->test->scheduler->thread, NULL);
    }
    tracker->test_end = gettime();

    if (debug)
        printf("[HPCBench] Entering finalization for test [%s] after %" FMT_U64 "ms\n", tracker->test->plugin->name, tracker->test_end - tracker->test_start);
    tracker->finalize_start = gettime();
    if (pthread_create(&finalize, NULL, test_finalize_function, tracker))
        error_and_exit("Failed to create finalize thread... Exiting!\n", 1);

    pthread_join(finalize, NULL);
    tracker->finalize_end = gettime();
    if (debug)
        printf("[HPCBench] Finalize for test [%s] completed in %" FMT_U64 "ms...\n", tracker->test->plugin->name, tracker->finalize_end - tracker->finalize_start);

    tracker->end = gettime();
    total = tracker->end - tracker->start;
    total_s = (((float)total) / 1000);
    printf("[HPCBench] Completed test [%s] in %.03fs...\n\n", tracker->test->plugin->name, total_s);
}

void *test_setup_function(void *ptr)
{
    test_tracker *tracker = ptr;
    tracker->test->plugin->setuptask(tracker->test);
}

void *test_scheduler_function(void *ptr)
{
    scheduler *sched = ptr;
    pthread_mutex_init(&sched->lock, NULL);
    pthread_cond_init(&sched->cond, NULL);
    sched->test->plugin->schedtask(sched);
}

void *test_worker_function(void *ptr)
{
    worker *worker = ptr;
    if (!worker->test->ctx->skip_affinity_set)
        hpcb_set_thread_cpu(worker, worker->cpuid);
    worker->test->plugin->workertask(worker->test, worker);
}

void *test_finalize_function(void *ptr)
{
    test_tracker *tracker = ptr;
    tracker->test->plugin->finalizetask(tracker->test);
}

void cleanup_and_exit(context *ctx)
{
    // Clean up items in memory and exit
}

void error_and_exit(char *message, int code) {
    printf("[ERROR] %s\n", message);
    exit(code);
}

void hpcb_debug_out(char *message) {
    if(debug)
        printf("%s");
}