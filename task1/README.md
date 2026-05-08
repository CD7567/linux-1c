# Task1: telegramfs

Модуль протестирован в виртуальной машине на базе Debian:
```shell
wget https://cloud.debian.org/images/cloud/trixie/latest/debian-13-generic-amd64.qcow2
```

Версия ядра `6.12.85+deb13-amd64`.

[Демо работы](DEMO.md)

## Инструкция по запуску

### Модуль ядра

#### Сборка

Сборка в релизном режиме:
```shell
make module
```

Сборка в отладочном режиме с дополнительным логированием:
```shell
make module DEBUG=1
```

#### Установка в систему

Установка модуля:
```shell
sudo insmod sudo insmod tgfs.ko
```

Удаление модуля:
```shell
sudo rmmod tgfs
```

### Пользовательская консольная утилита

#### Сборка

```shell
make tgctl
```

#### Использование

```
Usage:
  tgctl create [name]              -- Clear chat device
  tgctl list                       -- List available devices
  tgctl count <device>             -- Count chat messages
  tgctl clear <device>             -- Clear chat messages
  tgctl get-limit <device>         -- Get chat read limit
  tgctl set-limit <device> <value> -- Set chat read limit
```

Реализована система динамического добавления чатов:
- `tgctl create [name]` - создание чата, опционально с именем; в случае создания без имени имя автогенерируется по шаблону `chat{minor}`
- `tgctl list` - вывод текущего списка чатов

В качестве работы с чатом через `ioctl` реализованы:
- `tgctl count <device>` - вывести количество сообщений в чате
- `tgctl clear <device>` - очистить буфер сообщений
- `tgctl get-limit <device>` - считать лимит выводимых `read()` сообщений
- `tgctl set-limit <device> <value>` - установить лимит выводимых `read()` сообщений



## Архитектура

### Основные требования задания

#### 1. Файловый интерфейс к чатам
Реализовано через character device `/dev/telegram/<chat_name>`.  
Каждый чат — отдельный device node, управляемый через `struct tgfs_chat` с индивидуальным `minor` и внутренним состоянием.

#### 2. Операции `open()`, `read()`, `write()`, `release()`
- `open()` - поиск чата по `minor` через registry, сохранение указателя в `file->private_data`
- `read()` - snapshot-based формирование текстового буфера истории, затем `copy_to_user()` с учетом `ppos`
- `write()` - нормализация входа, `copy_from_user()`, добавление в кольцевой буфер через `tgfs_chat_push()`
- `release()` - минимальная реализация без явной очистки состояния

#### 3. Чтение последних сообщений (максимум 10)
Реализовано через `read_limit = 10` по умолчанию.
`tgfs_chat_snapshot()` вычисляет `min(msg_count, read_limit)` и форматирует только последние `N` сообщений из кольцевого буфера.

#### 4. Отправка сообщений через `write()`
Запись в chat device добавляет сообщение в историю.
`tgfs_chat_push()` записывает сообщение в `msgs[next_idx]`, обновляет `next_idx` и `msg_count` под `mutex`.

### 5. Независимость чатов
Каждый `struct tgfs_chat` имеет собственный буфер `msgs[]` и `mutex`.  
Операции над одним чатом не влияют на другие, а также защищены от параллельной записи.

#### 6. Обработка ошибок
**Реализовано:**
- `-ENODEV` — чат не найден
- `-EFBIG` — слишком длинное сообщение
- `-EFAULT` — ошибка `copy_from_user`/`copy_to_user`
- `-EINVAL` — некорректные параметры
- `-EEXIST` — имя чата уже занято
- `-ENOSPC` — достигнут лимит чатов

Все границы kernel/user space защищены через `copy_*_user()`, валидация входа на каждом слое (fops/chat/registry).

### Дополнительные задания

### 1. Динамическое создание чатов
Реализовано через список с заданным лимитом на количество чатов.
- Control device `/dev/telegram/create_chat` принимает `write()` или `ioctl(TGFS_IOCTL_CTL_CREATE_CHAT)`
- `tgfs_chat_create()` выделяет `struct tgfs_chat`, назначает `id`/`minor`/`name`, добавляет в registry
- `tgfs_chrdev_create_chat_device()` создает device node через `device_create()`
- При `rmmod` все chat devices уничтожаются через `tgfs_chat_registry_for_each()` перед удалением class

### 2. Метка времени
Реализовано засчет хранения метки в структуре сообщения.
- `struct tgfs_msg` содержит поле `time64_t ts`
- При записи сообщения сохраняется `ktime_get_real_seconds()`
- При чтении форматируется как `[HH:MM:SS] message` через `tgfs_format_hms()`

### 3. `ioctl` интерфейс
Реализовано для обоих типов устройств.
- Для chat devices: `GET_MSG_COUNT`, `CLEAR`, `GET_READ_LIMIT`, `SET_READ_LIMIT`
- Для control device: `CREATE_CHAT` (структурированный запрос), `GET_CHAT_COUNT`
- Разделение команд через разные magic numbers (`TGFS_IOCTL_CHAT_MAGIC` vs `TGFS_IOCTL_CTL_MAGIC`)

### 4. Кольцевой буфер
Сообщения в чате хранятся в самом обычном кольцевом буфере:
- Хранение: `msgs[TGFS_MAX_MSG_CNT]`, `next_idx`, `msg_count`
- Запись: `next_idx = (next_idx + 1) % TGFS_MAX_MSG_CNT`, старые сообщения перезаписываются
- Чтение: вычисление `start_idx` как точки начала валидной истории, затем последовательный обход последних `N` сообщений

### 5. Синхронизация доступа (поддержка нескольких клиентов)
Реализовано через `mutex`.
- `chat->lock` — защищает `msgs[]`, `msg_count`, `next_idx`, `read_limit` конкретного чата
- `registry.lock` — защищает глобальный список чатов, счетчики `next_chat_id`/`next_minor`
- Snapshot формируется под локом, затем лок отпускается перед `copy_to_user()`

### 6. `poll/select`
Не реализовано.
