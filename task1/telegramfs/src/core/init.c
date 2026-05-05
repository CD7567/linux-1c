#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>

#include "telegramfs/core.h"
#include "telegramfs/device.h"
#include "telegramfs/chat.h"

int tgfs_module_init(void)
{
    int err;

    pr_debug("[tgfs] starting chat store init\n");

	err = tgfs_chat_init();
	if (err < 0) {
        return err;
    }

    pr_debug("[tgfs] starting chrdev init\n");

	err = tgfs_chrdev_init();
	if (err < 0) {
		tgfs_chat_exit();
		return err;
	}

    pr_info("[tgfs] module initialized\n");

    return 0;
}

void tgfs_module_exit(void)
{
    pr_debug("[tgfs] starting chrdev destroy\n");
    tgfs_chrdev_exit();

    pr_debug("[tgfs] starting chat store destroy\n");
    tgfs_chat_exit();

	pr_info("[tgfs] module unloaded\n");
}

module_init(tgfs_module_init);
module_exit(tgfs_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CD7567");
MODULE_DESCRIPTION("Virtual Messenger File Interface");
