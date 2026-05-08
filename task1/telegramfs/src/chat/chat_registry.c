#include <linux/slab.h>

#include "telegramfs/chat_registry.h"
#include "telegramfs/chat.h"
#include "telegramfs/ioctl.h"


static struct tgfs_registry tgfs_registry;

static bool tgfs_name_is_valid(const char *name)
{
	const char *p;

	if (!name || !*name)
		return false;

	if (strcmp(name, "create_chat") == 0)
		return false;

	for (p = name; *p; p++) {
		if ((*p >= 'a' && *p <= 'z') ||
		    (*p >= 'A' && *p <= 'Z') ||
		    (*p >= '0' && *p <= '9') ||
		    *p == '_' || *p == '-') {
			continue;
		}
		return false;
	}

	return true;
}

int tgfs_chat_registry_init(void)
{
	mutex_init(&tgfs_registry.lock);
	INIT_LIST_HEAD(&tgfs_registry.chats);
	tgfs_registry.next_chat_id = 0;
	tgfs_registry.next_minor = 1;

	pr_debug("[tgfs] chat registry initialized\n");
	return 0;
}

void tgfs_chat_registry_exit(void)
{
	struct tgfs_chat *chat, *tmp;

	mutex_lock(&tgfs_registry.lock);
	list_for_each_entry_safe(chat, tmp, &tgfs_registry.chats, link) {
		list_del(&chat->link);

		mutex_destroy(&chat->lock);
		kfree(chat);
	}

	mutex_unlock(&tgfs_registry.lock);
	mutex_destroy(&tgfs_registry.lock);
	pr_debug("[tgfs] chat registry destroyed\n");
}

int tgfs_chat_create(const char *opt_name, struct tgfs_chat **out_chat)
{
	struct tgfs_chat *chat;
	char final_name[TGFS_MAX_CHAT_NAME_LEN];
	bool named = false;
	int id;
	int minor;
	int err = 0;

	if (!out_chat)
		return -EINVAL;

	*out_chat = NULL;

	mutex_lock(&tgfs_registry.lock);

	if (tgfs_registry.next_minor > TGFS_MAX_CHATS) {
		err = -ENOSPC;
		goto out_unlock;
	}

	if (opt_name && opt_name[0] != '\0') {
		if (!tgfs_name_is_valid(opt_name)) {
			err = -EINVAL;
			goto out_unlock;
		}

		list_for_each_entry(chat, &tgfs_registry.chats, link) {
			if (strcmp(chat->name, opt_name) == 0) {
				err = -EEXIST;
				goto out_unlock;
			}
		}

		strscpy(final_name, opt_name, sizeof(final_name));
		named = true;
	}

	id = tgfs_registry.next_chat_id++;
	minor = tgfs_registry.next_minor++;

	if (!named)
		snprintf(final_name, sizeof(final_name), "chat%d", id);

	chat = kzalloc(sizeof(*chat), GFP_KERNEL);
	if (!chat) {
		err = -ENOMEM;
		goto out_unlock;
	}

	chat->id = id;
	chat->minor = minor;
	strscpy(chat->name, final_name, sizeof(chat->name));
	mutex_init(&chat->lock);
	INIT_LIST_HEAD(&chat->link);
	chat->read_limit = TGFS_READ_MSG_LIMIT_DEFAULT;
	chat->msg_count = 0;
	chat->next_idx = 0;
	chat->dev = NULL;

	list_add_tail(&chat->link, &tgfs_registry.chats);

	pr_debug(
        "[tgfs] chat created: id=%d minor=%d name=%s\n",
		chat->id, chat->minor, chat->name
    );

	*out_chat = chat;

out_unlock:
	mutex_unlock(&tgfs_registry.lock);
	return err;
}

struct tgfs_chat *tgfs_chat_registry_find_by_minor(int minor)
{
	struct tgfs_chat *chat;

	mutex_lock(&tgfs_registry.lock);
	list_for_each_entry(chat, &tgfs_registry.chats, link) {
		if (chat->minor == minor) {
			mutex_unlock(&tgfs_registry.lock);
			return chat;
		}
	}
	mutex_unlock(&tgfs_registry.lock);
	return NULL;
}

struct tgfs_chat *tgfs_chat_registry_find_by_name(const char *name)
{
	struct tgfs_chat *chat;

	if (!name)
		return NULL;

	mutex_lock(&tgfs_registry.lock);
	list_for_each_entry(chat, &tgfs_registry.chats, link) {
		if (strcmp(chat->name, name) == 0) {
			mutex_unlock(&tgfs_registry.lock);
			return chat;
		}
	}
	mutex_unlock(&tgfs_registry.lock);
	return NULL;
}

ssize_t tgfs_chat_registry_snapshot(char *out, size_t out_size)
{
	struct tgfs_chat *chat;
	size_t rendered = 0;
	int written;

	if (!out)
		return -EINVAL;

	mutex_lock(&tgfs_registry.lock);

	list_for_each_entry(chat, &tgfs_registry.chats, link) {
		written = scnprintf(out + rendered,
				    out_size - rendered,
				    "%s\n",
				    chat->name);
		if (written <= 0) {
			mutex_unlock(&tgfs_registry.lock);
			return -EINVAL;
		}

		if ((size_t)written >= out_size - rendered) {
			mutex_unlock(&tgfs_registry.lock);
			return -ENOSPC;
		}

		rendered += written;
	}

	mutex_unlock(&tgfs_registry.lock);
	return rendered;
}

int tgfs_chat_registry_get_chat_count(size_t *out_count)
{
	struct tgfs_chat *chat;
	__u32 count = 0;

	if (!out_count)
		return -EINVAL;

	mutex_lock(&tgfs_registry.lock);
	list_for_each_entry(chat, &tgfs_registry.chats, link)
		count++;
	mutex_unlock(&tgfs_registry.lock);

	*out_count = count;
	return 0;
}

void tgfs_chat_registry_for_each(void (*fn)(struct tgfs_chat *chat))
{
	struct tgfs_chat *chat;

	if (!fn)
		return;

	mutex_lock(&tgfs_registry.lock);
	list_for_each_entry(chat, &tgfs_registry.chats, link)
		fn(chat);
	mutex_unlock(&tgfs_registry.lock);
}
