#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define MINIFS_MAGIC 0x20260110
#define MINIFS_MAX_FILE_SIZE 4096
#define MINIFS_DEFAULT_FS_SIZE (64 * 1024)


/* ------------------------------------------------------------------ */
/*                    MINIFS INTERNAL STRUCTURES                      */
/* ------------------------------------------------------------------ */


struct minifs_node {
	bool is_dir;

	// File inode attributes
	char *data;
	size_t size;

	// Directory inode attributes
	struct list_head children;
	struct list_head siblings;
};

struct minifs_sb_info {
	size_t max_bytes;
	size_t used_bytes;

	unsigned long files_count;
	unsigned long dirs_count;
	unsigned long read_ops;
	unsigned long write_ops;
	unsigned long create_ops;
	unsigned long mkdir_ops;
	unsigned long unlink_ops;
	unsigned long rmdir_ops;
};

static struct proc_dir_entry *minifs_proc_entry;
static struct super_block *minifs_current_sb;


/* ------------------------------------------------------------ */
/*                          HELPERS                             */
/* ------------------------------------------------------------ */


static inline struct minifs_node *MINIFS_I(struct inode *inode)
{
	return inode ? inode->i_private : NULL;
}

static inline struct minifs_sb_info *MINIFS_SB(struct super_block *sb)
{
	return sb ? sb->s_fs_info : NULL;
}

static bool minifs_dir_is_empty(struct inode *inode)
{
	struct minifs_node *node = MINIFS_I(inode);

	if (!node || !node->is_dir)
		return false;

	return list_empty(&node->children);
}

static struct minifs_node *minifs_alloc_node(bool is_dir)
{
	struct minifs_node *node;

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node)
		return NULL;

	node->is_dir = is_dir;
	node->data = NULL;
	node->size = 0;
	INIT_LIST_HEAD(&node->children);
	INIT_LIST_HEAD(&node->siblings);

	return node;
}


/* ------------------------------------------------------------ */
/*                     FORWARD DECLARATIONS                     */
/* ------------------------------------------------------------ */


static struct inode *minifs_get_inode(struct super_block *sb,
				      const struct inode *dir,
				      umode_t mode);

static int minifs_create(struct mnt_idmap *idmap,
			 struct inode *dir,
			 struct dentry *dentry,
			 umode_t mode,
			 bool excl);

static int minifs_mkdir(struct mnt_idmap *idmap,
			struct inode *dir,
			struct dentry *dentry,
			umode_t mode);

static int minifs_unlink(struct inode *dir, struct dentry *dentry);
static int minifs_rmdir(struct inode *dir, struct dentry *dentry);

static ssize_t minifs_read(struct file *file,
			   char __user *buf,
			   size_t len,
			   loff_t *ppos);

static ssize_t minifs_write(struct file *file,
			    const char __user *buf,
			    size_t len,
			    loff_t *ppos);

static int minifs_stats_show(struct seq_file *m, void *v);
static int minifs_stats_open(struct inode *inode, struct file *file);


/* ------------------------------------------------------------ */
/*                        FILE OPS                              */
/* ------------------------------------------------------------ */


static const struct file_operations minifs_file_ops = {
	.owner = THIS_MODULE,
	.read = minifs_read,
	.write = minifs_write,
	.llseek = generic_file_llseek,
};

static const struct inode_operations minifs_file_inode_ops = {
	.getattr = simple_getattr,
};

static const struct inode_operations minifs_dir_inode_ops = {
	.lookup = simple_lookup,
	.create = minifs_create,
	.mkdir = minifs_mkdir,
	.unlink = minifs_unlink,
	.rmdir = minifs_rmdir,
};


/* ------------------------------------------------------------ */
/*                        PROCFS OPS                            */
/* ------------------------------------------------------------ */


static const struct proc_ops minifs_proc_ops = {
	.proc_open = minifs_stats_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};


/* ------------------------------------------------------------ */
/*                       READ / WRITE                           */
/* ------------------------------------------------------------ */


static ssize_t minifs_read(struct file *file,
			   char __user *buf,
			   size_t len,
			   loff_t *ppos)
{
	struct inode *inode = file_inode(file);
	struct minifs_node *node = MINIFS_I(inode);
	struct minifs_sb_info *sbi = MINIFS_SB(inode->i_sb);
	ssize_t ret;

	pr_debug("[minifs] read: ino=%lu len=%zu pos=%lld\n",
		 inode->i_ino, len, *ppos);

	if (!node || node->is_dir)
		return -EISDIR;

	if (!node->data)
		return 0;

	ret = simple_read_from_buffer(buf, len, ppos, node->data, node->size);
	if (ret >= 0 && sbi)
		sbi->read_ops++;

	return ret;
}

static ssize_t minifs_write(struct file *file,
			    const char __user *buf,
			    size_t len,
			    loff_t *ppos)
{
	struct inode *inode = file_inode(file);
	struct minifs_node *node = MINIFS_I(inode);
	struct minifs_sb_info *sbi = MINIFS_SB(inode->i_sb);
	size_t to_copy;
	size_t old_size;
	size_t new_size;
	size_t growth;
	ssize_t ret;

	pr_debug("[minifs] write: ino=%lu len=%zu pos=%lld\n",
		 inode->i_ino, len, *ppos);

	if (!node)
		return -EIO;

	if (node->is_dir)
		return -EISDIR;

	if (!node->data) {
		node->data = kzalloc(MINIFS_MAX_FILE_SIZE, GFP_KERNEL);
		if (!node->data)
			return -ENOMEM;
	}

	if (*ppos >= MINIFS_MAX_FILE_SIZE)
		return -ENOSPC;

	to_copy = min_t(size_t, len, MINIFS_MAX_FILE_SIZE - *ppos);

	old_size = node->size;
	new_size = max_t(size_t, node->size, *ppos + to_copy);
	growth = new_size - old_size;

	if (sbi && growth && (sbi->used_bytes + growth > sbi->max_bytes))
		return -ENOSPC;

	ret = simple_write_to_buffer(node->data,
				     MINIFS_MAX_FILE_SIZE,
				     ppos,
				     buf,
				     to_copy);
	if (ret < 0)
		return ret;

	if (*ppos > node->size)
		node->size = *ppos;

	if (sbi && node->size > old_size)
		sbi->used_bytes += (node->size - old_size);

	i_size_write(inode, node->size);
	inode_set_mtime_to_ts(inode, current_time(inode));
	inode_set_ctime_current(inode);

	if (sbi)
		sbi->write_ops++;

	return ret;
}


/* ------------------------------------------------------------ */
/*                      INODE CREATION                          */
/* ------------------------------------------------------------ */


static struct inode *minifs_get_inode(struct super_block *sb,
				      const struct inode *dir,
				      umode_t mode)
{
	struct inode *inode;
	struct minifs_node *node;

	inode = new_inode(sb);
	if (!inode)
		return NULL;

	node = minifs_alloc_node(S_ISDIR(mode));
	if (!node) {
		iput(inode);
		return NULL;
	}

	inode->i_ino = get_next_ino();
	inode_init_owner(&nop_mnt_idmap, inode, dir, mode);

	inode_set_atime_to_ts(inode, current_time(inode));
	inode_set_mtime_to_ts(inode, current_time(inode));
	inode_set_ctime_current(inode);

	inode->i_size = 0;
	inode->i_private = node;

	switch (mode & S_IFMT) {
	case S_IFDIR:
		inode->i_op = &minifs_dir_inode_ops;
		inode->i_fop = &simple_dir_operations;
		inc_nlink(inode);
		break;

	case S_IFREG:
		inode->i_op = &minifs_file_inode_ops;
		inode->i_fop = &minifs_file_ops;
		break;

	default:
		init_special_inode(inode, mode, 0);
		break;
	}

	return inode;
}


/* ------------------------------------------------------------ */
/*                 CREATE / MKDIR / UNLINK / RMDIR              */
/* ------------------------------------------------------------ */


static int minifs_create(struct mnt_idmap *idmap,
			 struct inode *dir,
			 struct dentry *dentry,
			 umode_t mode,
			 bool excl)
{
	struct inode *inode;
	struct minifs_node *parent_node;
	struct minifs_node *node;
	struct minifs_sb_info *sbi;

	inode = minifs_get_inode(dir->i_sb, dir, S_IFREG | mode);
	if (!inode)
		return -ENOMEM;

	parent_node = MINIFS_I(dir);
	node = MINIFS_I(inode);
	sbi = MINIFS_SB(dir->i_sb);

	if (!parent_node || !parent_node->is_dir) {
		iput(inode);
		return -ENOTDIR;
	}

	list_add_tail(&node->siblings, &parent_node->children);

	d_instantiate(dentry, inode);
	dget(dentry);

	inode_set_mtime_to_ts(dir, current_time(dir));
	inode_set_ctime_current(dir);

	if (sbi) {
		sbi->files_count++;
		sbi->create_ops++;
	}

	return 0;
}

static int minifs_mkdir(struct mnt_idmap *idmap,
			struct inode *dir,
			struct dentry *dentry,
			umode_t mode)
{
	struct inode *inode;
	struct minifs_node *parent_node;
	struct minifs_node *node;
	struct minifs_sb_info *sbi;

	inode = minifs_get_inode(dir->i_sb, dir, S_IFDIR | mode);
	if (!inode)
		return -ENOMEM;

	parent_node = MINIFS_I(dir);
	node = MINIFS_I(inode);
	sbi = MINIFS_SB(dir->i_sb);

	if (!parent_node || !parent_node->is_dir) {
		iput(inode);
		return -ENOTDIR;
	}

	list_add_tail(&node->siblings, &parent_node->children);

	inc_nlink(dir);
	d_instantiate(dentry, inode);
	dget(dentry);

	inode_set_mtime_to_ts(dir, current_time(dir));
	inode_set_ctime_current(dir);

	if (sbi) {
		sbi->dirs_count++;
		sbi->mkdir_ops++;
	}

	return 0;
}

static int minifs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);
	struct minifs_node *node = MINIFS_I(inode);
	struct minifs_sb_info *sbi = MINIFS_SB(dir->i_sb);

	if (!inode)
		return -ENOENT;

	if (!node || node->is_dir)
		return -EISDIR;

	list_del_init(&node->siblings);

	if (sbi) {
		if (sbi->files_count > 0)
			sbi->files_count--;
		sbi->unlink_ops++;

		if (sbi->used_bytes >= node->size)
			sbi->used_bytes -= node->size;
		else
			sbi->used_bytes = 0;
	}

	inode_set_ctime_current(inode);
	inode_set_mtime_to_ts(dir, current_time(dir));
	inode_set_ctime_current(dir);

	drop_nlink(inode);
	d_drop(dentry);
	dput(dentry);

	return 0;
}

static int minifs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);
	struct minifs_node *node = MINIFS_I(inode);
	struct minifs_sb_info *sbi = MINIFS_SB(dir->i_sb);

	if (!inode)
		return -ENOENT;

	if (!node || !node->is_dir)
		return -ENOTDIR;

	if (!minifs_dir_is_empty(inode))
		return -ENOTEMPTY;

	list_del_init(&node->siblings);

	if (sbi) {
		if (sbi->dirs_count > 0)
			sbi->dirs_count--;
		sbi->rmdir_ops++;
	}

	inode_set_mtime_to_ts(dir, current_time(dir));
	inode_set_ctime_current(dir);

	drop_nlink(inode);
	drop_nlink(dir);
	d_drop(dentry);
	dput(dentry);

	return 0;
}

/* ------------------------------------------------------------ */
/*                      EVICT / SUPER OPS                       */
/* ------------------------------------------------------------ */

static void minifs_evict_inode(struct inode *inode)
{
	struct minifs_node *node = MINIFS_I(inode);

	truncate_inode_pages_final(&inode->i_data);
	clear_inode(inode);

	if (node) {
		kfree(node->data);
		kfree(node);
		inode->i_private = NULL;
	}
}

static const struct super_operations minifs_super_ops = {
	.statfs = simple_statfs,
	.drop_inode = generic_delete_inode,
	.evict_inode = minifs_evict_inode,
};

static void minifs_kill_sb(struct super_block *sb)
{
	struct minifs_sb_info *sbi = MINIFS_SB(sb);

	if (minifs_current_sb == sb)
		minifs_current_sb = NULL;

	kill_litter_super(sb);
	kfree(sbi);
}


/* ------------------------------------------------------------ */
/*                        FILL SUPER                            */
/* ------------------------------------------------------------ */


static int minifs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct inode *root_inode;
	struct minifs_sb_info *sbi;

	sbi = kzalloc(sizeof(*sbi), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;

	sbi->max_bytes = MINIFS_DEFAULT_FS_SIZE;
	sb->s_fs_info = sbi;

	sb->s_magic = MINIFS_MAGIC;
	sb->s_op = &minifs_super_ops;
	sb->s_time_gran = 1;

	root_inode = minifs_get_inode(sb, NULL, S_IFDIR | 0755);
	if (!root_inode) {
		kfree(sbi);
		sb->s_fs_info = NULL;
		return -ENOMEM;
	}

	sb->s_root = d_make_root(root_inode);
	if (!sb->s_root) {
		iput(root_inode);
		kfree(sbi);
		sb->s_fs_info = NULL;
		return -ENOMEM;
	}

	sbi->dirs_count = 1; /* root */
	minifs_current_sb = sb;

	return 0;
}


/* ------------------------------------------------------------ */
/*                          MOUNT                               */
/* ------------------------------------------------------------ */


static struct dentry *minifs_mount(struct file_system_type *fs_type,
				   int flags,
				   const char *dev_name,
				   void *data)
{
	return mount_nodev(fs_type, flags, data, minifs_fill_super);
}

static struct file_system_type minifs_type = {
	.owner = THIS_MODULE,
	.name = "minifs",
	.mount = minifs_mount,
	.kill_sb = minifs_kill_sb,
};


/* ------------------------------------------------------------ */
/*                     /proc/minifs_stats                       */
/* ------------------------------------------------------------ */


static int minifs_stats_show(struct seq_file *m, void *v)
{
	struct super_block *sb = minifs_current_sb;
	struct minifs_sb_info *sbi = NULL;

	seq_puts(m, "minifs_stats\n");

	if (sb)
		sbi = MINIFS_SB(sb);

	if (!sbi) {
		seq_puts(m, "state=unavailable\n");
		return 0;
	}

	seq_printf(m, "files=%lu\n", sbi->files_count);
	seq_printf(m, "dirs=%lu\n", sbi->dirs_count);
	seq_printf(m, "used_bytes=%zu\n", sbi->used_bytes);
	seq_printf(m, "max_bytes=%zu\n", sbi->max_bytes);
	seq_printf(m, "read_ops=%lu\n", sbi->read_ops);
	seq_printf(m, "write_ops=%lu\n", sbi->write_ops);
	seq_printf(m, "create_ops=%lu\n", sbi->create_ops);
	seq_printf(m, "mkdir_ops=%lu\n", sbi->mkdir_ops);
	seq_printf(m, "unlink_ops=%lu\n", sbi->unlink_ops);
	seq_printf(m, "rmdir_ops=%lu\n", sbi->rmdir_ops);

	return 0;
}

static int minifs_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, minifs_stats_show, NULL);
}


/* ------------------------------------------------------------ */
/*                    MODULE INIT / EXIT                        */
/* ------------------------------------------------------------ */


static int __init minifs_init(void)
{
	int ret;

	ret = register_filesystem(&minifs_type);
	if (ret)
		return ret;

	minifs_proc_entry = proc_create("minifs_stats", 0444, NULL, &minifs_proc_ops);
	if (!minifs_proc_entry)
		pr_err("[minifs] failed to create /proc/minifs_stats\n");

	pr_info("[minifs] registered\n");
	return 0;
}

static void __exit minifs_exit(void)
{
	int ret;

	if (minifs_proc_entry)
		remove_proc_entry("minifs_stats", NULL);

	ret = unregister_filesystem(&minifs_type);
	if (ret)
		pr_err("[minifs] unregister_filesystem failed: %d\n", ret);
	else
		pr_info("[minifs] unregistered\n");
}

module_init(minifs_init);
module_exit(minifs_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("CD7567");
MODULE_DESCRIPTION("Educational in-memory VFS filesystem");
