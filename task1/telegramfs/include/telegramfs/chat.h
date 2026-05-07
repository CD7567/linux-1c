#ifndef TELEGRAMFS_CHAT_H
#define TELEGRAMFS_CHAT_H

#include <linux/mutex.h>
#include <linux/time64.h>

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
#define TGFS_READ_MSG_LIMIT_DEFAULT 10

/*
 * Max size of serialized message
 */
#define TGFS_MAX_RENDERED_MSG_SIZE (sizeof("[00:00:00] ") + TGFS_MAX_MSG_SIZE)

/*
 * Size of snapshot buffer
 */
#define TGFS_SNAPSHOT_BUF_SIZE (TGFS_MAX_MSG_CNT * TGFS_MAX_RENDERED_MSG_SIZE)

/*
 * Chat message interface
 */
struct tgfs_msg {
	char text[TGFS_MAX_MSG_SIZE];
	size_t len;
    time64_t ts;
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
    size_t read_limit;
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


//
// ioctl stuff
//

int tgfs_chat_clear(struct tgfs_chat *chat);
int tgfs_chat_get_msg_count(struct tgfs_chat *chat, size_t *out_count);
int tgfs_chat_get_read_limit(struct tgfs_chat *chat, size_t *out_limit);
int tgfs_chat_set_read_limit(struct tgfs_chat *chat, size_t new_limit);

#endif /* TELEGRAMFS_CHAT_H */
