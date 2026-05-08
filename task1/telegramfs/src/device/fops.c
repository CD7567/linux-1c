#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/printk.h>

#include "telegramfs/device.h"
#include "telegramfs/chat.h"
#include "telegramfs/chat_registry.h"
#include "telegramfs/ioctl.h"

static bool tgfs_is_control_file(struct file *file)
{
	return iminor(file_inode(file)) == TGFS_CONTROL_MINOR;
}

static void tgfs_trim_newline(char *buf, size_t *len)
{
	while (*len > 0 && buf[*len - 1] == '\n') {
		(*len)--;
	}

	buf[*len] = '\0';
}

static int tgfs_open(struct inode *inode, struct file *file)
{
	int minor = iminor(inode);
	struct tgfs_chat *chat;

	pr_debug("[tgfs] open: minor=%d\n", minor);

	if (minor == TGFS_CONTROL_MINOR) {
		file->private_data = NULL;
		return 0;
	}

	chat = tgfs_chat_registry_find_by_minor(minor);
	if (!chat)
		return -ENODEV;

	file->private_data = chat;
	return 0;
}

static int tgfs_release(struct inode *inode, struct file *file)
{
    pr_debug("[tgfs] call tgfs_release\n");
	return 0;
}

static ssize_t tgfs_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos)
{
	char *kbuf;
	ssize_t len;
	size_t to_copy;

	if (count == 0)
		return 0;

	if (tgfs_is_control_file(file)) {
		kbuf = kzalloc(TGFS_CHAT_REGISTRY_SNAPSHOT_BUF_SIZE, GFP_KERNEL);
		if (!kbuf)
			return -ENOMEM;

		len = tgfs_chat_registry_snapshot(kbuf, TGFS_CHAT_REGISTRY_SNAPSHOT_BUF_SIZE);
	} else {
		struct tgfs_chat *chat = file->private_data;

		if (!chat)
			return -EINVAL;

		kbuf = kzalloc(TGFS_CHAT_SNAPSHOT_BUF_SIZE, GFP_KERNEL);
		if (!kbuf)
			return -ENOMEM;

		len = tgfs_chat_snapshot(chat, kbuf, TGFS_CHAT_SNAPSHOT_BUF_SIZE);
	}

	if (len < 0) {
		kfree(kbuf);
		return len;
	}

	if (*ppos >= len) {
		kfree(kbuf);
		return 0;
	}

	to_copy = min_t(size_t, count, len - *ppos);
	if (copy_to_user(buf, kbuf + *ppos, to_copy)) {
		kfree(kbuf);
		return -EFAULT;
	}

	*ppos += to_copy;
	kfree(kbuf);
	return to_copy;
}

static ssize_t tgfs_write(struct file *file, const char __user *buf,
			  size_t count, loff_t *ppos)
{
	char kbuf[TGFS_MAX_MSG_SIZE];
	size_t len;
	int err;

	if (count == 0)
		return 0;

	if (count >= TGFS_MAX_MSG_SIZE)
		return -EFBIG;

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;

	len = count;
	kbuf[len] = '\0';
	tgfs_trim_newline(kbuf, &len);

	if (tgfs_is_control_file(file)) {
		struct tgfs_chat *chat;
		const char *name = len > 0 ? kbuf : NULL;

		err = tgfs_chat_create(name, &chat);
		if (err < 0)
			return err;

		err = tgfs_chrdev_create_chat_device(chat);
		if (err < 0)
			return err;

		return count;
	} else {
		struct tgfs_chat *chat = file->private_data;

		if (!chat)
			return -EINVAL;

		if (len == 0)
			return count;

		err = tgfs_chat_push(chat, kbuf, len);
		if (err < 0)
			return err;

		return count;
	}
}

static long tgfs_control_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg)
{
	switch (cmd) {
	case TGFS_IOCTL_CTL_CREATE_CHAT: {
		struct tgfs_ctl_create_req req;
		struct tgfs_chat *chat;
		const char *name = NULL;
		int err;

		if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
			return -EFAULT;

		req.name[TGFS_MAX_CHAT_NAME_LEN - 1] = '\0';
		if (req.name[0] != '\0')
			name = req.name;

		err = tgfs_chat_create(name, &chat);
		if (err < 0)
			return err;

		err = tgfs_chrdev_create_chat_device(chat);
		if (err < 0)
			return err;

		req.chat_id = chat->id;
		strscpy(req.device_name, chat->name, sizeof(req.device_name));

		if (copy_to_user((void __user *)arg, &req, sizeof(req)))
			return -EFAULT;

		return 0;
	}

	case TGFS_IOCTL_CTL_GET_CHAT_COUNT: {
		size_t count;
		int err;

		err = tgfs_chat_registry_get_chat_count(&count);
		if (err < 0)
			return err;

		if (copy_to_user((void __user *)arg, &count, sizeof(count)))
			return -EFAULT;

		return 0;
	}

	default:
		return -ENOTTY;
	}
}

static long tgfs_chat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct tgfs_chat *chat = file->private_data;
	size_t value;
	int err;

	if (!chat)
		return -EINVAL;

	pr_debug(
        "[tgfs] ioctl: chat=%d cmd=0x%x arg=0x%lx\n",
		chat->id, cmd, arg
    );

	switch (cmd) {
	case TGFS_IOCTL_CHAT_GET_MSG_COUNT:
		err = tgfs_chat_get_msg_count(chat, &value);
		if (err < 0)
			return err;

		if (copy_to_user((size_t __user *)arg, &value, sizeof(value)))
			return -EFAULT;

		pr_debug("[tgfs] ioctl: GET_MSG_COUNT=%zu\n", value);
		return 0;

	case TGFS_IOCTL_CHAT_CLEAR:
		err = tgfs_chat_clear(chat);
		if (err < 0)
			return err;

		pr_debug("[tgfs] ioctl: CLEAR_CHAT done\n");
		return 0;

	case TGFS_IOCTL_CHAT_GET_READ_LIMIT:
		err = tgfs_chat_get_read_limit(chat, &value);
		if (err < 0)
			return err;

		if (copy_to_user((size_t __user *)arg, &value, sizeof(value)))
			return -EFAULT;

		pr_debug("[tgfs] ioctl: GET_READ_LIMIT=%zu\n", value);
		return 0;

	case TGFS_IOCTL_CHAT_SET_READ_LIMIT:
		if (copy_from_user((void *)&value, (size_t __user *)arg, sizeof(value)))
			return -EFAULT;

		err = tgfs_chat_set_read_limit(chat, value);
		if (err < 0)
			return err;

		pr_debug("[tgfs] ioctl: SET_READ_LIMIT=%zu\n", value);
		return 0;

	default:
		pr_debug("[tgfs] ioctl: unsupported cmd=0x%x\n", cmd);
		return -ENOTTY;
	}
}

static long tgfs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	if (tgfs_is_control_file(file))
		return tgfs_control_ioctl(file, cmd, arg);

	return tgfs_chat_ioctl(file, cmd, arg);
}

struct file_operations tgfs_fops = {
	.owner = THIS_MODULE,
	.open = tgfs_open,
	.release = tgfs_release,
	.read = tgfs_read,
	.write = tgfs_write,
    .unlocked_ioctl = tgfs_ioctl,
};
