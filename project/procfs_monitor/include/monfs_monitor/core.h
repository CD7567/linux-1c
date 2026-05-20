#ifndef MONFS_MONITOR_CORE_H
#define MONFS_MONITOR_CORE_H

/*
 * This module entrypoint
 */
int monfs_monitor_module_init(void);

/*
 * This module destructor
 */
void monfs_monitor_module_exit(void);

#endif /* MONFS_MONITOR_CORE_H */
