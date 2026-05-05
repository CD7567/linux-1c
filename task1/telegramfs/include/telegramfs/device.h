#ifndef TELEGRAMFS_DEVICE_H
#define TELEGRAMFS_DEVICE_H

#include <linux/fs.h>

extern struct file_operations tgfs_fops;

int tgfs_chrdev_init(void);
void tgfs_chrdev_exit(void);

#endif
