
#ifndef __x_list__
#define __x_list__

struct list_head {
	struct list_head* next, *prev;
};

#define list_entry(ptr, type, member) \
	((type*) ((char*) (ptr) - (unsigned long)(&((type*)0)->member)))

static inline void __list_add(struct list_head *new, struct list_head *prev, struct list_head *next) {
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

/* Insert into a list after head
 * Useful for implementing stacks */
static inline void list_add(struct list_head* new, struct list_head* head) {
	__list_add(new ,head, head->next);
}

/* Insert into a list before head 
 * Useful for implementing queues */
static inline void list_add_tail(struct list_head* new, struct list_head* head) {
	__list_add(new, head->prev, head);
}

#endif