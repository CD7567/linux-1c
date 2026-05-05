#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/printk.h>

#include "telegramfs/chat.h"

struct tgfs_chat tgfs_chats[TGFS_MAX_CHATS];

int tgfs_chat_init(void)
{
	for (int i = 0; i < TGFS_MAX_CHATS; i++) {
		tgfs_chats[i].id = i;
		mutex_init(&tgfs_chats[i].lock);
		tgfs_chats[i].msg_count = 0;
		tgfs_chats[i].next_idx = 0;
	}

	pr_debug("[tgfs] chat store initialized\n");
   
	return 0;
}

void tgfs_chat_exit(void)
{
	for (int i = 0; i < TGFS_MAX_CHATS; i++) {
        mutex_destroy(&tgfs_chats[i].lock);
    }

	pr_debug("[tgfs] chat store destroyed\n");
}
