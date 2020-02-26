#include <stddef.h>
#include <mm.h>
#include <string.h>

#if defined IN_KERNEL
#include <printk.h>
#define PRINT printk
#else
#include <stdio.h>
#define PRINT printf
#endif

enum { MEM_BLK_MAGIC = 0xABCD1234 };

struct mem_blk {
	int magic_num;
	int in_use;
	size_t blk_len;
	void *user_start;
	size_t user_len;
};

struct mem_blk_tail {
	struct mem_blk *header;
};

static struct mem_blk_tail *get_tail(const struct mem_blk *p)
{
	struct mem_blk_tail *tail;

	tail = (struct mem_blk_tail *)
		((size_t)p + p->blk_len - (sizeof (struct mem_blk_tail)));

	return tail;
}

static void init_free_blk(void *p, size_t size)
{
	struct mem_blk *free_mem;
	int i;

	free_mem = (struct mem_blk *)p;

	/* For memset/user_memset */
	for (i = 0; i < size; i++) {
		*((uint8_t *)free_mem + i) = 0;
	}

	free_mem->magic_num = MEM_BLK_MAGIC;
	free_mem->in_use = 0;
	free_mem->blk_len = size;
	get_tail(free_mem)->header = free_mem;
}

static void dump_mem_blk(const struct mem_blk *p)
{
	PRINT("mem_blk@%p\n", p);
	PRINT("magic_num=%x\n", p->magic_num);
	PRINT("in_use=%d\n", p->in_use);
	PRINT("blk_len=%u\n", p->blk_len);
	PRINT("user_start=%p\n", p->user_start);
	PRINT("user_len=%u\n", p->user_len);
}

static struct mem_blk *prev_mem_blk(const struct mem_pool *pool,
				    const struct mem_blk *p)
{
	struct mem_blk_tail *tail;

	tail = (struct mem_blk_tail *)
		((void *)p - sizeof (struct mem_blk_tail));

	if ((void *)tail < pool->start) {
		return NULL;
	}

	return tail->header;
}

static struct mem_blk *next_mem_blk(const struct mem_pool *pool,
				    const struct mem_blk *p)
{
	struct mem_blk *header;

	header = (struct mem_blk *)((void *)p + p->blk_len);

	if (((void *)header) >= pool->end) {
		return NULL;
	}

	return header;
}

/* Whether the memory block is big enough */
static int big_mem_blk(const struct mem_blk *p, size_t size)
{
	size_t user_avail;

	user_avail = p->blk_len - sizeof (struct mem_blk)
		- sizeof (struct mem_blk_tail);
	if (user_avail >= size) {
		return true;
	} else {
		return false;
	}
}

static int is_blk_free(const struct mem_blk *p)
{
	return (p->in_use == 0);
}

static void merge_free_blk(struct mem_pool *pool, struct mem_blk *free)
{
	struct mem_blk *p;
	struct mem_blk *most_prev = free;
	struct mem_blk *most_next = free;

	for (p = free; p != NULL; p = prev_mem_blk(pool, p)) {
		if (is_blk_free(p)) {
			most_prev = p;
		} else {
			break;
		}
	}

	for (p = free; p != NULL; p = next_mem_blk(pool, p)) {
		if (is_blk_free(p)) {
			most_next = p;
		} else {
			break;
		}
	}

#ifdef DEBUG_MEMORY_ALLOCATION
	PRINT("most_prev=%p\n", most_prev);
	PRINT("most_next=%p\n", most_next);
#endif
	init_free_blk(most_prev,
		      (size_t)most_next + most_next->blk_len
		      - (size_t)most_prev);
}

static int enlarge_mem_pool(struct mem_pool *p)
{
	void *ret;
	size_t increment = (size_t)(p->end - p->start);

	ret = p->sbrk_func(increment);
	if (ret == (void *)-1) {
		return -1;
	}
#ifdef DEBUG_MEMORY_ALLOCATION
	PRINT("increment=%u.\n", increment);
#endif
	p->end += increment;
	init_free_blk(ret, increment);
	merge_free_blk(p, ret);

	return 0;
}

/* Search the block pool for a big enough block. */
static struct mem_blk *find_a_free_blk(struct mem_pool *pool, size_t size)
{
	struct mem_blk *p;

	for (p = (struct mem_blk *)pool->start; p != NULL;
	     p = next_mem_blk(pool, p)) {
		if (p->in_use == 0 && big_mem_blk(p, size)) {
			return p;
		}
	}

	if (enlarge_mem_pool(pool) == 0) {
		return find_a_free_blk(pool, size);
	} else {
		return NULL;
	}
}

/*
 * Whether the memory block should be splitted into two blocks.
 * (one block for allocation and one new free block)
 */
static int should_split(const struct mem_blk *p, size_t size)
{
	if (p->blk_len > sizeof (*p) + size + sizeof (struct mem_blk_tail)
			+ sizeof (*p) + sizeof (struct mem_blk_tail)) {
		/* The remaining space is bigger than the size of memory header. */
		return true;
	} else {
		return false;
	}
}

/* Split a free memory block into two. */
static void split_mem_blk(struct mem_pool *pool, struct mem_blk *p, size_t size)
{
	struct mem_blk *next;
	size_t old_blk_len;
	size_t new_blk_len;

	old_blk_len = p->blk_len;
	new_blk_len = sizeof (struct mem_blk) + size +
		sizeof (struct mem_blk_tail);
#ifdef DEBUG_MEMORY_ALLOCATION
	PRINT("new_blk_len=%u.\n", new_blk_len);
#endif

	init_free_blk(p, new_blk_len);
	next = next_mem_blk(pool, p);
	init_free_blk(next, old_blk_len - new_blk_len);

#ifdef DEBUG_MEMORY_ALLOCATION
	PRINT("Dumping newly created memory block by split.\n");
	dump_mem_blk(next);
#endif
}

static void mark_mem_blk(struct mem_pool *pool, struct mem_blk *p, size_t size)
{
	if (should_split(p, size)) {
		split_mem_blk(pool, p, size);
	}

	p->in_use = 1;
	p->user_start = (void *)p + sizeof (struct mem_blk);
	p->user_len = size;

#ifdef DEBUG_MEMORY_ALLOCATION
	PRINT("Dumping marked memory block\n");
	dump_mem_blk(p);
#endif
}

/*
 * size is guaranteed by the caller to be aligned(4-bytes for 32bit system,
 * 8-bytes for 64bit system).
 */
static struct mem_blk *get_mem_blk(struct mem_pool *pool, size_t size)
{
	struct mem_blk *p;

	p = find_a_free_blk(pool, size);
#ifdef DEBUG_MEMORY_ALLOCATION
	PRINT("found a free block: %p\n", p);
#endif
	if (p != NULL) {
		mark_mem_blk(pool, p, size);
		return p;
	} else {
		return NULL;
	}
}

static int is_mem_blk_aligned(void)
{
	size_t size;
	int ret;

	size = sizeof (struct mem_blk);
#ifdef ARM64
	ret = (ALIGNED_TO_8BYTES(size) == size);
#else
	ret = (ALIGNED_TO_4BYTES(size) == size);
#endif

	return ret;
}

static void init_mm(struct mem_pool *cur_mem_pool,
				void *sbrk_func(intptr_t),
				void *lock,
				void (*lock_init_func)(void *lock),
				void (*lock_func)(void *lock),
				void (*unlock_func)(void *lock))
{
	void *start;
	size_t initial_blk_len;

	if (!is_mem_blk_aligned()) {
		PRINT("Error: struct mem_blk is not aligned, there will be a gap between struct mem_blk and user_start\n");
		return;
	}

	cur_mem_pool->sbrk_func = sbrk_func;
	cur_mem_pool->total_length = 0;
	initial_blk_len = (sizeof (struct mem_blk))
		+ (sizeof (struct mem_blk_tail));

	start = cur_mem_pool->sbrk_func(initial_blk_len);
	if (start == (void *)-1) {
		return;
	}
	cur_mem_pool->start = start;
	cur_mem_pool->end = start + initial_blk_len;

	init_free_blk(start, initial_blk_len);

	cur_mem_pool->lock = lock;
	cur_mem_pool->lock_init_func = lock_init_func;
	cur_mem_pool->lock_func = lock_func;
	cur_mem_pool->unlock_func = unlock_func;

	if (cur_mem_pool->lock_init_func != NULL) {
		cur_mem_pool->lock_init_func(cur_mem_pool->lock);
	}
}

static int mem_blk_magic_ok(const struct mem_blk *p)
{
	int ret = (p->magic_num == MEM_BLK_MAGIC);

	if (!ret) {
		PRINT("Check memory block magic failed, expected=%x, got=%x\n",
			MEM_BLK_MAGIC, p->magic_num);
		dump_mem_blk(p);
	}

	return ret;
}

static int mem_blk_ok(const struct mem_blk *p)
{
	if (!mem_blk_magic_ok(p)) {
		return false;
	}

	if (p->in_use != 1) {
		PRINT("Memory block is not in use.\n");
		return false;
	}

	return true;
}

static void free_mem_blk(struct mem_pool *pool, struct mem_blk *p)
{
	p->in_use = 0;

	merge_free_blk(pool, p);
}

static void *alloc_mem(struct mem_pool *pool, size_t size)
{
	size_t aligned_user_len;
	struct mem_blk *p;

#ifdef DEBUG_MEMORY_ALLOCATION
	PRINT("%s: size=%u\n", __FUNCTION__, size);
#endif

	if (size == 0) {
		return NULL;
	}

#ifdef ARM64
	aligned_user_len = ALIGNED_TO_8BYTES(size);
#else
	aligned_user_len = ALIGNED_TO_4BYTES(size);
#endif
	if (pool->lock_func != NULL) {
		pool->lock_func(pool->lock);
	}
	p = get_mem_blk(pool, aligned_user_len);

	if (p != NULL) {
		p->user_len = size;
		pool->total_length += size;
#ifdef DEBUG_MEMORY_ALLOCATION
		PRINT("total_length=%u\n", pool->total_length);
#endif
		if (pool->unlock_func != NULL) {
			pool->unlock_func(pool->lock);
		}
		return p->user_start;
	} else {
		if (pool->unlock_func != NULL) {
			pool->unlock_func(pool->lock);
		}
		return NULL;
	}
}

static void free_mem(struct mem_pool *pool, void *user_start)
{
	struct mem_blk *mem_blk;

#ifdef DEBUG_MEMORY_ALLOCATION
	PRINT("%s: user_start=%p\n", __FUNCTION__, user_start);
#endif

	if (user_start == NULL) {
		return;
	}

	mem_blk = user_start - sizeof (struct mem_blk);

	if (pool->lock_func != NULL) {
		pool->lock_func(pool->lock);
	}
	if (mem_blk_ok(mem_blk)) {
		pool->total_length -= mem_blk->user_len;
#ifdef DEBUG_MEMORY_ALLOCATION
		PRINT("total_length=%u\n", pool->total_length);
#endif
		free_mem_blk(pool, mem_blk);
	} else {
		PRINT("Invalid memory pointer to free: %p\n", user_start);
	}
	if (pool->unlock_func != NULL) {
		pool->unlock_func(pool->lock);
	}
}

static size_t get_pool_malloc_total_length(struct mem_pool *pool)
{
	struct mem_blk *p;
	size_t total_length = 0;

	for (p = (struct mem_blk *)pool->start; p != NULL;
	     p = next_mem_blk(pool, p)) {
		if (!is_blk_free(p)) {
			total_length += p->user_len;
		}
	}

	if (pool->total_length != total_length) {
		return (size_t)(-1);
	}

	return pool->total_length;
}
