#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "telegramfs/ioctl.h"

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s count <device>             -- Count chat messages\n"
		"  %s clear <device>             -- Clear chat messages\n"
		"  %s get-limit <device>         -- Get chat read limit\n"
		"  %s set-limit <device> <value> -- Set chat read limit\n"
		"\n"
		"Examples:\n"
		"  %s count /dev/telegram/chat0\n"
		"  %s clear /dev/telegram/chat0\n"
		"  %s get-limit /dev/telegram/chat0\n"
		"  %s set-limit /dev/telegram/chat0 5\n",
		prog, prog, prog, prog,
		prog, prog, prog, prog);
}

static int open_device(const char *path)
{
	int fd = open(path, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));
		return -1;
	}
	return fd;
}

static int cmd_count(const char *path)
{
	int fd;
	size_t count = 0;

	fd = open_device(path);
	if (fd < 0)
		return 1;

	if (ioctl(fd, TGFS_IOCTL_GET_MSG_COUNT, &count) < 0) {
		fprintf(stderr, "ioctl(GET_MSG_COUNT) failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	printf("%zu\n", count);

	close(fd);
	return 0;
}

static int cmd_clear(const char *path)
{
	int fd;

	fd = open_device(path);
	if (fd < 0)
		return 1;

	if (ioctl(fd, TGFS_IOCTL_CLEAR_CHAT) < 0) {
		fprintf(stderr, "ioctl(CLEAR_CHAT) failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	printf("chat cleared\n");

	close(fd);
	return 0;
}

static int cmd_get_limit(const char *path)
{
	int fd;
	size_t limit = 0;

	fd = open_device(path);
	if (fd < 0)
		return 1;

	if (ioctl(fd, TGFS_IOCTL_GET_READ_LIMIT, &limit) < 0) {
		fprintf(stderr, "ioctl(GET_READ_LIMIT) failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	printf("%zu\n", limit);

	close(fd);
	return 0;
}

static int parse_size(const char *s, size_t *out)
{
	char *end = NULL;
	unsigned long long v;

	if (!s || !out)
		return -1;

	errno = 0;
	v = strtoull(s, &end, 10);

	if (errno != 0 || end == s || *end != '\0')
		return -1;

	*out = (size_t)v;
	return 0;
}

static int cmd_set_limit(const char *path, const char *value_str)
{
	int fd;
	size_t limit;

	if (parse_size(value_str, &limit) < 0) {
		fprintf(stderr, "invalid limit: %s\n", value_str);
		return 1;
	}

	fd = open_device(path);
	if (fd < 0)
		return 1;

	if (ioctl(fd, TGFS_IOCTL_SET_READ_LIMIT, &limit) < 0) {
		fprintf(stderr, "ioctl(SET_READ_LIMIT) failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	printf("read limit set to %zu\n", limit);

	close(fd);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		usage(argv[0]);
		return 1;
	}

	if (strcmp(argv[1], "count") == 0) {
		if (argc != 3) {
			usage(argv[0]);
			return 1;
		}
		return cmd_count(argv[2]);
	}

	if (strcmp(argv[1], "clear") == 0) {
		if (argc != 3) {
			usage(argv[0]);
			return 1;
		}
		return cmd_clear(argv[2]);
	}

	if (strcmp(argv[1], "get-limit") == 0) {
		if (argc != 3) {
			usage(argv[0]);
			return 1;
		}
		return cmd_get_limit(argv[2]);
	}

	if (strcmp(argv[1], "set-limit") == 0) {
		if (argc != 4) {
			usage(argv[0]);
			return 1;
		}
		return cmd_set_limit(argv[2], argv[3]);
	}

	fprintf(stderr, "unknown command: %s\n", argv[1]);
	usage(argv[0]);
	return 1;
}
