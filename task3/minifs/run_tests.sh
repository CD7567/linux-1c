#!/usr/bin/env bash
set -euo pipefail

MNT="${1:-/mnt/minifs}"

pass() {
    echo "[PASS] $*"
}

fail() {
    echo "[FAIL] $*" >&2
    exit 1
}

expect_file_contains() {
    local file="$1"
    local expected="$2"
    local content
    content="$(cat "$file")"
    [[ "$content" == "$expected" ]] || fail "Expected '$file' to contain '$expected', got '$content'"
}

expect_exists() {
    local path="$1"
    [[ -e "$path" ]] || fail "Expected path to exist: $path"
}

expect_not_exists() {
    local path="$1"
    [[ ! -e "$path" ]] || fail "Expected path to NOT exist: $path"
}

expect_grep() {
    local pattern="$1"
    local file="$2"
    grep -Eq "$pattern" "$file" || fail "Expected pattern '$pattern' in $file"
}

expect_command_fail() {
    local desc="$1"
    shift
    if "$@"; then
        fail "Command unexpectedly succeeded: $desc"
    else
        pass "$desc"
    fi
}

echo "== minifs test start =="
echo "Mountpoint: $MNT"

mountpoint -q "$MNT" || fail "$MNT is not mounted"

# ------------------------------------------------------------------
# 1. Root directory is accessible
# ------------------------------------------------------------------
[[ -d "$MNT" ]] || fail "Root mountpoint is not a directory"
ls -la "$MNT" >/dev/null
pass "Root directory is accessible"

# ------------------------------------------------------------------
# 2. Basic create/read/write
# ------------------------------------------------------------------
echo -n "hello" > "$MNT/file.txt"
expect_exists "$MNT/file.txt"
expect_file_contains "$MNT/file.txt" "hello"
pass "Basic create/read/write works"

# ------------------------------------------------------------------
# 3. File overwrite
# ------------------------------------------------------------------
echo -n "world" > "$MNT/file.txt"
expect_file_contains "$MNT/file.txt" "world"
pass "Overwrite works"

# ------------------------------------------------------------------
# 4. Multiple files + ls
# ------------------------------------------------------------------
echo -n "one" > "$MNT/a.txt"
echo -n "two" > "$MNT/b.txt"

expect_file_contains "$MNT/a.txt" "one"
expect_file_contains "$MNT/b.txt" "two"

LS_OUT="$(ls -1 "$MNT")"
echo "$LS_OUT" | grep -qx "file.txt" || fail "file.txt not visible in ls"
echo "$LS_OUT" | grep -qx "a.txt" || fail "a.txt not visible in ls"
echo "$LS_OUT" | grep -qx "b.txt" || fail "b.txt not visible in ls"
pass "Multiple files and ls work"

# ------------------------------------------------------------------
# 5. mkdir + nested directories
# ------------------------------------------------------------------
mkdir "$MNT/dir"
mkdir "$MNT/dir/sub"
expect_exists "$MNT/dir"
expect_exists "$MNT/dir/sub"

echo -n "nested" > "$MNT/dir/sub/f.txt"
expect_file_contains "$MNT/dir/sub/f.txt" "nested"
pass "mkdir and nested directories work"

# ------------------------------------------------------------------
# 6. unlink (rm file)
# ------------------------------------------------------------------
rm "$MNT/a.txt"
expect_not_exists "$MNT/a.txt"
pass "unlink works for regular file"

rm "$MNT/dir/sub/f.txt"
expect_not_exists "$MNT/dir/sub/f.txt"
pass "unlink works for nested file"

# ------------------------------------------------------------------
# 7. rmdir: (non)empty directories
# ------------------------------------------------------------------
rmdir "$MNT/dir/sub"
expect_not_exists "$MNT/dir/sub"
pass "rmdir works for empty directory"

echo -n "x" > "$MNT/dir/keep.txt"
expect_command_fail "rmdir fails for non-empty directory" rmdir "$MNT/dir"
rm "$MNT/dir/keep.txt"
rmdir "$MNT/dir"
expect_not_exists "$MNT/dir"
pass "rmdir behavior is correct"

# ------------------------------------------------------------------
# 8. /proc/minifs_stats
# ------------------------------------------------------------------
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

# ------------------------------------------------------------------
# 9. FS size limit
# ------------------------------------------------------------------
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

# ------------------------------------------------------------------
# 10. Final cleanup umount
# ------------------------------------------------------------------
rm -f "$MNT/file.txt" "$MNT/b.txt" "$BIG1" "$BIG2" || true
ls -la "$MNT"
pass "Cleanup before umount completed"

rm -f "$STATS_TMP" /tmp/minifs_big_1.bin /tmp/minifs_big_2.bin

echo "== minifs test finished successfully =="
