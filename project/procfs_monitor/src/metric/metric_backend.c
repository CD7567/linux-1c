#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/sched/signal.h>
#include <linux/sched/mm.h>
#include <linux/pid.h>
#include <linux/sysinfo.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/string.h>

#include "metric_backend/metric_backend.h"

static u64 prev_total_cpu_time = 0;
static u64 prev_idle_cpu_time = 0;
static DEFINE_MUTEX(procfs_monitor_backend_cpu_lock);


/* ------------------------------------------------------------------ */
/*                      SYSTEM MONITOR BACKEND                        */
/* ------------------------------------------------------------------ */


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


int procfs_monitor_collect_system_snapshot(
    struct procfs_monitor_system_snapshot *out)
{
    struct sysinfo info;
    struct task_struct *task;
    u64 total_cpu_time;
    u64 idle_cpu_time;
    u64 delta_total;
    u64 delta_idle;
    u64 busy_delta;

    if (!out)
        return -EINVAL;

    memset(out, 0, sizeof(*out));

    out->uptime_sec = jiffies / HZ;

    si_meminfo(&info);
    out->mem_total_kb = ((u64)info.totalram * info.mem_unit) / 1024;
    out->mem_available_kb = ((u64)si_mem_available() * PAGE_SIZE) / 1024;
    out->mem_used_kb = out->mem_total_kb - out->mem_available_kb;

    for_each_process(task) {
        out->tasks_total++;

        if (task_is_running(task))
            out->tasks_runnable++;
        else if (task->exit_state & EXIT_ZOMBIE)
            out->tasks_zombie++;
        else if (task->__state & TASK_INTERRUPTIBLE)
            out->tasks_interruptible_sleep++;
        else if (task->__state & TASK_UNINTERRUPTIBLE)
            out->tasks_uninterruptible_sleep++;
        else if (task->__state & __TASK_STOPPED)
            out->tasks_stopped++;
        else
            out->tasks_other++;
    }

    procfs_monitor_get_cpu_times(&total_cpu_time, &idle_cpu_time);

    mutex_lock(&procfs_monitor_backend_cpu_lock);

    if (prev_total_cpu_time != 0 &&
        total_cpu_time >= prev_total_cpu_time &&
        idle_cpu_time >= prev_idle_cpu_time) {
        delta_total = total_cpu_time - prev_total_cpu_time;
        delta_idle = idle_cpu_time - prev_idle_cpu_time;
        busy_delta = delta_total - delta_idle;

        if (delta_total > 0)
            out->cpu_usage_x10 =
                (unsigned int)div64_u64(busy_delta * 1000, delta_total);
    } else {
        out->cpu_usage_x10 = 0;
    }

    prev_total_cpu_time = total_cpu_time;
    prev_idle_cpu_time = idle_cpu_time;

    mutex_unlock(&procfs_monitor_backend_cpu_lock);

    return 0;
}


/* ------------------------------------------------------------------ */
/*                      PROCESS MONITOR BACKEND                       */
/* ------------------------------------------------------------------ */


static struct task_struct *procfs_monitor_get_task_by_pid(pid_t pid)
{
    struct pid *kpid;
    struct task_struct *task;

    kpid = find_get_pid(pid);
    if (!kpid)
        return NULL;

    task = get_pid_task(kpid, PIDTYPE_PID);

    put_pid(kpid);
    return task;
}

static const char *procfs_monitor_task_state_to_str(struct task_struct *task)
{
    if (task_is_running(task))
        return "running";

    if (task->__state & TASK_INTERRUPTIBLE)
        return "interruptible-sleep";

    if (task->__state & TASK_UNINTERRUPTIBLE)
        return "uninterruptible-sleep";

    if (task->__state & __TASK_STOPPED)
        return "stopped";

    if (task->__state & __TASK_TRACED)
        return "traced";

    if (task->exit_state & EXIT_ZOMBIE)
        return "zombie";

    if (task->exit_state & EXIT_DEAD)
        return "dead";

#ifdef TASK_PARKED
    if (task->__state & TASK_PARKED)
        return "parked";
#endif

#ifdef TASK_IDLE
    if (task->__state & TASK_IDLE)
        return "idle";
#endif

    return "unknown";
}

static int procfs_monitor_get_task_memory(struct task_struct *task,
                                          u64 *vm_size_kb,
                                          u64 *rss_kb)
{
    struct mm_struct *mm;
    u64 vm_pages;
    u64 rss_pages;

    mm = get_task_mm(task);
    if (!mm) {
        *vm_size_kb = 0;
        *rss_kb = 0;
        return 0;
    }

    vm_pages = (u64)mm->total_vm;
    rss_pages = (u64)get_mm_rss(mm);

    mmput(mm);

    *vm_size_kb = (vm_pages * PAGE_SIZE) / 1024;
    *rss_kb = (rss_pages * PAGE_SIZE) / 1024;

    return 0;
}

static void procfs_monitor_get_task_cputime(struct task_struct *task,
                                            u64 *user_ticks,
                                            u64 *system_ticks,
                                            u64 *total_ticks)
{
    u64 utime;
    u64 stime;

    utime = (u64)task->utime;
    stime = (u64)task->stime;

    *user_ticks = utime;
    *system_ticks = stime;
    *total_ticks = utime + stime;
}


bool procfs_monitor_pid_exists(pid_t pid)
{
    struct pid *kpid;

    kpid = find_get_pid(pid);
    if (!kpid)
        return false;

    put_pid(kpid);
    return true;
}

int procfs_monitor_collect_process_snapshot(
    pid_t pid,
    struct procfs_monitor_process_snapshot *out)
{
    struct task_struct *task;

    if (!out)
        return -EINVAL;

    memset(out, 0, sizeof(*out));

    task = procfs_monitor_get_task_by_pid(pid);
    if (!task)
        return -ESRCH;

    out->pid = pid;
    out->ppid = task_ppid_nr(task);
    strscpy(out->comm, task->comm, sizeof(out->comm));
    out->state = procfs_monitor_task_state_to_str(task);
    out->threads = get_nr_threads(task);

    procfs_monitor_get_task_memory(task, &out->vm_size_kb, &out->rss_kb);

    procfs_monitor_get_task_cputime(task,
                                    &out->cpu_time_user_ticks,
                                    &out->cpu_time_system_ticks,
                                    &out->cpu_time_total_ticks);

    put_task_struct(task);
    return 0;
}
