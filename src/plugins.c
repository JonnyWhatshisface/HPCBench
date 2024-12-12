#include <dlfcn.h>
#include <stdio.h>

#include "hpcbench.h"
#include "plugins.h"
#include "utilities.h"

int plugins_scan(context *ctx)
{
    char path[4096], ppath[4096];
    struct dirent *dentry;
    void *phandle;
    plugins *current, *plist;
    plugins *(*plugin_func)();
    void *(*plugin_setup)(test * test);
    void *(*plugin_run)(test * test, worker * thread);
    void *(*plugin_sched)(test * test, scheduler * sched);
    void *(*plugin_finalize)(test * test);

    plist = current = NULL;

    if (strlen(ctx->plugin_path) <= 1)
    {
        char *selfpath = "/proc/self/exe";

        if (!selfpath || readlink(selfpath, path, sizeof(path)) <= 0)
            goto DIE;

        char *pathend = strrchr(path, '/');
        ++pathend;
        *pathend = '\0';

        if (pathend == NULL)
            goto DIE;
        strncat(path, "plugins/", strlen(path) + 9);
    }
    else
    {
        strncpy(path, ctx->plugin_path, sizeof(ctx->plugin_path));
    }

    DIR *dir = opendir(path);
    if (dir == NULL)
        goto DIE;

    while (dentry = readdir(dir))
    {
        phandle = NULL;
        plugin_func = NULL;
        // Clean up extension check...
        if ((!strstr(dentry->d_name, ".so")) && (!strstr(dentry->d_name, ".o")))
            continue;

        snprintf(ppath, sizeof(ppath), "%s%s", path, dentry->d_name);

        // Invalid plugins cause a segfault here. Need to look in to why.
        if ((phandle = dlopen(ppath, RTLD_LAZY | RTLD_LOCAL)) == NULL) 
            continue;

        plugin_func = dlsym(phandle, "hpcb_plugin_info");
        if (dlerror() != NULL || plugin_func == NULL)
        {
            // Fail
            dlclose(phandle);
            continue;
        }

        plugin_setup = dlsym(phandle, "hpcb_plugin_setup");
        if (dlerror() != NULL || plugin_setup == NULL)
        {
            // Fail
            dlclose(phandle);
            continue;
        }

        plugin_run = dlsym(phandle, "hpcb_plugin_entry");
        if (dlerror() != NULL || plugin_run == NULL)
        {
            // Fail
            dlclose(phandle);
            continue;
        }

        plugin_finalize = dlsym(phandle, "hpcb_plugin_finalize");
        if (dlerror() != NULL || plugin_finalize == NULL)
        {
            // Fail
            dlclose(phandle);
            continue;
        }

        plugins *thisplug = plugin_func(ctx);
        if (thisplug == NULL)
            continue;

        if (thisplug->scheduler == SCHED)
        {
            plugin_sched = dlsym(phandle, "hpcb_plugin_scheduler_entry");
            if (dlerror() != NULL || plugin_sched == NULL)
            {
                // Debug output needed
                free(thisplug);
                dlclose(phandle);
                continue;
            }
            thisplug->schedtask = (void *)plugin_sched;
        }
        thisplug->setuptask = (void *)plugin_setup;
        thisplug->workertask = (void *)plugin_run;
        thisplug->finalizetask = (void *)plugin_finalize;
        thisplug->phandle = phandle;
        thisplug->next = NULL;
        //plugin_register_options(ctx, thisplug); // Call from build_requested_tests() - remove this

        if (plist == NULL)
        {
            current = plist = thisplug;
            ctx->plugins = plist;
        }
        else
        {
            current = current->next = thisplug;
        }
    }
    return 0;
DIE:
    printf("No usable plugins found...\n");
    exit(1);
}

int plugin_register_options(context *ctx, plugins *plugin)
{
    // Hacked together in five minutes. Come back and clean up.
    // This is called during the test building from build_requested_tests()
    if (plugin->options == NULL)
        return 0;
    int optcount = 0;
    arguments *p = plugin->options;
    arguments *p2;
    struct option *opt;

    if (p == NULL)
        return 1;
    while (p->name != NULL)
    {
        optcount++;
        p++;
    }

    struct option *opts = calloc((optcount + 2), sizeof(struct option));
    //p = NULL;
    //p = *(plugin).options;
    p2 = p - optcount;
    opt = opts;
    while (p2->name != NULL)
    {
        (*opt).name = p2->name;
        (*opt).has_arg = p2->has_arg;
        (*opt).flag = p2->flag;
        (*opt).val = p2->val;
        p2++;
        opt++;
    }
    opt++;
    (*opt).name = 0;
    (*opt).has_arg = 0;
    (*opt).flag = 0;
    (*opt).val = 0;

    plugin->longopts = opts;

    // Deep copy argv[] from ctx to plugin
    plugin->argv = malloc((ctx->argc + 1) * sizeof(*plugin->argv));
    int i;
    for (i = 0; i < ctx->argc; ++i)
    {
        size_t length = strlen(ctx->argv[i]) + 1;
        (*plugin).argv[i] = calloc(1, length);
        memcpy(plugin->argv[i], ctx->argv[i], length);
    }
    plugin->argv[ctx->argc] = NULL;

    return 0;
}

int plugin_destroy(context *ctx, plugins *plugin)
{
}