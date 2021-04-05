/*******************************************************************************
    Copyright (c) 2015-2019 NVIDIA Corporation

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

        The above copyright notice and this permission notice shall be
        included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#ifndef __UVM8_MMU_H__
#define __UVM8_MMU_H__

#include "uvm8_forward_decl.h"
#include "uvm8_hal_types.h"
#include "uvm8_pmm_gpu.h"
#include "uvmtypes.h"
#include "uvm_common.h"
#include "uvm8_tracker.h"
#include "uvm8_test_ioctl.h"

// Used when the page size isn't known and should not matter
#define UVM_PAGE_SIZE_AGNOSTIC 0

// The size of VA that should be reserved per peer identity mapping
// This should be at least the maximum amount of memory of any GPU
#define UVM_PEER_IDENTITY_VA_SIZE (64ull * 1024 * 1024 * 1024)

// GPUs which support ATS perform a parallel lookup on both ATS and GMMU page
// tables. The ATS lookup can be disabled by setting a bit in the GMMU page
// tables. All GPUs which support ATS use the same mechanism (a bit in PDE1),
// and have the same PDE1 coverage (512MB).
//
// If the PTE format changes, this will need to move to the HAL.
#define UVM_GMMU_ATS_GRANULARITY (512ull * 1024 * 1024)

// this represents an allocation containing either a page table or page directory
typedef struct
{
    uvm_gpu_phys_address_t addr;

    NvU64 size;
    union
    {
        struct page *page;
        uvm_gpu_chunk_t *chunk;
    } handle;
} uvm_mmu_page_table_alloc_t;

// This structure in general refers to a page directory
// although it is also used to represent a page table, in which case
// entries is not allocated.
typedef struct uvm_page_directory_struct uvm_page_directory_t;

struct uvm_page_directory_struct
{
    // parent directory
    uvm_page_directory_t *host_parent;

    // index of this entry in the parent directory
    NvU32 index_in_parent;

    // allocation that holds actual page table used by device
    uvm_mmu_page_table_alloc_t phys_alloc;

    // count of references to all entries
    NvU32 ref_count;

    // depth from the root
    NvU32 depth;

    // pointers to child directories on the host.
    // this array is variable length, so it needs to be last to allow it to take up extra space
    uvm_page_directory_t *entries[0];
};

struct uvm_mmu_mode_hal_struct
{
    // bit pattern of a valid PTE
    NvU64 (*make_pte)(uvm_aperture_t aperture, NvU64 address, uvm_prot_t prot, bool vol, NvU32 page_size);

    // bit pattern of a sked reflected PTE
    NvU64 (*make_sked_reflected_pte)(void);

    // bit pattern of sparse PTE
    // Sparse PTEs will indicate to MMU to route all reads and writes to the
    // debug pages. Therefore, accesses to sparse mappings do not generate
    // faults.
    NvU64 (*make_sparse_pte)(void);

    // Bit pattern of an "unmapped" PTE. The GPU MMU recognizes two flavors of
    // empty PTEs:
    // 1) Invalid: Bit pattern of all 0s. There is no HAL function for this.
    // 2) Unmapped: This pattern.
    //
    // The subtle difference is for big PTEs. Invalid big PTEs indicate to the
    // GPU MMU that there might be 4k PTEs present instead, and that those 4k
    // entries should be read and cached. Unmapped big PTEs indicate that there
    // are no 4k PTEs below the unmapped big entry, so MMU should stop its walk
    // and not cache any 4k entries which may be in memory.
    //
    // This is an optimization which reduces TLB pressure, reduces the number of
    // TLB invalidates we must issue, and means we don't have to initialize the
    // 4k PTEs which are covered by big PTEs since the MMU will never read them.
    NvU64 (*unmapped_pte)(NvU32 page_size);

    // Bit pattern used for debug purposes to clobber PTEs which ought to be
    // unused. In practice this will generate a PRIV violation or a physical
    // memory out-of-range error so we can immediately identify bad PTE usage.
    NvU64 (*poisoned_pte)(NvU32 page_size);

    // write a PDE bit-pattern to entry based on the data in entries (which may point to two items for dual PDEs)
    // any of allocs are allowed to be NULL, in which case they are to be treated as empty
    void (*make_pde)(void *entry, uvm_mmu_page_table_alloc_t **allocs, NvU32 depth);

    // size of an entry in a directory/table.  Generally either 8 or 16 bytes (in the case of Pascal dual PDEs)
    NvLength (*entry_size)(NvU32 depth);

    // Two for dual PDEs, one otherwise.
    NvU32 (*entries_per_index)(NvU32 depth);

    // For dual PDEs, this is ether 1 or 0, depending on the page size.
    // This is used to index the host copy only.  GPU PDEs are always entirely re-written using make_pde.
    NvLength (*entry_offset)(NvU32 depth, NvU32 page_size);

    // number of virtual address bits used to index the directory/table at a given depth
    NvU32 (*index_bits)(NvU32 depth, NvU32 page_size);

    // total number of bits that represent the virtual address space
    NvU32 (*num_va_bits)(void);

    // the size, in bytes, of a directory/table at a given depth.
    NvLength (*allocation_size)(NvU32 depth, NvU32 page_size);

    // the depth which corresponds to the page tables
    NvU32 (*page_table_depth)(NvU32 page_size);

    // bitwise-or of supported page sizes
    NvU32 (*page_sizes)(void);
};

struct uvm_page_table_range_struct
{
    uvm_page_directory_t *table;
    NvU32 start_index;
    NvU32 entry_count;
    NvU32 page_size;
};

typedef enum
{
    UVM_PAGE_TREE_TYPE_USER,
    UVM_PAGE_TREE_TYPE_KERNEL,
    UVM_PAGE_TREE_TYPE_COUNT
} uvm_page_tree_type_t;

struct uvm_page_tree_struct
{
    uvm_mutex_t lock;
    uvm_gpu_t *gpu;
    uvm_page_directory_t *root;
    uvm_mmu_mode_hal_t *hal;
    uvm_page_tree_type_t type;
    NvU32 big_page_size;
    uvm_aperture_t location;

    // Tracker for all GPU operations on the tree
    uvm_tracker_t tracker;
};

// A vector of page table ranges
struct uvm_page_table_range_vec_struct
{
    // The tree the range vector is from
    uvm_page_tree_t *tree;

    // Start of the covered VA in bytes, always page_size aligned
    NvU64 start;

    // Size of the covered VA in bytes, always page_size aligned
    NvU64 size;

    // Page size used for all the page table ranges
    NvU32 page_size;

    // Page table ranges covering the the VA
    uvm_page_table_range_t *ranges;

    // Number of allocated ranges
    size_t range_count;
};

// Called at module init
NV_STATUS uvm_mmu_init(void);

// Initialize MMU-specific information for the GPU, including GPU's peer
// mappings
void uvm_mmu_init_gpu(uvm_gpu_t *gpu);

// Create a page tree structure and allocate the root directory. location
// behavior:
// - UVM_APERTURE_VID       Force all page table allocations into vidmem
// - UVM_APERTURE_SYS       Force all page table allocations into sysmem
// - UVM_APERTURE_DEFAULT   Let the implementation decide
NV_STATUS uvm_page_tree_init(uvm_gpu_t *gpu,
                             uvm_page_tree_type_t type,
                             NvU32 big_page_size,
                             uvm_aperture_t location,
                             uvm_page_tree_t *tree_out);

// Destroy the root directory of the page tree.
// This function asserts that there are no other elements left in the tree.
// All PTEs should have been put with uvm_page_tree_put_ptes before this function is called
void uvm_page_tree_deinit(uvm_page_tree_t *tree);

// Returns the range of PTEs that correspond to [start, start + size).
// It is the caller's responsibility to ensure that this range falls within the same table
// this function asserts that start and size are multiples of page_size because
// sub-page alignment information is not represented in *range PTE ranges may
// overlap.  In general, a PTE can be retained (get) multiple times, as long as
// it is released (put) as many times.
NV_STATUS uvm_page_tree_get_ptes(uvm_page_tree_t *tree, NvU32 page_size, NvU64 start, NvLength size,
        uvm_pmm_alloc_flags_t pmm_flags, uvm_page_table_range_t *range);

// Same as uvm_page_tree_get_ptes(), but doesn't synchronize the GPU work.
//
// All pending operations can be waited on with uvm_page_tree_wait().
NV_STATUS uvm_page_tree_get_ptes_async(uvm_page_tree_t *tree, NvU32 page_size, NvU64 start, NvLength size,
        uvm_pmm_alloc_flags_t pmm_flags, uvm_page_table_range_t *range);

// Returns a single-entry page table range for the addresses passed.
// The size parameter must be a page size supported by this tree.
// This is equivalent to calling uvm_page_tree_get_ptes() with size equal to page_size
NV_STATUS uvm_page_tree_get_entry(uvm_page_tree_t *tree, NvU32 page_size, NvU64 start,
        uvm_pmm_alloc_flags_t pmm_flags, uvm_page_table_range_t *single);

// For a single-entry page table range, write the PDE (which could be a dual PDE) to the GPU.
// This is useful when the GPU currently has the a PTE but that entry can also contain a PDE.
// It is an error to call this on a PTE-only range.
// The parameter single can otherwise be any single-entry range such as those allocated from get_entry() or get_ptes()
// This function performs no TLB invalidations.
void uvm_page_tree_write_pde(uvm_page_tree_t *tree, uvm_page_table_range_t *single, uvm_push_t *push);

// For a single-entry page table range, clear the PDE on the GPU.
// It is an error to call this on a PTE-only range.
// The parameter single can otherwise be any single-entry range such as those allocated from get_entry() or get_ptes()
// This function performs no TLB invalidations.
void uvm_page_tree_clear_pde(uvm_page_tree_t *tree, uvm_page_table_range_t *single, uvm_push_t *push);

// For a single-entry page table range, allocate a sibling table for a given
// page size. This sibling entry will be inserted into the host cache, but will
// not be written to the GPU page tree. uvm_page_tree_write_pde() can be used to
// submit entries to the GPU page tree, using single. The range returned refers
// to all the PTEs in the sibling directory directly.
//
// It is the caller's responsibility to initialize the returned table before
// calling uvm_page_tree_write_pde.
NV_STATUS uvm_page_tree_alloc_table(uvm_page_tree_t *tree,
                                    NvU32 page_size,
                                    uvm_pmm_alloc_flags_t pmm_flags,
                                    uvm_page_table_range_t *single,
                                    uvm_page_table_range_t *children);

// Gets PTEs from the upper portion of an existing range and returns them in a
// new range. num_upper_pages is the number of pages that should be in the new
// range. It must be in the range [1, existing->entry_count].
//
// The existing range is unmodified.
void uvm_page_table_range_get_upper(uvm_page_tree_t *tree,
                                    uvm_page_table_range_t *existing,
                                    uvm_page_table_range_t *upper,
                                    NvU32 num_upper_pages);

// Releases range's references on its upper pages to shrink the range down to
// to new_page_count, which must be >= range->entry_count. The range start
// remains the same.
//
// new_page_count is allowed to be 0, in which case this is equivalent to
// calling uvm_page_tree_put_ptes.
void uvm_page_table_range_shrink(uvm_page_tree_t *tree, uvm_page_table_range_t *range, NvU32 new_page_count);

// Releases the range of PTEs.
// It is the caller's responsibility to ensure that the empty PTE patterns have already been written in the range passed to the function.
void uvm_page_tree_put_ptes(uvm_page_tree_t *tree, uvm_page_table_range_t *range);

// Same as uvm_page_tree_put_ptes(), but doesn't synchronize the GPU work.
//
// All pending operations can be waited on with uvm_page_tree_wait().
void uvm_page_tree_put_ptes_async(uvm_page_tree_t *tree, uvm_page_table_range_t *range);

// Synchronize any pending operations
NV_STATUS uvm_page_tree_wait(uvm_page_tree_t *tree);

// Returns the physical allocation that contains the root directory
static uvm_mmu_page_table_alloc_t *uvm_page_tree_pdb(uvm_page_tree_t *tree)
{
    return &tree->root->phys_alloc;
}

// Initialize a page table range vector covering the specified VA range [start, start + size)
//
// This splits the VA in the minimum amount of page table ranges required to
// cover it and calls uvm_page_tree_get_ptes() for each of them.
//
// Start and size are in bytes and need to be page_size aligned.
NV_STATUS uvm_page_table_range_vec_init(uvm_page_tree_t *tree, NvU64 start, NvU64 size, NvU32 page_size,
        uvm_page_table_range_vec_t *range_vec);

// Allocate and initialize a page table range vector.
NV_STATUS uvm_page_table_range_vec_create(uvm_page_tree_t *tree, NvU64 start, NvU64 size, NvU32 page_size,
        uvm_page_table_range_vec_t **range_vec_out);

// Split a page table range vector in two using new_end as the split
// point. new_range_vec will contain the upper portion of range_vec, starting at
// new_end + 1.
//
// new_end + 1 is required to be within the address range of range_vec and be aligned to
// range_vec's page_size.
//
// On failure, the original range vector is left unmodified.
NV_STATUS uvm_page_table_range_vec_split_upper(uvm_page_table_range_vec_t *range_vec,
                                               NvU64 new_end,
                                               uvm_page_table_range_vec_t *new_range_vec);

// Deinitialize a page table range vector and set all fields to 0.
//
// Put all the PTEs that the range vector covered.
void uvm_page_table_range_vec_deinit(uvm_page_table_range_vec_t *range_vec);

// Deinitialize and free a page table range vector.
void uvm_page_table_range_vec_destroy(uvm_page_table_range_vec_t *range_vec);

// A PTE making function used by uvm_page_table_range_vec_write_ptes()
//
// The function gets called for each page_size aligned offset within the VA
// covered by the range vector and is supposed to return the desired PTE bits
// for each offset.
// The caller_data pointer is what the caller passed in as caller_data to uvm_page_table_range_vec_write_ptes().
typedef NvU64 (*uvm_page_table_range_pte_maker_t)(uvm_page_table_range_vec_t *range_vec, NvU64 offset, void *caller_data);

// Write all PTEs covered by the range vector using the given PTE making function.
//
// After writing all the PTEs a TLB invalidate operation is performed including
// the passed in tlb_membar.
//
// See comments about uvm_page_table_range_pte_maker_t for details about the PTE making callback.
NV_STATUS uvm_page_table_range_vec_write_ptes(uvm_page_table_range_vec_t *range_vec, uvm_membar_t tlb_membar,
        uvm_page_table_range_pte_maker_t pte_maker, void *caller_data);

// Set all PTEs covered by the range vector to an empty PTE
//
// After clearing all PTEs a TLB invalidate is performed including the given
// membar.
NV_STATUS uvm_page_table_range_vec_clear_ptes(uvm_page_table_range_vec_t *range_vec, uvm_membar_t tlb_membar);

// Create the necessary big page identity mappings for the GPU
NV_STATUS uvm_mmu_create_big_page_identity_mappings(uvm_gpu_t *gpu);

// Destroy the big page identity mappings
void uvm_mmu_destroy_big_page_identity_mappings(uvm_gpu_t *gpu);

// Create peer identity mappings
NV_STATUS uvm_mmu_create_peer_identity_mappings(uvm_gpu_t *gpu, uvm_gpu_t *peer);

// Destroy peer identity mappings
void uvm_mmu_destroy_peer_identity_mappings(uvm_gpu_t *gpu, uvm_gpu_t *peer);

// Translate a phyiscal vidmem address to a virtual one using the big page
// identity mapping if big page swizzling is enabled for the GPU. Otherwise,
// return the address as-is.
uvm_gpu_address_t uvm_mmu_gpu_address_for_big_page_physical(uvm_gpu_address_t physical, uvm_gpu_t *gpu);

static NvU64 uvm_mmu_page_tree_entries(uvm_page_tree_t *tree, NvU32 depth, NvU32 page_size)
{
    return 1ull << tree->hal->index_bits(depth, page_size);
}

static NvU64 uvm_mmu_pde_coverage(uvm_page_tree_t *tree, NvU32 page_size)
{
    NvU32 depth = tree->hal->page_table_depth(page_size);
    return uvm_mmu_page_tree_entries(tree, depth, page_size) * page_size;
}

static bool uvm_mmu_page_size_supported(uvm_page_tree_t *tree, NvU32 page_size)
{
    UVM_ASSERT_MSG(is_power_of_2(page_size), "0x%x\n", page_size);

    return (tree->hal->page_sizes() & page_size) != 0;
}

static NvU32 uvm_mmu_biggest_page_size_up_to(uvm_page_tree_t *tree, NvU32 max_page_size)
{
    NvU32 page_sizes;
    NvU32 page_size;

    UVM_ASSERT_MSG(is_power_of_2(max_page_size), "0x%x\n", max_page_size);

    // Calculate the supported page sizes that are not larger than the max
    page_sizes = tree->hal->page_sizes() & (max_page_size | (max_page_size - 1));

    // And pick the biggest one of them
    page_size = 1 << __fls(page_sizes);

    UVM_ASSERT_MSG(uvm_mmu_page_size_supported(tree, page_size), "page_size 0x%x", page_size);

    return page_size;
}

static NvU32 uvm_mmu_biggest_page_size(uvm_page_tree_t *tree)
{
    return 1 << __fls(tree->hal->page_sizes());
}

static NvU32 uvm_mmu_pte_size(uvm_page_tree_t *tree, NvU32 page_size)
{
    return tree->hal->entry_size(tree->hal->page_table_depth(page_size));
}

static NvU64 uvm_page_table_range_size(uvm_page_table_range_t *range)
{
    return range->entry_count * range->page_size;
}

// Get the physical address of the entry at entry_index within the range (counted from range->start_index).
static uvm_gpu_phys_address_t uvm_page_table_range_entry_address(uvm_page_tree_t *tree, uvm_page_table_range_t *range,
        size_t entry_index)
{
    NvU32 entry_size = uvm_mmu_pte_size(tree, range->page_size);
    uvm_gpu_phys_address_t entry = range->table->phys_alloc.addr;

    UVM_ASSERT(entry_index < range->entry_count);

    entry.address += (range->start_index + entry_index) * entry_size;
    return entry;
}

static uvm_aperture_t uvm_page_table_range_aperture(uvm_page_table_range_t *range)
{
    return range->table->phys_alloc.addr.aperture;
}

NV_STATUS uvm8_test_invalidate_tlb(UVM_TEST_INVALIDATE_TLB_PARAMS *params, struct file *filp);

#endif
