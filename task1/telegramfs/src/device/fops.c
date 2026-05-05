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

static ssize_t tgfs_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct tgfs_chat *chat = file->private_data;
    size_t i, start_idx, real_idx, total, copied = 0;
    loff_t pos = *ppos;

    pr_debug("[tgfs] read: count=%zu, pos=%lld\n", count, pos);

    mutex_lock(&chat->lock);
    pr_debug("[tgfs] read: locked chat (msg_count=%lu, next_idx=%lu)\n",
             chat->msg_count, chat->next_idx);

    start_idx = chat->msg_count < TGFS_MAX_MSG_CNT ? 0 : chat->next_idx;
    total = 0;

    for (i = 0; i < chat->msg_count; i++) {
        real_idx = (start_idx + i) % TGFS_MAX_MSG_CNT;
        total += strlen(chat->msgs[real_idx]) + 1;
    }

    pr_debug("[tgfs] read: total bytes available = %zu\n", total);

    if (pos >= (loff_t)total) {
        pr_debug("[tgfs] read: EOF (pos=%lld >= total=%zu), returning 0\n", pos, total);
        mutex_unlock(&chat->lock);
        return 0;
    }

    size_t bytes_to_skip = (size_t)pos;
    size_t msg_i = 0;
    size_t offset = 0;

    pr_debug("[tgfs] read: need to skip %zu bytes to reach pos\n", bytes_to_skip);

    for (i = 0; i < chat->msg_count; i++) {
        real_idx = (start_idx + i) % TGFS_MAX_MSG_CNT;
        size_t len = strlen(chat->msgs[real_idx]) + 1; // +1 for '\n'

        if (bytes_to_skip < len) {
            offset = bytes_to_skip;
            msg_i = i;
            pr_debug("[tgfs] read: start at msg[%zu] (real_idx=%zu), offset=%zu\n",
                     i, real_idx, offset);
            break;
        }

        bytes_to_skip -= len;
    }

    while (copied < count && msg_i < chat->msg_count) {
        real_idx = (start_idx + msg_i) % TGFS_MAX_MSG_CNT;
        const char *src = chat->msgs[real_idx];
        size_t src_len = strlen(src);
        size_t to_copy = src_len - offset;

        if (to_copy > count - copied)
            to_copy = count - copied;

        pr_debug("[tgfs] read: copying msg[%zu] (len=%zu, offset=%zu, to_copy=%zu)\n",
                 msg_i, src_len, offset, to_copy);

        if (copy_to_user(buf + copied, src + offset, to_copy)) {
            pr_debug("[tgfs] read: copy_to_user failed at offset %zu\n", copied);
            mutex_unlock(&chat->lock);
            return -EFAULT;
        }

        copied += to_copy;
        offset += to_copy;

        if (offset == src_len && copied < count) {
            pr_debug("[tgfs] read: adding newline after message\n");

            if (copy_to_user(buf + copied, "\n", 1)) {
                pr_debug("[tgfs] read: copy_to_user of newline failed\n");
                mutex_unlock(&chat->lock);
                return -EFAULT;
            }

            copied++;
            offset = 0;
        }

        msg_i++;
    }

    *ppos += copied;
    pr_debug("[tgfs] read: finished, copied %zu bytes, new pos=%lld\n", copied, *ppos);
    mutex_unlock(&chat->lock);
    return copied;
}

static ssize_t tgfs_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    struct tgfs_chat *chat = file->private_data;
    char kbuf[TGFS_MAX_MSG_SIZE];
    ssize_t ret;
    size_t msg_len;

    pr_debug("[tgfs] write: count=%zu, ppos=%lld\n", count, ppos ? *ppos : 0);

    if (count == 0) {
        pr_debug("[tgfs] write: zero-length write, returning 0\n");
        return 0;
    }

    if (count >= TGFS_MAX_MSG_SIZE) {
        pr_debug("[tgfs] write: message too large (%zu >= %d), returning -EFBIG\n",
                 count, TGFS_MAX_MSG_SIZE);
        return -EFBIG;
    }

    mutex_lock(&chat->lock);
    pr_debug("[tgfs] write: locked chat (msg_count=%lu, next_idx=%lu)\n",
             chat->msg_count, chat->next_idx);

    if (copy_from_user(kbuf, buf, count)) {
        pr_debug("[tgfs] write: copy_from_user failed\n");
        ret = -EFAULT;
        goto out;
    }

    pr_debug("[tgfs] write: copied %zu bytes from user\n", count);

    msg_len = count;

    // Fix trailing '\n' behaviour
    if (msg_len > 0 && kbuf[msg_len-1] == '\n') {
        if (msg_len == 1) {
            pr_debug("[tgfs] write: single newline, ignoring but returning 1\n");
            ret = 1;
            goto out;
        }

        msg_len--;
        pr_debug("[tgfs] write: stripped trailing newline, new length=%zu\n", msg_len);
    }

    if (msg_len == 0) {
        pr_debug("[tgfs] write: message became empty after stripping, returning count=%zu\n", count);
        ret = count;
        goto out;
    }

    pr_debug("[tgfs] write: storing message (len=%zu): \"%.*s\"\n", msg_len, (int)msg_len, kbuf);

    memcpy(chat->msgs[chat->next_idx], kbuf, msg_len);
    chat->msgs[chat->next_idx][msg_len] = '\0';

    pr_debug("[tgfs] write: stored at index %lu\n", chat->next_idx);

    chat->next_idx = (chat->next_idx + 1) % TGFS_MAX_MSG_CNT;
    if (chat->msg_count < TGFS_MAX_MSG_CNT) {
        chat->msg_count++;
    }

    pr_debug("[tgfs] write: updated next_idx=%lu, msg_count=%lu\n",
             chat->next_idx, chat->msg_count);

    ret = count;

out:
    pr_debug("[tgfs] write: returning %zd\n", ret);
    mutex_unlock(&chat->lock);
    return ret;
}

struct file_operations tgfs_fops = {
	.owner = THIS_MODULE,
	.open = tgfs_open,
	.release = tgfs_release,
	.read = tgfs_read,
	.write = tgfs_write,
};
