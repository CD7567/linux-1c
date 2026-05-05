#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/printk.h>

#include "telegramfs/device.h"
#include "telegramfs/chat.h"

static int tgfs_open(struct inode *inode, struct file *file)
{
	int minor = iminor(inode);

    pr_debug("[tgfs] call tgfs_open: minor=%d\n", minor);

	if (minor < 0 || minor >= TGFS_MAX_CHATS) {
        return -ENODEV;
    }

	file->private_data = &tgfs_chats[minor];
	return 0;
}

static int tgfs_release(struct inode *inode, struct file *file)
{
    pr_debug("[tgfs] call tgfs_release\n");
	return 0;
}

static ssize_t tgfs_normalize_user_message(
    const char __user *buf,
	size_t count,
	char *kbuf,
	size_t kbuf_size
)
{
	if (!buf || !kbuf) {
        return -EINVAL;
    }
		
	if (count == 0) {
        return 0;
    }

	if (count >= kbuf_size) {
        return -EFBIG;
    }

	pr_debug(
        "[tgfs] normalize: count=%zu kbuf_size=%zu\n",
		count,
        kbuf_size
    );

	if (copy_from_user(kbuf, buf, count)) {
		pr_debug("[tgfs] normalize: copy_from_user failed\n");
		return -EFAULT;
	}

	size_t msg_len = count;

	if (msg_len > 0 && kbuf[msg_len - 1] == '\n') {
		pr_debug("[tgfs] normalize: stripping trailing newline\n");
		msg_len--;
	}

	if (msg_len == 0) {
		pr_debug("[tgfs] normalize: message became empty\n");
		return 0;
	}

	kbuf[msg_len] = '\0';

	pr_debug(
        "[tgfs] normalize: result len=%zu text=\"%.*s\"\n",
		msg_len,
        (int)msg_len, kbuf
    );

	return (ssize_t)msg_len;
}

static ssize_t tgfs_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	struct tgfs_chat *chat = file->private_data;
	char *snapshot;
	ssize_t snapshot_len;
	size_t available;
	size_t to_copy;

	if (!chat || !buf || !ppos) {
        return -EINVAL;
    }

	if (count == 0) {
        return 0;
    }

	pr_debug(
        "[tgfs] read: chat=%d count=%zu ppos=%lld\n",
		chat->id,
        count,
        *ppos
    );

	snapshot = kmalloc(TGFS_SNAPSHOT_BUF_SIZE, GFP_KERNEL);
	if (!snapshot) {
		pr_debug("[tgfs] read: kmalloc failed size=%lu\n", TGFS_SNAPSHOT_BUF_SIZE);
		return -ENOMEM;
	}

	snapshot_len = tgfs_chat_snapshot(chat, snapshot, TGFS_SNAPSHOT_BUF_SIZE);
	if (snapshot_len < 0) {
		pr_debug("[tgfs] read: snapshot failed err=%zd\n", snapshot_len);
		kfree(snapshot);
		return snapshot_len;
	}

	pr_debug("[tgfs] read: snapshot_len=%zd\n", snapshot_len);

	if (*ppos >= snapshot_len) {
		pr_debug(
            "[tgfs] read: EOF ppos=%lld snapshot_len=%zd\n",
			*ppos,
            snapshot_len
        );
		kfree(snapshot);
		return 0;
	}

	available = snapshot_len - *ppos;
	to_copy = min(count, available);

	pr_debug(
        "[tgfs] read: available=%zu to_copy=%zu\n",
		available,
        to_copy
    );

	if (copy_to_user(buf, snapshot + *ppos, to_copy)) {
		pr_debug("[tgfs] read: copy_to_user failed to_copy=%zu\n", to_copy);
		kfree(snapshot);
		return -EFAULT;
	}

	*ppos += to_copy;

	pr_debug(
        "[tgfs] read: done copied=%zu new_ppos=%lld\n",
		to_copy,
        *ppos
    );

	kfree(snapshot);
	return to_copy;
}

static ssize_t tgfs_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	struct tgfs_chat *chat = file->private_data;
	char kbuf[TGFS_MAX_MSG_SIZE];
	ssize_t msg_len;
	int err;

	if (!chat || !buf) {
        return -EINVAL;
    }

	pr_debug(
        "[tgfs] write: chat=%d count=%zu ppos=%lld\n",
		chat->id,
        count,
        ppos ? *ppos : 0
    );

	msg_len = tgfs_normalize_user_message(buf, count, kbuf, sizeof(kbuf));
	if (msg_len < 0) {
		pr_debug("[tgfs] write: normalize failed err=%zd\n", msg_len);
		return msg_len;
	}

	if (msg_len == 0) {
		pr_debug("[tgfs] write: empty normalized message, skip store\n");
		return count;
	}

	err = tgfs_chat_push(chat, kbuf, msg_len);
	if (err < 0) {
		pr_debug("[tgfs] write: chat_push failed err=%d\n", err);
		return err;
	}

	pr_debug(
        "[tgfs] write: stored chat=%d len=%zd\n",
		chat->id,
        msg_len
    );

	return count;
}

struct file_operations tgfs_fops = {
	.owner = THIS_MODULE,
	.open = tgfs_open,
	.release = tgfs_release,
	.read = tgfs_read,
	.write = tgfs_write,
};
