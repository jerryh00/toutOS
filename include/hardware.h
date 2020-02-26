#ifndef __HARDWARE_H__
#define __HARDWARE_H__

#ifdef QEMU_VIRT
#define UART_BASE       0x09000000
#define UART_UARTDR	(UART_BASE + 0x00)
#define UART_UARTIBRD	(UART_BASE + 0x24)
#define UART_UARTFBRD	(UART_BASE + 0x28)
#define UART_UARTLCR_H	(UART_BASE + 0x2C)
#define UART_UARTCR	(UART_BASE + 0x30)
#define UART_UARTIFLS	(UART_BASE + 0x34)
#define UART_UARTIMSC	(UART_BASE + 0x38)
#define UART_UARTRIS	(UART_BASE + 0x3C)
#define UART_UARTMIS	(UART_BASE + 0x40)
#define UART_UARTICR	(UART_BASE + 0x44)
#define UART_UARTPeriphID2	(UART_BASE + 0xFE8)

#define VIRT_GIC_DIST_BASE 0x08000000
#define VIRT_GIC_CPU_BASE 0x08010000

#define VIRT_RTC_BASE 0x09010000
#define VIRT_RTC_RTCMR (VIRT_RTC_BASE + 0x4)
#define VIRT_RTC_RTCLR (VIRT_RTC_BASE + 0x8)
#define VIRT_RTC_RTCIMSC (VIRT_RTC_BASE + 0x10)
#define VIRT_RTC_RTCICR (VIRT_RTC_BASE + 0x1C)

#define VIRT_INT_TIMER 30
#define VIRT_INT_UART 33
#define VIRT_INT_RTC 34

#define PHYS_SDRAM_1	0x00000000UL
#define PHYS_SDRAM_1_SIZE	0x00000000UL
#define NS_IMAGE_OFFSET	0x00000000UL

#define PHYS_SDRAM_2	0x60000000UL
#define PHYS_SDRAM_2_SIZE	0xE0000000UL
#endif

#define TCR_EL1_RSVD		(1 << 31)
#define TCR_EPD1_DISABLE	(1 << 23)
#define TCR_TG0_4K		(0 << 14)
#define TCR_TG1_4K		(0 << 30)
#define TCR_SHARED_INNER	(3 << 12)
#define TCR_ORGN_WBWA		(1 << 10)
#define TCR_IRGN_WBWA		(1 << 8)
#define TCR_T0SZ(x)		((64 - (x)) << 0)
#define TCR_T1SZ(x)		((64 - (x)) << 16)

/*
 * Memory types
 */
#define MT_DEVICE_NGNRNE	0
#define MT_DEVICE_NGNRE		1
#define MT_DEVICE_GRE		2
#define MT_NORMAL_NC		3
#define MT_NORMAL		4

#define MEMORY_ATTRIBUTES	((0x00 << (MT_DEVICE_NGNRNE * 8)) |	\
				(0x04 << (MT_DEVICE_NGNRE * 8))   |	\
				(0x0c << (MT_DEVICE_GRE * 8))     |	\
				(0x44 << (MT_NORMAL_NC * 8))      |	\
				(0xffUL << (MT_NORMAL * 8)))

#define PTE_BLOCK_MEMTYPE(x)	((x) << 2)
#define PTE_BLOCK_INNER_SHARE	(3 << 8)
#define PTE_TYPE_MASK		(3 << 0)
#define PTE_TYPE_FAULT		(0 << 0)
#define PTE_TYPE_TABLE		(3 << 0)
#define PTE_TYPE_BLOCK		(1 << 0)
#define PTE_BLOCK_AF		(1 << 10)
#define PTE_BLOCK_NG		(1 << 11)
#define PTE_BLOCK_NON_SHARE	(0 << 8)
#define PTE_BLOCK_PXN		(1UL << 53)
#define PTE_BLOCK_UXN		(1UL << 54)
#define PTE_USER		(1UL << 6)
#define PTE_RDONLY		(1UL << 7)

#define PAGE_SIZE 4096

#ifdef QEMU_VIRT
#define RAM_ADDR_START 0x40000000
#define KERNEL_START RAM_ADDR_START
#endif

#endif
