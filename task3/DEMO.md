# Демо работы

Тестовые кейсы оформлены в скрипт:
```shell
sudo bash run_tests.sh
```

Предусловие для выполнения тестов:
1. Установлен модуль ядра
2. minifs смонтирована по пути `/mnt/minifs`

Полный вывод тестового скрипта:
```
== minifs test start ==
Mountpoint: /mnt/minifs
[PASS] Root directory is accessible
[PASS] Basic create/read/write works
[PASS] Overwrite works
[PASS] Multiple files and ls work
[PASS] mkdir and nested directories work
[PASS] unlink works for regular file
[PASS] unlink works for nested file
[PASS] rmdir works for empty directory
rmdir: failed to remove '/mnt/minifs/dir': Directory not empty
[PASS] rmdir fails for non-empty directory
[PASS] rmdir behavior is correct
[PASS] /proc/minifs_stats exists and has expected fields
---- /proc/minifs_stats ----
minifs_stats
files=2
dirs=1
used_bytes=8
max_bytes=65536
read_ops=10
write_ops=6
create_ops=5
mkdir_ops=2
unlink_ops=3
rmdir_ops=2
----------------------------
[PASS] Single file max-size write works
[PASS] FS size limit blocks oversized write
total 4
drwxr-xr-x 2 root root    0 May 12 13:12 .
drwxr-xr-x 4 root root 4096 May 12 08:34 ..
[PASS] Cleanup before umount completed
== minifs test finished successfully ==
```

## Тестовые кейсы

### 1. Доступ к смонтированной директории
```shell
[[ -d "$MNT" ]] || fail "Root mountpoint is not a directory"
ls -la "$MNT" >/dev/null
pass "Root directory is accessible"
```

### 2. Базовые операции create/read/write
```shell
echo -n "hello" > "$MNT/file.txt"
expect_exists "$MNT/file.txt"
expect_file_contains "$MNT/file.txt" "hello"
pass "Basic create/read/write works"
```

### 3. Перезапись существующего файла
```shell
echo -n "world" > "$MNT/file.txt"
expect_file_contains "$MNT/file.txt" "world"
pass "Overwrite works"
```

### 4. Создание и запись нескольких файлов
```shell
echo -n "one" > "$MNT/a.txt"
echo -n "two" > "$MNT/b.txt"

expect_file_contains "$MNT/a.txt" "one"
expect_file_contains "$MNT/b.txt" "two"

LS_OUT="$(ls -1 "$MNT")"
echo "$LS_OUT" | grep -qx "file.txt" || fail "file.txt not visible in ls"
echo "$LS_OUT" | grep -qx "a.txt" || fail "a.txt not visible in ls"
echo "$LS_OUT" | grep -qx "b.txt" || fail "b.txt not visible in ls"
pass "Multiple files and ls work"
```

### 5. mkdir + поддиректории
```shell
mkdir "$MNT/dir"
mkdir "$MNT/dir/sub"
expect_exists "$MNT/dir"
expect_exists "$MNT/dir/sub"

echo -n "nested" > "$MNT/dir/sub/f.txt"
expect_file_contains "$MNT/dir/sub/f.txt" "nested"
pass "mkdir and nested directories work"
```

### 6. Удаление файлов
```shell
rm "$MNT/a.txt"
expect_not_exists "$MNT/a.txt"
pass "unlink works for regular file"

rm "$MNT/dir/sub/f.txt"
expect_not_exists "$MNT/dir/sub/f.txt"
pass "unlink works for nested file"
```

### 7. Удаление директорий
```shell
rmdir "$MNT/dir/sub"
expect_not_exists "$MNT/dir/sub"
pass "rmdir works for empty directory"

echo -n "x" > "$MNT/dir/keep.txt"
expect_command_fail "rmdir fails for non-empty directory" rmdir "$MNT/dir"
rm "$MNT/dir/keep.txt"
rmdir "$MNT/dir"
expect_not_exists "$MNT/dir"
pass "rmdir behavior is correct"
```

### 8. Чтение статистики /proc/minifs_stats
```shell
STATS_TMP="$(mktemp)"
cat /proc/minifs_stats > "$STATS_TMP"

expect_grep '^minifs_stats$' "$STATS_TMP"
expect_grep '^files=' "$STATS_TMP"
expect_grep '^dirs=' "$STATS_TMP"
expect_grep '^used_bytes=' "$STATS_TMP"
expect_grep '^max_bytes=' "$STATS_TMP"
expect_grep '^read_ops=' "$STATS_TMP"
expect_grep '^write_ops=' "$STATS_TMP"
expect_grep '^create_ops=' "$STATS_TMP"
expect_grep '^mkdir_ops=' "$STATS_TMP"
expect_grep '^unlink_ops=' "$STATS_TMP"
expect_grep '^rmdir_ops=' "$STATS_TMP"
pass "/proc/minifs_stats exists and has expected fields"

echo "---- /proc/minifs_stats ----"
cat "$STATS_TMP"
echo "----------------------------"
```

### 9. Проверка лимита размера ФС
```shell
BIG1="$MNT/big1.bin"
BIG2="$MNT/big2.bin"

python3 - <<'PY' > /tmp/minifs_big_1.bin
import sys
sys.stdout.write("A" * 4096)
PY

cp /tmp/minifs_big_1.bin "$BIG1"
expect_exists "$BIG1"
pass "Single file max-size write works"

python3 - <<'PY' > /tmp/minifs_big_2.bin
import sys
sys.stdout.write("B" * 70000)
PY

if cp /tmp/minifs_big_2.bin "$BIG2" 2>/dev/null; then
    fail "FS size limit test failed: unexpectedly wrote oversized file"
else
    pass "FS size limit blocks oversized write"
fi
```

### 10. Финальная очистка файлов
```shell
rm -f "$MNT/file.txt" "$MNT/b.txt" "$BIG1" "$BIG2" || true
ls -la "$MNT"
pass "Cleanup before umount completed"

rm -f "$STATS_TMP" /tmp/minifs_big_1.bin /tmp/minifs_big_2.bin
```
