#ifndef TELEGRAMFS_CHAT_H
#define TELEGRAMFS_CHAT_H

#include <linux/mutex.h>

#define TGFS_MAX_CHATS       3
#define TGFS_MAX_MSG_CNT     128
#define TGFS_MAX_MSG_SIZE    256

struct tgfs_chat {
	int id;
	struct mutex lock;
	char msgs[TGFS_MAX_MSG_CNT][TGFS_MAX_MSG_SIZE];
	size_t msg_count;
	size_t next_idx;
};

extern struct tgfs_chat tgfs_chats[TGFS_MAX_CHATS];
int tgfs_chat_init(void);
void tgfs_chat_exit(void);

#endif /* TELEGRAMFS_CHAT_H */
