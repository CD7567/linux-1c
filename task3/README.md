# Task3: minifs

Модуль протестирован в виртуальной машине на базе Debian:
```shell
wget https://cloud.debian.org/images/cloud/trixie/latest/debian-13-generic-amd64.qcow2
```

Версия ядра `6.12.85+deb13-amd64`.

[Демо работы](DEMO.md)

## Инструкция по запуску

### Сборка

Сборка в релизном режиме:
```shell
make module
```

Сборка в отладочном режиме с дополнительным логированием:
```shell
make module DEBUG=1
```

### Установка в систему

Установка модуля:
```shell
sudo insmod sudo insmod minifs.ko
```

Удаление модуля:
```shell
sudo rmmod minifs
```

### Монтирование и размонтирование ФС

Монтирование ФС:
```shell
sudo mount -t minifs none /mnt/minifs
```

Размонтирование ФС:
```shell
sudo umount /mnt/minifs
```
