#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>

#include "telegramfs/ioctl.h"

#define TGFS_CONTROL_DEV "/dev/telegram/create_chat"
#define TGFS_LIST_BUF_SIZE 8192

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s create [name]              -- Clear chat device\n"
		"  %s list                       -- List available devices\n"
		"  %s count <device>             -- Count chat messages\n"
		"  %s clear <device>             -- Clear chat messages\n"
		"  %s get-limit <device>         -- Get chat read limit\n"
		"  %s set-limit <device> <value> -- Set chat read limit\n",
		prog, prog, prog, prog, prog, prog);
}

static int open_device(const char *path)
{
	int fd = open(path, O_RDWR);
	if (fd < 0)
		fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));
	return fd;
}

static int parse_u32(const char *s, size_t *out)
{
	char *end = NULL;
	size_t v;

	if (!s || !out)
		return -1;

	errno = 0;
	v = strtoul(s, &end, 10);
	if (errno != 0 || end == s || *end != '\0' || v > UINT32_MAX)
		return -1;

	*out = v;
	return 0;
}

static int cmd_create(const char *name)
{
	int fd;
	struct tgfs_ctl_create_req req;

	memset(&req, 0, sizeof(req));
	if (name)
		snprintf(req.name, sizeof(req.name), "%s", name);

	fd = open_device(TGFS_CONTROL_DEV);
	if (fd < 0)
		return 1;

	if (ioctl(fd, TGFS_IOCTL_CTL_CREATE_CHAT, &req) < 0) {
		fprintf(stderr, "ioctl(CREATE_CHAT) failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	printf("%s\n", req.device_name);
	close(fd);
	return 0;
}

static int cmd_list(void)
{
	int fd;
	ssize_t n;
	char buf[TGFS_LIST_BUF_SIZE];

	fd = open(TGFS_CONTROL_DEV, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "failed to open %s: %s\n", TGFS_CONTROL_DEV, strerror(errno));
		return 1;
	}

	n = read(fd, buf, sizeof(buf) - 1);
	if (n < 0) {
		fprintf(stderr, "read(list) failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	buf[n] = '\0';
	printf("%s", buf);

	close(fd);
	return 0;
}

static int cmd_count(const char *path)
{
	int fd;
	size_t count = 0;

	fd = open_device(path);
	if (fd < 0)
		return 1;

	if (ioctl(fd, TGFS_IOCTL_CHAT_GET_MSG_COUNT, &count) < 0) {
		fprintf(stderr, "ioctl(GET_MSG_COUNT) failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	printf("%lu\n", count);
	close(fd);
	return 0;
}

static int cmd_clear(const char *path)
{
	int fd;

	fd = open_device(path);
	if (fd < 0)
		return 1;

	if (ioctl(fd, TGFS_IOCTL_CHAT_CLEAR) < 0) {
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

	if (ioctl(fd, TGFS_IOCTL_CHAT_GET_READ_LIMIT, &limit) < 0) {
		fprintf(stderr, "ioctl(GET_READ_LIMIT) failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	printf("%lu\n", limit);
	close(fd);
	return 0;
}

static int cmd_set_limit(const char *path, const char *value_str)
{
	int fd;
	size_t limit;

	if (parse_u32(value_str, &limit) < 0) {
		fprintf(stderr, "invalid limit: %s\n", value_str);
		return 1;
	}

	fd = open_device(path);
	if (fd < 0)
		return 1;

	if (ioctl(fd, TGFS_IOCTL_CHAT_SET_READ_LIMIT, &limit) < 0) {
		fprintf(stderr, "ioctl(SET_READ_LIMIT) failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	printf("read limit set to %lu\n", limit);
	close(fd);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	if (strcmp(argv[1], "create") == 0) {
		if (argc == 2)
			return cmd_create(NULL);
		if (argc == 3)
			return cmd_create(argv[2]);
		usage(argv[0]);
		return 1;
	}

	if (strcmp(argv[1], "list") == 0) {
		if (argc != 2) {
			usage(argv[0]);
			return 1;
		}
		return cmd_list();
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

	usage(argv[0]);
	return 1;
}
