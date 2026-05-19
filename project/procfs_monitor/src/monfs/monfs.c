#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/namei.h>
#include <linux/mount.h>

#include "metric_backend/metric_backend.h"

#include "monfs_monitor/monfs.h"


enum monfs_inode_kind {
    MONFS_INO_ROOT = 1,
    MONFS_INO_SYSTEM_FILE,
    MONFS_INO_PID_DIR,
    MONFS_INO_PID_FILE,
};

struct monfs_inode_info {
    enum monfs_inode_kind kind;
    pid_t pid;
};

static struct file_system_type monfs_type;


/* ------------------------------------------------------------------ */
/*                            MONFS RENDER                            */
/* ------------------------------------------------------------------ */


static inline struct monfs_inode_info *MONFS_I(struct inode *inode)
{
    return inode ? inode->i_private : NULL;
}

static struct inode *monfs_get_inode(struct super_block *sb,
                                     const struct inode *dir,
                                     umode_t mode,
                                     enum monfs_inode_kind kind,
                                     pid_t pid)
{
    struct inode *inode;
    struct monfs_inode_info *info;

    inode = new_inode(sb);
    if (!inode)
        return NULL;

    info = kzalloc(sizeof(*info), GFP_KERNEL);
    if (!info) {
        iput(inode);
        return NULL;
    }

    info->kind = kind;
    info->pid = pid;

    inode->i_ino = get_next_ino();
    inode_init_owner(&nop_mnt_idmap, inode, dir, mode);
    inode_set_atime_to_ts(inode, current_time(inode));
    inode_set_mtime_to_ts(inode, current_time(inode));
    inode_set_ctime_current(inode);
    inode->i_private = info;
    inode->i_size = 0;

    return inode;
}

static ssize_t monfs_render_system(char **out_buf)
{
    struct procfs_monitor_system_snapshot snap;
    unsigned int cpu_usage_int;
    unsigned int cpu_usage_frac;
    char *buf;
    int len;
    int ret;

    if (!out_buf)
        return -EINVAL;

    ret = procfs_monitor_collect_system_snapshot(&snap);
    if (ret)
        return ret;

    cpu_usage_int = snap.cpu_usage_x10 / 10;
    cpu_usage_frac = snap.cpu_usage_x10 % 10;

    buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    len = scnprintf(buf, PAGE_SIZE,
                    "uptime_sec: %lu\n"
                    "mem_total_kb: %llu\n"
                    "mem_available_kb: %llu\n"
                    "mem_used_kb: %llu\n"
                    "cpu_usage_percent: %u.%u\n"
                    "tasks_total: %lu\n"
                    "tasks_runnable: %lu\n"
                    "tasks_interruptible_sleep: %lu\n"
                    "tasks_uninterruptible_sleep: %lu\n"
                    "tasks_stopped: %lu\n"
                    "tasks_zombie: %lu\n"
                    "tasks_other: %lu\n",
                    snap.uptime_sec,
                    snap.mem_total_kb,
                    snap.mem_available_kb,
                    snap.mem_used_kb,
                    cpu_usage_int,
                    cpu_usage_frac,
                    snap.tasks_total,
                    snap.tasks_runnable,
                    snap.tasks_interruptible_sleep,
                    snap.tasks_uninterruptible_sleep,
                    snap.tasks_stopped,
                    snap.tasks_zombie,
                    snap.tasks_other);

    *out_buf = buf;
    return len;
}

static ssize_t monfs_render_pid(pid_t pid, char **out_buf)
{
    struct procfs_monitor_process_snapshot snap;
    char *buf;
    int len;
    int ret;

    if (!out_buf)
        return -EINVAL;

    ret = procfs_monitor_collect_process_snapshot(pid, &snap);
    if (ret)
        return ret;

    buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    len = scnprintf(buf, PAGE_SIZE,
                    "pid: %d\n"
                    "ppid: %d\n"
                    "comm: %s\n"
                    "state: %s\n"
                    "threads: %d\n"
                    "vm_size_kb: %llu\n"
                    "rss_kb: %llu\n"
                    "cpu_time_user_ticks: %llu\n"
                    "cpu_time_system_ticks: %llu\n"
                    "cpu_time_total_ticks: %llu\n",
                    snap.pid,
                    snap.ppid,
                    snap.comm,
                    snap.state,
                    snap.threads,
                    snap.vm_size_kb,
                    snap.rss_kb,
                    snap.cpu_time_user_ticks,
                    snap.cpu_time_system_ticks,
                    snap.cpu_time_total_ticks);

    *out_buf = buf;
    return len;
}

static ssize_t monfs_read(struct file *file,
                          char __user *buf,
                          size_t len,
                          loff_t *ppos)
{
    struct inode *inode = file_inode(file);
    struct monfs_inode_info *info = MONFS_I(inode);
    char *kbuf = NULL;
    ssize_t out_len;
    ssize_t ret;

    if (!info)
        return -EIO;

    switch (info->kind) {
    case MONFS_INO_SYSTEM_FILE:
        out_len = monfs_render_system(&kbuf);
        break;
    case MONFS_INO_PID_FILE:
        out_len = monfs_render_pid(info->pid, &kbuf);
        break;
    default:
        return -EISDIR;
    }

    if (out_len < 0)
        return out_len;

    ret = simple_read_from_buffer(buf, len, ppos, kbuf, out_len);
    kfree(kbuf);
    return ret;
}

static const struct file_operations monfs_file_ops = {
    .owner = THIS_MODULE,
    .read = monfs_read,
    .llseek = default_llseek,
};

static const struct inode_operations monfs_file_inode_ops = {
    .getattr = simple_getattr,
};

static int monfs_pid_iterate(struct file *file, struct dir_context *ctx)
{
    struct task_struct *task;
    loff_t pos;
    loff_t index = 0;

    if (!dir_emit_dots(file, ctx))
        return 0;

    pos = ctx->pos - 2;
    if (pos < 0)
        pos = 0;

    for_each_process(task) {
        char name[16];
        int len;

        if (index < pos) {
            index++;
            continue;
        }

        len = scnprintf(name, sizeof(name), "%d", task->pid);

        if (!dir_emit(ctx, name, len, task->pid, DT_REG))
            return 0;

        ctx->pos++;
        index++;
    }

    return 0;
}

static const struct file_operations monfs_pid_dir_ops = {
    .owner = THIS_MODULE,
    .iterate_shared = monfs_pid_iterate,
    .llseek = generic_file_llseek,
};

static struct dentry *monfs_negative_lookup(struct dentry *dentry)
{
    d_add(dentry, NULL);
    return NULL;
}

static struct dentry *monfs_pid_lookup(struct inode *dir,
                                       struct dentry *dentry,
                                       unsigned int flags)
{
    struct inode *inode;
    char name[32];
    pid_t pid;
    int ret;

    pr_debug("[monfs] pid_lookup: name=%.*s\n",
             dentry->d_name.len, dentry->d_name.name);

    if (dentry->d_name.len == 0 || dentry->d_name.len >= sizeof(name)) {
        pr_debug("[monfs] pid_lookup: invalid length\n");
        return monfs_negative_lookup(dentry);
    }

    memcpy(name, dentry->d_name.name, dentry->d_name.len);
    name[dentry->d_name.len] = '\0';

    ret = kstrtoint(name, 10, &pid);
    if (ret) {
        pr_debug("[monfs] pid_lookup: parse failed for '%s'\n", name);
        return monfs_negative_lookup(dentry);
    }

    if (pid <= 0) {
        pr_debug("[monfs] pid_lookup: non-positive pid=%d\n", pid);
        return monfs_negative_lookup(dentry);
    }

    if (!procfs_monitor_pid_exists(pid)) {
        pr_debug("[monfs] pid_lookup: pid=%d not found\n", pid);
        return monfs_negative_lookup(dentry);
    }

    inode = monfs_get_inode(dir->i_sb,
                            dir,
                            S_IFREG | 0444,
                            MONFS_INO_PID_FILE,
                            pid);
    if (!inode)
        return ERR_PTR(-ENOMEM);

    inode->i_op = &monfs_file_inode_ops;
    inode->i_fop = &monfs_file_ops;

    d_add(dentry, inode);
    return NULL;
}

static const struct inode_operations monfs_pid_dir_inode_ops = {
    .lookup = monfs_pid_lookup,
};

static struct dentry *monfs_root_lookup(struct inode *dir,
                                        struct dentry *dentry,
                                        unsigned int flags)
{
    struct inode *inode;

    pr_debug("[monfs] root_lookup: name=%.*s\n",
             dentry->d_name.len, dentry->d_name.name);

    if (strcmp(dentry->d_name.name, "system") == 0) {
        inode = monfs_get_inode(dir->i_sb,
                                dir,
                                S_IFREG | 0444,
                                MONFS_INO_SYSTEM_FILE,
                                0);
        if (!inode)
            return ERR_PTR(-ENOMEM);

        inode->i_op = &monfs_file_inode_ops;
        inode->i_fop = &monfs_file_ops;

        d_add(dentry, inode);
        return NULL;
    }

    if (strcmp(dentry->d_name.name, "pid") == 0) {
        inode = monfs_get_inode(dir->i_sb,
                                dir,
                                S_IFDIR | 0555,
                                MONFS_INO_PID_DIR,
                                0);
        if (!inode)
            return ERR_PTR(-ENOMEM);

        inode->i_op = &monfs_pid_dir_inode_ops;
        inode->i_fop = &monfs_pid_dir_ops;
        inc_nlink(inode);

        d_add(dentry, inode);
        return NULL;
    }

    pr_debug("[monfs] root_lookup: negative name=%.*s\n",
             dentry->d_name.len, dentry->d_name.name);

    return monfs_negative_lookup(dentry);
}

static int monfs_root_iterate(struct file *file, struct dir_context *ctx)
{
    pr_debug("[monfs] root_iterate enter: ctx->pos=%lld\n", ctx->pos);

    if (!dir_emit_dots(file, ctx)) {
        pr_debug("[monfs] root_iterate: dir_emit_dots returned false\n");
        return 0;
    }

    if (ctx->pos == 2) {
        if (!dir_emit(ctx, "system", 6, 2, DT_REG))
            return 0;
        ctx->pos++;
    }

    if (ctx->pos == 3) {
        if (!dir_emit(ctx, "pid", 3, 3, DT_DIR))
            return 0;
        ctx->pos++;
    }

    pr_debug("[monfs] root_iterate exit: ctx->pos=%lld\n", ctx->pos);
    return 0;
}

static const struct file_operations monfs_root_dir_ops = {
    .owner = THIS_MODULE,
    .iterate_shared = monfs_root_iterate,
    .llseek = generic_file_llseek,
};

static const struct inode_operations monfs_root_inode_ops = {
    .lookup = monfs_root_lookup,
};


/* ------------------------------------------------------------------ */
/*                          MONFS MANAGEMENT                          */
/* ------------------------------------------------------------------ */


static void monfs_evict_inode(struct inode *inode)
{
    struct monfs_inode_info *info = MONFS_I(inode);

    pr_debug("[monfs] evict_inode: ino=%lu kind=%d pid=%d\n",
             inode->i_ino,
             info ? info->kind : -1,
             info ? info->pid : -1);

    truncate_inode_pages_final(&inode->i_data);
    clear_inode(inode);

    kfree(info);
    inode->i_private = NULL;
}

static const struct super_operations monfs_super_ops = {
    .statfs = simple_statfs,
    .drop_inode = generic_delete_inode,
    .evict_inode = monfs_evict_inode,
};

static int monfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *root_inode;

    sb->s_magic = MONFS_MAGIC;
    sb->s_op = &monfs_super_ops;
    sb->s_time_gran = 1;

    root_inode = monfs_get_inode(sb,
                                 NULL,
                                 S_IFDIR | 0555,
                                 MONFS_INO_ROOT,
                                 0);
    if (!root_inode)
        return -ENOMEM;

    root_inode->i_op = &monfs_root_inode_ops;
    root_inode->i_fop = &monfs_root_dir_ops;
    inc_nlink(root_inode);

    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        iput(root_inode);
        return -ENOMEM;
    }

    return 0;
}

static struct dentry *monfs_mount(struct file_system_type *fs_type,
                                  int flags,
                                  const char *dev_name,
                                  void *data)
{
    return mount_nodev(fs_type, flags, data, monfs_fill_super);
}

static void monfs_kill_sb(struct super_block *sb)
{
    pr_info("[monfs] kill_sb enter\n");
    kill_anon_super(sb);
    pr_info("[monfs] kill_sb exit\n");
}

static struct file_system_type monfs_type = {
    .owner = THIS_MODULE,
    .name = MONFS_NAME,
    .mount = monfs_mount,
    .kill_sb = monfs_kill_sb,
};

int monfs_monitor_monfs_init(void)
{
    int ret;

    ret = register_filesystem(&monfs_type);
    if (ret) {
        pr_err("[monfs] register_filesystem failed: %d\n", ret);
        return ret;
    }

    pr_info("[monfs] registered filesystem '%s'\n", MONFS_NAME);
    return 0;
}

void monfs_monitor_monfs_exit(void)
{
    int ret;

    ret = unregister_filesystem(&monfs_type);
    if (ret)
        pr_err("[monfs] unregister_filesystem failed: %d\n", ret);
    else
        pr_info("[monfs] unregistered filesystem '%s'\n", MONFS_NAME);
}
