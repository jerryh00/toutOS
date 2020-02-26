#include <mmu.h>
#include <hardware.h>
#include <misc.h>
#include <stddef.h>
#include <string.h>
#include <print_early.h>
#define printk print_early

#include "page_table.c"

#ifdef MMU_BY_BLOCK
/* 4GB adress space. */
static uint64_t _pgdir1[4]  __attribute__ ((aligned (PAGE_SIZE)));
#endif

extern char PAGE_TABLE_RAM_SIZE[];

static uint64_t pg_dir_id_map[PAGE_SIZE/8]  __attribute__
((aligned (PAGE_SIZE)));
static uint64_t pg_dir_linear_map[PAGE_SIZE/8]  __attribute__
((aligned (PAGE_SIZE)));
extern uint64_t pg_mem[];	/* Allocated by ld script */

void *dummy = unmap_one_user_page;

static void *pg_calloc(unsigned int order)
{
	size_t size;
	static size_t offset = 0;
	uint64_t allocated;

	size = PAGE_SIZE * (1UL << order);
	if (offset + size >= (size_t)PAGE_TABLE_RAM_SIZE) {
		printk("Error: out of page table memory.\n");
		printk("offset=%d\n", offset);
		printk("size=%d\n", size);
		return NULL;
	}

	allocated = ((uint64_t)pg_mem) + offset;
	offset += size;

#ifdef DEBUG_PAGE_TABLE
	printk("allocated=%p\n", allocated);
#endif
	memset((void *)allocated, 0, size);

	return (void *)allocated;
}

static uint64_t get_tcr(void)
{
	uint64_t ips, va_bits;
	uint64_t tcr;	/* TCR_ELn Translation Control Register */

	ips = 2;	/* 010 40bits, 1TB. */
	va_bits = VA_BITS;

	tcr =  (ips << 32) ;

	/* PTWs cacheable, inner/outer WBWA and inner shareable */
	tcr |= TCR_TG1_4K | TCR_TG0_4K | TCR_SHARED_INNER | TCR_ORGN_WBWA |
		TCR_IRGN_WBWA;
	tcr |= TCR_T0SZ(va_bits);
	tcr |= TCR_T1SZ(va_bits);

	return tcr;
}

#ifdef MMU_BY_BLOCK
void add_map(void)
{
	uint64_t attrs;

	/* RAM */
	attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_INNER_SHARE;
	attrs = attrs | PTE_TYPE_BLOCK | PTE_BLOCK_AF;

#ifdef QEMU_VIRT
	_pgdir1[1] = 0x40000000 | attrs;
	_pgdir1[2] = 0x80000000 | attrs;
#endif

	/* Device memory */
	attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) | PTE_BLOCK_NON_SHARE |
		PTE_BLOCK_PXN | PTE_BLOCK_UXN;
	attrs = attrs | PTE_TYPE_BLOCK | PTE_BLOCK_AF;

#ifdef QEMU_VIRT
	_pgdir1[0] = 0 | attrs;
#endif
}
#else

void add_map(void)
{
	int i;

	struct memory_map map[2] = {
		{	/* Device memory */
			.phy_addr = 0,
			.virt_addr = 0,
			.size = (1UL<<30),
			.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
				PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
				PTE_BLOCK_UXN,
		},
		{	/* RAM */
			.phy_addr = (1UL<<30),
			.virt_addr = (1UL<<30),
			.size = (4UL<<30),
			.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
				PTE_BLOCK_INNER_SHARE | PTE_BLOCK_UXN,
		},
	};

	for (i = 0; i < NUM_ELEMENTS(map); i++) {
		add_single_map(&map[i], pg_dir_id_map, pg_calloc, false);

		map[i].virt_addr += PAGE_OFFSET;

		add_single_map(&map[i], pg_dir_linear_map, pg_calloc, false);
	}
}
#endif

void enable_paging(void)
{
	uint64_t reg;

	/* Testcase for identity mapping. */
	assert((uint64_t)walk_virt_addr(pg_dir_id_map, &reg, false) ==
	       ((uint64_t)&reg & ~(PAGE_SIZE - 1)));
	assert((uint64_t)walk_virt_addr(pg_dir_linear_map,
				      (void *)0xFFFFFFC050FFFFC0, false) ==
	       ((uint64_t)(0xFFFFFFC050FFFFC0-PAGE_OFFSET) & ~(PAGE_SIZE - 1)));

#ifdef MMU_BY_BLOCK
	write_sys_reg(TTBR0_EL1, _pgdir1);
#else
	write_sys_reg(TTBR0_EL1, pg_dir_id_map);
	write_sys_reg(TTBR1_EL1, pg_dir_linear_map);
#endif
	write_sys_reg(TCR_EL1, get_tcr());

	write_sys_reg(MAIR_EL1, MEMORY_ATTRIBUTES);

#define CR_M	(1 << 0)	/* MMU enable */
	write_sys_reg(SCTLR_EL1, read_reg(SCTLR_EL1) | CR_M);
}

void *bss_should_be_zero;
extern char bss_start[];
extern char bss_end[];

void clear_bss(void)
{
	char *p;

	printk("bss_start=%p\n", bss_start);
	printk("bss_end=%p\n", bss_end);

	for (p = (char *)bss_start; p < (char *)bss_end; p++) {
		*p = 0;
	}

	printk("&bss_should_be_zero=%p\n", &bss_should_be_zero);
	printk("bss_should_be_zero=%p\n", bss_should_be_zero);
}
