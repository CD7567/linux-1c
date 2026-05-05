#ifndef TELEGRAMFS_DEVICE_H
#define TELEGRAMFS_DEVICE_H

#include <linux/fs.h>

/*
 * Available chrdev operations
 */
extern struct file_operations tgfs_fops;

/*
 * Initialize chrdev
 */
int tgfs_chrdev_init(void);

/*
 * Destroy chrdev
 */
void tgfs_chrdev_exit(void);

#endif /* TELEGRAMFS_DEVICE_H */
