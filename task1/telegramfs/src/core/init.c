#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>

#include "telegramfs/core.h"
#include "telegramfs/device.h"

int tgfs_module_init(void)
{
    int err;

    err = tgfs_chrdev_init();
    if (err < 0) {
        pr_err("tgfs: failed to register chrdev\n");
        return err;
    }

    pr_info("tgfs: module initialized\n");
    return 0;
}

void tgfs_module_exit(void)
{
    tgfs_chrdev_exit();
	pr_info("tgfs: module unloaded\n");
}

module_init(tgfs_module_init);
module_exit(tgfs_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CD7567");
MODULE_DESCRIPTION("Virtual Messenger File Interface");
