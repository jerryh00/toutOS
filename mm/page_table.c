#include <stddef.h>
#include <misc.h>
#include <arch.h>

/* Hard-coded for a 4k page size and 40 va-bit setup. */
static int layers[] = {30, 21, 12};

static int get_pg_entry_index(uint64_t addr, int layer)
{
	return ((addr >> layer) & ((1 << 9) - 1));
}

static uint64_t get_phy_addr(uint64_t descriptor)
{
	return (descriptor & ~(PAGE_SIZE - 1) & ((1ULL << 48) - 1));
}

static void setup_pg_for_addr(uint64_t *pg_dir, uint64_t phy_addr,
			      uint64_t virt_addr, uint64_t attrs,
			      void *(*pg_calloc_func)(unsigned int order),
			      int mmu_on)
{
	int i;
	int pg_entry_index;
	uint64_t *next_layer_pt;
	uint64_t new_pt;

	next_layer_pt = pg_dir;

	for (i = 0; i < NUM_ELEMENTS(layers); i++) {
		pg_entry_index = get_pg_entry_index(virt_addr, layers[i]);
#ifdef DEBUG_PAGE_TABLE
		printk("pg_entry_index=%d\n", pg_entry_index);
		printk("next_layer_pt=%p\n", next_layer_pt);
		printk("next_layer_pt[pg_entry_index]=%p\n",
		       next_layer_pt[pg_entry_index]);
		printk("next_layer_pt[0]=%p\n", next_layer_pt[0]);
#endif
		if (next_layer_pt[pg_entry_index] == 0) {
			if (layers[i] != 12) {
				new_pt = (uint64_t)pg_calloc_func(0);
				if (new_pt == 0) {
					assert(0);
				}
				if (!is_pointer_aligned(new_pt, ~0xFFF)) {
					printk("new_pt=%p is not aligned\n",
					       new_pt);
					assert(0);
				}
				if (mmu_on) {
					new_pt = (uint64_t)__pa(new_pt);
				}
				next_layer_pt[pg_entry_index] = new_pt |
					attrs | PTE_TYPE_TABLE | PTE_BLOCK_AF;
			} else {
				next_layer_pt[pg_entry_index] = phy_addr |
					attrs | PTE_TYPE_TABLE | PTE_BLOCK_AF;
			}
		}

		next_layer_pt = (uint64_t *)
			get_phy_addr(next_layer_pt[pg_entry_index]);
		if (mmu_on) {
			next_layer_pt = __va(next_layer_pt);
		}
	}
}

/*
 * Traverse the page table for a given virtual address.
 * Returns the page-aligned physical address.
 */
static void *walk_virt_addr(uint64_t *pg_dir, void *virt_addr, int mmu_on)
{
	int i;
	int pg_entry_index;
	uint64_t *next_layer_pt;

#ifdef DEBUG_PAGE_TABLE
	printk("virt_addr=%p\n", virt_addr);
#endif
	next_layer_pt = pg_dir;

	for (i = 0; i < NUM_ELEMENTS(layers); i++) {
		pg_entry_index = get_pg_entry_index((uint64_t)virt_addr,
						    layers[i]);
#ifdef DEBUG_PAGE_TABLE
		printk("pg_entry_index=%d\n", pg_entry_index);
#endif
#if defined DEBUG_PAGE_TABLE || defined SETUP_USER_PAGE_MAPPING
		printk("entry=%p\n", next_layer_pt[pg_entry_index]);
#endif
		next_layer_pt = (uint64_t *)
			get_phy_addr(next_layer_pt[pg_entry_index]);
		if (mmu_on) {
			next_layer_pt = __va(next_layer_pt);
		}
#ifdef DEBUG_PAGE_TABLE
		printk("next_layer_pt=%p\n", next_layer_pt);
#endif
	}

	if (mmu_on) {
		next_layer_pt = __pa(next_layer_pt);
	}
	return next_layer_pt;
}

static int unmap_one_user_page(int asid, uint64_t *pg_dir, void *virt_addr,
			void (*free_pages)(void *addr, unsigned int order))
{
	int i;
	int pg_entry_index;
	uint64_t *next_layer_pt;

#ifdef DEBUG_PAGE_TABLE
	printk("virt_addr=%p\n", virt_addr);
#endif
	next_layer_pt = pg_dir;

	for (i = 0; i < NUM_ELEMENTS(layers) - 1; i++) {
		pg_entry_index = get_pg_entry_index((uint64_t)virt_addr,
						    layers[i]);
#ifdef DEBUG_PAGE_TABLE
		printk("pg_entry_index=%d\n", pg_entry_index);
		printk("entry=%p\n", next_layer_pt[pg_entry_index]);
#endif
		next_layer_pt = (uint64_t *)
			get_phy_addr(next_layer_pt[pg_entry_index]);
		next_layer_pt = __va(next_layer_pt);
#ifdef DEBUG_PAGE_TABLE
		printk("next_layer_pt=%p\n", next_layer_pt);
#endif
	}

	pg_entry_index = get_pg_entry_index((uint64_t)virt_addr, layers[i]);
	if (next_layer_pt[pg_entry_index] != 0) {
#ifdef DEBUG_PAGE_TABLE
		printk("Freeing %p\n", __va(get_phy_addr(next_layer_pt[pg_entry_index])));
#endif
		free_pages(__va(get_phy_addr(next_layer_pt[pg_entry_index])), 0);
	} else {
		printk("user page table entry to free is NULL\n");
	}
	next_layer_pt[pg_entry_index] = 0;
	invalidate_tlb_by_va(asid, virt_addr);

	return 0;
}

#ifndef MMU_BY_BLOCK
static void add_single_map(const struct memory_map *m, uint64_t *pg_dir_start,
			   void *(*pg_calloc_func)(unsigned int order),
			   int mmu_on)
{
	uint64_t phy_addr;
	uint64_t virt_addr;
	uint64_t end_addr;

	end_addr = m->phy_addr + m->size;
	for (phy_addr = m->phy_addr, virt_addr = m->virt_addr;
	     phy_addr < end_addr;
	     phy_addr += PAGE_SIZE, virt_addr += PAGE_SIZE) {
		setup_pg_for_addr(pg_dir_start, phy_addr, virt_addr, m->attrs,
				  pg_calloc_func, mmu_on);
	}
}
#endif
