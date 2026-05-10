#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/capability.h>
#include <linux/types.h>
#include <linux/highmem.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/huge_mm.h>

/*
 * VA->PA syscall request
 */
struct vapa_req {
    __u64 va;     // [in]
    __u64 pa;     // [out]
    __u64 pfn;    // [out]
    __u64 flags;  // [out]
    __s32 status; // [out]
};

/*
 * Syscall return status enum
 */
enum {
    VAPA_STATUS_PRESENT  = 0,
    VAPA_STATUS_SWAPPED  = 1,
    VAPA_STATUS_UNMAPPED = 2,
    VAPA_STATUS_NOACCESS = 3,
    VAPA_STATUS_ERROR    = 4,
};

/*
 * Syscall result flag definitions
 */
#define VAPA_FLAG_PRESENT    (1ULL << 0)
#define VAPA_FLAG_SWAPPED    (1ULL << 1)
#define VAPA_FLAG_ANON       (1ULL << 2)
#define VAPA_FLAG_FILE       (1ULL << 3)
#define VAPA_FLAG_HUGE       (1ULL << 4)
#define VAPA_FLAG_SOFT_DIRTY (1ULL << 5)

/*
 * Remove junk values from result struct
 */
static void vapa_zero_result(struct vapa_req *req)
{
    req->pa = 0;
    req->pfn = 0;
    req->flags = 0;
    req->status = VAPA_STATUS_ERROR;
}

/*
 * Try to determine page status from address
 * 
 * Param:
 *   mm - process address space
 *   addr - virtual address in that space
 *   req - result struct
 * 
 * Return:
 *   0 - success
 *   <0 - internal error
 */
static int vapa_translate(
    struct mm_struct *mm,
    unsigned long addr,
    struct vapa_req *req
)
{
    struct vm_area_struct *vma;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    spinlock_t *ptl;
    unsigned long pfn;
    unsigned long offset;

    vapa_zero_result(req);
    
    // Find virtual memory area that covers given address
    vma = find_vma(mm, addr);
    if (!vma || addr < vma->vm_start) {
        // VMA starts after given address, we are in the gap
        req->status = VAPA_STATUS_UNMAPPED;
        return 0;
    }

    // Policy sanity check
    if (!(vma->vm_flags & (VM_READ | VM_WRITE | VM_EXEC))) {
        req->status = VAPA_STATUS_NOACCESS;
        return 0;
    }

    // Checking out mapping type:
    // ANON -> heap/stack/anon mmap
    // FILE -> file mmap
    if (vma_is_anonymous(vma))
        req->flags |= VAPA_FLAG_ANON;
    else if (vma->vm_file)
        req->flags |= VAPA_FLAG_FILE;


    // Starting page table walk standard to x86_64


    // Page global directory
    pgd = pgd_offset(mm, addr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
        req->status = VAPA_STATUS_UNMAPPED;
        return 0;
    }

    //  Page 4th-level directory
    p4d = p4d_offset(pgd, addr);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
        req->status = VAPA_STATUS_UNMAPPED;
        return 0;
    }

    // Page upper directory
    pud = pud_offset(p4d, addr);
    if (pud_none(*pud) || pud_bad(*pud)) {
        req->status = VAPA_STATUS_UNMAPPED;
        return 0;
    }

    // Handling huge page if it can be done on PUD level
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
    if (pud_trans_huge(*pud) || pud_devmap(*pud)) {
        pfn = pud_pfn(*pud) + ((addr & ~PUD_MASK) >> PAGE_SHIFT);
        offset = addr & ~PAGE_MASK;

        req->pfn = pfn;
        req->pa = (pfn << PAGE_SHIFT) | offset;
        req->flags |= VAPA_FLAG_PRESENT | VAPA_FLAG_HUGE;
        req->status = VAPA_STATUS_PRESENT;
        return 0;
    }
#endif

    // Page middle directory
    pmd = pmd_offset(pud, addr);
    if (pmd_none(*pmd) || pmd_bad(*pmd)) {
        req->status = VAPA_STATUS_UNMAPPED;
        return 0;
    }

    // Handling huge page if it can be done on PMD level
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
    if (pmd_trans_huge(*pmd) || pmd_devmap(*pmd)) {
        pfn = pmd_pfn(*pmd) + ((addr & ~PMD_MASK) >> PAGE_SHIFT);
        offset = addr & ~PAGE_MASK;

        req->pfn = pfn;
        req->pa = (pfn << PAGE_SHIFT) | offset;
        req->flags |= VAPA_FLAG_PRESENT | VAPA_FLAG_HUGE;
        req->status = VAPA_STATUS_PRESENT;
        return 0;
    }
#endif

    // Page table entry - obtaining lock and pointer
    pte = pte_offset_map_lock(mm, pmd, addr, &ptl);
    if (!pte) {
        req->status = VAPA_STATUS_ERROR;
        return 0;
    }

    // Got no materialized translation -> UNMAPPED
    if (pte_none(*pte)) {
        req->status = VAPA_STATUS_UNMAPPED;
        goto out_unlock;
    }

    // Got materialized translation & not present -> SWAP or ?
    if (!pte_present(*pte)) {
        swp_entry_t swp = pte_to_swp_entry(*pte);

        if (!non_swap_entry(swp)) {
            // This page is swapped
            req->status = VAPA_STATUS_SWAPPED;
            req->flags |= VAPA_FLAG_SWAPPED;
        } else {
            // Not present nor swap, UNMAPPED for simplicity
            req->status = VAPA_STATUS_UNMAPPED;
        }

        goto out_unlock;
    }


    // Obtaining PFN to reconstruct phy address
    pfn = pte_pfn(*pte);

    offset = addr & ~PAGE_MASK; // Lesser bits of VA

    req->pfn = pfn;
    req->pa = (pfn << PAGE_SHIFT) | offset; // Physical base | offset

    req->flags |= VAPA_FLAG_PRESENT;
    req->status = VAPA_STATUS_PRESENT;

    // Mark as soft-dirty if this check is available
#ifdef CONFIG_MEM_SOFT_DIRTY
    if (pte_soft_dirty(*pte))
        req->flags |= VAPA_FLAG_SOFT_DIRTY;
#endif

out_unlock:
    // Unmap and unlock PTE
    pte_unmap_unlock(pte, ptl);
    return 0;
}

SYSCALL_DEFINE1(vapa, struct vapa_req __user *, ureq)
{
    struct vapa_req req;
    struct mm_struct *mm = current->mm; // Task-struct of current process -> memory descriptor
    unsigned long addr;
    int ret;

    if (!capable(CAP_SYS_ADMIN))
        return -EPERM;

    // Might be null for kthreads
    if (!mm)
        return -EINVAL;

    // Properly handling user/kernel boundary
    if (copy_from_user(&req, ureq, sizeof(req)))
        return -EFAULT;

    addr = (unsigned long)req.va;

    // x86_64 specific check not to touch kernel space
#ifdef CONFIG_X86_64
    if (addr >= TASK_SIZE_MAX) {
        vapa_zero_result(&req);
        req.status = VAPA_STATUS_NOACCESS;

        if (copy_to_user(ureq, &req, sizeof(req)))
            return -EFAULT;

        return 0;
    }
#endif

    mmap_read_lock(mm);
    ret = vapa_translate(mm, addr, &req);
    mmap_read_unlock(mm);

    if (ret) {
        vapa_zero_result(&req);
        req.status = VAPA_STATUS_ERROR;
    }

    // Properly handling user/kernel boundary
    if (copy_to_user(ureq, &req, sizeof(req)))
        return -EFAULT;

    return 0;
}
