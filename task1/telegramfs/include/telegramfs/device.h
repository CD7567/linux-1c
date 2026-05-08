#ifndef TELEGRAMFS_DEVICE_H
#define TELEGRAMFS_DEVICE_H

#include <linux/fs.h>

#include "telegramfs/chat.h"

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

/*
 * Create new chrdev for this chat 
 */
int tgfs_chrdev_create_chat_device(struct tgfs_chat *chat);

/*
 * Destroy chrdev for this chat 
 */
void tgfs_chrdev_destroy_chat_device(struct tgfs_chat *chat);

#endif /* TELEGRAMFS_DEVICE_H */
