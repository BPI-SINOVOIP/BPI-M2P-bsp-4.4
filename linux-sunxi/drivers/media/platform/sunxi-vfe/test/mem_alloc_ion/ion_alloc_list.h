#ifndef _ION_LIST_H
#define _ION_LIST_H

#define ion_offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(aw_ptr, type, member) ({ \
	const typeof(((type *)0)->member) * __mptr = (aw_ptr); \
	(type *)((char *)__mptr - ion_offsetof(type, member)); })

static inline void aw_prefetch(const void *x) {(void)x; }
static inline void aw_prefetchw(const void *x) {(void)x; }

#define AW_LIST_LOCATION1  ((void *) 0x00100100)
#define AW_LIST_LOCATION2  ((void *) 0x00200200)

struct aw_mem_list_head {
	struct aw_mem_list_head *aw_next, *aw_prev;
};

#define AW_MEM_LIST_HEAD_INIT(aw_name) { &(aw_name), &(aw_name) }

#define LIST_HEAD(aw_name) \
	struct aw_mem_list_head aw_name = AW_MEM_LIST_HEAD_INIT(aw_name)

#define AW_MEM_INIT_LIST_HEAD(aw_ptr) do { \
	(aw_ptr)->aw_next = (aw_ptr); (aw_ptr)->aw_prev = (aw_ptr); \
} while (0)

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the aw_prev/aw_next entries already!
 */
static inline void __aw_list_add(struct aw_mem_list_head *new,
      struct aw_mem_list_head *aw_prev,
      struct aw_mem_list_head *aw_next)
{
	aw_next->aw_prev = new;
	new->aw_next = aw_next;
	new->aw_prev = aw_prev;
	aw_prev->aw_next = new;
}

/**
 * list_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void aw_mem_list_add(struct aw_mem_list_head *new, struct aw_mem_list_head *head)
{
	__aw_list_add(new, head, head->aw_next);
}

/**
 * aw_mem_list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void aw_mem_list_add_tail(struct aw_mem_list_head *new, struct aw_mem_list_head *head)
{
	__aw_list_add(new, head->aw_prev, head);
}

static inline void __aw_mem_list_del(struct aw_mem_list_head *aw_prev, struct aw_mem_list_head *aw_next)
{
	aw_next->aw_prev = aw_prev;
	aw_prev->aw_next = aw_next;
}

static inline void aw_mem_list_del(struct aw_mem_list_head *entry)
{
	__aw_mem_list_del(entry->aw_prev, entry->aw_next);
	entry->aw_next = AW_LIST_LOCATION1;
	entry->aw_prev = AW_LIST_LOCATION2;
}

#define aw_mem_list_entry(aw_ptr, type, member) container_of(aw_ptr, type, member)

#define aw_mem_list_for_each_safe(aw_pos, aw_n, aw_head) \
	for (aw_pos = (aw_head)->aw_next, aw_n = aw_pos->aw_next; aw_pos != (aw_head); \
		aw_pos = aw_n, aw_n = aw_pos->aw_next)

#define aw_mem_list_for_each_entry(aw_pos, aw_head, member) \
	for (aw_pos = aw_mem_list_entry((aw_head)->aw_next, typeof(*aw_pos), member); \
		aw_prefetch(aw_pos->member.aw_next), &aw_pos->member != (aw_head);  \
		aw_pos = aw_mem_list_entry(aw_pos->member.aw_next, typeof(*aw_pos), member))
#endif
