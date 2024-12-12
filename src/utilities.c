#include "hpcbench.h"
#include "plugins.h"
#include "utilities.h"

void build_cpu_map(context *ctx)
{
    unsigned int socket_count = 0, logical_cpu_count = 0, threads_per_core = 0, cpuloop = 0;
    regex_t regex;
    DIR *dir = opendir("/sys/devices/system/cpu");
    struct dirent *entry;

    if (regcomp(&regex, CPU_REGEX, REG_EXTENDED))
        error_and_exit("Error compiling RegEx... Exiting!\n", 1);

    // Probably better ways...
    if (dir != NULL)
    {
        // Count the digits so we know the max length to use later for
        // strncpy() and etc
        int size_packageid = floor(log10(abs(MAX_SOCKET_COUNT))) + 2;
        int size_siblings = floor(log10(abs(MAX_CPU_THREADS * MAX_CPU_COUNT + 2)));

        /*
         * First loop we allocate all the objects. Then we walk the
         * allocated items in order and gather the data on the CPU's.
         * This also avoids the out-of-order results from readdir() .
         */

        while (entry = readdir(dir))
        {
            if (!regexec(&regex, entry->d_name, 0, NULL, 0))
            {
                int cpuid_me = atoi(&entry->d_name[3]);
                cpu *cpu_me = (cpu *)calloc(1, sizeof(cpu));
                ctx->cpus[cpuid_me] = cpu_me;
            }
        }
        closedir(dir);

        for (cpuloop = 0; cpuloop <= MAX_CPU_COUNT -1; cpuloop++)
        {
            if (ctx->cpus[cpuloop] != NULL) 
            {
                int sibling_count = 0, token_count = 0;
                char *token, *tmpstr;
                char packageid[size_packageid];
                char siblings[size_siblings];
                char packageid_path[strlen(PHYSICAL_PACKAGE_ID_PATH) + 8];
                char siblings_path[strlen(THREAD_SIBLING_LIST_PATH) + 8];
                cpu *cpu_me;
                package *package_me;

                snprintf(packageid_path, sizeof(packageid_path), PHYSICAL_PACKAGE_ID_PATH, cpuloop);
                snprintf(siblings_path, sizeof(siblings_path), THREAD_SIBLING_LIST_PATH, cpuloop);
                FILE *pkg_id = fopen(packageid_path, "r");
                FILE *siblings_list = fopen(siblings_path, "r");
                if (siblings_list == NULL || pkg_id == NULL)
                    error_and_exit("Failed to open package ID and siblings list... Exiting!\n", 1);

                fgets(packageid, sizeof(packageid), pkg_id);
                fgets(siblings, sizeof(siblings), siblings_list);
                packageid[strlen(packageid)] = '\0';
                siblings[strlen(siblings)] = '\0';
                int packageid_i = atoi(packageid);
                if (ctx->packages[packageid_i] == NULL)
                {
                    ctx->packages[packageid_i] = (package *)calloc(1, sizeof(package));
                    if (ctx->packages[packageid_i] == NULL)
                        error_and_exit("Error allocating memory... Exiting!\n", 1);
                    ctx->packages[packageid_i]->id = packageid_i;
                    ctx->packages[packageid_i]->socket = socket_count;
                    ctx->sockets[socket_count] = ctx->packages[packageid_i];
                    ++socket_count;
                }

                int cpuid_me = cpuloop;
                cpu_me = ctx->cpus[cpuid_me];
                if (cpu_me == NULL)
                    error_and_exit("Previous memory allocation for CPU failed. Exiting!\n", 1);
                cpu_me->package = ctx->packages[packageid_i];
                cpu_me->id = cpuid_me;
                ctx->packages[packageid_i]->cpus[cpuid_me] = cpu_me;

                tmpstr = strdup(siblings);
                while ((token = strsep(&tmpstr, ",")))
                {
                    ++token_count;
                    if (!threads_per_core)
                        ++sibling_count;

                    if (token_count == 1)
                    {
                        cpu_me->type = (atoi(token) == cpuid_me) ? PRIMARY : SIBLING;
                        cpu_me->primary = ctx->cpus[atoi(token)];
                    }

                    if (cpuid_me != atoi(token))
                        if (cpu_me->siblings[atoi(token)] == NULL)
                            cpu_me->siblings[atoi(token)] = ctx->cpus[atoi(token)];
                }

                if (!threads_per_core)
                    threads_per_core = sibling_count;
                free(tmpstr);

                ctx->cpus[cpuid_me] = cpu_me;
                fclose(pkg_id);
                fclose(siblings_list);
                ++logical_cpu_count;
            }
        }
    }

    regfree(&regex);

    ctx->socket_count = socket_count;
    ctx->logical_cpu_count = logical_cpu_count;
    ctx->threads_per_core = threads_per_core;
    ctx->physical_core_count = logical_cpu_count;
}

void print_cpu_info(context *ctx)
{
    int socket, cpu;

    printf("  * Logical / HT threads within brackets ( i.e. [sibling] )\n\n");
    printf("    Socket Count: %d\n", ctx->socket_count);
    printf("    Physical Cores: %d\n", ctx->physical_core_count);
    printf("    Threads Per Core: %d\n", ctx->threads_per_core);
    printf("    Logical CPU Total: %d\n", ctx->logical_cpu_count);
    printf("\n");

    for (socket = 0; socket < MAX_SOCKET_COUNT; socket++)
    {
        if (ctx->sockets[socket] != NULL)
        {
            printf("    -> Socket %d - pkgid %d - CPU's: ", ctx->sockets[socket]->socket, ctx->sockets[socket]->id);
            for (cpu = 0; cpu <= MAX_CPU_COUNT - 1; cpu++)
            {
                if (ctx->sockets[socket]->cpus[cpu] != NULL)
                    if (ctx->sockets[socket]->cpus[cpu]->type == PRIMARY)
                        printf("%d ", ctx->sockets[socket]->cpus[cpu]->id);
                    else if (ctx->sockets[socket]->cpus[cpu]->type == SIBLING)
                        printf("[%d] ", ctx->sockets[socket]->cpus[cpu]->id);
            }
            printf("\n");
        }
    }
    printf("\n");
}

test **build_requested_tests(context *ctx, char *requested)
{
    char *tmpstr, *token;
    test **plugs = calloc((MAX_PARALLEL_TESTS + 1), sizeof(plugins **));
    int count = 0;
    tmpstr = strdup(requested);
    while ((token = strsep(&tmpstr, ",")))
    {
        plugins *cur = ctx->plugins;
        while (cur != NULL)
        {
            if (!strcmp(cur->name, token))
            {
                test *test = calloc(1, sizeof(struct test));
                test->plugin = cur;
                test->ctx = ctx;
                plugs[count] = test;
                plugin_register_options(ctx, cur);
                count++;
            }
            cur = cur->next;
        }
        plugs[count] = NULL;
    }
    free(tmpstr);
    return plugs;
}

cpu **build_requested_cpu_map(context *ctx, char *input)
{
    cpu **map;
    if (!strcmp(input, "all"))
    {
        return ctx->cpus;
    }
    else if (!strcmp(input, "primary"))
    {
        map = calloc((MAX_CPU_COUNT + 2), sizeof(cpu *));
        int count;
        for (count = 0; count <= MAX_CPU_COUNT - 1; count++)
        {
            if (ctx->cpus[count] != NULL)
                if (ctx->cpus[count]->type == PRIMARY)
                    map[count] = ctx->cpus[count];
        }
        return map;
    }
    else if (!strcmp(input, "siblings"))
    {
        if(ctx->threads_per_core <= 1)
            return NULL;
        map = calloc((MAX_CPU_COUNT + 2), sizeof(cpu *));
        int count;
        for (count = 0; count <= MAX_CPU_COUNT - 1; count++)
        {
            if (ctx->cpus[count] != NULL)
                if (ctx->cpus[count]->type == SIBLING)
                    map[count] = ctx->cpus[count];
        }
        return map;
    }
    else
    {
        map = calloc((MAX_CPU_COUNT + 2), sizeof(cpu *));
        char *token, *token2, *tmpstr, *tmpstr2;
        tmpstr = strdup(input);
        while ((token = strsep(&tmpstr, ",")))
        {
            if (strstr(token, "-") != NULL)
            {
                int range[2];
                int which = 0, count = 0;
                tmpstr2 = strdup(token);

                while ((token2 = strsep(&tmpstr2, "-")) && (check_if_number(token2)))
                {
                    if (which >= 2)
                        return NULL;

                    range[which] = atoi(token2);
                    which++;
                }

                if (range[0] > range[1])
                    return NULL;

                for (count = range[0]; count <= range[1]; count++)
                {
                    if (ctx->cpus[count] != NULL)
                        map[count] = ctx->cpus[count];
                    else
                        return NULL;
                }
                free(tmpstr2);
            }
            else
            {
                // Not a range
                if ((check_if_number(token)) && (ctx->cpus[atoi(token)] != NULL))
                    map[atoi(token)] = ctx->cpus[atoi(token)];
                else
                    return NULL;
            }
        }
        free(tmpstr);
    }
    return map;
}

int check_output_path(context *ctx, char *path)
{
    char *real_path;
    int needs_free = 0;
    if (path == NULL)
    {
        path = get_binary_path();
        needs_free = 1;
    }
    real_path = realpath(path, NULL);
    if (real_path == NULL)
        return 1;
    ctx->outputpath = strdup(real_path);
    if (needs_free)
        free(path);
    return 0;
}

char *get_binary_path(void)
{
    char *buf, *path_end;
    buf = malloc(PATH_MAX);
    if (buf == NULL)
        error_and_exit("Malloc failed... Exiting!\n", 1);

    if (readlink("/proc/self/exe", buf, PATH_MAX) <= 0)
        error_and_exit("Readlink failed for /proc/self/exe - Exiting!\n", 1);

    path_end = strchr(buf, '/');
    if (path_end == NULL)
        error_and_exit("Path incomplete... Exiting!\n", 1);

    *path_end = '\0';
    return buf;
}

int check_if_percentage(char *arg)
{
    int len = strlen(arg);
    if (!strcmp(&arg[len - 1], "%"))
        return 1;
    return 0;
}

int check_if_number(char *arg)
{
    int i, len = strlen(arg);
    if (len > 0)
    {
        for (i = 0; i < len; ++i)
        {
            if (!isdigit(arg[i]))
                return 0;
        }
    }
    return 1;
}

void configure_cgroups(context *ctx, char *args) {
    // Leverage asprintf
    char *tmpstr, *tmpstr2, *token, *token2, *token3;
    tmpstr = strdup(args);
    int all_in_one = 0;
    while ((token = strsep(&tmpstr, ","))) {
        //printf("Token 1: %s\n", token);
        // Iterate first level
        token2 = strsep(&token, ":");
        // Checking token 2 before bothering with the next
        if (strcmp(token2, "all") == 0) {
            // All threads should be in the same group
            // and the cgroup specification should end
            // here
            printf("Specified all threads\n");
        } else if (strcmp(token2, "main") == 0) {
            // All non-worker threads should be placed
            // in to specified cgroup.
            printf("Specified main thread cgroup\n");
        } else if (check_if_number(token2)) {
            // Verify numa node is valid
            printf("It's a number, likely a numa node...\n");
        } else {
            printf("I don't know what %s is\n", token2);
        }
        token3 = strsep(&token, ":");
        //printf("Token 2: %s and %s\n", token2, token3);
    }
    // Confirm cgroup configuration
    printf("\n");
}

void build_pci_dev_map(context *ctx) {

}

char *get_pci_dev_affinity(pci_dev *dev) {

}

int set_pci_dev_affinity(pci_dev *dev, char *affinity) {

}
