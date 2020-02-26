OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

PAGE_OFFSET = 0xFFFFFFC000000000;
PAGE_SIZE = 4096;
PAGE_TABLE_RAM_SIZE = 0x2000000;
USER_ADDR_START = 0x08048000;
USER_ADDR_SIZE = 128K;

/* QEMU_VIRT */
LOAD_ADDR_START = 0x40000000;

KERNEL_LINEAR_START = PAGE_OFFSET + LOAD_ADDR_START;

MEMORY
{
	vma_user : ORIGIN = USER_ADDR_START, LENGTH = USER_ADDR_SIZE
	lma_boot : ORIGIN = LOAD_ADDR_START, LENGTH = 64K
	lma_kernel : ORIGIN = ORIGIN(lma_boot) + LENGTH(lma_boot), LENGTH = 448K
	lma_user : ORIGIN = ORIGIN(lma_kernel) + LENGTH(lma_kernel), LENGTH = LENGTH(vma_user)
	vma_linear_mapping : ORIGIN = PAGE_OFFSET + ORIGIN(lma_kernel), LENGTH = 448K
}

/*
 * These symbols are used in kernel code(linear address space), Add
 * PAGE_OFFSET to remove "relocation truncated to fit" error.
 */
PROVIDE (_lma_user = PAGE_OFFSET + ORIGIN(lma_user));
PROVIDE (_vma_user = PAGE_OFFSET + ORIGIN(vma_user));
PROVIDE (_user_size = PAGE_OFFSET + LENGTH(vma_user));

SECTIONS
{
	.user_text : { user_text_start = .;
		./usr/*(.text)
		user_text_end = ALIGN(PAGE_SIZE);
		. = user_text_end; } >vma_user AT>lma_user
	.user_rodata : { user_rodata_start = .;
		./usr/*(.rodata)
		user_rodata_end = ALIGN(PAGE_SIZE);
		. = user_rodata_end; } >vma_user AT>lma_user
	.user_data : { user_data_start = .;
		./usr/*(.data)
		user_data_end = ALIGN(PAGE_SIZE);
		. = user_data_end; } >vma_user AT>lma_user
	.user_bss : { user_bss_start = .;
		./usr/*(.bss)
		user_bss_end = ALIGN(PAGE_SIZE);
		. = user_bss_end; } >vma_user AT>lma_user

	.text : { ./boot/boot.o(.text) ./boot/pre_mmu.o(.text) ./boot/print_early.o(.text) ./mm/mmu.o(.text) } >lma_boot AT>lma_boot
	.rodata : { ./boot/boot.o(.rodata) ./boot/pre_mmu.o(.rodata) ./boot/print_early.o(.rodata) ./mm/mmu.o(.rodata) } >lma_boot AT>lma_boot
	.data : { ./boot/boot.o(.data) ./boot/pre_mmu.o(.data) ./boot/print_early.o(.data) ./mm/mmu.o(.data) } >lma_boot AT>lma_boot
	.bss : { bss_start = .;
		./boot/boot.o(.bss) ./boot/pre_mmu.o(.bss) ./mm/mmu.o(.bss)
		bss_end = .; } >lma_boot AT>lma_boot

	.linear_text : { *(.text) } >vma_linear_mapping AT>lma_kernel
	.linear_rodata . : { *(.rodata) } >vma_linear_mapping AT>lma_kernel
	.linear_data . : { *(.data) } > vma_linear_mapping AT>lma_kernel
	.linear_bss . : { linear_bss_start = .;
		*(.bss)
		linear_bss_end = .; } > vma_linear_mapping AT>lma_kernel
	.user_init (ADDR(.linear_text) + LENGTH(lma_kernel)) : { . += LENGTH(vma_user); }
	.page_table ALIGN(PAGE_SIZE) : { . += PAGE_TABLE_RAM_SIZE;
		pg_mem = ADDR(.page_table) - PAGE_OFFSET; }
}

PROVIDE (_user_text_start = PAGE_OFFSET + user_text_start);
PROVIDE (_user_text_end = PAGE_OFFSET + user_text_end);
PROVIDE (_user_rodata_start = PAGE_OFFSET + user_rodata_start);
PROVIDE (_user_rodata_end = PAGE_OFFSET + user_rodata_end);
PROVIDE (_user_data_start = PAGE_OFFSET + user_data_start);
PROVIDE (_user_data_end = PAGE_OFFSET + user_data_end);
PROVIDE (_user_bss_start = PAGE_OFFSET + user_bss_start);
PROVIDE (_user_bss_end = PAGE_OFFSET + user_bss_end);
