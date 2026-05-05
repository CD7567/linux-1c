#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>

#include "telegramfs/device.h"

static dev_t tgfs_devt;
static struct cdev tgfs_cdev;
static struct class *tgfs_class;

int tgfs_chrdev_init(void)
{
    int err;

    err = alloc_chrdev_region(&tgfs_devt, 0, 1, "tgfs");
    if (err < 0)
        return err;

    cdev_init(&tgfs_cdev, &tgfs_fops);
    tgfs_cdev.owner = THIS_MODULE;

    err = cdev_add(&tgfs_cdev, tgfs_devt, 1);
    if (err < 0)
        goto err_region;

    tgfs_class = class_create("tgfs");
    if (IS_ERR(tgfs_class)) {
        err = PTR_ERR(tgfs_class);
        goto err_cdev;
    }

    if (IS_ERR(device_create(tgfs_class, NULL, tgfs_devt, NULL, "telegram"))) {
        err = -ENODEV;
        goto err_class;
    }

    pr_info("tgfs: chardev registered at %d:%d\n", MAJOR(tgfs_devt), MINOR(tgfs_devt));
    return 0;

err_class:
    class_destroy(tgfs_class);
err_cdev:
    cdev_del(&tgfs_cdev);
err_region:
    unregister_chrdev_region(tgfs_devt, 1);
    return err;
}

void tgfs_chrdev_exit(void)
{
    device_destroy(tgfs_class, tgfs_devt);
    class_destroy(tgfs_class);
    cdev_del(&tgfs_cdev);
    unregister_chrdev_region(tgfs_devt, 1);
    pr_info("tgfs: chardev unregistered\n");
}
