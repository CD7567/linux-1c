#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/printk.h>

#include "telegramfs/device.h"
#include "telegramfs/chat.h"
#include "telegramfs/chat_registry.h"

static dev_t tgfs_devt;
static struct cdev tgfs_cdev;
static struct class *tgfs_class;

static void tgfs_chrdev_destroy_chat_device_cb(struct tgfs_chat *chat)
{
	tgfs_chrdev_destroy_chat_device(chat);
}

int tgfs_chrdev_init(void)
{
	int err;

	err = alloc_chrdev_region(&tgfs_devt, 0, TGFS_MAX_CHATS + 1, "tgfs");
	if (err < 0) {
		pr_emerg("[tgfs] failed to allocate chrdev region\n");
		return err;
	}

	cdev_init(&tgfs_cdev, &tgfs_fops);
	tgfs_cdev.owner = THIS_MODULE;

	err = cdev_add(&tgfs_cdev, tgfs_devt, TGFS_MAX_CHATS + 1);
	if (err < 0) {
		pr_emerg("[tgfs] failed to add chrdev\n");
		goto err_region;
	}

	tgfs_class = class_create("telegram");
	if (IS_ERR(tgfs_class)) {
		pr_emerg("[tgfs] failed to create chrdev class\n");
		err = PTR_ERR(tgfs_class);
		goto err_cdev;
	}

	if (IS_ERR(device_create(tgfs_class, NULL,
				 MKDEV(MAJOR(tgfs_devt), TGFS_CONTROL_MINOR),
				 NULL,
				 "telegram/create_chat"))) {
		pr_emerg("[tgfs] failed to create control device\n");
		err = -ENODEV;
		goto err_class;
	}

	pr_info("[tgfs] control device registered: /dev/telegram/create_chat\n");
	return 0;

err_class:
	class_destroy(tgfs_class);
err_cdev:
	cdev_del(&tgfs_cdev);
err_region:
	unregister_chrdev_region(tgfs_devt, TGFS_MAX_CHATS + 1);
	return err;
}

void tgfs_chrdev_exit(void)
{
	tgfs_chat_registry_for_each(tgfs_chrdev_destroy_chat_device_cb);

	device_destroy(tgfs_class, MKDEV(MAJOR(tgfs_devt), TGFS_CONTROL_MINOR));
	class_destroy(tgfs_class);
	cdev_del(&tgfs_cdev);
	unregister_chrdev_region(tgfs_devt, TGFS_MAX_CHATS + 1);
	pr_info("[tgfs] chrdevs unregistered\n");
}

int tgfs_chrdev_create_chat_device(struct tgfs_chat *chat)
{
	if (!chat)
		return -EINVAL;

	chat->dev = device_create(tgfs_class,
				  NULL,
				  MKDEV(MAJOR(tgfs_devt), chat->minor),
				  NULL,
				  "telegram/%s",
				  chat->name);
	if (IS_ERR(chat->dev)) {
		int err = PTR_ERR(chat->dev);
		chat->dev = NULL;
		pr_err("[tgfs] failed to create chat device %s\n", chat->name);
		return err;
	}

	pr_info("[tgfs] created /dev/telegram/%s (minor=%d)\n",
		chat->name, chat->minor);
	return 0;
}

void tgfs_chrdev_destroy_chat_device(struct tgfs_chat *chat)
{
	if (!chat || !chat->dev)
		return;

	device_destroy(tgfs_class, MKDEV(MAJOR(tgfs_devt), chat->minor));
	chat->dev = NULL;
}
