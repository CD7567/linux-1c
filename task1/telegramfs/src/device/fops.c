#include <linux/module.h>
#include <linux/fs.h>

#include "telegramfs/device.h"

static int tgfs_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int tgfs_release(struct inode *inode, struct file *file)
{
    return 0;
}

struct file_operations tgfs_fops = {
    .owner = THIS_MODULE,
    .open = tgfs_open,
    .release = tgfs_release,
};
