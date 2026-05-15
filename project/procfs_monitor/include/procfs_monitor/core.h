#ifndef PROCFS_MONITOR_CORE_H
#define PROCFS_MONITOR_CORE_H

/*
 * This module entrypoint
 */
int procfs_monitor_module_init(void);

/*
 * This module destructor
 */
void procfs_monitor_module_exit(void);

#endif /* PROCFS_MONITOR_CORE_H */
