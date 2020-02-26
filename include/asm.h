#ifndef __ASM_H__
#define __ASM_H__

/* re. kernel: include/linux/linkage.h */
#ifndef ENTRY
#define ENTRY(name) \
.globl name; \
name:
#endif

#ifndef END
#define END(name) \
.size name, .-name
#endif

#ifndef ENDPROC
#define ENDPROC(name) \
.type name, @function; \
END(name)
#endif

/*
 *Register aliases.
 */
lr      .req    x30             // link register

#endif
