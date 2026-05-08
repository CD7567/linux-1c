#ifndef TELEGRAMFS_IOCTL_H
#define TELEGRAMFS_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>


//
// ioctl chat operations
//


/*
 * Magic constant for ioctl operations on chat devices
 */
#define TGFS_IOCTL_CHAT_MAGIC 's'

/*
 * Get message count for current chat
 */
#define TGFS_IOCTL_CHAT_GET_MSG_COUNT _IOR(TGFS_IOCTL_CHAT_MAGIC, 1, size_t)

/*
 * Clear all messages from current chat
 */
#define TGFS_IOCTL_CHAT_CLEAR _IO(TGFS_IOCTL_CHAT_MAGIC, 2)

/*
 * Get current read limit
 */
#define TGFS_IOCTL_CHAT_GET_READ_LIMIT _IOR(TGFS_IOCTL_CHAT_MAGIC, 3, size_t)

/*
 * Set new read limit
 */
#define TGFS_IOCTL_CHAT_SET_READ_LIMIT _IOW(TGFS_IOCTL_CHAT_MAGIC, 4, size_t)


//
// ioctl chat create operations
//


/*
 * Magic constant for ioctl operations on chat create device
 */
#define TGFS_IOCTL_CTL_MAGIC 'p'

/*
 * Max length for chat names
 */
#define TGFS_MAX_CHAT_NAME_LEN 64

/*
 * Structured ioctl request to create a chat
 */
struct tgfs_ctl_create_req {
	char name[TGFS_MAX_CHAT_NAME_LEN];
	__u32 chat_id;
	char device_name[TGFS_MAX_CHAT_NAME_LEN];
};

/*
 * Create a new chat dynamically
 */
#define TGFS_IOCTL_CTL_CREATE_CHAT      _IOWR(TGFS_IOCTL_CTL_MAGIC, 1, struct tgfs_ctl_create_req)

/*
 * Get current chat count
 */
#define TGFS_IOCTL_CTL_GET_CHAT_COUNT   _IOR(TGFS_IOCTL_CTL_MAGIC, 2, size_t)

#endif /* TELEGRAMFS_IOCTL_H */
