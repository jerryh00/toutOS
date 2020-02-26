IMAGE = kernel.elf
LINK_SCRIPT = boot_kernel.ld

CROSS_COMPILE = aarch64-linux-gnu-

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
OBJDUMP = $(CROSS_COMPILE)objdump
OBJCOPY = $(CROSS_COMPILE)objcopy

CFLAGS = -Wall -fno-common -O0 -g \
         -nostdlib -nostartfiles -ffreestanding \
	 -mgeneral-regs-only \
         -march=armv8-a -nostdinc -fno-builtin -I include \
	 --include include/arch.h

# -Werror: turn all warnings into errors.
CFLAGS += -Werror

CPPFLAGS += -D ARM64
#CPPFLAGS += -D TEST_TIMER
#CPPFLAGS += -D TEST_MUTEX
#CPPFLAGS += -D TEST_KMALLOC_GET_PAGES

NUM_CPUS = 2
CPPFLAGS += -D QEMU_VIRT -D NUM_CPUS=$(NUM_CPUS)

C_SRC := $(shell find . -iname '*.c' |grep -v 'page_table.c\|mm.c')
ASM_SRC := $(shell find . -iname '*.S' |grep -v 'kernel.S')
OBJS = $(patsubst %.c, %.o, $(C_SRC)) $(patsubst %.S, %.o, $(ASM_SRC))

%.o : %.c include/*.h Makefile
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

all: $(IMAGE)

mm/mmu.c: mm/page_table.c
	touch mm/mmu.c

kernel/sched.c: mm/page_table.c
	touch kernel/sched.c

mm/memory.c: mm/mm.c
	touch mm/memory.c

$(IMAGE): $(LINK_SCRIPT) $(OBJS)
	$(LD) $(OBJS) -T $(LINK_SCRIPT) -o $(IMAGE)
	$(OBJDUMP) -d -S $(IMAGE) > kernel.S
	$(OBJDUMP) -t $(IMAGE) | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > kernel.sym
	$(OBJCOPY) -O binary $(IMAGE) kernel.bin

qemu: $(IMAGE)
	qemu-system-aarch64 -machine virt -cpu cortex-a57 \
	                    -smp $(NUM_CPUS) -m 1024 \
			    -nographic -serial mon:stdio \
	                    -kernel $(IMAGE)

clean:
	rm -f $(IMAGE) *.o kernel.S *.sym *.bin
	make -C tests clean

test:
	make -C tests

.PHONY: all qemu clean
