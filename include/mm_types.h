#ifndef _MM_TYPES_H
#define _MM_TYPES_H

#include <list.h>

struct mm_struct;

struct vm_area_struct {
	unsigned long vm_start;
	unsigned long vm_end;

	struct vm_area_struct *vm_next, *vm_prev;

	struct mm_struct *vm_mm;
	unsigned long vm_page_prot;
	unsigned long vm_flags;

	unsigned long lma;
	struct list_head pages_block_list;
};

struct mm_struct {
	struct vm_area_struct *mmap;            /* list of VMAs */
	unsigned long start_brk;
	unsigned long brk;
};

#define VM_READ 0x00000001
#define VM_WRITE 0x00000002
#define VM_EXEC 0x00000004
#define VM_SHARED 0x00000008

struct pages_block {
	struct list_head list;
	void *user_virt_addr;
	void *linear_addr;
	unsigned int order;
};

#endif
