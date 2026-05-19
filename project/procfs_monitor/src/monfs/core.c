#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>

#include "monfs_monitor/core.h"
#include "monfs_monitor/monfs.h"

int monfs_monitor_module_init(void)
{
    int ret;

    pr_debug("[monfs_monitor] starting procfs init\n");
    ret = monfs_monitor_monfs_init();
    if (ret) {
        pr_err("[monfs_monitor] procfs init failed: %d\n", ret);
        return ret;
    }

    pr_info("[monfs_monitor] module initialized\n");

    return 0;
}

void monfs_monitor_module_exit(void)
{
    pr_debug("[monfs_monitor] starting procfs destroy\n");
    monfs_monitor_monfs_exit();

	pr_info("[monfs_monitor] module unloaded\n");
}

module_init(monfs_monitor_module_init);
module_exit(monfs_monitor_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CD7567");
MODULE_DESCRIPTION("System Monitor File Interface");
