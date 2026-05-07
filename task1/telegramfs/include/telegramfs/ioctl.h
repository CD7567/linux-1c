#ifndef TELEGRAMFS_IOCTL_H
#define TELEGRAMFS_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define TGFS_IOCTL_MAGIC 's'

/*
 * Get message count for current chat
 */
#define TGFS_IOCTL_GET_MSG_COUNT _IOR(TGFS_IOCTL_MAGIC, 1, size_t)

/*
 * Clear all messages from current chat
 */
#define TGFS_IOCTL_CLEAR_CHAT _IO(TGFS_IOCTL_MAGIC, 2)

/*
 * Get current read limit
 */
#define TGFS_IOCTL_GET_READ_LIMIT _IOR(TGFS_IOCTL_MAGIC, 3, size_t)

/*
 * Set new read limit
 */
#define TGFS_IOCTL_SET_READ_LIMIT _IOW(TGFS_IOCTL_MAGIC, 4, size_t)

#endif /* TELEGRAMFS_IOCTL_H */
