/* Testing for Linux style linked-list with list_entry, etc */

#include <stdio.h>
#include <stdlib.h>


#define list_entry(ptr, type, member) \
	((type*) ((char*) (ptr) - (unsigned long)(&((type*)0)->member)))

static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next) 
{
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

struct list_head {
	struct list_head* next, *prev;
};

struct test_s {
	void* data;
	int priv_num;
	struct list_head queue;
};


int main(int argc, char* argv[]) {
	struct test_s t[10];
	int i = 0;
	for (i = 0; i < 10; i++){
		t[i].priv_num = i;
		t[i].queue.prev = &t[ (i == 0) ? 0 : (i-1)].queue;
		t[i].queue.next = &t[ (i == 10) ? 0 : (i+1)].queue;

		printf("%d: %x\n", i, &t[i]);
	}
	printf("done");

	struct test_s *q = list_entry(&t[2].queue, struct test_s, queue);
	printf("%x -> %x",q,  q->priv_num);
	return 0;
}
