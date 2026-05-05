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

		for (int j = 0; j < TGFS_MAX_MSG_CNT; j++) {
			tgfs_chats[i].msgs[j].text[0] = '\0';
			tgfs_chats[i].msgs[j].len = 0;
		}
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

/*
 * Helper function to get index of the first message within defined limit
 */
static size_t tgfs_chat_start_idx(const struct tgfs_chat *chat)
{
	if (chat->msg_count < TGFS_MAX_MSG_CNT) {
        return 0;
    }

	return chat->next_idx;
}

int tgfs_chat_push(struct tgfs_chat *chat, const char *msg, size_t len)
{
	struct tgfs_msg *slot;

	if (!chat || !msg)
		return -EINVAL;

	if (len == 0)
		return -EINVAL;

	if (len >= TGFS_MAX_MSG_SIZE)
		return -EFBIG;

	pr_debug("[tgfs] chat_push: chat=%d len=%zu\n", chat->id, len);

	mutex_lock(&chat->lock);

	pr_debug(
        "[tgfs] chat_push: locked chat=%d next_idx=%zu msg_count=%zu\n",
		chat->id,
        chat->next_idx,
        chat->msg_count
    );

	slot = &chat->msgs[chat->next_idx];

	memcpy(slot->text, msg, len);
	slot->text[len] = '\0';
	slot->len = len;

	pr_debug(
        "[tgfs] chat_push: stored at idx=%zu text=\"%.*s\"\n",
		chat->next_idx,
        (int)len,
        msg
    );

	chat->next_idx = (chat->next_idx + 1) % TGFS_MAX_MSG_CNT;
	if (chat->msg_count < TGFS_MAX_MSG_CNT) {
        chat->msg_count++;
    }

	pr_debug(
        "[tgfs] chat_push: updated chat=%d next_idx=%zu msg_count=%zu\n",
		chat->id,
        chat->next_idx,
        chat->msg_count
    );

	mutex_unlock(&chat->lock);

	return 0;
}

ssize_t tgfs_chat_snapshot(struct tgfs_chat *chat, char *out, size_t out_size)
{
	size_t rendered = 0;
	size_t start_idx;
	size_t first_msg;
	size_t msg_to_show;

	if (!chat || !out)
		return -EINVAL;

	mutex_lock(&chat->lock);

	pr_debug(
        "[tgfs] snapshot: locked chat=%d msg_count=%zu next_idx=%zu out_size=%zu\n",
		chat->id,
        chat->msg_count,
        chat->next_idx,
        out_size
    );

	if (chat->msg_count == 0) {
		pr_debug("[tgfs] snapshot: chat=%d is empty\n", chat->id);
		mutex_unlock(&chat->lock);
		return 0;
	}

	start_idx = tgfs_chat_start_idx(chat);

	msg_to_show = chat->msg_count < TGFS_READ_MSG_LIMIT ?
		      chat->msg_count : TGFS_READ_MSG_LIMIT;

	first_msg = chat->msg_count - msg_to_show;

	pr_debug(
        "[tgfs] snapshot: chat=%d start_idx=%zu first_msg=%zu msg_to_show=%zu\n",
		chat->id,
        start_idx,
        first_msg,
        msg_to_show
    );

	for (size_t i = 0; i < msg_to_show; i++) {
		size_t logical_idx = first_msg + i;
		size_t real_idx = (start_idx + logical_idx) % TGFS_MAX_MSG_CNT;
		struct tgfs_msg *msg = &chat->msgs[real_idx];
		int written;

		pr_debug(
            "[tgfs] snapshot: chat=%d logical_idx=%zu real_idx=%zu len=%zu\n",
			chat->id,
            logical_idx,
            real_idx,
            msg->len
        );

		written = scnprintf(
            out + rendered,
			out_size - rendered,
			"[msg] %s\n",
			msg->text
        );

		if (written <= 0) {
			pr_debug(
                "[tgfs] snapshot: formatting failed chat=%d logical_idx=%zu\n",
				chat->id, logical_idx
            );

			mutex_unlock(&chat->lock);
			return -EINVAL;
		}

		if ((size_t)written >= out_size - rendered) {
			pr_debug(
                "[tgfs] snapshot: no space chat=%d rendered=%zu written=%d out_size=%zu\n",
				chat->id,
                rendered,
                written,
                out_size
            );

			mutex_unlock(&chat->lock);
			return -ENOSPC;
		}

		rendered += written;
	}

	pr_debug(
        "[tgfs] snapshot: chat=%d rendered=%zu bytes\n",
		chat->id,
        rendered
    );

	mutex_unlock(&chat->lock);
	return rendered;
}
