#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>

#include "telegramfs/core.h"

int tgfs_module_init(void)
{
	pr_info("telegram_fs: module initialized\n");
	return 0;
}

void tgfs_module_exit(void)
{
	pr_info("telegram_fs: module unloaded\n");
}

module_init(tgfs_module_init);
module_exit(tgfs_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CD7567");
MODULE_DESCRIPTION("Virtual Messenger File Interface");
