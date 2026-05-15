# Task4: kbd_monitor

Модуль протестирован в виртуальной машине на базе Debian:
```shell
wget https://cloud.debian.org/images/cloud/trixie/latest/debian-13-generic-amd64.qcow2
```

Версия ядра `6.12.85+deb13-amd64`.

[Демо работы](DEMO.md)

## Инструкция по запуску

### Сборка

Сборка:
```shell
make
```
```

### Установка в систему

Установка модуля:
```shell
sudo insmod kbd_monitor.ko
```

Удаление модуля:
```shell
sudo rmmod kbd_monitor
```

### Чтение procfs

```
cat /proc/kbd_monitor
```
