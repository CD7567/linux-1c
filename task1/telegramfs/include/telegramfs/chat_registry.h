#ifndef TELEGRAMFS_CHAT_REGISTRY_H
#define TELEGRAMFS_CHAT_REGISTRY_H


#include <linux/mutex.h>
#include <linux/types.h>

#include "telegramfs/chat.h"

/*
 * Minor for control device
 */
#define TGFS_CONTROL_MINOR 0

/*
 * Size of snapshot buffer
 */
#define TGFS_CHAT_REGISTRY_SNAPSHOT_BUF_SIZE (TGFS_MAX_CHATS * TGFS_MAX_CHAT_NAME_LEN)

/*
 * Chat registry interface
 */
struct tgfs_registry {
	struct mutex lock;
	struct list_head chats;
	int next_chat_id;
	int next_minor;
};


/*
 * Initialize chat registry
 */
int tgfs_chat_registry_init(void);

/*
 * Destroy chat registry
 */
void tgfs_chat_registry_exit(void);


/*
 * Create a new chat
 */
int tgfs_chat_create(const char *opt_name, struct tgfs_chat **out_chat);

/*
 * Find chat by device minor
 */
struct tgfs_chat *tgfs_chat_registry_find_by_minor(int minor);

/*
 * Find chat by name
 */
struct tgfs_chat *tgfs_chat_registry_find_by_name(const char *name);

/*
 * Get chat registry snapshot
 */
ssize_t tgfs_chat_registry_snapshot(char *out, size_t out_size);

/*
 * Count currently registered chats
 */
int tgfs_chat_registry_get_chat_count(size_t *out_count);

/*
 * Iterate over chat registry
 */
void tgfs_chat_registry_for_each(void (*fn)(struct tgfs_chat *chat));

#endif /* TELEGRAMFS_CHAT_REGISTRY_H */
