#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>

#include "procfs_monitor/core.h"
#include "procfs_monitor/procfs.h"

int procfs_monitor_module_init(void)
{
    int ret;

    pr_debug("[procfs_monitor] starting procfs init\n");
    ret = procfs_monitor_procfs_init();
    if (ret) {
        pr_err("[procfs_monitor] procfs init failed: %d\n", ret);
        return ret;
    }

    pr_info("[procfs_monitor] module initialized\n");

    return 0;
}

void procfs_monitor_module_exit(void)
{
    pr_debug("[procfs_monitor] starting procfs destroy\n");
    procfs_monitor_procfs_exit();

	pr_info("[procfs_monitor] module unloaded\n");
}

module_init(procfs_monitor_module_init);
module_exit(procfs_monitor_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CD7567");
MODULE_DESCRIPTION("System Monitor File Interface");
