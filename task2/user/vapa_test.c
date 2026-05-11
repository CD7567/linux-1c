#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef __NR_vapa
#define __NR_vapa 549
#endif

struct vapa_req {
    uint64_t va;      /* [in]  virtual address */
    uint64_t pa;      /* [out] physical address */
    uint64_t pfn;     /* [out] page frame number */
    uint64_t flags;   /* [out] state/type flags */
    int32_t  status;  /* [out] translation status */
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

static const char *status_to_str(int status)
{
    switch (status) {
    case VAPA_STATUS_PRESENT:  return "PRESENT";
    case VAPA_STATUS_SWAPPED:  return "SWAPPED";
    case VAPA_STATUS_UNMAPPED: return "UNMAPPED";
    case VAPA_STATUS_NOACCESS: return "NOACCESS";
    case VAPA_STATUS_ERROR:    return "ERROR";
    default:                   return "UNKNOWN";
    }
}

static void print_flags(uint64_t flags)
{
    int first = 1;

    printf("0x%016" PRIx64 " [", flags);

    if (flags & VAPA_FLAG_PRESENT) {
        printf("%sPRESENT", first ? "" : "|");
        first = 0;
    }
    if (flags & VAPA_FLAG_SWAPPED) {
        printf("%sSWAPPED", first ? "" : "|");
        first = 0;
    }
    if (flags & VAPA_FLAG_ANON) {
        printf("%sANON", first ? "" : "|");
        first = 0;
    }
    if (flags & VAPA_FLAG_FILE) {
        printf("%sFILE", first ? "" : "|");
        first = 0;
    }
    if (flags & VAPA_FLAG_HUGE) {
        printf("%sHUGE", first ? "" : "|");
        first = 0;
    }
    if (flags & VAPA_FLAG_SOFT_DIRTY) {
        printf("%sSOFT_DIRTY", first ? "" : "|");
        first = 0;
    }

    if (first)
        printf("none");

    printf("]");
}

static int do_vapa(void *addr, struct vapa_req *req)
{
    memset(req, 0, sizeof(*req));
    req->va = (uint64_t)(uintptr_t)addr;

    long ret = syscall(__NR_vapa, req);
    if (ret < 0)
        return -1;

    return 0;
}

static void run_case(const char *label, void *addr)
{
    struct vapa_req req;

    printf("== %s ==\n", label);
    printf("input VA : 0x%016" PRIx64 "\n", (uint64_t)(uintptr_t)addr);

    if (do_vapa(addr, &req) < 0) {
        printf("syscall() failed: errno=%d (%s)\n\n", errno, strerror(errno));
        return;
    }

    printf("status   : %d (%s)\n", req.status, status_to_str(req.status));
    printf("pfn      : 0x%016" PRIx64 "\n", req.pfn);
    printf("pa       : 0x%016" PRIx64 "\n", req.pa);
    printf("flags    : ");
    print_flags(req.flags);
    printf("\n\n");
}

static int create_backing_file(char *path, size_t path_sz, size_t len)
{
    int fd;
    char *buf;
    ssize_t wr;

    snprintf(path, path_sz, "/tmp/vapa-demo-%ld.bin", (long)getpid());

    fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0600);
    if (fd < 0)
        return -1;

    buf = malloc(len);
    if (!buf) {
        close(fd);
        return -1;
    }

    for (size_t i = 0; i < len; i++)
        buf[i] = (char)('A' + (i % 26));

    wr = write(fd, buf, len);
    free(buf);

    if (wr != (ssize_t)len) {
        close(fd);
        return -1;
    }

    if (lseek(fd, 0, SEEK_SET) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

int main(void)
{
    long page_sz;
    char *heap = NULL;
    char *anon = NULL;
    char *filemap = NULL;
    void *anon_saved = NULL;
    char tmp_path[256];
    int fd = -1;

    page_sz = sysconf(_SC_PAGESIZE);
    if (page_sz <= 0) {
        fprintf(stderr, "sysconf(_SC_PAGESIZE) failed\n");
        return 1;
    }

    printf("vapa demo started\n");
    printf("page size: %ld bytes\n", page_sz);
    printf("syscall number: %d\n", __NR_vapa);
    printf("NOTE: run as root or with CAP_SYS_ADMIN, otherwise syscall returns EPERM.\n\n");

    /*
     * 1. heap / malloc
     *
     * After writing to heap-buffer page should become usual anon page.
     */
    heap = malloc((size_t)page_sz * 2);
    if (!heap) {
        perror("malloc");
        return 1;
    }

    strcpy(heap, "hello from heap");
    run_case("heap: malloc base", heap);
    run_case("heap: malloc + 17", heap + 17);

    /*
     * 2. anonymous mmap
     *
     * Same as 1 anon mapping, but page-aligned.
     */
    anon = mmap(NULL, (size_t)page_sz * 3,
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (anon == MAP_FAILED) {
        perror("mmap anonymous");
        free(heap);
        return 1;
    }

    /*
     * Touching all so that we see stable PRESENT.
     */
    anon[0] = 'X';
    anon[page_sz] = 'Y';
    anon[2 * page_sz] = 'Z';

    run_case("anon mmap: page 0", anon);
    run_case("anon mmap: page 1", anon + page_sz);
    run_case("anon mmap: page 2 + 123", anon + 2 * page_sz + 123);

    /*
     * 3. file-backed mapping
     *
     * FILE instead of ANON.
     */
    fd = create_backing_file(tmp_path, sizeof(tmp_path), (size_t)page_sz * 2);
    if (fd < 0) {
        perror("create backing file");
        munmap(anon, (size_t)page_sz * 3);
        free(heap);
        return 1;
    }

    filemap = mmap(NULL, (size_t)page_sz * 2,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE, fd, 0);
    if (filemap == MAP_FAILED) {
        perror("mmap file");
        close(fd);
        unlink(tmp_path);
        munmap(anon, (size_t)page_sz * 3);
        free(heap);
        return 1;
    }

    volatile char c = filemap[0];
    (void)c;

    run_case("file-backed mmap: page 0", filemap);
    run_case("file-backed mmap: page 1 + 64", filemap + page_sz + 64);

    /*
     * 4. UNMAPPED
     */
    anon_saved = anon;
    if (munmap(anon, (size_t)page_sz * 3) != 0) {
        perror("munmap anonymous");
    } else {
        run_case("after munmap: expected UNMAPPED", anon_saved);
    }

    /*
     * 5. kernel-like high address
     *
     * For x86_64 with addr >= TASK_SIZE_MAX we expect NOACCESS.
     */
    run_case("kernel-like high address: expected NOACCESS",
             (void *)(uintptr_t)0xffff800000000000ULL);

    /*
     * 6. NULL
     *
     * Likely UNMAPPED.
     */
    run_case("NULL address", NULL);

    /*
     * 7. Trying close but not inside heap-buffer.
     */
    run_case("heap far offset (best-effort experiment)", heap + page_sz * 16);



    if (filemap && filemap != MAP_FAILED)
        munmap(filemap, (size_t)page_sz * 2);

    if (fd >= 0)
        close(fd);

    if (tmp_path[0] != '\0')
        unlink(tmp_path);

    free(heap);

    return 0;
}
