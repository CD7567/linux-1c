#ifndef TELEGRAMFS_CHAT_H
#define TELEGRAMFS_CHAT_H

#include <linux/mutex.h>

/*
 * In-memory chat store limit
 */
#define TGFS_MAX_CHATS 3

/*
 * Limit of messages in a single chat
 */
#define TGFS_MAX_MSG_CNT 128

/*
 * Message buffer size
 */
#define TGFS_MAX_MSG_SIZE 256

/*
 * Maximum amount of last messages when reading
 */
#define TGFS_READ_MSG_LIMIT 10

/*
 * Max size of serialized message
 */
#define TGFS_MAX_RENDERED_MSG_SIZE (sizeof("[msg] ") - 1 + TGFS_MAX_MSG_SIZE + 1)

/*
 * Size of snapshot buffer
 */
#define TGFS_SNAPSHOT_BUF_SIZE (TGFS_READ_MSG_LIMIT * TGFS_MAX_RENDERED_MSG_SIZE)

/*
 * Chat message interface
 */
struct tgfs_msg {
	char text[TGFS_MAX_MSG_SIZE];
	size_t len;
};

/*
 * Chat interface
 */
struct tgfs_chat {
	int id;
	struct mutex lock;
	struct tgfs_msg msgs[TGFS_MAX_MSG_CNT];
	size_t msg_count;
	size_t next_idx;
};

/*
 * In-memory chat store
 */
extern struct tgfs_chat tgfs_chats[TGFS_MAX_CHATS];

/*
 * Initialize chat store
 */
int tgfs_chat_init(void);

/*
 * Destroy chat store
 */
void tgfs_chat_exit(void);

/*
 * Push message into a chat.
 *
 * This function expects normalized message that is already in kernel space
 */
int tgfs_chat_push(struct tgfs_chat *chat, const char *msg, size_t len);

/*
 * Collect last messages into a string buffer
 */
ssize_t tgfs_chat_snapshot(struct tgfs_chat *chat, char *out, size_t out_size);

#endif /* TELEGRAMFS_CHAT_H */
