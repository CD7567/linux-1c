#ifndef PROCFS_MONITOR_PROCFS_H
#define PROCFS_MONITOR_PROCFS_H

#define PROCFS_MONITOR_DIRNAME        "procfs_monitor"
#define PROCFS_MONITOR_SYSTEM_NAME    "system"
#define PROCFS_MONITOR_PROC_DIRNAME   "pid"
#define PROCFS_MONITOR_REGISTER_NAME  "register"

#define PROCFS_MONITOR_INPUT_MAX      64

/*
 * Initialize monitor procfs
 */
int procfs_monitor_procfs_init(void);

/*
 * Destroy monitor procfs
 */
void procfs_monitor_procfs_exit(void);

#endif /* PROCFS_MONITOR_PROCFS_H */
