#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/sysinfo.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/cpumask.h>
#include <linux/math64.h>

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

static u64 prev_total_cpu_time = 0;
static u64 prev_idle_cpu_time = 0;
static DEFINE_MUTEX(procfs_monitor_system_lock);

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

static void procfs_monitor_get_cpu_times(u64 *total, u64 *idle)
{
    int cpu;
    u64 total_time = 0;
    u64 idle_time = 0;

    for_each_possible_cpu(cpu) {
        struct kernel_cpustat kcpustat;
        u64 user, nice, system, idle_v, iowait, irq, softirq, steal;

        kcpustat = kcpustat_cpu(cpu);

        user    = kcpustat.cpustat[CPUTIME_USER];
        nice    = kcpustat.cpustat[CPUTIME_NICE];
        system  = kcpustat.cpustat[CPUTIME_SYSTEM];
        idle_v  = kcpustat.cpustat[CPUTIME_IDLE];
        iowait  = kcpustat.cpustat[CPUTIME_IOWAIT];
        irq     = kcpustat.cpustat[CPUTIME_IRQ];
        softirq = kcpustat.cpustat[CPUTIME_SOFTIRQ];
        steal   = kcpustat.cpustat[CPUTIME_STEAL];

        total_time += user + nice + system + idle_v + iowait + irq + softirq + steal;
        idle_time  += idle_v + iowait;
    }

    *total = total_time;
    *idle = idle_time;
}

static int procfs_monitor_system_show(struct seq_file *m, void *v)
{
    struct sysinfo info;
    struct task_struct *task;
    u64 mem_total_kb;
    u64 mem_available_kb;
    u64 mem_used_kb;
    u64 total_cpu_time;
    u64 idle_cpu_time;
    u64 delta_total;
    u64 delta_idle;
    u64 busy_delta;
    unsigned int cpu_usage_x10 = 0;
    unsigned int cpu_usage_int;
    unsigned int cpu_usage_frac;
    unsigned long uptime_sec;
    unsigned long tasks_total = 0;

    uptime_sec = jiffies / HZ;

    si_meminfo(&info);
    mem_total_kb = ((u64)info.totalram * info.mem_unit) / 1024;
    mem_available_kb = ((u64)si_mem_available() * PAGE_SIZE) / 1024;
    mem_used_kb = mem_total_kb - mem_available_kb;


    for_each_process(task)
        tasks_total++;

    procfs_monitor_get_cpu_times(&total_cpu_time, &idle_cpu_time);

    mutex_lock(&procfs_monitor_system_lock);

    if (prev_total_cpu_time != 0 &&
        total_cpu_time >= prev_total_cpu_time &&
        idle_cpu_time >= prev_idle_cpu_time) {

        delta_total = total_cpu_time - prev_total_cpu_time;
        delta_idle  = idle_cpu_time - prev_idle_cpu_time;
        busy_delta  = delta_total - delta_idle;

        if (delta_total > 0)
            cpu_usage_x10 = (unsigned int)div64_u64(busy_delta * 1000, delta_total);
    } else {
        cpu_usage_x10 = 0;
    }

    prev_total_cpu_time = total_cpu_time;
    prev_idle_cpu_time  = idle_cpu_time;

    mutex_unlock(&procfs_monitor_system_lock);

    cpu_usage_int  = cpu_usage_x10 / 10;
    cpu_usage_frac = cpu_usage_x10 % 10;

    /*
     * Uptime in seconds since kernel startup
     */
    seq_printf(m, "uptime_sec: %lu\n", uptime_sec);

    /*
     * Total RAM in Kb
     */
    seq_printf(m, "mem_total_kb: %llu\n", mem_total_kb);

    /*
     * RAM available for allocation without swapping
     */
    seq_printf(m, "mem_available_kb: %llu\n", mem_available_kb);

    /*
     * RAM used in Kb
     */
    seq_printf(m, "mem_used_kb: %llu\n", mem_used_kb);

    /*
     * CPU usage (delta is since previous file open)
     */
    seq_printf(m, "cpu_usage_percent: %u.%u\n", cpu_usage_int, cpu_usage_frac);

    /*
     * Total processes (not threads!)
     */
    seq_printf(m, "tasks_total: %lu\n", tasks_total);

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
