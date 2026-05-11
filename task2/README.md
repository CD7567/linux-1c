# Task2: vapa

Модуль протестирован в виртуальной машине на базе Debian:
```shell
wget https://cloud.debian.org/images/cloud/trixie/latest/debian-13-generic-amd64.qcow2
```

[Демо работы](DEMO.md)

## Инструкция по запуску

### Модуль ядра

#### Сборка ядра с патчем
Первым делом нужно скачать исходники ядра, наиболее близкое к исходному ядру выбранной
виртуалки это 6.12:

```shell
wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.12.tar.xz && \
tar -xf linux-6.12.tar.xz && \
cd linux-6.12
```

Теперь устанавливаем исходник системного вызова в ядро:
```shell
cp <path-to-dev-dir>/kernel/vapa_syscall.c kernel
```

Включаем его в сборку добавлением в `Makefile` этого таргета `obj-y += vapa_syscall.o`,
например, через `neovim`:
```shell
nvim kernel/Makefile
```

Патчим таблицу системных вызовов, в моем случае оказался доступен номер 549, под ним и поставим:
```shell
echo "549    64       vapa                    sys_vapa" >> arch/x86/entry/syscalls/syscall_64.tbl
```

Наконец, устанавливаем постфикс нашей версии ядра:
```shell
scripts/config --set-str LOCALVERSION "-vapa"
```

И собираем ядро:
```shell
make -j$(nproc)
```

#### Установка пропатченного ядра
Сначала устанавливаем нашу сборку в систему:
```shell
sudo make modules_install && \
sudo make install
```

При этом `make` пересобирает initramfs и конфиг GRUB, но виртуалка без графической оболочки, поэтому
меню `GRUB` мы не увидим, и надо вручную сделать автовыбор нашего ядра. Для этого пропатчим через neovim конфиг
изменением `GRUB_DEFAULT="Advanced options for Debian GNU/Linux>Debian GNU/Linux, with Linux 6.12.0-vapa"`:
```shell
sudo nvim /etc/default/grub
```

Теперь пересобираем конфиг GRUB:
```shell
sudo update-grub
```

И теперь перезапускаемся:
```shell
sudo reboot
```

Получаем перезапуск в наше ядро:
```
$ uname -r
6.12.0-vapa
```

### Пользовательская тестовая утилита
Собираем ее просто обычным вызовом компилятора:
```shell
gcc -O2 -Wall -o vapa_test vapa_test.c
```
