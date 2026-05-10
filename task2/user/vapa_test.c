#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <string.h>

#ifndef __NR_vapa
#define __NR_vapa 549   // тот же номер, что в syscall_64.tbl
#endif

struct vapa_req {
    uint64_t va;
    uint64_t pa;
    uint64_t pfn;
    uint64_t flags;
    int32_t  status;
};

enum {
    VAPA_STATUS_PRESENT  = 0,
    VAPA_STATUS_SWAPPED  = 1,
    VAPA_STATUS_UNMAPPED = 2,
    VAPA_STATUS_NOACCESS = 3,
    VAPA_STATUS_ERROR    = 4,
};

#define VAPA_FLAG_PRESENT    (1ULL << 0)
#define VAPA_FLAG_SWAPPED    (1ULL << 1)
#define VAPA_FLAG_ANON       (1ULL << 2)
#define VAPA_FLAG_FILE       (1ULL << 3)
#define VAPA_FLAG_HUGE       (1ULL << 4)
#define VAPA_FLAG_SOFT_DIRTY (1ULL << 5)

static const char *status_str(int st)
{
    switch (st) {
    case VAPA_STATUS_PRESENT:  return "PRESENT";
    case VAPA_STATUS_SWAPPED:  return "SWAPPED";
    case VAPA_STATUS_UNMAPPED: return "UNMAPPED";
    case VAPA_STATUS_NOACCESS: return "NOACCESS";
    case VAPA_STATUS_ERROR:    return "ERROR";
    default:                   return "UNKNOWN";
    }
}

int main(void)
{
    size_t buf_size = 4096 * 4;
    char *buf = malloc(buf_size);
    struct vapa_req req;
    long ret;

    if (!buf) {
        perror("malloc");
        return 1;
    }

    for (size_t i = 0; i < buf_size; i += 4096)
        buf[i] = (char)(i / 4096);

    void *addr = buf + 4096 * 2;  // третья страница

    memset(&req, 0, sizeof(req));
    req.va = (uint64_t)(uintptr_t)addr;

    ret = syscall(__NR_vapa, &req);
    if (ret < 0) {
        fprintf(stderr, "va_to_pa syscall failed: ret=%ld errno=%d (%s)\n",
                ret, errno, strerror(errno));
        free(buf);
        return 1;
    }

    printf("VA: 0x%llx\n", (unsigned long long)req.va);
    printf("status: %d (%s)\n", req.status, status_str(req.status));
    printf("pfn: 0x%llx\n", (unsigned long long)req.pfn);
    printf("pa:  0x%llx\n", (unsigned long long)req.pa);
    printf("flags: 0x%llx", (unsigned long long)req.flags);

    if (req.flags & VAPA_FLAG_PRESENT)    printf(" PRESENT");
    if (req.flags & VAPA_FLAG_ANON)       printf(" ANON");
    if (req.flags & VAPA_FLAG_FILE)       printf(" FILE");
    if (req.flags & VAPA_FLAG_HUGE)       printf(" HUGE");
    if (req.flags & VAPA_FLAG_SOFT_DIRTY) printf(" SOFT_DIRTY");
    printf("\n");

    free(buf);
    return 0;
}
