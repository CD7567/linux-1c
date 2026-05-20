# Демо работы

## Монитор на базе procfs

### Просмотр системных метрик

```
-<dev@debian-dev:/mnt/shared-dev>-                                                                                                                                                                                  -<pts/0>-
-<1:%>- cat /proc/procfs_monitor/system 
uptime_sec: 17180397
mem_total_kb: 1952216
mem_available_kb: 1659436
mem_used_kb: 292780
cpu_usage_percent: 0.5
tasks_total: 136
tasks_runnable: 1
tasks_interruptible_sleep: 65
tasks_uninterruptible_sleep: 70
tasks_stopped: 0
tasks_zombie: 0
tasks_other: 0
```

### Регистрация pid узла on-demand

```
-<dev@debian-dev:/mnt/shared-dev>-                                                                                                                                                                                  -<pts/0>-
-<130:%>- ll /proc/procfs_monitor/pid 
total 0
-<dev@debian-dev:/mnt/shared-dev>-                                                                                                                                                                                  -<pts/0>-
-<%>- echo "1" > /proc/procfs_monitor/register
-<dev@debian-dev:/mnt/shared-dev>-                                                                                                                                                                                  -<pts/0>-
-<%>- ll /proc/procfs_monitor/pid             
total 0
-r--r--r-- 1 root root 0 May 20 07:25 1
```

### Просмотр процессных метрик

```
-<dev@debian-dev:/mnt/shared-dev>-                                                                                                                                                                                  -<pts/0>-
-<%>- cat /proc/procfs_monitor/pid/1
pid: 1
ppid: 0
comm: systemd
state: interruptible-sleep
threads: 1
vm_size_kb: 23272
rss_kb: 13964
cpu_time_user_ticks: 183625196
cpu_time_system_ticks: 1227341401
cpu_time_total_ticks: 1410966597
```

## Монитор на базе monfs

### Структура ФС

Корневой каталог, как и описано в задании, содержит файл для чтения системных метрик и каталог,
в котором динамически отдается контент файлов с попроцессными метриками.

```
-<dev@debian-dev:/mnt/shared-dev>-                                                                                                                                                                                  -<pts/0>-
-<%>- ll /mnt/monfs                      
total 0
dr-xr-xr-x 2 dev dev 0 May 20 07:27 pid
-r--r--r-- 1 dev dev 0 May 20 07:27 system
```

Содержимое каталога с попроцессными метриками создается на лету и нигде непосредственно не хранится.

```
-<dev@debian-dev:/mnt/shared-dev>-                                                                                                                                                                                  -<pts/0>-
-<%>- ll /mnt/monfs/pid
total 0
-r--r--r-- 1 dev dev 0 May 20 07:28 1
-r--r--r-- 1 dev dev 0 May 20 07:28 10
-r--r--r-- 1 dev dev 0 May 20 07:28 1007
-r--r--r-- 1 dev dev 0 May 20 07:28 1008
-r--r--r-- 1 dev dev 0 May 20 07:28 1132
-r--r--r-- 1 dev dev 0 May 20 07:28 1155
-r--r--r-- 1 dev dev 0 May 20 07:28 12
-r--r--r-- 1 dev dev 0 May 20 07:28 13
-r--r--r-- 1 dev dev 0 May 20 07:28 1373
-r--r--r-- 1 dev dev 0 May 20 07:28 1382
-r--r--r-- 1 dev dev 0 May 20 07:28 1383
...
```

### Чтение метрик

Вывод системных метрик аналогичен procfs варианту:
```
-<dev@debian-dev:/mnt/shared-dev>-                                                                                                                                                                                  -<pts/0>-
-<%>- cat /mnt/monfs/system
uptime_sec: 17180893
mem_total_kb: 1952216
mem_available_kb: 1673192
mem_used_kb: 279024
cpu_usage_percent: 0.1
tasks_total: 136
tasks_runnable: 1
tasks_interruptible_sleep: 64
tasks_uninterruptible_sleep: 71
tasks_stopped: 0
tasks_zombie: 0
tasks_other: 0
```

Так же аналогичны и попроцессные метрики:
```
-<dev@debian-dev:/mnt/shared-dev>-                                                                                                                                                                                  -<pts/0>-
-<%>- cat /mnt/monfs/pid/1 
pid: 1
ppid: 0
comm: systemd
state: interruptible-sleep
threads: 1
vm_size_kb: 23272
rss_kb: 13964
cpu_time_user_ticks: 183625196
cpu_time_system_ticks: 1231341401
cpu_time_total_ticks: 1414966597
```
