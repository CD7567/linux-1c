#ifndef PROCFS_METRIC_BACKEND_H
#define PROCFS_METRIC_BACKEND_H

#include <linux/types.h>
#include <linux/pid.h>

#define PROCFS_MONITOR_COMM_LEN 16


/* ------------------------------------------------------------------ */
/*                      SYSTEM MONITOR BACKEND                        */
/* ------------------------------------------------------------------ */


struct procfs_monitor_system_snapshot {
    unsigned long uptime_sec;

    u64 mem_total_kb;
    u64 mem_available_kb;
    u64 mem_used_kb;

    unsigned int cpu_usage_x10;

    unsigned long tasks_total;
    unsigned long tasks_runnable;
    unsigned long tasks_interruptible_sleep;
    unsigned long tasks_uninterruptible_sleep;
    unsigned long tasks_stopped;
    unsigned long tasks_zombie;
    unsigned long tasks_other;
};


int procfs_monitor_collect_system_snapshot(struct procfs_monitor_system_snapshot *out);


/* ------------------------------------------------------------------ */
/*                      PROCESS MONITOR BACKEND                       */
/* ------------------------------------------------------------------ */


struct procfs_monitor_process_snapshot {
    pid_t pid;
    pid_t ppid;
    char comm[PROCFS_MONITOR_COMM_LEN];
    const char *state;
    int threads;

    u64 vm_size_kb;
    u64 rss_kb;

    u64 cpu_time_user_ticks;
    u64 cpu_time_system_ticks;
    u64 cpu_time_total_ticks;
};


bool procfs_monitor_pid_exists(pid_t pid);

int procfs_monitor_collect_process_snapshot(
    pid_t pid,
    struct procfs_monitor_process_snapshot *out);


#endif /* PROCFS_METRIC_BACKEND_H */
