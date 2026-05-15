# Демо работы

Вводим в виртуалке с ps/2 клавиатурой символы:
```
-<dev@debian-dev:/mnt/shared-dev>-                                                                   -<pts/0>-
-<%>- abcdef
```

## Логирование нажатий

Читаем логи ядра:
```shell
sudo dmesg | tail -n 12
```

Получаем следующий вывод:
```
[ 6757.170571] kbd_monitor: ts=6757253863361 raw=0x1e -> code=0x1e type=press key=A
[ 6757.265867] kbd_monitor: ts=6757349164184 raw=0x9e -> code=0x1e type=release key=A
[ 6757.406118] kbd_monitor: ts=6757489418465 raw=0x30 -> code=0x30 type=press key=B
[ 6757.476258] kbd_monitor: ts=6757559559906 raw=0xb0 -> code=0x30 type=release key=B
[ 6757.692087] kbd_monitor: ts=6757775369936 raw=0x2e -> code=0x2e type=press key=C
[ 6757.776858] kbd_monitor: ts=6757860160144 raw=0xae -> code=0x2e type=release key=C
[ 6758.007747] kbd_monitor: ts=6758091057565 raw=0x20 -> code=0x20 type=press key=D
[ 6758.112766] kbd_monitor: ts=6758196073042 raw=0xa0 -> code=0x20 type=release key=D
[ 6758.278435] kbd_monitor: ts=6758361737856 raw=0x12 -> code=0x12 type=press key=E
[ 6758.378419] kbd_monitor: ts=6758461727268 raw=0x92 -> code=0x12 type=release key=E
[ 6758.504610] kbd_monitor: ts=6758587928869 raw=0x21 -> code=0x21 type=press key=F
[ 6758.579771] kbd_monitor: ts=6758663091055 raw=0xa1 -> code=0x21 type=release key=F
```

То есть, временная метка получения сигнала модулем и логика парсинга кодов.

## Чтение procfs

Читаем procfs:
```shell
cat /proc/kbd_monitor
```

Получаем следующий вывод:
```
size=128 unprocessed=0
ts=6757253863361 raw=0x1e code=0x1e -> type=press key=A
ts=6757349164184 raw=0x9e code=0x1e -> type=release key=A
ts=6757489418465 raw=0x30 code=0x30 -> type=press key=B
ts=6757559559906 raw=0xb0 code=0x30 -> type=release key=B
ts=6757775369936 raw=0x2e code=0x2e -> type=press key=C
ts=6757860160144 raw=0xae code=0x2e -> type=release key=C
ts=6758091057565 raw=0x20 code=0x20 -> type=press key=D
ts=6758196073042 raw=0xa0 code=0x20 -> type=release key=D
ts=6758361737856 raw=0x12 code=0x12 -> type=press key=E
ts=6758461727268 raw=0x92 code=0x12 -> type=release key=E
ts=6758587928869 raw=0x21 code=0x21 -> type=press key=F
ts=6758663091055 raw=0xa1 code=0x21 -> type=release key=F
```

Вывод статистики по состоянию кольцевого буфера (размер и дельта по чтению), 
также в хронологическом порядке записанные на данный момент события из буфера.
