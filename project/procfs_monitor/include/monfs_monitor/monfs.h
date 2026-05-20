#ifndef MONFS_MONITOR_MONFS_H
#define MONFS_MONITOR_MONFS_H

#define MONFS_NAME   "monfs"
#define MONFS_MAGIC  0x20260520

/*
 * Initialize monitor monfs
 */
int monfs_monitor_monfs_init(void);

/*
 * Destroy monitor monfs
 */
void monfs_monitor_monfs_exit(void);

#endif /* MONFS_MONITOR_MONFS_H */
