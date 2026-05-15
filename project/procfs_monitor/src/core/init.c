#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>

#include "procfs_monitor/core.h"

int procfs_monitor_module_init(void)
{
    pr_info("[procfs_monitor] module initialized\n");

    return 0;
}

void procfs_monitor_module_exit(void)
{
	pr_info("[procfs_monitor] module unloaded\n");
}

module_init(procfs_monitor_module_init);
module_exit(procfs_monitor_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CD7567");
MODULE_DESCRIPTION("System Monitor File Interface");
