#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>

#include "procfs_monitor/procfs.h"

struct procfs_monitor_pid_entry {
    pid_t pid;
    char name[16];
    struct proc_dir_entry *pde;
    struct list_head list;
};

static struct proc_dir_entry *procfs_monitor_root;
static struct proc_dir_entry *procfs_monitor_system;
static struct proc_dir_entry *procfs_monitor_proc_dir;
static struct proc_dir_entry *procfs_monitor_register;

static LIST_HEAD(procfs_monitor_pid_entries);
static DEFINE_MUTEX(procfs_monitor_pid_lock);

static bool procfs_monitor_pid_exists(pid_t pid)
{
    struct pid *kpid;

    kpid = find_get_pid(pid);
    if (!kpid)
        return false;

    put_pid(kpid);
    return true;
}

static struct procfs_monitor_pid_entry *procfs_monitor_find_pid_entry(pid_t pid)
{
    struct procfs_monitor_pid_entry *entry;

    list_for_each_entry(entry, &procfs_monitor_pid_entries, list) {
        if (entry->pid == pid)
            return entry;
    }

    return NULL;
}

static int procfs_monitor_system_show(struct seq_file *m, void *v)
{
    seq_puts(m, "system\n");
    return 0;
}

static int procfs_monitor_system_open(struct inode *inode, struct file *file)
{
    return single_open(file, procfs_monitor_system_show, NULL);
}

static const struct proc_ops procfs_monitor_system_ops = {
    .proc_open    = procfs_monitor_system_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static int procfs_monitor_pid_show(struct seq_file *m, void *v)
{
    struct procfs_monitor_pid_entry *entry = m->private;

    if (!entry)
        return -EINVAL;

    if (!procfs_monitor_pid_exists(entry->pid)) {
        seq_puts(m, "process-not-found\n");
        return 0;
    }

    seq_printf(m, "%d\n", entry->pid);
    return 0;
}

static int procfs_monitor_pid_open(struct inode *inode, struct file *file)
{
    void *data = pde_data(inode);

    return single_open(file, procfs_monitor_pid_show, data);
}

static const struct proc_ops procfs_monitor_pid_ops = {
    .proc_open    = procfs_monitor_pid_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static int procfs_monitor_register_pid_entry(pid_t pid)
{
    struct procfs_monitor_pid_entry *entry;

    if (pid <= 0)
        return -EINVAL;

    if (!procfs_monitor_pid_exists(pid))
        return -ESRCH;

    mutex_lock(&procfs_monitor_pid_lock);

    if (procfs_monitor_find_pid_entry(pid)) {
        mutex_unlock(&procfs_monitor_pid_lock);
        return -EEXIST;
    }

    entry = kzalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry) {
        mutex_unlock(&procfs_monitor_pid_lock);
        return -ENOMEM;
    }

    entry->pid = pid;
    snprintf(entry->name, sizeof(entry->name), "%d", pid);

    entry->pde = proc_create_data(entry->name,
                                  0444,
                                  procfs_monitor_proc_dir,
                                  &procfs_monitor_pid_ops,
                                  entry);
    if (!entry->pde) {
        kfree(entry);
        mutex_unlock(&procfs_monitor_pid_lock);
        return -ENOMEM;
    }

    list_add_tail(&entry->list, &procfs_monitor_pid_entries);
    mutex_unlock(&procfs_monitor_pid_lock);

    pr_info("[procfs_monitor] registered pid entry %d\n", pid);
    return 0;
}

static void procfs_monitor_unregister_all_pid_entries(void)
{
    struct procfs_monitor_pid_entry *entry, *tmp;

    mutex_lock(&procfs_monitor_pid_lock);

    list_for_each_entry_safe(entry, tmp, &procfs_monitor_pid_entries, list) {
        list_del(&entry->list);

        if (entry->pde)
            remove_proc_entry(entry->name, procfs_monitor_proc_dir);

        kfree(entry);
    }

    mutex_unlock(&procfs_monitor_pid_lock);
}

static ssize_t procfs_monitor_register_write(struct file *file,
                                             const char __user *buffer,
                                             size_t count,
                                             loff_t *ppos)
{
    char kbuf[PROCFS_MONITOR_INPUT_MAX];
    size_t len;
    int pid;
    int ret;

    if (count == 0)
        return 0;

    len = min(count, (size_t)(PROCFS_MONITOR_INPUT_MAX - 1));

    if (copy_from_user(kbuf, buffer, len))
        return -EFAULT;

    kbuf[len] = '\0';
    strim(kbuf);

    ret = kstrtoint(kbuf, 10, &pid);
    if (ret)
        return ret;

    ret = procfs_monitor_register_pid_entry((pid_t)pid);
    if (ret)
        return ret;

    *ppos += count;
    return count;
}

static const struct proc_ops procfs_monitor_register_ops = {
    .proc_write = procfs_monitor_register_write,
};


int procfs_monitor_procfs_init(void)
{
    procfs_monitor_root = proc_mkdir(PROCFS_MONITOR_DIRNAME, NULL);
    if (!procfs_monitor_root) {
        pr_err("[procfs_monitor] failed to create /proc/%s\n",
               PROCFS_MONITOR_DIRNAME);
        return -ENOMEM;
    }

    procfs_monitor_system = proc_create(PROCFS_MONITOR_SYSTEM_NAME,
                                        0444,
                                        procfs_monitor_root,
                                        &procfs_monitor_system_ops);
    if (!procfs_monitor_system) {
        pr_err("[procfs_monitor] failed to create /proc/%s/%s\n",
               PROCFS_MONITOR_DIRNAME, PROCFS_MONITOR_SYSTEM_NAME);
        goto err_remove_root;
    }

    pr_debug("[procfs_monitor] created /proc/%s/%s\n",
             PROCFS_MONITOR_DIRNAME, PROCFS_MONITOR_SYSTEM_NAME);

    procfs_monitor_proc_dir = proc_mkdir(PROCFS_MONITOR_PROC_DIRNAME,
                                         procfs_monitor_root);
    if (!procfs_monitor_proc_dir) {
        pr_err("[procfs_monitor] failed to create /proc/%s/%s\n",
               PROCFS_MONITOR_DIRNAME, PROCFS_MONITOR_PROC_DIRNAME);
        goto err_remove_system;
    }

    pr_debug("[procfs_monitor] created /proc/%s/%s/\n",
             PROCFS_MONITOR_DIRNAME, PROCFS_MONITOR_PROC_DIRNAME);


    procfs_monitor_register = proc_create(PROCFS_MONITOR_REGISTER_NAME,
                                          0222,
                                          procfs_monitor_root,
                                          &procfs_monitor_register_ops);
    if (!procfs_monitor_register) {
        pr_err("[procfs_monitor] failed to create /proc/%s/%s\n",
               PROCFS_MONITOR_DIRNAME, PROCFS_MONITOR_REGISTER_NAME);
        goto err_remove_proc_dir;
    }

    pr_debug("[procfs_monitor] created /proc/%s/%s\n",
             PROCFS_MONITOR_DIRNAME, PROCFS_MONITOR_SYSTEM_NAME);
    

    return 0;

err_remove_proc_dir:
    remove_proc_entry(PROCFS_MONITOR_PROC_DIRNAME, procfs_monitor_root);
    procfs_monitor_proc_dir = NULL;

err_remove_system:
    remove_proc_entry(PROCFS_MONITOR_SYSTEM_NAME, procfs_monitor_root);
    procfs_monitor_system = NULL;

err_remove_root:
    remove_proc_entry(PROCFS_MONITOR_DIRNAME, NULL);
    procfs_monitor_root = NULL;

    return -ENOMEM;
}

void procfs_monitor_procfs_exit(void)
{
    procfs_monitor_unregister_all_pid_entries();

    if (procfs_monitor_register) {
        remove_proc_entry(PROCFS_MONITOR_REGISTER_NAME, procfs_monitor_root);
        procfs_monitor_register = NULL;
    }

    if (procfs_monitor_proc_dir) {
        remove_proc_entry(PROCFS_MONITOR_PROC_DIRNAME, procfs_monitor_root);
        procfs_monitor_proc_dir = NULL;
    }

    if (procfs_monitor_system) {
        remove_proc_entry(PROCFS_MONITOR_SYSTEM_NAME, procfs_monitor_root);
        procfs_monitor_system = NULL;
    }

    if (procfs_monitor_root) {
        remove_proc_entry(PROCFS_MONITOR_DIRNAME, NULL);
        procfs_monitor_root = NULL;
    }

    pr_debug("[procfs_monitor] removed /proc/%s subtree\n",
             PROCFS_MONITOR_DIRNAME);
}
