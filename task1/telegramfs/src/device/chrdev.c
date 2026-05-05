#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include "linux/printk.h"
#include "telegramfs/device.h"
#include "telegramfs/chat.h"

static dev_t tgfs_devt;
static struct cdev tgfs_cdev;
static struct class *tgfs_class;

int tgfs_chrdev_init(void)
{
    int err;

    err = alloc_chrdev_region(&tgfs_devt, 0, TGFS_MAX_CHATS, "tgfs");
    if (err < 0) {
        pr_emerg("[tgfs] failed to allocate chrdev region\n");
        return err;
    }

    cdev_init(&tgfs_cdev, &tgfs_fops);
    tgfs_cdev.owner = THIS_MODULE;

    err = cdev_add(&tgfs_cdev, tgfs_devt, TGFS_MAX_CHATS);
    if (err < 0) {
        pr_emerg("[tgfs] failed to add chrdev\n");
        goto err_region;
    }

    tgfs_class = class_create("telegram");
    if (IS_ERR(tgfs_class)) {
        pr_emerg("[tgfs] failed to create chrdev class\n");
        err = PTR_ERR(tgfs_class);
        goto err_cdev;
    }

    for (int i = 0; i < TGFS_MAX_CHATS; i++) {
        if (IS_ERR(device_create(tgfs_class, NULL, MKDEV(MAJOR(tgfs_devt), i), NULL, "telegram/chat%d", i))) {
            pr_emerg("[tgfs] failed to create chrdev %d\n", i);

            while (--i >= 0) {
                device_destroy(tgfs_class, MKDEV(MAJOR(tgfs_devt), i));
            }

            class_destroy(tgfs_class);
            err = -ENODEV;
            goto err_cdev;
        }
    }

    pr_debug("[tgfs] chrdev registered, %d minors\n", TGFS_MAX_CHATS);
    return 0;

err_cdev:
    cdev_del(&tgfs_cdev);
err_region:
    unregister_chrdev_region(tgfs_devt, TGFS_MAX_CHATS);
    return err;
}

void tgfs_chrdev_exit(void)
{
    for (int i = 0; i < TGFS_MAX_CHATS; i++) {
        device_destroy(tgfs_class, MKDEV(MAJOR(tgfs_devt), i));
    }

    class_destroy(tgfs_class);
    cdev_del(&tgfs_cdev);
    unregister_chrdev_region(tgfs_devt, TGFS_MAX_CHATS);
    pr_debug("[tgfs] chrdev destroyed\n");
}
